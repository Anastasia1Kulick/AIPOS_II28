#include <stdio.h>
#include <string.h>
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
    char buff[1024];

    printf("SERVER\n\n");

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

    printf("Waiting for connections, port %d\n", MY_PORT);

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

        DWORD thID;
        CreateThread(NULL, 0, WorkWithClient,
            (LPVOID)client_socket, 0, &thID);
    }
    return 0;
}

DWORD WINAPI WorkWithClient(LPVOID client_socket)
{
    SOCKET my_sock;
    my_sock=(SOCKET)client_socket;
    char buff[20*1024];
    #define sHELLO "Hello, Student!\r\n"

    send(my_sock,sHELLO,strlen(sHELLO),0);

    char group[64];
    int filled = 0;
    int bytes_recv;

    while( (bytes_recv=recv(my_sock,&buff[0],sizeof(buff),0))
           && bytes_recv !=SOCKET_ERROR)
    {
        int i;
        for (i = 0; i < bytes_recv; i++)
        {
            group[filled++] = buff[i];
            if (filled < 64)
                continue;

            int counts[256];
            int order[256];
            int nunique = 0;
            int k;
            memset(counts, 0, sizeof(counts));
            for (k = 0; k < 64; k++)
            {
                unsigned char ch = (unsigned char)group[k];
                if (counts[ch] == 0)
                    order[nunique++] = ch;
                counts[ch]++;
            }

            char outmsg[1024];
            if (nunique < 3)
            {
                strcpy(outmsg,
                    "Less than 3 distinct characters. Connection closed.\r\n");
                send(my_sock, outmsg, strlen(outmsg), 0);
                nclients--;
                printf("-disconnect (less than 3 distinct characters)\n");
                PRINTNUSERS
                closesocket(my_sock);
                return 0;
            }

            strcpy(outmsg, "<");
            for (k = 0; k < nunique; k++)
            {
                char piece[64];
                unsigned char ch = (unsigned char)order[k];
                char shown[8];
                if (ch == '\n') strcpy(shown, "\\n");
                else if (ch == '\r') strcpy(shown, "\\r");
                else if (ch == '\t') strcpy(shown, "\\t");
                else { shown[0] = (char)ch; shown[1] = 0; }
                sprintf(piece, "%s'%s-%d'",
                    k ? ", " : "",
                    shown, counts[ch]);
                strcat(outmsg, piece);
            }
            strcat(outmsg, ">\r\n");
            send(my_sock, outmsg, strlen(outmsg), 0);
            printf("group processed: %s", outmsg);
            fflush(stdout);
            filled = 0;
        }
    }

    nclients--;
    printf("-disconnect\n");
    PRINTNUSERS
    closesocket(my_sock);
    return 0;
}