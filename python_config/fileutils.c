/* File utilities for Nintendo Switch port of CPython 3.14.
   Based on Python-3.14.6/Python/fileutils.c, adapted for Switch/libnx. */

#include "Python.h"
#include "pycore_fileutils.h"     // fileutils definitions
#include "pycore_runtime.h"       // _PyRuntime
#include "osdefs.h"               // SEP

#include <stdlib.h>               // mbstowcs()
#include <unistd.h>               // getcwd(), isatty()
#include <fcntl.h>                // fcntl(F_GETFD)
#include <sys/stat.h>             // stat(), fstat()
#include <errno.h>

#ifdef O_CLOEXEC
/* Does open() support the O_CLOEXEC flag? */
int _Py_open_cloexec_works = -1;
#endif


PyObject *
_Py_device_encoding(int fd)
{
    int valid;
    if (!isatty(fd)) {
        Py_RETURN_NONE;
    }

    /* On Switch, use UTF-8 for device encoding */
    _Py_DECLARE_STR(utf_8, "utf-8");
    return &_Py_STR(utf_8);
}


/* Return information about a file using fstat(). */
int
_Py_fstat_noraise(int fd, struct _Py_stat_struct *status)
{
    return fstat(fd, status);
}

/* Return information about a file. Raise exception on error. */
int
_Py_fstat(int fd, struct _Py_stat_struct *status)
{
    int res;

    Py_BEGIN_ALLOW_THREADS
    res = _Py_fstat_noraise(fd, status);
    Py_END_ALLOW_THREADS

    if (res != 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }
    return 0;
}


/* Call stat() with a raw filename. */
int
_Py_wstat(const wchar_t* path, struct stat *buf)
{
    char *fname;
    fname = _Py_EncodeLocaleRaw(path, NULL);
    if (fname == NULL) {
        errno = EINVAL;
        return -1;
    }
    int err = stat(fname, buf);
    PyMem_RawFree(fname);
    return err;
}


/* Call stat() on the given Python path object. */
int
_Py_stat(PyObject *path, struct stat *statbuf)
{
    PyObject *bytes;
    char *cpath;

    bytes = PyUnicode_EncodeFSDefault(path);
    if (bytes == NULL) {
        return -2;
    }

    /* check for embedded null bytes */
    if (PyBytes_AsStringAndSize(bytes, &cpath, NULL) == -1) {
        Py_DECREF(bytes);
        return -2;
    }

    int ret = stat(cpath, statbuf);
    Py_DECREF(bytes);
    return ret;
}


/* Get the inheritable flag of the specified file descriptor. */
int
_Py_get_inheritable(int fd)
{
    int flags = fcntl(fd, F_GETFD, 0);
    if (flags == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }
    return !(flags & FD_CLOEXEC);
}


/* Set the inheritable flag of the specified file descriptor. */
int
_Py_set_inheritable(int fd, int inheritable, int *atomic_flag_works)
{
    if (atomic_flag_works != NULL && !inheritable) {
        if (_Py_atomic_load_int_relaxed(atomic_flag_works) == -1) {
            int isInheritable = _Py_get_inheritable(fd);
            if (isInheritable == -1)
                return -1;
            _Py_atomic_store_int_relaxed(atomic_flag_works, !isInheritable);
        }

        if (_Py_atomic_load_int_relaxed(atomic_flag_works))
            return 0;
    }

    int flags = fcntl(fd, F_GETFD, 0);
    if (flags == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    if (inheritable) {
        flags &= ~FD_CLOEXEC;
    } else {
        flags |= FD_CLOEXEC;
    }

    if (fcntl(fd, F_SETFD, flags) == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }
    return 0;
}


/* Get current working directory as wide string. */
wchar_t*
_Py_wgetcwd(wchar_t *buf, size_t size)
{
    char *cbuf = NULL;
    wchar_t *result = buf;

    if (!buf) {
        result = (wchar_t *)PyMem_RawMalloc(size * sizeof(wchar_t));
        if (!result) {
            errno = ENOMEM;
            return NULL;
        }
    }

    cbuf = (char *)PyMem_RawMalloc(size);
    if (!cbuf) {
        if (!buf) {
            PyMem_RawFree(result);
        }
        errno = ENOMEM;
        return NULL;
    }

    char *cwd = getcwd(cbuf, size);
    if (!cwd) {
        PyMem_RawFree(cbuf);
        if (!buf) {
            PyMem_RawFree(result);
        }
        return NULL;
    }

    /* Convert to wide string */
    size_t len = strlen(cwd);
    for (size_t i = 0; i < len && i < size - 1; i++) {
        result[i] = (wchar_t)cwd[i];
    }
    result[len] = L'\0';

    PyMem_RawFree(cbuf);
    return result;
}


/* Get current working directory as Python string. */
PyObject *
_Py_getcwd(void)
{
    char buf[PATH_MAX];
    char *cwd = getcwd(buf, sizeof(buf));
    if (!cwd) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    return PyUnicode_DecodeFSDefault(cwd);
}


/* Open a file with optional close-on-exec flag. */
int
_Py_open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list va;
        va_start(va, flags);
        mode = (mode_t)va_arg(va, int);
        va_end(va);
    }

#ifdef O_CLOEXEC
    /* Check if O_CLOEXEC is supported */
    if (_Py_open_cloexec_works == -1) {
        /* Test by opening /dev/null with O_CLOEXEC */
        int fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (fd >= 0) {
            int flags_fd = fcntl(fd, F_GETFD);
            _Py_open_cloexec_works = (flags_fd & FD_CLOEXEC) ? 1 : 0;
            close(fd);
        } else {
            _Py_open_cloexec_works = 0;
        }
    }

    if (_Py_open_cloexec_works) {
        flags |= O_CLOEXEC;
    }
#endif

    int fd = open(path, flags, mode);
    return fd;
}


/* Like _Py_open() but doesn't raise an exception on error. */
int
_Py_open_noraise(const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list va;
        va_start(va, flags);
        mode = (mode_t)va_arg(va, int);
        va_end(va);
    }

#ifdef O_CLOEXEC
    if (_Py_open_cloexec_works == -1) {
        _Py_open_cloexec_works = 0; /* Assume not supported in noraise path */
    }

    if (_Py_open_cloexec_works) {
        flags |= O_CLOEXEC;
    }
#endif

    return open(path, flags, mode);
}


/* Check if a file descriptor refers to a terminal. */
int
_Py_isatty(int fd)
{
    return isatty(fd);
}


/* Verify that a file descriptor is valid. */
int
_PyVerify_fd(int fd)
{
    return fcntl(fd, F_GETFD) != -1;
}
