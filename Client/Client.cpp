#include "stdafx.h"

#define WIN32_LEAN_AND_MEAN

#include <windows.h> 
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <tchar.h>

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define BUFSIZE 512
#define DEFAULT_PORT_SERVER_CLIENT "49023"
#define DEFAULT_PORT_CLIENT_SERVER "49027"

DWORD WINAPI InstanceThreadClientServer(LPVOID);
DWORD WINAPI InstanceThreadServerClient(LPVOID);

int _tmain(int argc, TCHAR *argv[])
{
    HANDLE hPipeClientServer, hPipeServerClient;
    HANDLE hThreadClientServer, hThreadServerClient;
    HANDLE hHeap = GetProcessHeap();
    TCHAR* pchRequest = (TCHAR*)HeapAlloc(hHeap, 0, BUFSIZE * sizeof(TCHAR));
    TCHAR* pchReply = (TCHAR*)HeapAlloc(hHeap, 0, BUFSIZE * sizeof(TCHAR));
    BOOL   fSuccess = FALSE;
    DWORD  cbRead, cbToWrite, cbWritten, dwMode;
    DWORD  dwThreadId = 0;
    LPTSTR lpszPipeClientServer = TEXT("\\\\.\\pipe\\clientserver");
    LPTSTR lpszPipeServerClient = TEXT("\\\\.\\pipe\\serverclient");
    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

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

    hThreadClientServer = CreateThread(
        NULL,              // no security attribute 
        0,                 // default stack size 
        InstanceThreadClientServer,    // thread proc
        (LPVOID)hPipeClientServer,    // thread parameter 
        0,                 // not suspended 
        &dwThreadId);      // returns thread ID 

    if (hThreadClientServer == NULL)
    {
        _tprintf(TEXT("CreateThread failed, GLE=%d.\n"), GetLastError());
        return -1;
    }
    else CloseHandle(hThreadClientServer);
    // =================================================================
    while (1) 
    {
        hPipeServerClient = CreateFile(
            lpszPipeServerClient,   // pipe name 
            GENERIC_READ |  // read and write access 
            GENERIC_WRITE,
            0,              // no sharing 
            NULL,           // default security attributes
            OPEN_EXISTING,  // opens existing pipe 
            0,              // default attributes 
            NULL);          // no template file 

                            // Break if the pipe handle is valid. 

        if (hPipeServerClient != INVALID_HANDLE_VALUE)
            break;

        // Exit if an error other than ERROR_PIPE_BUSY occurs. 
        if (GetLastError() != ERROR_PIPE_BUSY)
        {
            _tprintf(TEXT("Could not open pipe(2). GLE=%d\n"), GetLastError());
            return -1;
        }

        // All pipe instances are busy, so wait for 20 seconds. 
        if (!WaitNamedPipe(lpszPipeServerClient, 20000))
        {
            printf("Could not open pipe: 20 second wait timed out.");
            return -1;
        }
    }
    hThreadServerClient = CreateThread(
        NULL,              // no security attribute 
        0,                 // default stack size 
        InstanceThreadServerClient,    // thread proc
        (LPVOID)hPipeServerClient,    // thread parameter 
        0,                 // not suspended 
        &dwThreadId);      // returns thread ID 

    if (hThreadServerClient == NULL)
    {
        _tprintf(TEXT("CreateThread failed, GLE=%d.\n"), GetLastError());
        return -1;
    }
    else CloseHandle(hThreadServerClient);
    
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
    //==========================================================
    fSuccess = SetNamedPipeHandleState(
        hPipeServerClient,    // pipe handle 
        &dwMode,  // new pipe mode 
        NULL,     // don't set maximum bytes 
        NULL);    // don't set maximum time 
    if (!fSuccess)
    {
        _tprintf(TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError());
        return -1;
    }

    for (;;) {
        Sleep(100);
    }

    CloseHandle(hPipeClientServer);
    WSACleanup();
    HeapFree(hHeap, 0, pchRequest);
    HeapFree(hHeap, 0, pchReply);
    return 0;
}

