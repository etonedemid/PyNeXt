/* GIL implementation for Nintendo Switch port of CPython 3.14.
   Based on Python-3.14.6/Python/ceval_gil.c, adapted for Switch/libnx. */

#include "Python.h"
#include "pycore_ceval.h"         // _PyEval_SignalReceived()
#include "pycore_gc.h"            // _Py_RunGC()
#include "pycore_initconfig.h"    // _PyStatus_OK()
#include "pycore_runtime.h"       // _PyRuntime

/* First some general settings */

#define DEFAULT_INTERVAL 5000
static unsigned long gil_interval = DEFAULT_INTERVAL;
#define INTERVAL (gil_interval >= 1 ? gil_interval : 1)

/* Enable forcing switching of threads at least every `gil_interval` */
#define FORCE_SWITCHING


#include "condvar.h"
#ifndef Py_HAVE_CONDVAR
#error You need either a POSIX-compatible or a Windows system!
#endif

#define MUTEX_T PyMUTEX_T
#define MUTEX_INIT(mut) \
    if (PyMUTEX_INIT(&(mut))) { \
        Py_FatalError("PyMUTEX_INIT(" #mut ") failed"); };
#define MUTEX_FINI(mut) \
    if (PyMUTEX_FINI(&(mut))) { \
        Py_FatalError("PyMUTEX_FINI(" #mut ") failed"); };
#define MUTEX_LOCK(mut) \
    if (PyMUTEX_LOCK(&(mut))) { \
        Py_FatalError("PyMUTEX_LOCK(" #mut ") failed"); };
#define MUTEX_UNLOCK(mut) \
    if (PyMUTEX_UNLOCK(&(mut))) { \
        Py_FatalError("PyMUTEX_UNLOCK(" #mut ") failed"); };

#define COND_T PyCOND_T
#define COND_INIT(cond) \
    if (PyCOND_INIT(&(cond))) { \
        Py_FatalError("PyCOND_INIT(" #cond ") failed"); };
#define COND_FINI(cond) \
    if (PyCOND_FINI(&(cond))) { \
        Py_FatalError("PyCOND_FINI(" #cond ") failed"); };
#define COND_SIGNAL(cond) \
    if (PyCOND_SIGNAL(&(cond))) { \
        Py_FatalError("PyCOND_SIGNAL(" #cond ") failed"); };
#define COND_WAIT(cond, mut) \
    if (PyCOND_WAIT(&(cond), &(mut))) { \
        Py_FatalError("PyCOND_WAIT(" #cond ") failed"); };
#define COND_TIMED_WAIT(cond, mut, microseconds, timeout_result) \
    { \
        int r = PyCOND_TIMEDWAIT(&(cond), &(mut), (microseconds)); \
        if (r < 0) \
            Py_FatalError("PyCOND_WAIT(" #cond ") failed"); \
        if (r) /* 1 == timeout, 2 == impl. can't say, so assume timeout */ \
            timeout_result = 1; \
        else \
            timeout_result = 0; \
    }


/*
 * Implementation of the Global Interpreter Lock (GIL).
 */

#include <stdlib.h>
#include <errno.h>

static void _gil_initialize(struct _gil_runtime_state *gil)
{
    gil->locked = -1;
    gil->interval = DEFAULT_INTERVAL;
}

static int gil_created(struct _gil_runtime_state *gil)
{
    if (gil == NULL) {
        return 0;
    }
    return (_Py_atomic_load_int_acquire(&gil->locked) >= 0);
}

static void create_gil(struct _gil_runtime_state *gil)
{
    MUTEX_INIT(gil->mutex);
#ifdef FORCE_SWITCHING
    MUTEX_INIT(gil->switch_mutex);
#endif
    COND_INIT(gil->cond);
#ifdef FORCE_SWITCHING
    COND_INIT(gil->switch_cond);
#endif
    _Py_atomic_store_ptr_relaxed(&gil->last_holder, 0);
    _Py_ANNOTATE_RWLOCK_CREATE(&gil->locked);
    _Py_atomic_store_int_release(&gil->locked, 0);
}

static void destroy_gil(struct _gil_runtime_state *gil)
{
    /* some pthread-like implementations tie the mutex to the cond
     * and must have the cond destroyed first. */
    COND_FINI(gil->cond);
    MUTEX_FINI(gil->mutex);
#ifdef FORCE_SWITCHING
    COND_FINI(gil->switch_cond);
    MUTEX_FINI(gil->switch_mutex);
#endif
    _Py_atomic_store_int_release(&gil->locked, -1);
    _Py_ANNOTATE_RWLOCK_DESTROY(&gil->locked);
}

static inline void
drop_gil_impl(PyThreadState *tstate, struct _gil_runtime_state *gil)
{
    MUTEX_LOCK(gil->mutex);
    _Py_ANNOTATE_RWLOCK_RELEASED(&gil->locked, /*is_write=*/1);
    _Py_atomic_store_int_relaxed(&gil->locked, 0);
    if (tstate != NULL) {
        tstate->holds_gil = 0;
    }
    COND_SIGNAL(gil->cond);
    MUTEX_UNLOCK(gil->mutex);
}

static void
drop_gil(PyInterpreterState *interp, PyThreadState *tstate, int final_release)
{
    struct _ceval_state *ceval = &interp->ceval;
    struct _gil_runtime_state *gil = ceval->gil;

    assert(final_release || tstate != NULL);

    if (!_Py_atomic_load_int_relaxed(&gil->locked)) {
        Py_FatalError("drop_gil: GIL is not locked");
    }

    if (!final_release) {
        _Py_atomic_store_ptr_relaxed(&gil->last_holder, tstate);
    }

    drop_gil_impl(tstate, gil);

#ifdef FORCE_SWITCHING
    if (!final_release &&
        _Py_eval_breaker_bit_is_set(tstate, _PY_GIL_DROP_REQUEST_BIT)) {
        MUTEX_LOCK(gil->switch_mutex);
        if (((PyThreadState*)_Py_atomic_load_ptr_relaxed(&gil->last_holder)) == tstate)
        {
            assert(_PyThreadState_CheckConsistency(tstate));
            _Py_unset_eval_breaker_bit(tstate, _PY_GIL_DROP_REQUEST_BIT);
            COND_WAIT(gil->switch_cond, gil->switch_mutex);
        }
        MUTEX_UNLOCK(gil->switch_mutex);
    }
#endif
}

static void
take_gil(PyInterpreterState *interp, PyThreadState *tstate)
{
    struct _ceval_state *ceval = &interp->ceval;
    struct _gil_runtime_state *gil = ceval->gil;
    int timed_out = 0;

    MUTEX_LOCK(gil->mutex);

    while (_Py_atomic_load_int_acquire(&gil->locked)) {
        COND_TIMED_WAIT(gil->cond, gil->mutex, INTERVAL, timed_out);

        if (timed_out) {
            _Py_atomic_store_int_relaxed(&gil->locked, -1);
            timed_out = 0;
        }
    }

    /* We have the GIL */
    _Py_ANNOTATE_RWLOCK_ACQUIRED(&gil->locked, /*is_write=*/1);
    _Py_atomic_store_int_release(&gil->locked, 1);
    MUTEX_UNLOCK(gil->mutex);

    if (tstate != NULL) {
        tstate->holds_gil = 1;
    }
}

static void
switch_to_another_thread(struct _gil_runtime_state *gil)
{
#ifdef FORCE_SWITCHING
    COND_SIGNAL(gil->switch_cond);
#endif
}

/* Public API */

void
_PyEval_AcquireThread(PyThreadState *tstate)
{
    PyInterpreterState *interp = tstate->interp;
    take_gil(interp, tstate);
}

void
_PyEval_ReleaseThread(PyThreadState *tstate)
{
    PyInterpreterState *interp = tstate->interp;
    drop_gil(interp, tstate, 0);
}

int
PyGILState_Check(void)
{
    if (!_PyRuntime.gilstate.check_enabled) {
        return 1;
    }

    PyThreadState *tstate = _PyThreadState_GET();
    if (tstate == NULL || tstate->interp != _PyRuntime.gilstate.autoInterpreterState) {
        return 0;
    }
    return 1;
}

void
_PyGILState_Init(PyInterpreterState *interp, PyThreadState *tstate)
{
    assert(_PyRuntime.gilstate.autoInterpreterState == NULL);
    _PyRuntime.gilstate.autoInterpreterState = interp;
    _Py_atomic_store_ptr_relaxed(&_PyRuntime.ceval.last_holder, tstate);
}

void
_PyGILState_Fini(void)
{
    _PyRuntime.gilstate.autoInterpreterState = NULL;
}

/* GIL initialization and finalization */

int
_PyEval_InitGIL(PyInterpreterState *interp)
{
    struct _ceval_state *ceval = &interp->ceval;
    struct _gil_runtime_state *gil;

    gil = (struct _gil_runtime_state *)PyMem_RawMalloc(sizeof(struct _gil_runtime_state));
    if (!gil) {
        return 0;
    }

    _gil_initialize(gil);
    create_gil(gil);
    ceval->gil = gil;

    /* Take the GIL for the main thread */
    take_gil(interp, NULL);

    return 1;
}

void
_PyEval_FiniGIL(PyInterpreterState *interp)
{
    struct _ceval_state *ceval = &interp->ceval;
    struct _gil_runtime_state *gil = ceval->gil;

    if (gil != NULL) {
        destroy_gil(gil);
        PyMem_RawFree(gil);
        ceval->gil = NULL;
    }
}

/* sys.{get,set}switchinterval() */

double
_PyEval_GetSwitchInterval(void)
{
    return (double)_Py_atomic_load_int_relaxed(&_PyRuntime.ceval.gil_interval) / 1000.0;
}

void
_PyEval_SetSwitchInterval(double interval)
{
    int new_interval = (int)(interval * 1000);
    if (new_interval < 1) {
        new_interval = 1;
    }
    _Py_atomic_store_int_release(&_PyRuntime.ceval.gil_interval, new_interval);
}
