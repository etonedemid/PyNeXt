/*
 * Portable condition variable support for Nintendo Switch port of CPython 3.14.
 * Everything is inline, this header can be included where needed.
 */

#ifndef _CONDVAR_H_
#define _CONDVAR_H_

#include "Python.h"

#ifdef SWITCH
#define Py_HAVE_CONDVAR
#include "switch/kernel/mutex.h"
#include "switch/kernel/svc.h"
#include "switch/kernel/condvar.h"
#include "switch/types.h"

/* The following functions return 0 on success, nonzero on error */
#define PyMUTEX_T Mutex

Py_LOCAL_INLINE(int)
PyMUTEX_INIT(PyMUTEX_T *mut) {
    mutexInit(mut);
    return 0;
}

Py_LOCAL_INLINE(int)
PyMUTEX_FINI(PyMUTEX_T *mut) {
    /* libnx doesn't have a destroy function, just reinit */
    mutexInit(mut);
    return 0;
}

Py_LOCAL_INLINE(int)
PyMUTEX_LOCK(PyMUTEX_T *mut) {
    mutexLock(mut);
    return 0;
}

Py_LOCAL_INLINE(int)
PyMUTEX_UNLOCK(PyMUTEX_T *mut) {
    mutexUnlock(mut);
    return 0;
}


#define PyCOND_T CondVar

Py_LOCAL_INLINE(int)
PyCOND_INIT(PyCOND_T *cond, PyMUTEX_T *mut) {
    condvarInit(cond, mut);
    return 0;
}

/* For compatibility with code that doesn't pass mutex */
Py_LOCAL_INLINE(int)
PyCOND_INIT_STANDALONE(PyCOND_T *cond) {
    /* We need a dummy mutex for libnx condvars - caller must ensure this is safe */
    static Mutex dummy_mutex;
    if (!mutexIsInitialized(&dummy_mutex)) {
        mutexInit(&dummy_mutex);
    }
    condvarInit(cond, &dummy_mutex);
    return 0;
}

#define PyCOND_FINI(cond)       0
#define PyCOND_SIGNAL(cond)     condvarWakeOne((cond))
#define PyCOND_BROADCAST(cond)  condvarWakeAll((cond))
#define PyCOND_WAIT(cond, mut)  condvarWait((cond))

/* return 0 for success, 1 on timeout, -1 on error */
Py_LOCAL_INLINE(int)
PyCOND_TIMEDWAIT(PyCOND_T *cond, PyMUTEX_T *mut, PY_LONG_LONG us)
{
    int r;
    u64 ns = us * 1000; // microseconds to nanoseconds

    r = condvarWaitTimeout((cond), ns);
    if (r == 0xEA01)
        return 1;
    else if (r)
        return -1;
    else
        return 0;
}


#else
#error "Unsupported platform - need pthreads, Windows, or Switch"
#endif

#endif /* _CONDVAR_H_ */