DWORD WINAPI InstanceThreadClientServer(LPVOID lpvParam)
// This routine is a thread processing function to read from and reply to a client
// via the open pipe connection passed from the main loop. Note this allows
// the main loop to continue executing, potentially creating more threads of
// of this procedure to run concurrently, depending on the number of incoming
// client connections.
{
    HANDLE hHeap = GetProcessHeap();
    TCHAR* pchRequest = (TCHAR*)HeapAlloc(hHeap, 0, BUFSIZE * sizeof(TCHAR));
    TCHAR* pchReply = (TCHAR*)HeapAlloc(hHeap, 0, BUFSIZE * sizeof(TCHAR));

    DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0;
    BOOL fSuccess = FALSE;
    HANDLE hPipe = NULL;

    SOCKET ConnectSocketClientServer = INVALID_SOCKET;
    struct addrinfo *result = NULL,
        *ptr = NULL,
        hints;
    int iResult = 0;

    // Do some extra error checking since the app will keep running even if this
    // thread fails.

    if (lpvParam == NULL)
    {
        printf("\nERROR - Pipe Server Failure:\n");
        printf("   InstanceThread got an unexpected NULL value in lpvParam.\n");
        printf("   InstanceThread exitting.\n");
        if (pchReply != NULL) HeapFree(hHeap, 0, pchReply);
        if (pchRequest != NULL) HeapFree(hHeap, 0, pchRequest);
        return (DWORD)-1;
    }

    if (pchRequest == NULL)
    {
        printf("\nERROR - Pipe Server Failure:\n");
        printf("   InstanceThread got an unexpected NULL heap allocation.\n");
        printf("   InstanceThread exitting.\n");
        if (pchReply != NULL) HeapFree(hHeap, 0, pchReply);
        return (DWORD)-1;
    }

    if (pchReply == NULL)
    {
        printf("\nERROR - Pipe Server Failure:\n");
        printf("   InstanceThread got an unexpected NULL heap allocation.\n");
        printf("   InstanceThread exitting.\n");
        if (pchRequest != NULL) HeapFree(hHeap, 0, pchRequest);
        return (DWORD)-1;
    }

    hPipe = (HANDLE)lpvParam;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo("localhost", DEFAULT_PORT_CLIENT_SERVER, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        // Create a SOCKET for connecting to server
        ConnectSocketClientServer = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ConnectSocketClientServer == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect(ConnectSocketClientServer, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocketClientServer);
            ConnectSocketClientServer = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocketClientServer == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }

    // Loop until done reading
    while (1)
    {
        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

        fSuccess = ReadFile(
            hStdin,
            pchReply,
            BUFSIZE * sizeof(*pchReply),
            &cbReplyBytes,
            NULL);

        if (!fSuccess)
        {
            _tprintf(TEXT("ReadFile stdin failed, GLE=%d.\n"), GetLastError());
            break;
        }

        cbWritten = send(ConnectSocketClientServer, (char*)pchReply, cbReplyBytes, 0);

        if (cbReplyBytes != cbWritten)
        {
            _tprintf(TEXT("InstanceThread WriteFile failed, GLE=%d.\n"), GetLastError());
            break;
        }
    }

    // Flush the pipe to allow the client to read the pipe's contents 
    // before disconnecting. Then disconnect the pipe, and close the 
    // handle to this pipe instance. 

    FlushFileBuffers(hPipe);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
    closesocket(ConnectSocketClientServer);

    HeapFree(hHeap, 0, pchRequest);
    HeapFree(hHeap, 0, pchReply);
    return 1;
}

