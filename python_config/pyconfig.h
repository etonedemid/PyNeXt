/* pyconfig.h - Nintendo Switch port of CPython 3.14 */

#ifndef Py_PYCONFIG_H
#define Py_PYCONFIG_H

#include "osdefs.h"

/* Platform identification */
#define SWITCH 1
#define __SWITCH__ 1

/* Build configuration */
#define BUILD_GNU_TYPE "aarch64-none-elf"
#define HOST_GNU_TYPE "aarch64-none-elf"

/* SOABI for extensions (not used on Switch but required) */
#define SOABI "cpython-314-switch"
#define ABIFLAGS ""

/* Size and alignment */
#define SIZEOF_SHORT 2
#define SIZEOF_INT 4
#define SIZEOF_LONG 8
#define SIZEOF_LONG_LONG 8
#define SIZEOF_SIZE_T 8
#define SIZEOF_PTRDIFF_T 8
#define SIZEOF_TIME_T 8
#define SIZEOF_PID_T 4
#define SIZEOF_FLOAT 4
#define SIZEOF_DOUBLE 8
#define SIZEOF_LDOUBLE 16

#define ALIGNOF_LONG 8
#define ALIGNOF_MAX_ALIGN_T 16
#define ALIGNOF_SIZE_T 8

/* Character types */
#define HAVE_SSIZE_T 1
#define HAVE_UINTPTR_T 1
#define HAVE_INTPTR_T 1
#define HAVE_WCHAR_T 1
#define PY_FORMAT_SIZE_T "z"

/* Double format - little endian on Switch (ARM64) */
#define DOUBLE_IS_LITTLE_ENDIAN_IEEE754 1

/* Atomic operations - GCC builtins available on ARM64 */
#define HAVE_BUILTIN_ATOMIC 1

/* C99 features */
#define HAVE_C99_BOOL 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1

/* Computed gotos for state machine optimization */
#define HAVE_COMPUTED_GOTOS 1

/* Headers available on Switch/libnx */
#define HAVE_ALLOCA_H 1
#define HAVE_DIRENT_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1

/* NOT available on Switch */
#undef HAVE_DLFCN_H
#undef HAVE_ENDIAN_H
#undef HAVE_LANGINFO_H
#undef HAVE_GRP_H
#undef HAVE_PWD_H
#undef HAVE_SIGNAL_H
#undef HAVE_SYS_SOCKET_H
#undef HAVE_NETINET_IN_H
#undef HAVE_ARPA_INET_H
#undef HAVE_NETDB_H

/* Functions available on Switch */
#define HAVE_ACOSH 1
#define HAVE_ASINH 1
#define HAVE_ATANH 1
#define HAVE_COPYSIGN 1
#define HAVE_CLOCK 1
#define HAVE_DUP2 1
#define HAVE_ERF 1
#define HAVE_ERFC 1
#define HAVE_EXPM1 1
#define HAVE_FCHMOD 1
#define HAVE_FSEEK64 1
#define HAVE_FSEEKO 1
#define HAVE_FSTATVFS 1
#define HAVE_FSYNC 1
#define HAVE_FTELL64 1
#define HAVE_FTELLO 1
#define HAVE_FTRUNCATE 1
#define HAVE_GETPID 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_HYPOT 1
#define HAVE_INET_PTON 1
#define HAVE_LOG1P 1
#define HAVE_LROUND 1
#define HAVE_MBRTOWC 1
#define HAVE_MEMMOVE 1
#define HAVE_MKTIME 1
#define HAVE_POW 1
#define HAVE_PUTENV 1
#define HAVE_RECV 1
#define HAVE_SEND 1
#define HAVE_SETENV 1
#define HAVE_SOCKET 1
#define HAVE_SQRT 1
#define HAVE_STRERROR_R 1
#define HAVE_SWAB 1
#define HAVE_TELL 1
#define HAVE_TRUNC 1
#define HAVE_UNSETENV 1
#define HAVE_WCSTOMBS 1

