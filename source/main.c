#include <stdio.h>
#include <switch.h>

#include <nxpy/Python.h>
#include <nxpy/cpython/initconfig.h>

#define MAINPY "main.py"

int main(int argc, char *argv[])
{
	gfxInitDefault();
	consoleInit(NULL);
	consoleDebugInit(debugDevice_CONSOLE);

	printf("Args:\n");
	for (int i=0; i<argc; i++) {
		printf("%s\n", argv[i]);
	}

	socketInitializeDefault();

	/* Calculate absolute home dir */
	char cwd[PATH_MAX];
	getcwd(cwd, sizeof(cwd));
	/* Strip the leading sdmc: to workaround a bug somewhere... */
	char *stripped_cwd = strchr(cwd, '/');
	if (stripped_cwd == NULL) stripped_cwd = cwd;

	/* Use PyConfig-based initialization for Python 3.14 */
	PyStatus status;
	PyConfig config;

	PyConfig_InitIsolatedConfig(&config);
	config.site_import = 0;              /* No site.py import */
	config.user_site_directory = 0;      /* No user site directory */
	config.use_environment = 0;          /* Ignore environment variables */

	/* Set Python home using PyConfig API (replaces deprecated Py_SetPythonHome) */
	Py_ssize_t path_len = strlen(stripped_cwd);
	wchar_t *home_path = (wchar_t *)PyMem_RawMalloc((path_len + 1) * sizeof(wchar_t));
	if (!home_path) {
		printf("Error: memory allocation failed\n");
		return 1;
	}
	for (Py_ssize_t i = 0; i < path_len; i++) {
		home_path[i] = (wchar_t)stripped_cwd[i];
	}
	home_path[path_len] = L'\0';

	config.home = home_path;

	status = Py_InitializeFromConfig(&config);
	if (PyStatus_Exception(status)) {
		const char *msg = PyStatus_Message(&status);
		printf("Error: Python initialization failed");
		if (msg && msg[0] != '\0') {
			printf(": %s", msg);
		}
		printf("\n");
		PyConfig_Clear(&config);
		PyMem_RawFree(home_path);
		return 1;
	}

	PyConfig_Clear(&config);

	/* Print some info */
	printf("Python %s on %s\n", Py_GetVersion(), Py_GetPlatform());
	
	/* set up import path */
	PyObject *sysPath = PySys_GetObject("path");
	PyObject *path = PyUnicode_FromString("");
	PyList_Insert(sysPath, 0, path);

	FILE * mainpy = fopen(MAINPY, "r");

	if (mainpy == NULL) {
		printf("Error: could not open " MAINPY "\n");
	} else {
		/* execute main.py */
		PyRun_AnyFile(mainpy, MAINPY);
		fclose(mainpy);
	}

	Py_DECREF(path); /* are these decrefs needed? Are they in the right place? */
	Py_DECREF(sysPath);

	Py_Finalize();

	while(appletMainLoop()) {

		hidScanInput();

		u32 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

		if (kDown & KEY_PLUS) break; // break in order to return to hbmenu

		gfxFlushBuffers();
		gfxSwapBuffers();
		gfxWaitForVsync();
	}

	socketExit();
	gfxExit();

	return 0;
}
