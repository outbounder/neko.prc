#define UNICODE
#include <windows.h>
#include "include\neko.h"

value exec(value cmd)
{
	INT ExitCode;

	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	// if CreateProcess fails, exit with GetLastError, otherwise exit with (process ID * -1)
	ExitCode = CreateProcess(NULL, *val_string(cmd), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi) \
        ? pi.dwProcessId : GetLastError() * -1;

	return alloc_int(ExitCode);
}

DEFINE_PRIM(exec,1); // function test with 0 arguments
