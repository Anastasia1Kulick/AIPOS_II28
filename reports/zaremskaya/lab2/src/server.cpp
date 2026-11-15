#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>

#define MY_PORT 666

int main(int argc, char* argv[])
{
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);

    char buff[1024];
    printf("UDP SERVER (TASK 6)\n");

    if (WSAStartup(0x0202, (WSADATA *)&buff[0]))
    {
        printf("Error WSAStartup %d\n", WSAGetLastError());
        return -1;
    }

    SOCKET my_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (my_sock == INVALID_SOCKET)
    {
        printf("Socket() error: %d\n", WSAGetLastError());
        WSACleanup();
        return -1;
    }

    sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(MY_PORT);

    if (bind(my_sock, (sockaddr *)&local_addr, sizeof(local_addr)))
    {
        printf("bind error: %d\n", WSAGetLastError());
        closesocket(my_sock);
        WSACleanup();
        return -1;
    }

    printf("Ожидание UDP-пакетов на порту %d\n", MY_PORT);

    char group[48];
    int filled = 0;
    int total = 0;
    int match = 0;
    const char STOP[] = "~#~";
    int hello_sent = 0;

    while (1)
    {
        sockaddr_in client_addr;
        int client_addr_size = sizeof(client_addr);
        
        int bsize = recvfrom(my_sock, &buff[0], sizeof(buff)-1, 0,
                             (sockaddr *) &client_addr, &client_addr_size);
                             
        if (bsize == SOCKET_ERROR) continue;

        if (!hello_sent)
        {
            const char sHELLO[] = "Hello, Student!\r\n";
            sendto(my_sock, sHELLO, strlen(sHELLO), 0, (sockaddr *)&client_addr, client_addr_size);
            printf("+Новый клиент [%s:%d]\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            hello_sent = 1;
        }

        for (int i = 0; i < bsize; i++)
        {
            unsigned char ch = (unsigned char)buff[i];

            if (ch == (unsigned char)STOP[match])      match++;
            else if (ch == (unsigned char)STOP[0])     match = 1;
            else                                       match = 0;

            group[filled++] = (char)ch;
            total++;

            if (match == 3)
            {
                const char *msg = "Сеанс окончен.\r\n";
                sendto(my_sock, msg, strlen(msg), 0, (sockaddr *)&client_addr, client_addr_size);
                printf("-Клиент отключен (~#~)\n");
  
                filled = total = match = 0;
                hello_sent = 0;
                break;
            }

            if (filled == 48)
            {
                int sum = 0;
                char outmsg[128];
                for (int k = 0; k < 48; k++)
                    sum += (unsigned char)group[k];
                    
                sprintf(outmsg, "SUM=%d TOTAL=%d\r\n", sum, total);
                sendto(my_sock, outmsg, strlen(outmsg), 0, (sockaddr *)&client_addr, client_addr_size);
                printf("Отправлено: %s", outmsg);
                filled = 0;
            }
        }
    }

    closesocket(my_sock);
    WSACleanup();
    return 0;
}