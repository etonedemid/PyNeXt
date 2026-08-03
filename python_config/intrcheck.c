/* Interrupt checking stubs for Nintendo Switch port of CPython 3.14.
   Python 3.14 no longer uses intrcheck.c; signal handling is in Modules/signalmodule.c.
   This file provides stub implementations for compatibility. */

#include "Python.h"

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

int
PyOS_InterruptOccurred(void)
{
    /* No signal support on Switch - never interrupted by signals */
    return 0;
}
