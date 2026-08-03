/* Thread package for Nintendo Switch port of CPython 3.14.
   Based on Python-3.14.6/Python/thread.c with SWITCH platform support added. */

#include "Python.h"
#include "pycore_ceval.h"         // _PyEval_MakePendingCalls()
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "pycore_runtime.h"       // _PyRuntime
#include "pycore_structseq.h"     // _PyStructSequence_FiniBuiltin()

#ifndef DONT_HAVE_STDIO_H
#  include <stdio.h>
#endif

#include <stdlib.h>


// Define PY_TIMEOUT_MAX constant for Switch.
#define PY_TIMEOUT_MAX_VALUE LLONG_MAX
const long long PY_TIMEOUT_MAX = PY_TIMEOUT_MAX_VALUE;


static void PyThread__init_thread(void); /* Forward */

#define initialized _PyRuntime.threads.initialized

void
PyThread_init_thread(void)
{
    if (initialized) {
        return;
    }
    initialized = 1;
    PyThread__init_thread();
}


/* Switch-specific thread implementation */
#ifdef SWITCH
#   define PYTHREAD_NAME "nx"
#   include "thread_nx.h"
#else
#   error "Require native threads. See https://bugs.python.org/issue31370"
#endif


/* return the current thread stack size */
size_t
PyThread_get_stacksize(void)
{
    return _PyInterpreterState_GET()->threads.stacksize;
}

/* Only platforms defining a THREAD_SET_STACKSIZE() macro
   in thread_<platform>.h support changing the stack size.
   Return 0 if stack size is valid,
      -1 if stack size value is invalid,
      -2 if setting stack size is not supported. */
int
PyThread_set_stacksize(size_t size)
{
#if defined(THREAD_SET_STACKSIZE)
    return THREAD_SET_STACKSIZE(size);
#else
    return -2;
#endif
}


int
PyThread_ParseTimeoutArg(PyObject *arg, int blocking, PY_TIMEOUT_T *timeout_p)
{
    assert(_PyTime_FromSeconds(-1) == PyThread_UNSET_TIMEOUT);
    if (arg == NULL || arg == Py_None) {
        *timeout_p = blocking ? PyThread_UNSET_TIMEOUT : 0;
        return 0;
    }
    if (!blocking) {
        PyErr_SetString(PyExc_ValueError,
                        "can't specify a timeout for a non-blocking call");
        return -1;
    }

    PyTime_t timeout;
    if (_PyTime_FromSecondsObject(&timeout, arg, _PyTime_ROUND_TIMEOUT) < 0) {
        return -1;
    }
    if (timeout < 0) {
        PyErr_SetString(PyExc_ValueError,
                        "timeout value must be a non-negative number");
        return -1;
    }

    if (_PyTime_AsMicroseconds(timeout,
                               _PyTime_ROUND_TIMEOUT) > PY_TIMEOUT_MAX) {
        PyErr_SetString(PyExc_OverflowError,
                        "timeout value is too large");
        return -1;
    }
    *timeout_p = timeout;
    return 0;
}

PyLockStatus
PyThread_acquire_lock_timed_with_retries(PyThread_type_lock lock,
                                         PY_TIMEOUT_T timeout)
{
    PyThreadState *tstate = _PyThreadState_GET();
    PyTime_t endtime = 0;
    if (timeout > 0) {
        endtime = _PyDeadline_Init(timeout);
    }

    PyLockStatus r;
    do {
        PyTime_t microseconds;
        microseconds = _PyTime_AsMicroseconds(timeout, _PyTime_ROUND_CEILING);

        /* first a simple non-blocking try without releasing the GIL */
        r = PyThread_acquire_lock_timed(lock, 0, 0);
        if (r == PY_LOCK_FAILURE && microseconds != 0) {
            Py_BEGIN_ALLOW_THREADS
            r = PyThread_acquire_lock_timed(lock, microseconds, 1);
            Py_END_ALLOW_THREADS
        }

        if (r == PY_LOCK_INTR) {
            /* Run signal handlers if we were interrupted. */
            if (_PyEval_MakePendingCalls(tstate) < 0) {
                return PY_LOCK_INTR;
            }

            /* If we're using a timeout, recompute the timeout after processing
             * signals, since those can take time. */
            if (timeout > 0) {
                timeout = _PyDeadline_Get(endtime);

                /* Check for negative values, since those mean block forever. */
                if (timeout < 0) {
                    r = PY_LOCK_FAILURE;
                }
            }
        }
    } while (r == PY_LOCK_INTR);  /* Retry if we were interrupted. */

    return r;
}


/* Thread Specific Storage (TSS) API

   Cross-platform components of TSS API implementation.
*/

Py_tss_t *
PyThread_tss_alloc(void)
{
    Py_tss_t *new_key = (Py_tss_t *)PyMem_RawMalloc(sizeof(Py_tss_t));
    if (new_key == NULL) {
        return NULL;
    }
    new_key->_is_initialized = 0;
    return new_key;
}

void
PyThread_tss_free(Py_tss_t *key)
{
    if (key != NULL) {
        PyThread_tss_delete(key);
        PyMem_RawFree((void *)key);
    }
}

int
PyThread_tss_is_created(Py_tss_t *key)
{
    assert(key != NULL);
    return key->_is_initialized;
}


PyDoc_STRVAR(threadinfo__doc__,
"sys.thread_info\n\
\n\
A named tuple holding information about the thread implementation.");

static PyStructSequence_Field threadinfo_fields[] = {
    {"name",    "name of the thread implementation"},
    {"lock",    "name of the lock implementation"},
    {"version", "name and version of the thread library"},
    {0}
};

static PyStructSequence_Desc threadinfo_desc = {
    "sys.thread_info",           /* name */
    threadinfo__doc__,           /* doc */
    threadinfo_fields,           /* fields */
    3
};

static PyTypeObject ThreadInfoType;

PyObject*
PyThread_GetInfo(void)
{
    PyObject *threadinfo, *value;
    int pos = 0;

    PyInterpreterState *interp = _PyInterpreterState_GET();
    if (_PyStructSequence_InitBuiltin(interp, &ThreadInfoType, &threadinfo_desc) < 0) {
        return NULL;
    }

    threadinfo = PyStructSequence_New(&ThreadInfoType);
    if (threadinfo == NULL)
        return NULL;

    value = PyUnicode_FromString(PYTHREAD_NAME);
    if (!value || PyStructSequence_SET_ITEM(threadinfo, pos++, value)) {
        goto error;
    }
    Py_DECREF(value);

#ifdef THREAD_LOCK_NAME
    value = PyUnicode_FromString(THREAD_LOCK_NAME);
#else
    value = Py_None;
#endif
    Py_INCREF(value);
    if (PyStructSequence_SET_ITEM(threadinfo, pos++, value)) {
        goto error;
    }
    Py_DECREF(value);

#ifdef THREAD_VERSION
    value = PyUnicode_FromString(THREAD_VERSION);
#else
    value = Py_None;
#endif
    Py_INCREF(value);
    if (PyStructSequence_SET_ITEM(threadinfo, pos++, value)) {
        goto error;
    }
    Py_DECREF(value);

    return threadinfo;

error:
    Py_CLEAR(threadinfo);
    return NULL;
}


/* Cleanup TSS keys at runtime finalization */
void
_PyThread_Fini(void)
{
    /* Platform-specific cleanup if needed */
}