/* NOT available on Switch */
#undef HAVE_ACCEPT4
#undef HAVE_ALARM
#undef HAVE_BIND_TEXTDOMAIN_CODESET
#undef HAVE_CHFLAGS
#undef HAVE_CHOWN
#undef HAVE_CLOCK_GETRES
#undef HAVE_CLOCK_GETTIME
#undef HAVE_CONFSTR
#undef HAVE_CTERMID
#undef HAVE_DLOPEN
#undef HAVE_EPOLL
#undef HAVE_EXECV
#undef HAVE_FACCESSAT
#undef HAVE_FCHDIR
#undef HAVE_FCHMODAT
#undef HAVE_FCHOWN
#undef HAVE_FDOPENDIR
#undef HAVE_FORK
#undef HAVE_FORKPTY
#undef HAVE_FPATHCONF
#undef HAVE_FUTIMENS
#undef HAVE_GAI_STRERROR
#undef HAVE_GETADDRINFO
#undef HAVE_GETC_UNLOCKED
#undef HAVE_GETENTROPY
#undef HAVE_GETGROUPLIST
#undef HAVE_GETGROUPS
#undef HAVE_GETHOSTBYNAME_R
#undef HAVE_GETITIMER
#undef HAVE_GETLOADAVG
#undef HAVE_GETLOGIN
#undef HAVE_GETNAMEINFO
#undef HAVE_GETPAGESIZE
#undef HAVE_GETPEERNAME
#undef HAVE_GETPGID
#undef HAVE_GETPGRP
#undef HAVE_GETPRIORITY
#undef HAVE_GETPWENT
#undef HAVE_GETRANDOM
#undef HAVE_GETRESGID
#undef HAVE_GETRESUID
#undef HAVE_GETSID
#undef HAVE_GETWD
#undef HAVE_IF_NAMEINDEX
#undef HAVE_INET_ATON
#undef HAVE_INITGROUPS
#undef HAVE_KILL
#undef HAVE_KILLPG
#undef HAVE_LCHFLAGS
#undef HAVE_LINK
#undef HAVE_LOCKF
#undef HAVE_MADVISE
#undef HAVE_MEMFD_CREATE
#undef HAVE_MKFIFO
#undef HAVE_MKSTEMP
#undef HAVE_NICE
#undef HAVE_OPENPTY
#undef HAVE_PSELECT
#undef HAVE_READLINK
#undef HAVE_RENAMEAT
#undef HAVE_SEMGET
#undef HAVE_SETGID
#undef HAVE_SETGROUPS
#undef HAVE_SETPGID
#undef HAVE_SETPRIORITY
#undef HAVE_SETREGID
#undef HAVE_SETRESGID
#undef HAVE_SETRESUID
#undef HAVE_SETREUID
#undef HAVE_SETSID
#undef HAVE_SETUID
#undef HAVE_SIGACTION
#undef HAVE_SIGALTSTACK
#undef HAVE_SIGSUSPEND
#undef HAVE_SOCKETPAIR
#undef HAVE_SYMLINK
#undef HAVE_SYSCONF
#undef HAVE_TCGETPGRP
#undef HAVE_TGKILL
#undef HAVE_TRUNCATE
#undef HAVE_UNAME
#undef HAVE_UNLINKAT
#undef HAVE_UTIMENSAT
#undef HAVE_WAITPID

/* Socket support (limited on Switch) */
#define HAVE_SOCKET 1
#define HAVE_SEND 1
#define HAVE_RECV 1
#define HAVE_INET_PTON 1

/* Define socket types manually for Switch */
#ifndef SOCK_RAW
#define SOCK_RAW 3
#endif
#ifndef SOCK_SEQPACKET
#define SOCK_SEQPACKET 5
#endif
#ifndef IN_CLASSA_NSHIFT
#define IN_CLASSA_NSHIFT 24
#endif

/* Threading - we use custom implementation via libnx */
#define WITH_THREAD 1
#define SWITCH_THREADS 1
/* Do NOT define _POSIX_THREADS or HAVE_PTHREAD_H - we use our own thread_nx.h */

/* File I/O */
#define HAVE_DIRENT_D_TYPE 1
#define HAVE_FSTATAT 1
#define HAVE_STATVFS 1

/* No dynamic loading on Switch */
#undef HAVE_DYNAMIC_LOADING
#undef HAVE_DLADDR
#undef HAVE_DLOPEN

/* No IPv6 by default (can be enabled if needed) */
#undef ENABLE_IPV6

/* No /dev/ptmx or /dev/ptc on Switch */
#undef HAVE_DEV_PTMX
#undef HAVE_DEV_PTC

/* Misc platform settings */
#define HAVE_ALIGNED_REQUIRED 1
#define HAVE_BROKEN_SEM_GETVALUE 1
#define HAVE_GCC_UINT128_T 1
#define HAVE_HTOLE64 1

/* Time-related - Switch has limited clock support */
#undef HAVE_DECL_TZNAME
#undef HAVE_ALTZONE

/* No process-related features */
#undef HAVE_FORK
#undef HAVE_EXECV
#undef HAVE_WAITPID
#undef HAVE_KILL
#undef HAVE_GETPGRP
#undef HAVE_SETSID

/* Use our custom implementations */
#define Py_HAVE_CONDVAR 1

/* Disable tracemalloc by default (can be enabled) */
#define DISABLE_TRACEMALLOC 1

/* Python internal settings */
#define PY_SSIZE_T_CLEAN 1
#define Py_BUILD_CORE 1

/* Ensure wchar_t is available */
#ifndef HAVE_WCHAR_H
#define HAVE_WCHAR_H 1
#endif

/* For pyatomic - use GCC builtins */
#define HAVE_BUILTIN_ATOMIC 1

/* Python version-specific defines for 3.14 */
#define PY_MAJOR_VERSION 3
#define PY_MINOR_VERSION 14
#define PY_MICRO_VERSION 6
#define PY_RELEASE_LEVEL PY_RELEASE_LEVEL_FINAL
#define PY_RELEASE_SERIAL 0

#endif /* Py_PYCONFIG_H */
