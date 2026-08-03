/* Python interpreter top-level routines for Nintendo Switch port of CPython 3.14.
   Minimal Switch-compatible version providing public APIs while delegating to
   Python's internal functions where possible. */

#include "Python.h"
#include "pycore_initconfig.h"    // _PyStatus_OK(), _PyConfig_InitCompatConfig()
#include "pycore_pylifecycle.h"   // Internal lifecycle helpers
#include "pycore_runtime.h"       // _PyRuntime, _PyRuntime_Initialize()
#include "pycore_runtime_init.h"  // _PyRuntimeState_INIT

/* Suppress deprecation warning for PyBytesObject.ob_shash */
_Py_COMP_DIAG_PUSH
_Py_COMP_DIAG_IGNORE_DEPR_DECLS

/* The following places the `_PyRuntime` structure in a location that can be
 * found without any external information. This is meant to ease access to the
 * interpreter state for various runtime debugging tools, but is *not* an
 * officially supported feature */

GENERATE_DEBUG_SECTION(PyRuntime, _PyRuntimeState _PyRuntime)
= _PyRuntimeState_INIT(_PyRuntime, _Py_Debug_Cookie);
_Py_COMP_DIAG_POP


static int runtime_initialized = 0;

/* Initialize the global runtime state. Called before any interpreter init. */
PyStatus
_PyRuntime_Initialize(void)
{
    if (runtime_initialized) {
        return _PyStatus_OK();
    }
    runtime_initialized = 1;
    return _PyRuntimeState_Init(&_PyRuntime);
}

void
_PyRuntime_Finalize(void)
{
    _PyRuntimeState_Fini(&_PyRuntime);
    runtime_initialized = 0;
}


/* APIs to access the initialization flags */

int
_Py_IsCoreInitialized(void)
{
    return _PyRuntime.core_initialized;
}

int
Py_IsInitialized(void)
{
    return _PyRuntime.initialized;
}

int
Py_IsFinalizing(void)
{
    return _PyRuntimeState_GetFinalizing(&_PyRuntime) != NULL;
}


/* Hack to force loading of object files */
int (*_PyOS_mystrnicmp_hack)(const char *, const char *, Py_ssize_t) = \
    PyOS_mystrnicmp; /* Python/pystrcmp.o */


/* Stub implementations for functions that depend on signals (not available on Switch).
   These are normally in Modules/signalmodule.c but we need them unconditionally. */

int
PyOS_InterruptOccurred(void)
{
    /* No signal support on Switch - never interrupted by signals */
    return 0;
}

/* PyOS_InitInterrupts and PyOS_FiniInterrupts are no longer used in Python 3.14,
   but provide stubs for compatibility with any code that might call them. */
void
PyOS_InitInterrupts(void)
{
    /* No-op on Switch - no signal support */
}

void
PyOS_FiniInterrupts(void)
{
    /* No-op on Switch - no signal support */
}


/* Py_InitializeEx() and Py_Initialize() - main entry points for initialization.
   Based on Python-3.14.6/Python/pylifecycle.c but adapted for Switch:
   - No signal handlers (install_sigs is ignored)
   - Uses PyConfig-based initialization */

void
Py_InitializeEx(int install_sigs)
{
    PyStatus status;

    /* Initialize global runtime state */
    status = _PyRuntime_Initialize();
    if (_PyStatus_EXCEPTION(status)) {
        Py_ExitStatusException(status);
    }

    _PyRuntimeState *runtime = &_PyRuntime;

    if (runtime->initialized) {
        /* bpo-33932: Calling Py_Initialize() twice does nothing. */
        return;
    }

    /* Create a compatible config for legacy-style initialization */
    PyConfig config;
    _PyConfig_InitCompatConfig(&config);

    /* On Switch, we don't have signals, so ignore install_sigs */
    (void)install_sigs;
    config.install_signal_handlers = 0;

    status = Py_InitializeFromConfig(&config);
    PyConfig_Clear(&config);
    if (_PyStatus_EXCEPTION(status)) {
        Py_ExitStatusException(status);
    }
}

void
Py_Initialize(void)
{
    /* On Switch, install_sigs=0 since we have no signal support */
    Py_InitializeEx(0);
}


/* Py_Finalize() and Py_FinalizeEx() - cleanup.
   Based on Python-3.14.6/Python/pylifecycle.c structure. */

int
Py_FinalizeEx(void)
{
    return _Py_Finalize(&_PyRuntime);
}

void
Py_Finalize(void)
{
    (void)_Py_Finalize(&_PyRuntime);
}


/* Py_EndInterpreter() - delete an interpreter.
   Based on Python-3.14.6/Python/pylifecycle.c structure. */

void
Py_EndInterpreter(PyThreadState *tstate)
{
    if (tstate == NULL) {
        return;
    }

    PyInterpreterState *interp = tstate->interp;
    assert(interp != NULL);

    /* Delete the thread state */
    PyThreadState_DeleteCurrent();

    /* Delete the interpreter */
    PyInterpreterState_Delete(interp);
}


/* Py_NewInterpreter() - create a new sub-interpreter.
   Based on Python-3.14.6/Python/pylifecycle.c structure. */

PyThreadState *
Py_NewInterpreter(void)
{
    PyStatus status;
    PyThreadState *tstate = NULL;

    status = _PyRuntime_Initialize();
    if (_PyStatus_EXCEPTION(status)) {
        Py_ExitStatusException(status);
    }

    /* Create a new interpreter with default config */
    PyInterpreterConfig interp_config;
    _PyInterpreterConfig_Init(&interp_config);

    status = _PyInterpreterState_NewEx(&tstate, &interp_config);
    if (_PyStatus_EXCEPTION(status)) {
        Py_ExitStatusException(status);
    }

    return tstate;
}


/* Helper: exit with a fatal error message based on PyStatus */
void
Py_ExitStatusException(PyStatus status)
{
    const char *err_msg = _PyStatus_Message(&status);
    if (err_msg != NULL && err_msg[0] != '\0') {
        Py_FatalError(err_msg);
    } else {
        Py_FatalError("Python initialization failed");
    }
}
