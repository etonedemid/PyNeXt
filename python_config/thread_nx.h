/* Nintendo Switch thread implementation for CPython 3.14 using libnx */

#ifndef Py_THREAD_NX_H
#define Py_THREAD_NX_H

#include "switch/kernel/mutex.h"
#include "switch/kernel/svc.h"
#include "switch/kernel/condvar.h"
#include "switch/types.h"
#include "switch/arm/tls.h"
#include "switch/kernel/thread.h"

/* Forward declaration - initialized is defined in thread.c via _PyRuntime */
extern int initialized;

#define THREAD_STACK_SIZE 0x4000
#define THREAD_PRIORITY 0x20
#define THREAD_CPU_ID -2


// copied from nx/source/internal.h
#define THREADVARS_MAGIC 0x21545624 // !TV$

// This structure is exactly 0x20 bytes, if more is needed modify getThreadVars() below
typedef struct {
    // Magic value used to check if the struct is initialized
    u32 magic;

    // Thread handle, for mutexes
    Handle handle;

    // Pointer to the current thread (if exists)
    Thread* thread_ptr;

    // Pointer to this thread's newlib state
    struct _reent* reent;

    // Pointer to this thread's thread-local segment
    void* tls_tp; // !! Offset needs to be TLS+0x1F8 for __aarch64_read_tp !!
} ThreadVars;

static inline ThreadVars* getThreadVars(void) {
    return (ThreadVars*)((u8*)armGetTls() + 0x1E0);
}
// end copy


/* Debug macros */
#ifdef Py_DEBUG
static int thread_debug = 0;
#define dprintf(args)   (void)((thread_debug & 1) && printf args)
#else
#define dprintf(args)
#endif

/*
 * Initialization.
 */
static void _noop(void) { }

static void
PyThread__init_thread(void)
{
    Thread thread1;
    threadCreate(&thread1, (void *) _noop, NULL, THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_CPU_ID);
    threadStart(&thread1);
    threadWaitForExit(&thread1);
    threadClose(&thread1);
}

/*
 * Thread support.
 */

typedef struct {
    ThreadFunc func;
    void * arg;
} _wrapperArgs;

static void _wrapperFunc(_wrapperArgs * args) {
    args->func(args->arg);
}

long
PyThread_start_new_thread(void (*func)(void *), void *arg)
{
    dprintf(("PyThread_start_new_thread called\n"));
    if (!initialized)
        PyThread_init_thread();

    _wrapperArgs wargs;
    wargs.arg = arg;
    wargs.func = func;

    Thread * t = (Thread *)PyMem_RawMalloc(sizeof(Thread));
    int rc = threadCreate(t, (ThreadFunc)_wrapperFunc, &wargs, THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_CPU_ID);
    if (rc < 0) return -1;
    rc = threadStart(t);

    return rc < 0 ? -1 : 0;
}

long
PyThread_get_thread_ident(void)
{
    if (!initialized)
        PyThread_init_thread();
    Thread * t = getThreadVars()->thread_ptr;
    return (long)t + 1;
}

void
PyThread_exit_thread(void)
{
    dprintf(("PyThread_exit_thread called\n"));
    svcExitThread();
}

/*
 * Lock support.
 */

typedef struct {
    char locked;
    CondVar cv;
    Mutex mutex;
} _thread_lock;

PyThread_type_lock
PyThread_allocate_lock(void)
{
    dprintf(("PyThread_allocate_lock called\n"));
    if (!initialized)
        PyThread_init_thread();

    _thread_lock * lock = (_thread_lock *)PyMem_RawMalloc(sizeof(_thread_lock));
    lock->locked = 0;

    mutexInit(&(lock->mutex));
    condvarInit(&(lock->cv), &(lock->mutex));

    dprintf(("PyThread_allocate_lock() -> %p\n", lock));
    return (PyThread_type_lock) lock;
}

void
PyThread_free_lock(PyThread_type_lock lock)
{
    _thread_lock * thelock = (_thread_lock *)lock;

    dprintf(("PyThread_free_lock(%p) called\n", lock));

    if (!thelock)
        return;
    
    PyMem_RawFree((void *)thelock);
}

int
PyThread_acquire_lock(PyThread_type_lock lock, int waitflag)
{
    return PyThread_acquire_lock_timed(lock, waitflag ? -1 : 0, 0);
}

PyLockStatus
PyThread_acquire_lock_timed(PyThread_type_lock lock, PY_TIMEOUT_T microseconds,
                            int intr_flag)
{
    PyLockStatus success;
    int status;
    _thread_lock * thelock = (_thread_lock *)lock;
    dprintf(("PyThread_acquire_lock_timed(%p, %lld, %d) called\n", 
            lock, microseconds, intr_flag));

    if (thelock->locked == 1 && microseconds == 0) {
        success = PY_LOCK_FAILURE;
        return success;
    }

    mutexLock(&(thelock->mutex));
    if (thelock->locked == 0) {
        success = PY_LOCK_ACQUIRED;
    } else if (microseconds == 0) {
        success = PY_LOCK_FAILURE;        
    } else {
        u64 ns = microseconds * 1000;

        success = PY_LOCK_FAILURE;
        while (success == PY_LOCK_FAILURE) {
            if (microseconds > 0) {
                status = condvarWaitTimeout(&(thelock->cv), ns);
                if (status == 0xEA01) // on timeout
                    break;
            }
            else {
                status = condvarWait(&(thelock->cv));
            }

            if (intr_flag && status == 0 && thelock->locked) {
                success = PY_LOCK_INTR;
                break;
            } else if (!thelock->locked) {
                success = PY_LOCK_ACQUIRED;
            } else {
                success = PY_LOCK_FAILURE;
            }
        }
    }

    if (success == PY_LOCK_ACQUIRED) {
        thelock->locked = 1;
    }
    mutexUnlock(&(thelock->mutex));

    return success;
}

void
PyThread_release_lock(PyThread_type_lock lock)
{
    _thread_lock * thelock = (_thread_lock *)lock;
    dprintf(("PyThread_release_lock(%p) called\n", lock));

    mutexLock(&(thelock->mutex));
    thelock->locked = 0;
    condvarWakeOne(&(thelock->cv));
    mutexUnlock(&(thelock->mutex));
}


/* Thread Specific Storage (TSS) for Switch using libnx TLS */

typedef struct {
    int _is_initialized;
} Py_tss_t;

int
PyThread_tss_create(Py_tss_t *key)
{
    if (!initialized)
        PyThread_init_thread();
    
    key->_is_initialized = 1;
    return 0;
}

void
PyThread_tss_delete(Py_tss_t *key)
{
    if (key != NULL) {
        key->_is_initialized = 0;
    }
}

int
PyThread_tss_set(Py_tss_t *key, void *value)
{
    /* Switch uses libnx TLS - for now we use a simple approach */
    return 0;
}

void *
PyThread_tss_get(Py_tss_t *key)
{
    return NULL;
}


/* Python 3.14 requires these platform-specific definitions */
#define THREAD_LOCK_NAME "libnx"

#endif /* Py_THREAD_NX_H */
