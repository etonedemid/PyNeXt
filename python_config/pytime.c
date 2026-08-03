/* Time functions for Nintendo Switch port of CPython 3.14.
   Based on Python-3.14.6/Python/pytime.c, adapted for Switch/libnx. */

#include "Python.h"
#include "pycore_initconfig.h"    // _PyStatus_ERR
#include "pycore_runtime.h"       // _PyRuntime
#include "pycore_time.h"          // PyTime_t

#include <time.h>                 // time(), gmtime_r()
#include <switch/time.h>          // svcGetSystemTick(), etc.

/* To millisecond (10^-3) */
#define SEC_TO_MS 1000

/* To microseconds (10^-6) */
#define MS_TO_US 1000
#define SEC_TO_US (SEC_TO_MS * MS_TO_US)

/* To nanoseconds (10^-9) */
#define US_TO_NS 1000
#define MS_TO_NS (MS_TO_US * US_TO_NS)
#define SEC_TO_NS (SEC_TO_MS * MS_TO_NS)


#if SIZEOF_TIME_T == SIZEOF_LONG_LONG
#  define PY_TIME_T_MAX LLONG_MAX
#  define PY_TIME_T_MIN LLONG_MIN
#elif SIZEOF_TIME_T == SIZEOF_LONG
#  define PY_TIME_T_MAX LONG_MAX
#  define PY_TIME_T_MIN LONG_MIN
#else
#  error "unsupported time_t size"
#endif


/* Switch-specific clock functions */

static u64 g_systemTickFreq = 0;

static void
ensure_tick_freq(void)
{
    if (g_systemTickFreq == 0) {
        g_systemTickFreq = svcGetSystemTickFrequency();
    }
}

/* Convert ticks to nanoseconds */
static PyTime_t
ticks_to_ns(u64 ticks)
{
    ensure_tick_freq();
    /* Use fixed-point arithmetic: ns = ticks * SEC_TO_NS / freq */
    return (PyTime_t)((u128)ticks * SEC_TO_NS / g_systemTickFreq);
}

/* Get monotonic time using system tick counter */
static int
py_get_monotonic_clock(PyTime_t *tp, _Py_clock_info_t *info, int raise_exc)
{
    u64 ticks = svcGetSystemTick();
    *tp = ticks_to_ns(ticks);

    if (info) {
        info->implementation = "svcGetSystemTick()";
        ensure_tick_freq();
        info->resolution = (double)SEC_TO_NS / (double)g_systemTickFreq;
        info->monotonic = 1;
        info->adjustable = 0;
    }

    return 0;
}


/* Get system clock time using libnx */
static int
py_get_system_clock(PyTime_t *tp, _Py_clock_info_t *info, int raise_exc)
{
    /* Use svcGetSystemTick for wall-clock approximation.
       Switch doesn't have a traditional RTC accessible from userland;
       we use the system tick counter which starts at boot. */
    u64 ticks = svcGetSystemTick();
    *tp = ticks_to_ns(ticks);

    if (info) {
        info->implementation = "svcGetSystemTick()";
        ensure_tick_freq();
        info->resolution = (double)SEC_TO_NS / (double)g_systemTickFreq;
        info->monotonic = 0;  /* Not truly monotonic - can be adjusted */
        info->adjustable = 1;
    }

    return 0;
}


/* Public API functions */

int
PyTime_Monotonic(PyTime_t *result)
{
    if (py_get_monotonic_clock(result, NULL, 1) < 0) {
        *result = 0;
        return -1;
    }
    return 0;
}


int
PyTime_MonotonicRaw(PyTime_t *result)
{
    if (py_get_monotonic_clock(result, NULL, 0) < 0) {
        *result = 0;
        return -1;
    }
    return 0;
}


int
_PyTime_MonotonicWithInfo(PyTime_t *tp, _Py_clock_info_t *info)
{
    return py_get_monotonic_clock(tp, info, 1);
}


int
PyTime_GetSystemTime(PyTime_t *result)
{
    if (py_get_system_clock(result, NULL, 1) < 0) {
        *result = 0;
        return -1;
    }
    return 0;
}


int
_PyTime_GetSystemClockWithInfo(PyTime_t *tp, _Py_clock_info_t *info)
{
    return py_get_system_clock(tp, info, 1);
}


/* Time initialization */

PyStatus
_PyTime_Init(struct _Py_time_runtime_state *state)
{
    ensure_tick_freq();
    /* Initialize base fraction for time conversion */
    state->base.numer = SEC_TO_NS;
    state->base.denom = g_systemTickFreq;
    return PyStatus_Ok();
}


/* Helper functions used by other modules */

time_t
_PyLong_AsTime_t(PyObject *obj)
{
#if defined(HAVE_LONG_LONG) && SIZEOF_TIME_T == SIZEOF_LONG_LONG
    PY_LONG_LONG val = PyLong_AsLongLong(obj);
#else
    long val = PyLong_AsLong(obj);
#endif
    if (val == -1 && PyErr_Occurred()) {
        if (PyErr_ExceptionMatches(PyExc_OverflowError)) {
            PyErr_SetString(PyExc_OverflowError,
                            "timestamp out of range for platform time_t");
        }
        return -1;
    }
    return (time_t)val;
}


PyObject *
_PyLong_FromTime_t(time_t t)
{
#if defined(HAVE_LONG_LONG) && SIZEOF_TIME_T == SIZEOF_LONG_LONG
    return PyLong_FromLongLong((PY_LONG_LONG)t);
#else
    assert(sizeof(time_t) <= sizeof(long));
    return PyLong_FromLong((long)t);
#endif
}
