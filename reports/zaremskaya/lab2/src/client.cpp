#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <conio.h>
#include <winsock2.h>
#include <windows.h>

#define PORT 666
#define SERVERADDR "127.0.0.1"

SOCKET my_sock = INVALID_SOCKET;
FILE *g_log = NULL;
int g_connected = 0;
sockaddr_in dest_addr;

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

int main(int argc, char* argv[])
{
    char buff[10240];
    char line[1024];
    int len = 0;
    int c, k;

    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);

    printf("UDP CLIENT (TASK 2)\n");
    printf("Нет автоподключения. Набери: connect 127.0.0.1 666 и нажми PgUp\n");
    printf("Дальше набор текста, отправка - PgUp, выход - quit + PgUp\n\n");

    g_log = fopen("udp-client.log", "a");

    if (WSAStartup(0x202, (WSADATA *)&buff[0]))
    {
        printf("WSAStart error %d\n", WSAGetLastError());
        return -1;
    }

    my_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (my_sock == INVALID_SOCKET)
    {
        printf("Socket() error %d\n", WSAGetLastError());
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
            if (k == 73 && len > 0)
            {
                line[len] = 0;
                if (!strcmp(line, "quit"))
                    break;

                if (!g_connected)
                {
                    char addr[256];
                    int port;
                    if (sscanf(line, "connect %255s %d", addr, &port) == 2)
                    {
                        dest_addr.sin_family = AF_INET;
                        dest_addr.sin_port = htons(port);
                        HOSTENT *hst;

                        if (inet_addr(addr) != INADDR_NONE)
                            dest_addr.sin_addr.s_addr = inet_addr(addr);
                        else if ((hst = gethostbyname(addr)) != NULL)
                            ((unsigned long *)&dest_addr.sin_addr)[0] = ((unsigned long **)hst->h_addr_list)[0][0];
                        else
                        {
                            printf("\nInvalid address %s\n", addr);
                            len = 0;
                            printf("S<=C:");
                            continue;
                        }

                        sprintf(buff, "%s:%d", addr, port);
                        printf("\nСоединение с %s успешно установлено\n", buff);
                        log_event("CONNECTION START", buff); // Пункт 1 лога
                        g_connected = 1;
                    }
                    else
                        printf("\nФормат: connect <адрес> <порт>\n");
                    
                    len = 0;
                    printf("S<=C:");
                    fflush(stdout);
                    continue;
                }

                sendto(my_sock, line, len, 0, (sockaddr *)&dest_addr, sizeof(dest_addr));
                printf("\nотправлено %d симв.\n", len);
 
                sockaddr_in from_addr;
                int from_len = sizeof(from_addr);
                int n = recvfrom(my_sock, buff, sizeof(buff)-1, 0, (sockaddr *)&from_addr, &from_len);
                
                if (n == SOCKET_ERROR)
                    printf("recvfrom() error: %d\n", WSAGetLastError());
                else if (n > 0)
                {
                    buff[n] = 0;
                    printf("S=>C:%s", buff);
                    if (buff[n-1] != '\n') printf("\n");
                    log_event("RECEIVED", buff); 
                }
                
                len = 0;
                printf("S<=C:");
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
        if (c >= 32 && len < (int)sizeof(line)-1)
        {
            line[len++] = (char)c;
            putchar(c);
            fflush(stdout);
        }
    }

    if (g_connected)
        log_event("CONNECTION END", "");
    if (my_sock != INVALID_SOCKET)
        closesocket(my_sock);
    WSACleanup();
    if (g_log) fclose(g_log);
    return 0;
}