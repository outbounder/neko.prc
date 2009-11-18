#include <windows.h>
#include "include\neko.h"

void DisplayError(char *pszAPI);
void ReadAndHandleOutput(HANDLE hPipeRead);
PROCESS_INFORMATION PrepAndLaunchRedirectedChild(char* cmd,
                                 HANDLE hChildStdOut,
                                 HANDLE hChildStdErr);

HANDLE hChildProcess = NULL;
HANDLE hStdIn = NULL; // Handle to parents std input.
BOOL bRunThread = TRUE;
char* output = "test";

value exec(value cmd)
{
  HANDLE hOutputReadTmp,hOutputRead,hOutputWrite;
  HANDLE hErrorWrite;
  SECURITY_ATTRIBUTES sa;


  // Set up the security attributes struct.
  sa.nLength= sizeof(SECURITY_ATTRIBUTES);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = TRUE;


  // Create the child output pipe.
  if (!CreatePipe(&hOutputReadTmp,&hOutputWrite,&sa,0))
  {
     DisplayError("CreatePipe");
     return alloc_string(output);
  }


  // Create a duplicate of the output write handle for the std error
  // write handle. This is necessary in case the child application
  // closes one of its std output handles.
  if (!DuplicateHandle(GetCurrentProcess(),hOutputWrite,
                       GetCurrentProcess(),&hErrorWrite,0,
                       TRUE,DUPLICATE_SAME_ACCESS))
    {
     DisplayError("DuplicateHandle");
     return alloc_string(output);
    }


  // Create new output read handle and the input write handles. Set
  // the Properties to FALSE. Otherwise, the child inherits the
  // properties and, as a result, non-closeable handles to the pipes
  // are created.
  if (!DuplicateHandle(GetCurrentProcess(),hOutputReadTmp,
                       GetCurrentProcess(),
                       &hOutputRead, // Address of new handle.
                       0,FALSE, // Make it uninheritable.
                       DUPLICATE_SAME_ACCESS))
    {
     DisplayError("DupliateHandle");
     return alloc_string(output);
    }


    // Close inheritable copies of the handles you do not want to be
    // inherited.
    if (!CloseHandle(hOutputReadTmp)) { DisplayError("CloseHandle"); return alloc_string(output);}


  PROCESS_INFORMATION pi = PrepAndLaunchRedirectedChild(val_string(cmd),hOutputWrite,hErrorWrite);


  // Close pipe handles (do not continue to modify the parent).
  // You need to make sure that no handles to the write end of the
  // output pipe are maintained in this process or else the pipe will
  // not close when the child process exits and the ReadFile will hang.
  if (!CloseHandle(hOutputWrite)) { DisplayError("CloseHandle"); return alloc_string(output);}
  if (!CloseHandle(hErrorWrite)) {DisplayError("CloseHandle"); return alloc_string(output);}


  // Read the child's output.
  ReadAndHandleOutput(hOutputRead);
  // Redirection is complete

  // Tell the thread to exit and wait for thread to die.
  bRunThread = FALSE;

  if (WaitForSingleObject(pi.hProcess,INFINITE) == WAIT_FAILED)
     DisplayError("WaitForSingleObject");

  if (!CloseHandle(hOutputRead)) { DisplayError("CloseHandle"); return alloc_string(output);}

  return alloc_string(output);
}


///////////////////////////////////////////////////////////////////////
// PrepAndLaunchRedirectedChild
// Sets up STARTUPINFO structure, and launches redirected child.
///////////////////////////////////////////////////////////////////////
PROCESS_INFORMATION PrepAndLaunchRedirectedChild(char* cmd,
                                 HANDLE hChildStdOut,
                                 HANDLE hChildStdErr)
{
  PROCESS_INFORMATION pi;
  STARTUPINFO si;

  // Set up the start up info struct.
  ZeroMemory(&si,sizeof(STARTUPINFO));
  si.cb = sizeof(STARTUPINFO);
  si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
  si.wShowWindow = SW_HIDE;
  si.hStdOutput = hChildStdOut;
  si.hStdError  = hChildStdErr;
  // Use this if you want to hide the child:
  //     si.wShowWindow = SW_HIDE;
  // Note that dwFlags must include STARTF_USESHOWWINDOW if you want to
  // use the wShowWindow flags.


  // Launch the process that you want to redirect (in this case,
  // Child.exe). Make sure Child.exe is in the same directory as
  // redirect.c launch redirect from a command line to prevent location
  // confusion.
  if (!CreateProcess(NULL,cmd,NULL,NULL,TRUE,
                     CREATE_NEW_CONSOLE,NULL,NULL,&si,&pi))
  {
     DisplayError("CreateProcess");
     return pi;
  }


  // Set global child process handle to cause threads to exit.
  hChildProcess = pi.hProcess;


  // Close any unnecessary handles.
  if (!CloseHandle(pi.hThread)) { DisplayError("CloseHandle"); return pi;}
  return pi;
}


///////////////////////////////////////////////////////////////////////
// ReadAndHandleOutput
// Monitors handle for input. Exits when child exits or pipe breaks.
///////////////////////////////////////////////////////////////////////
void ReadAndHandleOutput(HANDLE hPipeRead)
{
  CHAR lpBuffer[256];
  DWORD nBytesRead;
  DWORD nCharsWritten;

  while(TRUE)
  {
     if (!ReadFile(hPipeRead,lpBuffer,sizeof(lpBuffer),
                                      &nBytesRead,NULL) || !nBytesRead)
     {
        if (GetLastError() == ERROR_BROKEN_PIPE)
           break; // pipe done - normal exit path.
        else
           DisplayError("ReadFile"); // Something bad happened.
     }

     output = lpBuffer;
     // Display the character read on the screen.
     /* if (!WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE),lpBuffer,
                       nBytesRead,&nCharsWritten,NULL))
        DisplayError("WriteConsole"); */
  }
}

///////////////////////////////////////////////////////////////////////
// DisplayError
// Displays the error number and corresponding message.
///////////////////////////////////////////////////////////////////////
void DisplayError(char *pszAPI)
{
   LPVOID lpvMessageBuffer;
   CHAR szPrintBuffer[512];
   DWORD nCharsWritten;

   FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
            NULL, GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&lpvMessageBuffer, 0, NULL);

   wsprintf(szPrintBuffer,
     "ERROR: API    = %s.\n   error code = %d.\n   message    = %s.\n",
            pszAPI, GetLastError(), (char *)lpvMessageBuffer);

   /*WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE),szPrintBuffer,
                 lstrlen(szPrintBuffer),&nCharsWritten,NULL);*/
   //strcat(output, szPrintBuffer);
   output = szPrintBuffer;

   LocalFree(lpvMessageBuffer);
}

DEFINE_PRIM(exec,1); // function test with 0 arguments
