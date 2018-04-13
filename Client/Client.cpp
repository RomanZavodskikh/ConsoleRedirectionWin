#include "stdafx.h"

#include <windows.h> 
#include <stdio.h>
#include <conio.h>
#include <tchar.h>

#define BUFSIZE 512

int _tmain(int argc, TCHAR *argv[])
{
    HANDLE hPipeClientServer;
    HANDLE hHeap = GetProcessHeap();
    TCHAR* pchRequest = (TCHAR*)HeapAlloc(hHeap, 0, BUFSIZE * sizeof(TCHAR));
    TCHAR* pchReply = (TCHAR*)HeapAlloc(hHeap, 0, BUFSIZE * sizeof(TCHAR));
    TCHAR  chBuf[BUFSIZE];
    BOOL   fSuccess = FALSE;
    DWORD  cbRead, cbToWrite, cbWritten, dwMode;
    LPTSTR lpszPipeClientServer = TEXT("\\\\.\\pipe\\clientserver");

    // Try to open a named pipe; wait for it, if necessary. 

    while (1)
    {
        hPipeClientServer = CreateFile(
            lpszPipeClientServer,   // pipe name 
            GENERIC_READ |  // read and write access 
            GENERIC_WRITE,
            0,              // no sharing 
            NULL,           // default security attributes
            OPEN_EXISTING,  // opens existing pipe 
            0,              // default attributes 
            NULL);          // no template file 

                            // Break if the pipe handle is valid. 

        if (hPipeClientServer != INVALID_HANDLE_VALUE)
            break;

        // Exit if an error other than ERROR_PIPE_BUSY occurs. 

        if (GetLastError() != ERROR_PIPE_BUSY)
        {
            _tprintf(TEXT("Could not open pipe. GLE=%d\n"), GetLastError());
            return -1;
        }

        // All pipe instances are busy, so wait for 20 seconds. 

        if (!WaitNamedPipe(lpszPipeClientServer, 20000))
        {
            printf("Could not open pipe: 20 second wait timed out.");
            return -1;
        }
    }

    // The pipe connected; change to message-read mode. 

    dwMode = PIPE_READMODE_MESSAGE;
    fSuccess = SetNamedPipeHandleState(
        hPipeClientServer,    // pipe handle 
        &dwMode,  // new pipe mode 
        NULL,     // don't set maximum bytes 
        NULL);    // don't set maximum time 
    if (!fSuccess)
    {
        _tprintf(TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError());
        return -1;
    }

    while (1)
    {
        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

        fSuccess = ReadFile(
            hStdin,
            pchRequest,
            BUFSIZE * sizeof(*pchRequest),
            &cbRead,
            NULL);

        if (!fSuccess)
        {
            _tprintf(TEXT("ReadFile from stdin failed. GLE=%d\n"), GetLastError());
            return -1;
        }

        fSuccess = WriteFile(
            hPipeClientServer,                  // pipe handle 
            pchRequest,             // message 
            cbRead,              // message length 
            &cbWritten,             // bytes written 
            NULL);                  // not overlapped 

        if (!fSuccess || cbRead != cbWritten)
        {
            _tprintf(TEXT("WriteFile to pipe failed. GLE=%d\n"), GetLastError());
            return -1;
        }

        // Read from the pipe. 

        fSuccess = ReadFile(
            hPipeClientServer,    // pipe handle 
            pchReply,    // buffer to receive reply 
            BUFSIZE * sizeof(TCHAR),  // size of buffer 
            &cbRead,  // number of bytes read 
            NULL);    // not overlapped 

        if (!fSuccess)
        {
            _tprintf(TEXT("ReadFile from pipe failed. GLE=%d\n"), GetLastError());
            return -1;
        }

        fSuccess = WriteFile(
            hStdout,
            pchReply,
            cbRead,
            &cbWritten,
            NULL);

        if (!fSuccess || cbRead != cbWritten)
        {
            _tprintf(TEXT("WriteFile to stdout failed. GLE=%d\n"), GetLastError());
            return -1;
        }
    }

    CloseHandle(hPipeClientServer);
    HeapFree(hHeap, 0, pchRequest);
    HeapFree(hHeap, 0, pchReply);

    return 0;
}