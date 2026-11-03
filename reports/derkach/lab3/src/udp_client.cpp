
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <conio.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 666
#define SERVERADDR "127.0.0.1"

SOCKET g_sock = INVALID_SOCKET;
FILE* g_log = NULL;
volatile LONG g_connected = 0;
CRITICAL_SECTION g_log_lock;

void log_event(const char* what, const char* detail)
{
    if (!g_log)
        return;

    EnterCriticalSection(&g_log_lock);

    time_t t;
    time(&t);

    struct tm tm_info;
    localtime_s(&tm_info, &t);

    char ts[64];

    sprintf_s(ts, sizeof(ts),
        "%04d-%02d-%02d %02d:%02d:%02d",
        tm_info.tm_year + 1900,
        tm_info.tm_mon + 1,
        tm_info.tm_mday,
        tm_info.tm_hour,
        tm_info.tm_min,
        tm_info.tm_sec);

    fprintf(g_log, "%s %s %s\n",
        ts, what, detail ? detail : "");

    fflush(g_log);

    LeaveCriticalSection(&g_log_lock);
}

DWORD WINAPI RecvThread(LPVOID)
{
    char buff[4096];

    while (InterlockedCompareExchange(&g_connected, 1, 1))
    {
        int n = recv(g_sock, buff, sizeof(buff) - 1, 0);

        if (n == SOCKET_ERROR)
        {
            if (InterlockedCompareExchange(&g_connected, 1, 1))
            {
                printf("\nReceive error: %d\n", WSAGetLastError());
                log_event("RECEIVE ERROR", "");
            }
            break;
        }

        if (n == 0)
            continue;

        buff[n] = 0;

        printf("\nS=>C: %s\n", buff);

        log_event("RECEIVED", buff);

        if (strncmp(buff, "ERROR:", 6) == 0)
        {
            InterlockedExchange(&g_connected, 0);
            break;
        }
    }

    return 0;
}

int main()
{
    printf("UDP CLIENT\n\n");
    printf("Auto-start with server %s:%d\n", SERVERADDR, PORT);
    printf("Type text and send with PgDn or Enter.\n");
    printf("Type quit or exit and press PgDn or Enter to stop.\n\n");

    InitializeCriticalSection(&g_log_lock);

    g_log = fopen("udp-client.log", "a");

    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("WSAStartup error %d\n", WSAGetLastError());

        if (g_log)
            fclose(g_log);

        DeleteCriticalSection(&g_log_lock);
        return 1;
    }

    g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (g_sock == INVALID_SOCKET)
    {
        printf("Socket error %d\n", WSAGetLastError());

        WSACleanup();

        if (g_log)
            fclose(g_log);

        DeleteCriticalSection(&g_log_lock);
        return 1;
    }

    sockaddr_in dest_addr = {};

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    dest_addr.sin_addr.s_addr = inet_addr(SERVERADDR);

    if (connect(g_sock, (sockaddr*)&dest_addr, sizeof(dest_addr))
        == SOCKET_ERROR)
    {
        printf("Server address error %d\n", WSAGetLastError());

        closesocket(g_sock);
        WSACleanup();

        if (g_log)
            fclose(g_log);

        DeleteCriticalSection(&g_log_lock);
        return 1;
    }

    printf("UDP communication target: %s:%d\n",
        SERVERADDR, PORT);

    log_event("CLIENT START", SERVERADDR ":666");

    InterlockedExchange(&g_connected, 1);

    HANDLE recvThread = CreateThread(
        NULL, 0, RecvThread, NULL, 0, NULL);

    if (!recvThread)
    {
        printf("CreateThread error %lu\n", GetLastError());

        InterlockedExchange(&g_connected, 0);
        closesocket(g_sock);
        WSACleanup();

        if (g_log)
            fclose(g_log);

        DeleteCriticalSection(&g_log_lock);
        return 1;
    }

    char line[1024];
    int len = 0;

    printf("S<=C: ");
    fflush(stdout);

    while (InterlockedCompareExchange(&g_connected, 1, 1))
    {
        int c = _getch();

        if (c == 27)
            break;

        if (c == 0 || c == 224)
        {
            int k = _getch();

            if (k == 81 && len > 0)
            {
                line[len] = 0;

                if (!strcmp(line, "quit") || !strcmp(line, "exit"))
                    break;

                int sent = send(g_sock, line, len, 0);

                if (sent == SOCKET_ERROR)
                {
                    printf("\nSend error %d\n", WSAGetLastError());
                    break;
                }

                log_event("SENT", line);

                printf("\nSent %d chars.\nS<=C: ", sent);

                len = 0;
                fflush(stdout);
            }

            continue;
        }

        if (c == '\r' || c == '\n')
        {
            if (len > 0)
            {
                line[len] = 0;

                if (!strcmp(line, "quit") || !strcmp(line, "exit"))
                    break;

                int sent = send(g_sock, line, len, 0);

                if (sent == SOCKET_ERROR)
                {
                    printf("\nSend error %d\n", WSAGetLastError());
                    break;
                }

                log_event("SENT", line);

                printf("\nSent %d chars.\nS<=C: ", sent);

                len = 0;
                fflush(stdout);
            }

            continue;
        }

        if (c == 8)
        {
            if (len > 0)
            {
                len--;
                printf("\b \b");
                fflush(stdout);
            }

            continue;
        }

        if (c >= 32 && len < (int)sizeof(line) - 1)
        {
            line[len++] = (char)c;
            putchar(c);
            fflush(stdout);
        }
    }

    if (InterlockedCompareExchange(&g_connected, 1, 1))
    {
        const char* quitMessage = "QUIT";
        send(g_sock, quitMessage, (int)strlen(quitMessage), 0);
    }

    InterlockedExchange(&g_connected, 0);

    shutdown(g_sock, SD_BOTH);
    closesocket(g_sock);

    WaitForSingleObject(recvThread, 1000);
    CloseHandle(recvThread);

    log_event("CLIENT END", "");

    WSACleanup();

    if (g_log)
        fclose(g_log);

    DeleteCriticalSection(&g_log_lock);

    return 0;
}
