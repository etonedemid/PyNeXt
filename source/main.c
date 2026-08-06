#include <stdio.h>
#include <switch.h>

#include <nxpy/Python.h>
#include <nxpy/cpython/initconfig.h>

#define MAINPY "main.py"
#define LOGFILE "sdmc:/pylog.txt"

/* Global log file pointer so we can flush at any point */
static FILE *g_log = NULL;

/* Open log file and write a message. Flushes immediately. */
static void logmsg(const char *msg) {
    if (!g_log) {
        g_log = fopen(LOGFILE, "w");
    }
    if (g_log) {
        fprintf(g_log, "%s\n", msg ? msg : "(null)");
        fflush(g_log);
    }
    /* Also print to console for USB debugging */
    if (msg) printf("%s\n", msg);
}

/*
 * Background console update thread.
 * This is the KEY fix for the black-screen issue.
 * The Switch framebuffer is not pushed to the display until
 * consoleUpdate() is called. Since PyRun_AnyFile() blocks while
 * Python executes (and scripts often run infinite loops), we
 * need a separate thread to call consoleUpdate periodically.
 */
static volatile bool g_console_thread_running = false;
static volatile bool g_should_quit = false;
static Mutex g_console_mutex;

static void console_update_thread(void *args) {
    (void)args;
    while (g_console_thread_running) {
        mutexLock(&g_console_mutex);
        consoleUpdate(NULL);
        mutexUnlock(&g_console_mutex);
        /* ~30 FPS refresh (33ms = 33000000 ns) */
        svcSleepThread(33000000);
    }
}

/*
 * Pad polling thread - checks for Plus button to set quit flag.
 * Uses the shared PadState initialized on the main thread.
 */
static PadState g_pad;
static bool g_pad_initialized = false;

static void pad_poll_thread(void *args) {
    (void)args;
    while (g_console_thread_running) {
        if (g_pad_initialized) {
            padUpdate(&g_pad);
            if (padGetButtonsDown(&g_pad) & HidNpadButton_Plus) {
                g_should_quit = true;
            }
        }
        /* Also check appletMainLoop */
        if (!appletMainLoop()) {
            g_should_quit = true;
        }
        /* ~60 FPS (16ms = 16666667 ns) */
        svcSleepThread(16666667);
    }
}

/* This runs before main() — if we get no log at all, the crash is here or earlier */
__attribute__((constructor))
static void early_log(void) {
    logmsg("=== EARLY CONSTRUCTOR ===");
}

/*
 * PyNX custom module: _pynx
 * Exposes console_update() and should_quit() to Python scripts.
 * This is the KEY fix for the black-screen issue - Python scripts
 * must call _pynx.console_update() each frame to push rendered
 * content to the Switch display.
 */

static PyObject *pynx_console_update(PyObject *self, PyObject *noargs) {
    (void)self; (void)noargs;
    mutexLock(&g_console_mutex);
    consoleUpdate(NULL);
    mutexUnlock(&g_console_mutex);
    Py_RETURN_NONE;
}

static PyObject *pynx_should_quit(PyObject *self, PyObject *noargs) {
    (void)self; (void)noargs;
    return PyBool_FromLong(g_should_quit);
}

static PyObject *pynx_sleep_ms(PyObject *self, PyObject *args) {
    (void)self;
    int ms;
    if (!PyArg_ParseTuple(args, "i", &ms)) {
        return NULL;
    }
    if (ms > 0) {
        /* Release GIL while sleeping so the console/pad threads can run
         * and Python internal state remains valid. */
        Py_BEGIN_ALLOW_THREADS
        svcSleepThread(ms * 1000000LL); /* ms → ns */
        Py_END_ALLOW_THREADS
    }
    Py_RETURN_NONE;
}

static PyObject *pynx_flush_console(PyObject *self, PyObject *noargs) {
    (void)self; (void)noargs;
    mutexLock(&g_console_mutex);
    fflush(stdout);
    fflush(stderr);
    mutexUnlock(&g_console_mutex);
    Py_RETURN_NONE;
}

