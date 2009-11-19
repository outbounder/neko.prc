#include <windows.h>
#include "include\neko.h"

void DisplayError(char *pszAPI);
void ReadAndHandleOutput(HANDLE hPipeRead);
PROCESS_INFORMATION PrepAndLaunchRedirectedChild(char* cmd,
                                 HANDLE hChildStdOut,
                                 HANDLE hChildStdErr);

HANDLE hChildProcess = NULL;
buffer output;

value exec(value cmd)
{
    output = alloc_buffer("");
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
     return buffer_to_string(output);
  }


  // Create a duplicate of the output write handle for the std error
  // write handle. This is necessary in case the child application
  // closes one of its std output handles.
  if (!DuplicateHandle(GetCurrentProcess(),hOutputWrite,
                       GetCurrentProcess(),&hErrorWrite,0,
                       TRUE,DUPLICATE_SAME_ACCESS))
    {
     DisplayError("DuplicateHandle");
     return buffer_to_string(output);
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
     return buffer_to_string(output);
    }


    // Close inheritable copies of the handles you do not want to be
    // inherited.
    if (!CloseHandle(hOutputReadTmp)) { DisplayError("CloseHandle"); return buffer_to_string(output);}


  PROCESS_INFORMATION pi = PrepAndLaunchRedirectedChild(val_string(cmd),hOutputWrite,hErrorWrite);
  if(&pi == 0)
    return buffer_to_string(output);


  // Close pipe handles (do not continue to modify the parent).
  // You need to make sure that no handles to the write end of the
  // output pipe are maintained in this process or else the pipe will
  // not close when the child process exits and the ReadFile will hang.
  if (!CloseHandle(hOutputWrite)) { DisplayError("CloseHandle"); return buffer_to_string(output);}
  if (!CloseHandle(hErrorWrite)) {DisplayError("CloseHandle"); return buffer_to_string(output);}


  // Read the child's output.
  ReadAndHandleOutput(hOutputRead);
  // Redirection is complete

  if (WaitForSingleObject(pi.hProcess,INFINITE) == WAIT_FAILED)
  {
     DisplayError("WaitForSingleObject");
     return buffer_to_string(output);
  }

  if (!CloseHandle(hOutputRead)) { DisplayError("CloseHandle"); return buffer_to_string(output);}

  return buffer_to_string(output);
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


     buffer_append_sub(output, lpBuffer, nBytesRead);

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

   buffer_append(output,szPrintBuffer);

   LocalFree(lpvMessageBuffer);
}

DEFINE_PRIM(exec,1); // function test with 0 arguments
