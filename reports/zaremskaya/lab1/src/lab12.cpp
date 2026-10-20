#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <conio.h>
#include <winsock2.h>
#include <windows.h>

SOCKET g_sock = INVALID_SOCKET;
FILE *g_log = NULL;
int g_connected = 0;

void log_event(const char *what, const char *detail)
{
    time_t t;
    struct tm *tm;
    char ts[64];
    if (!g_log) return;
    time(&t);
    tm = localtime(&t);
    sprintf(ts, "%04d-%02d-%02d %02d:%02d:%02d",
        tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec);
    fprintf(g_log, "%s %s %s\n", ts, what, detail ? detail : "");
    fflush(g_log);
}

int do_connect(const char *addr, int port)
{
    char buff[1024];
    sockaddr_in dest_addr;
    HOSTENT *hst;

    g_sock=socket(AF_INET,SOCK_STREAM,0);
    if (g_sock == INVALID_SOCKET)
    {
        printf("Socket() error %d\n",WSAGetLastError());
        return 0;
    }

    dest_addr.sin_family=AF_INET;
    dest_addr.sin_port=htons((u_short)port);

    if (inet_addr(addr)!=INADDR_NONE)
        dest_addr.sin_addr.s_addr=inet_addr(addr);
    else
        if ((hst=gethostbyname(addr))!=NULL)
            ((unsigned long *)&dest_addr.sin_addr)[0]=
                ((unsigned long **)hst->h_addr_list)[0][0];
        else
        {
            printf("Invalid address %s\n", addr);
            closesocket(g_sock);
            g_sock = INVALID_SOCKET;
            return 0;
        }

    if (connect(g_sock,(sockaddr *)&dest_addr, sizeof(dest_addr)))
    {
        printf("Connect error %d\n",WSAGetLastError());
        closesocket(g_sock);
        g_sock = INVALID_SOCKET;
        return 0;
    }

    sprintf(buff, "%s:%d", addr, port);
    printf("Соединение с %s успешно установлено\n", buff);
    log_event("CONNECTION START", buff);
    g_connected = 1;
    return 1;
}

DWORD WINAPI RecvThread(LPVOID)
{
    char buff[4096];
    int n;
    while ((n = recv(g_sock, &buff[0], sizeof(buff)-1, 0)) > 0)
    {
        buff[n] = 0;
        printf("S=>C:%s", buff);
        if (n > 0 && buff[n-1] != '\n')
            printf("\n");
        log_event("RECEIVED", buff);
    }
    printf("\nСоединение закрыто сервером\n");
    log_event("CONNECTION END", "");
    g_connected = 0;
    return 0;
}

int main(int argc, char* argv[])
{
    char wsabuf[1024];
    char line[1024];
    int len = 0;
    int c, k;

    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);

    printf("TCP CLIENT VAR 2\n");
    printf("Нет автоподключения. Набери: connect 127.0.0.1 666 и нажми PgUp\n");
    printf("Дальше набор текста, отправка - PgUp, выход - quit + PgUp\n\n");

    g_log = fopen("tcp-client.log", "a");

    if (WSAStartup(0x202,(WSADATA *)&wsabuf[0]))
    {
        printf("WSAStart error %d\n",WSAGetLastError());
        return -1;
    }

    printf("S<=C:");
    fflush(stdout);

    while (1)
    {
        c = getch();
        if (c == 0 || c == 224)
        {
            k = getch();
            // 73 — PgUp
            if (k == 73 && len > 0)
            {
                char addr[256];
                int port;
                line[len] = 0;
                if (!strcmp(line, "quit"))
                    break;

                if (!g_connected)
                {
                    if (sscanf(line, "connect %255s %d", addr, &port) == 2)
                    {
                        if (do_connect(addr, port))
                        {
                            DWORD thID;
                         
                            CreateThread(NULL, 0, RecvThread, NULL, 0, &thID);
                        }
                    }
                    else
                        printf("\nФормат: connect <адрес> <порт>\n");
                    len = 0;
                    printf("S<=C:");
                    fflush(stdout);
                    continue;
                }

                send(g_sock, line, len, 0);
                printf("\nотправлено %d симв.\nS<=C:", len);
                len = 0;
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
        if (c >= 32 && len < (int)sizeof(line)-1)
        {
            line[len++] = (char)c;
            putchar(c);
            fflush(stdout);
        }
    }

    if (g_connected)
        log_event("CONNECTION END", "");
    if (g_sock != INVALID_SOCKET)
        closesocket(g_sock);
    WSACleanup();
    if (g_log) fclose(g_log);
    return 0;
}