static PyMethodDef PynxMethods[] = {
    {"console_update", pynx_console_update, METH_NOARGS,
     "Update the Switch display (push framebuffer to screen)."},
    {"should_quit", pynx_should_quit, METH_NOARGS,
     "Return True if the user pressed Plus to quit."},
    {"sleep_ms", pynx_sleep_ms, METH_VARARGS,
     "Sleep for the given number of milliseconds (uses svcSleepThread)."},
    {"flush_console", pynx_flush_console, METH_NOARGS,
     "Flush stdout/stderr to ensure output reaches the console."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef pynxmodule = {
    PyModuleDef_HEAD_INIT,
    "_pynx",
    "PyNX Switch integration module.",
    -1,
    PynxMethods
};

PyMODINIT_FUNC PyInit__pynx(void) {
    return PyModule_Create(&pynxmodule);
}

/* Register the _pynx module before Python init. */
static void register_pynx_module(void) {
    PyImport_AppendInittab("_pynx", PyInit__pynx);
}

int main(int argc, char *argv[])
{
    char logbuf[256];

    logmsg("=== main() entered ===");

    /* Register the _pynx module before Python initialization */
    register_pynx_module();

    /* Check sdmc: is mounted (may already be mounted by hbloader — that's fine) */
    logmsg("Checking fsdev mount");
    Result rc = fsdevMountSdmc();
    if (R_SUCCEEDED(rc)) {
        logmsg("sdmc: mounted OK");
    } else if (rc == 0x559) {
        logmsg("sdmc: already mounted (OK)");
    } else {
        snprintf(logbuf, sizeof(logbuf), "sdmc mount failed: 0x%08lx", (u64)rc);
        logmsg(logbuf);
    }

    logmsg("Step 1: appletInitialize");
    appletInitialize();
    logmsg("Step 1: appletInitialize done");

    logmsg("Step 2: consoleInit");
    consoleInit(NULL);
    consoleDebugInit(debugDevice_CONSOLE);
    logmsg("Step 2: consoleInit done");

    /* Initialize mutex for safe console access */
    mutexInit(&g_console_mutex);

    /* Start background console update thread so the screen is not black
     * while Python is running (PyRun_AnyFile is a blocking call). */
    logmsg("Step 2b: start console update thread");
    g_console_thread_running = true;
    Thread console_thread;
    Result ct_rc = threadCreate(&console_thread, console_update_thread, NULL,
                                NULL, 0x20000, 0x20, -2);
    logmsg("Step 2b: console update thread created");
    if (R_SUCCEEDED(ct_rc)) {
        threadStart(&console_thread);
    } else {
        snprintf(logbuf, sizeof(logbuf), "console thread create failed: 0x%08lx", (u64)ct_rc);
        logmsg(logbuf);
    }

    snprintf(logbuf, sizeof(logbuf), "Args count: %d", argc);
    logmsg(logbuf);
    for (int i = 0; i < argc; i++) {
        snprintf(logbuf, sizeof(logbuf), "  argv[%d] = %s", i, argv[i]);
        logmsg(logbuf);
    }

    logmsg("Step 3: socketInitialize");
    socketInitialize(socketGetDefaultInitConfig());
    logmsg("Step 3: socketInitialize done");

    /* Calculate absolute home dir */
    logmsg("Step 4: getcwd");
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    snprintf(logbuf, sizeof(logbuf), "  cwd = %.200s", cwd);
    logmsg(logbuf);

    /* Strip the leading sdmc: to workaround a bug somewhere... */
    char *stripped_cwd = strchr(cwd, '/');
    if (stripped_cwd == NULL) stripped_cwd = cwd;

    logmsg("Step 5: PyConfig setup");
    /* Use PyConfig-based initialization for Python 3.14 */
    PyStatus status;
    PyConfig config;

    logmsg("  PyConfig_InitIsolatedConfig");
    PyConfig_InitIsolatedConfig(&config);
    config.site_import = 0;              /* No site.py import */
    config.user_site_directory = 0;      /* No user site directory */
    config.use_environment = 0;          /* Ignore environment variables */

    logmsg("  alloc home_path");
    /* Set Python home using PyConfig API */
    Py_ssize_t path_len = strlen(stripped_cwd);
    wchar_t *home_path = (wchar_t *)PyMem_RawMalloc((path_len + 1) * sizeof(wchar_t));
    if (!home_path) {
        logmsg("Error: memory allocation failed");
        return 1;
    }
    for (Py_ssize_t i = 0; i < path_len; i++) {
        home_path[i] = (wchar_t)stripped_cwd[i];
    }
    home_path[path_len] = L'\0';
    config.home = home_path;
    snprintf(logbuf, sizeof(logbuf), "  home = %.200s", stripped_cwd);
    logmsg(logbuf);

    /* Determine app directory from argv[0] */
    char app_dir[PATH_MAX] = {0};
    if (argc > 0 && argv[0] && argv[0][0]) {
        strncpy(app_dir, argv[0], sizeof(app_dir) - 1);
        char *last_slash = strrchr(app_dir, '/');
        if (last_slash) *last_slash = '\0';
        else strncpy(app_dir, cwd, sizeof(app_dir) - 1);
    } else {
        strncpy(app_dir, cwd, sizeof(app_dir) - 1);
    }
    snprintf(logbuf, sizeof(logbuf), "  app_dir = %.200s", app_dir);
    logmsg(logbuf);

    logmsg("  setting module_search_paths");
    config.module_search_paths_set = 1;

    const char *paths_to_add[] = {
        "sdmc:/switch/PyNX/lib/python3.14",
        "/switch/PyNX/lib/python3.14",
        "sdmc:/switch/lib/python3.14",
        "/switch/lib/python3.14",
        "sdmc:/switch/PyNX",
        "sdmc:/switch",
        "/switch/PyNX",
        "/switch",
        NULL
    };

    /* Helper macro to append a path */
    for (int i = 0; paths_to_add[i] != NULL; i++) {
        wchar_t wpath[PATH_MAX];
        mbstowcs(wpath, paths_to_add[i], PATH_MAX);
        PyWideStringList_Append(&config.module_search_paths, wpath);
    }

    /* Add app_dir/lib/python3.14 dynamically */
    char dynamic_lib[PATH_MAX];
    snprintf(dynamic_lib, sizeof(dynamic_lib), "%s/lib/python3.14", app_dir);
    wchar_t wdyn[PATH_MAX];
    mbstowcs(wdyn, dynamic_lib, PATH_MAX);
    PyWideStringList_Append(&config.module_search_paths, wdyn);

    logmsg("Step 6: Py_InitializeFromConfig");
    status = Py_InitializeFromConfig(&config);
    if (PyStatus_Exception(status)) {
        snprintf(logbuf, sizeof(logbuf), "Error: Python init failed in %s: %s",
                 status.func ? status.func : "(null)",
                 status.err_msg ? status.err_msg : "(null)");
        logmsg(logbuf);
        PyConfig_Clear(&config);
        PyMem_RawFree(home_path);
        return 1;
    }
    logmsg("Step 6: Py_InitializeFromConfig OK");

    logmsg("Step 7: PyConfig_Clear");
    PyConfig_Clear(&config);
    logmsg("Step 7: PyConfig_Clear done");

    /* Force unbuffered stdout so Python print() reaches the console immediately */
    logmsg("Step 7b: setvbuf stdout unbuffered");
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    /* Print info */
    snprintf(logbuf, sizeof(logbuf), "Python %s on %s", Py_GetVersion(), Py_GetPlatform());
    logmsg(logbuf);

    logmsg("Step 8: setup sys.path");
    PyObject *sysPath = PySys_GetObject("path");
    PyObject *path = PyUnicode_FromString("");
    PyList_Insert(sysPath, 0, path);
    logmsg("Step 8: sys.path setup done");

    logmsg("Step 9: fopen main.py");
    FILE *mainpy = fopen(MAINPY, "r");
    if (!mainpy) {
        snprintf(logbuf, sizeof(logbuf), "  fopen(\"%s\") failed, trying app_dir", MAINPY);
        logmsg(logbuf);
        char alt_main[PATH_MAX];
        snprintf(alt_main, sizeof(alt_main), "%s/main.py", app_dir);
        snprintf(logbuf, sizeof(logbuf), "  trying: %s", alt_main);
        logmsg(logbuf);
        mainpy = fopen(alt_main, "r");
    }
    if (!mainpy) {
        logmsg("  trying: sdmc:/switch/PyNX/main.py");
        mainpy = fopen("sdmc:/switch/PyNX/main.py", "r");
    }
    if (!mainpy) {
        logmsg("Error: could not open main.py from any path");
        return 1;
    }
    logmsg("Step 9: fopen main.py OK");

        /* Initialize pad BEFORE running Python so Plus button can quit */
    logmsg("Step 10: padInitializeDefault");
    padInitializeDefault(&g_pad);
    g_pad_initialized = true;
    logmsg("Step 10: padInitializeDefault done");

    /* Start pad polling thread to detect Plus button press during Python execution */
    logmsg("Step 10b: start pad poll thread");
    Thread pad_thread;
    Result pt_rc = threadCreate(&pad_thread, pad_poll_thread, NULL,
                                NULL, 0x1000, 0x20, -1);
    logmsg("Step 10b: pad poll thread created");
    if (R_SUCCEEDED(pt_rc)) {
        threadStart(&pad_thread);
    } else {
        snprintf(logbuf, sizeof(logbuf), "pad thread create failed: 0x%08lx", (u64)pt_rc);
        logmsg(logbuf);
    }

    logmsg("Step 11: PyRun_AnyFile main.py");
    PyRun_AnyFile(mainpy, MAINPY);
    fclose(mainpy);
    logmsg("Step 11: PyRun_AnyFile done");

    logmsg("Step 12: Py_DECREF path/sysPath");
    Py_DECREF(path);
    Py_DECREF(sysPath);

    logmsg("Step 13: Py_Finalize");
    Py_Finalize();
    logmsg("Step 13: Py_Finalize done");

    /* Main loop runs after Python exits (shouldn't normally happen with infinite-loop scripts) */
    logmsg("Step 14: main loop");
    while (appletMainLoop()) {
        padUpdate(&g_pad);
        HidNpadButton kDown = padGetButtonsDown(&g_pad);
        if (kDown & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
        nanosleep(&(struct timespec){.tv_sec=0, .tv_nsec=16000000}, NULL);
    }
    logmsg("Step 14: main loop exited");

    /* Stop background threads */
    logmsg("Step 15: stop background threads");
    g_console_thread_running = false;
    /* Small delay to let threads notice the flag and exit */
    svcSleepThread(100000000); /* 100ms */
    logmsg("Step 15: threads stopped");

    logmsg("Step 16: cleanup");
    socketExit();
    logmsg("=== PyNX exit ===");

    if (g_log) fclose(g_log);
    return 0;
}
