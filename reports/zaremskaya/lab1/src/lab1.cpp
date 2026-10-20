#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>

#define MY_PORT 666

#define PRINTNUSERS if (nclients)\
    printf("%d user on-line\n",nclients);\
    else printf("No User on line\n");

DWORD WINAPI WorkWithClient(LPVOID client_socket);

int nclients = 0;

int main(int argc, char* argv[])
{

    SetConsoleOutputCP(65001);  
    SetConsoleCP(65001);

    char buff[1024];

    printf("TCP SERVER VAR 6\n");

    if (WSAStartup(0x0202,(WSADATA *) &buff[0]))
    {
        printf("Error WSAStartup %d\n", WSAGetLastError());
        return -1;
    }

    SOCKET mysocket;
    if ((mysocket=socket(AF_INET,SOCK_STREAM,0))==INVALID_SOCKET)
    {
        printf("Error socket %d\n",WSAGetLastError());
        WSACleanup();
        return -1;
    }

    sockaddr_in local_addr;
    local_addr.sin_family=AF_INET;
    local_addr.sin_port=htons(MY_PORT);
    local_addr.sin_addr.s_addr=0;

    if (bind(mysocket,(sockaddr *) &local_addr, sizeof(local_addr)))
    {
        printf("Error bind %d\n",WSAGetLastError());
        closesocket(mysocket);
        WSACleanup();
        return -1;
    }

    if (listen(mysocket, 0x100))
    {
        printf("Error listen %d\n",WSAGetLastError());
        closesocket(mysocket);
        WSACleanup();
        return -1;
    }

    printf("Ожидание подключений, порт %d\n", MY_PORT);

    SOCKET client_socket;
    sockaddr_in client_addr;
    int client_addr_size=sizeof(client_addr);

    while((client_socket=accept(mysocket, (sockaddr *)
            &client_addr, &client_addr_size))!=INVALID_SOCKET)
    {
        nclients++;

        HOSTENT *hst;
        hst=gethostbyaddr((char *)&client_addr.sin_addr.s_addr,4, AF_INET);

        printf("+%s [%s] new connect!\n",
            (hst)?hst->h_name:"",
            inet_ntoa(client_addr.sin_addr));
        PRINTNUSERS

        SOCKET *psock = (SOCKET *)malloc(sizeof(SOCKET));
        *psock = client_socket;

        DWORD thID;

        CreateThread(NULL, 0, WorkWithClient,
            (LPVOID)psock, 0, &thID);
    }
    return 0;
}

DWORD WINAPI WorkWithClient(LPVOID client_socket)
{
    SOCKET my_sock;
    my_sock = *((SOCKET *)client_socket);
    free(client_socket);

    char buff[20*1024];
    const char sHELLO[] = "Hello, Student!\r\n";

    send(my_sock,sHELLO,strlen(sHELLO),0);

    char group[48];
    int filled = 0;
    int total = 0;
    int match = 0;
    const char STOP[] = "~#~";
    int bytes_recv;
    int i;

    while( (bytes_recv=recv(my_sock,&buff[0],sizeof(buff),0)) > 0 )
    {
        for (i = 0; i < bytes_recv; i++)
        {
            unsigned char ch = (unsigned char)buff[i];

            if (ch == (unsigned char)STOP[match])
                match++;
            else if (ch == (unsigned char)STOP[0])
                match = 1;
            else
                match = 0;

            group[filled++] = (char)ch;
            total++;

            if (match == 3)
            {
                const char *msg = "Сеанс окончен.\r\n";
                send(my_sock, msg, strlen(msg), 0);
                nclients--;
                printf("-disconnect (~#~)\n");
                PRINTNUSERS
                closesocket(my_sock);
                return 0;
            }

            if (filled == 48)
            {
                int sum = 0;
                int k;
                char outmsg[128];
                for (k = 0; k < 48; k++)
                    sum += (unsigned char)group[k];
                sprintf(outmsg, "SUM=%d TOTAL=%d\r\n", sum, total);
                send(my_sock, outmsg, strlen(outmsg), 0);
                filled = 0;
            }
        }
    }

    nclients--;
    printf("-disconnect\n");
    PRINTNUSERS
    closesocket(my_sock);
    return 0;
}