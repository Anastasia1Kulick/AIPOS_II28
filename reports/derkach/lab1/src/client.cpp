#include <stdio.h>
#include <string.h>
#include <time.h>
#include <conio.h>
#include <winsock2.h>
#include <windows.h>

#define PORT 666
#define SERVERADDR "127.0.0.1"

SOCKET g_sock = INVALID_SOCKET;
FILE *g_log = NULL;
int g_connected = 0;

void log_event(const char *what, const char *detail)
{
    time_t t;
    struct tm *tm;
    char ts[64];
    if (!g_log)
        return;
    time(&t);
    tm = localtime(&t);
    sprintf(ts, "%04d-%02d-%02d %02d:%02d:%02d",
              tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
              tm->tm_hour, tm->tm_min, tm->tm_sec);
    fprintf(g_log, "%s %s %s\n", ts, what, detail ? detail : "");
    fflush(g_log);
}

DWORD WINAPI RecvThread(LPVOID)
{
    char buff[4096];
    int n;
    while ((n = recv(g_sock, &buff[0], sizeof(buff) - 1, 0)) > 0)
    {
        buff[n] = 0;
        printf("S=>C:%s", buff);
        if (n > 0 && buff[n - 1] != '\n')
            printf("\n");
        log_event("RECEIVED", buff);
    }
    printf("\nConnection closed by server\n");
    log_event("CONNECTION END", "");
    g_connected = 0;
    return 0;
}

int main(int argc, char *argv[])
{
    char buff[1024];

    printf("CLIENT\n\n");
    printf("Auto-connect to %s:%d\n", SERVERADDR, PORT);
    printf("Type text, send - PgDn or Enter, exit - quit/exit + PgDn or Enter\n\n");

    g_log = fopen("tcp-client.log", "a");

    if (WSAStartup(0x202, (WSADATA *)&buff[0]))
    {
        printf("WSAStart error %d\n", WSAGetLastError());
        return -1;
    }

    g_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (g_sock == INVALID_SOCKET)
    {
        printf("Socket() error %d\n", WSAGetLastError());
        return -1;
    }

    sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    HOSTENT *hst;

    if (inet_addr(SERVERADDR) != INADDR_NONE)
        dest_addr.sin_addr.s_addr = inet_addr(SERVERADDR);
    else if (hst = gethostbyname(SERVERADDR))
        ((unsigned long *)&dest_addr.sin_addr)[0] =
             ((unsigned long **)hst->h_addr_list)[0][0];
    else
    {
        printf("Invalid address %s\n", SERVERADDR);
        closesocket(g_sock);
        WSACleanup();
        return -1;
    }

    if (connect(g_sock, (sockaddr *)&dest_addr, sizeof(dest_addr)))
    {
        printf("Connect error %d\n", WSAGetLastError());
        return -1;
    }

    printf("Connection to %s established successfully\n", SERVERADDR);
    log_event("CONNECTION START", SERVERADDR ":666");
    g_connected = 1;

    DWORD thID;
    CreateThread(NULL, 0, RecvThread, NULL, 0, &thID);

    char line[1024];
    int len = 0;
    printf("S<=C:");
    fflush(stdout);

    while (g_connected)
    {
        int c = getch();

        /* Esc - immediate exit */
        if (c == 27)
            break;

        /* extended keys (arrows, PgUp, PgDn, ...) */
        if (c == 0 || c == 224)
        {
            int k = getch();
            if (k == 81 && len > 0)   /* PgDn */
            {
                line[len] = 0;
                if (!strcmp(line, "quit") || !strcmp(line, "exit"))
                    break;
                send(g_sock, line, len, 0);
                printf("\nsent %d chars.\nS<=C:", len);
                log_event("SENT", line);
                len = 0;
            }
            continue;
        }

        /* Enter - also send */
        if (c == '\r' || c == '\n')
        {
            if (len > 0)
            {
                line[len] = 0;
                if (!strcmp(line, "quit") || !strcmp(line, "exit"))
                    break;
                send(g_sock, line, len, 0);
                printf("\nsent %d chars.\nS<=C:", len);
                log_event("SENT", line);
                len = 0;
            }
            continue;
        }

        /* Backspace */
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

        /* printable characters */
        if (c >= 32 && len < (int)sizeof(line) - 1)
        {
            line[len++] = (char)c;
            putchar(c);
            fflush(stdout);
        }
    }

    log_event("CONNECTION END", "");
    closesocket(g_sock);
    WSACleanup();
    if (g_log)
        fclose(g_log);
    return 0;
}