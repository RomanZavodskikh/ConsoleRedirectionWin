#include "stdafx.h"

#define WIN32_LEAN_AND_MEAN

#include <windows.h> 
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h> 
#include <stdlib.h>
#include <tchar.h>
#include <strsafe.h>

#define BUFSIZE 512
#define DEFAULT_PORT_SERVER_CLIENT "49023"
#define DEFAULT_PORT_CLIENT_SERVER "49027"

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

DWORD WINAPI InstanceThreadClientServer(LPVOID);
DWORD WINAPI InstanceThreadServerClient(LPVOID);

int _tmain(int argc, char** argv)
{
    BOOL   fConnected = FALSE;
    DWORD  dwThreadId = 0;
    HANDLE hThreadClientServer = NULL;
    HANDLE hThreadServerClient = NULL;
    WSADATA wsaData;
    int iResult;

    if (argc < 2)
    {
        _tprintf(TEXT("Usage: %s <ip addr of another instance>\n"), argv[0]);
        return 1;
    }

    // The main loop creates an instance of the named pipe and 
    // then waits for a client to connect to it. When the client 
    // connects, a thread is created to handle communications 
    // with that client, and this loop is free to wait for the
    // next client connect request. It is an infinite loop.

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    // Create a thread for this client. 
    hThreadClientServer = CreateThread(
        NULL,              // no security attribute 
        0,                 // default stack size 
        InstanceThreadClientServer,    // thread proc
        NULL,    // thread parameter 
        0,                 // not suspended 
        &dwThreadId);      // returns thread ID 

    if (hThreadClientServer == NULL)
    {
        _tprintf(TEXT("CreateThread failed, GLE=%d.\n"), GetLastError());
        return -1;
    }
    else CloseHandle(hThreadClientServer);
    // =================================================================

    // Create a thread for this client. 
    hThreadServerClient = CreateThread(
        NULL,              // no security attribute 
        0,                 // default stack size 
        InstanceThreadServerClient,    // thread proc
        argv[1],    // thread parameter 
        0,                 // not suspended 
        &dwThreadId);      // returns thread ID 

    if (hThreadServerClient == NULL)
    {
        _tprintf(TEXT("CreateThread failed, GLE=%d.\n"), GetLastError());
        return -1;
    }
    else CloseHandle(hThreadServerClient);

    for (;;)
    {
        Sleep(100);
    }

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

    SOCKET ConnectSocketClientServer = INVALID_SOCKET;
    SOCKET ConnectSocketListen = INVALID_SOCKET;
    struct addrinfo *result = NULL,
        *ptr = NULL,
        hints;
    int iResult;

    // Do some extra error checking since the app will keep running even if this
    // thread fails.
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

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT_CLIENT_SERVER, &hints, &result);
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
    ConnectSocketClientServer = accept(ConnectSocketListen, NULL, NULL);
    if (ConnectSocketClientServer == INVALID_SOCKET) {
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

        cbBytesRead = recv(ConnectSocketClientServer, (char*)pchRequest, BUFSIZE * sizeof(TCHAR), 0);
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

    SOCKET ConnectSocketServerClient = INVALID_SOCKET;
    struct addrinfo *result = NULL,
        *ptr = NULL,
        hints;
    int iResult;

    // Do some extra error checking since the app will keep running even if this
    // thread fails.
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

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Attempt to connect to an address until one succeeds
    do
    {
        // Resolve the server address and port
        char bufName[80];
        size_t ign;
        wcstombs_s(&ign, bufName, (TCHAR*)lpvParam, 80);
        iResult = getaddrinfo(bufName, DEFAULT_PORT_SERVER_CLIENT, &hints, &result);
        if (iResult != 0) {
            printf("getaddrinfo failed with error: %d with host %s\n", iResult, (char*)bufName);
            Sleep(1000);
            continue;
        }

        for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
            // Create a SOCKET for connecting to server
            ConnectSocketServerClient = socket(ptr->ai_family, ptr->ai_socktype,
                ptr->ai_protocol);
            if (ConnectSocketServerClient == INVALID_SOCKET) {
                printf("socket failed with error: %ld\n", WSAGetLastError());
                WSACleanup();
                return 1;
            }

            // Connect to server.
            iResult = connect(ConnectSocketServerClient, ptr->ai_addr, (int)ptr->ai_addrlen);
            if (iResult == SOCKET_ERROR) {
                closesocket(ConnectSocketServerClient);
                ConnectSocketServerClient = INVALID_SOCKET;
                continue;
            }
            break;
        }
        Sleep(100);
    } while (ConnectSocketServerClient == INVALID_SOCKET);
    freeaddrinfo(result);

    if (ConnectSocketServerClient == INVALID_SOCKET) {
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


        cbWritten = send(ConnectSocketServerClient, (char*)pchReply, cbReplyBytes, 0);
        if (cbReplyBytes != cbWritten)
        {
            _tprintf(TEXT("InstanceThread WriteFile failed, GLE=%d.\n"), GetLastError());
            break;
        }
    }

    HeapFree(hHeap, 0, pchRequest);
    HeapFree(hHeap, 0, pchReply);
    return 1;
}