DWORD WINAPI InstanceThreadServerClient(LPVOID lpvParam)
// This routine is a thread processing function to read from and reply to a client
// via the open pipe connection passed from the main loop. Note this allows
// the main loop to continue executing, potentially creating more threads of
// of this procedure to run concurrently, depending on the number of incoming
// client connections.
{
    HANDLE hHeap = GetProcessHeap();
    TCHAR* pchRequest = (TCHAR*)HeapAlloc(hHeap, 0, BUFSIZE * sizeof(TCHAR));
    TCHAR* pchReply = (TCHAR*)HeapAlloc(hHeap, 0, BUFSIZE * sizeof(TCHAR));

    DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0;
    BOOL fSuccess = FALSE;
    HANDLE hPipe = NULL;

    SOCKET ConnectSocketListen = INVALID_SOCKET;
    SOCKET ConnectSocketServerClient = INVALID_SOCKET;
    struct addrinfo *result = NULL,
        *ptr = NULL,
        hints;
    int iResult;

    // Do some extra error checking since the app will keep running even if this
    // thread fails.

    if (lpvParam == NULL)
    {
        printf("\nERROR - Pipe Server Failure:\n");
        printf("   InstanceThread got an unexpected NULL value in lpvParam.\n");
        printf("   InstanceThread exitting.\n");
        if (pchReply != NULL) HeapFree(hHeap, 0, pchReply);
        if (pchRequest != NULL) HeapFree(hHeap, 0, pchRequest);
        return (DWORD)-1;
    }

    if (pchRequest == NULL)
    {
        printf("\nERROR - Pipe Server Failure:\n");
        printf("   InstanceThread got an unexpected NULL heap allocation.\n");
        printf("   InstanceThread exitting.\n");
        if (pchReply != NULL) HeapFree(hHeap, 0, pchReply);
        return (DWORD)-1;
    }

    if (pchReply == NULL)
    {
        printf("\nERROR - Pipe Server Failure:\n");
        printf("   InstanceThread got an unexpected NULL heap allocation.\n");
        printf("   InstanceThread exitting.\n");
        if (pchRequest != NULL) HeapFree(hHeap, 0, pchRequest);
        return (DWORD)-1;
    }

    hPipe = (HANDLE)lpvParam;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo("localhost", DEFAULT_PORT_SERVER_CLIENT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for connecting to server
    ConnectSocketListen = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ConnectSocketListen == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(ConnectSocketListen, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ConnectSocketListen);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ConnectSocketListen, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocketListen);
        WSACleanup();
        return 1;
    }

    // Accept a client socket
    ConnectSocketServerClient = accept(ConnectSocketListen, NULL, NULL);
    if (ConnectSocketServerClient == INVALID_SOCKET) {
        printf("accept failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocketListen);
        WSACleanup();
        return 1;
    }

    // No longer need server socket
    closesocket(ConnectSocketListen);

    // Loop until done reading
    while (1)
    {
        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

        cbBytesRead = recv(ConnectSocketServerClient, (char*)pchRequest, BUFSIZE * sizeof(TCHAR), 0);
        if (cbBytesRead == 0)
        {
            if (GetLastError() == ERROR_BROKEN_PIPE)
            {
                _tprintf(TEXT("InstanceThread: client disconnected.\n"));
            }
            else
            {
                _tprintf(TEXT("InstanceThread ReadFile failed, GLE=%d.\n"), GetLastError());
            }
            break;
        }

        fSuccess = WriteFile(
            hStdout,
            pchRequest,
            cbBytesRead,
            &cbReplyBytes,
            NULL);

        if (!fSuccess || cbBytesRead != cbReplyBytes)
        {
            _tprintf(TEXT("WriteFile stdout failed, GLE=%d.\n"), GetLastError());
            break;
        }
    }

    // Flush the pipe to allow the client to read the pipe's contents 
    // before disconnecting. Then disconnect the pipe, and close the 
    // handle to this pipe instance. 

    FlushFileBuffers(hPipe);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
    closesocket(ConnectSocketServerClient);

    HeapFree(hHeap, 0, pchRequest);
    HeapFree(hHeap, 0, pchReply);
    return 1;
}