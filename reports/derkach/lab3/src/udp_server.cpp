
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <unordered_map>
#include <string>

#pragma comment(lib, "ws2_32.lib")

#define MY_PORT 666
#define GROUP_SIZE 64
#define BUFFER_SIZE 20480

struct ClientState
{
    char group[GROUP_SIZE];
    int filled;
    int stopped;
};

std::unordered_map<std::string, ClientState> clients;

std::string client_key(const sockaddr_in& addr)
{
    return std::string(inet_ntoa(addr.sin_addr)) + ":" +
           std::to_string(ntohs(addr.sin_port));
}

void send_message(SOCKET sock, const sockaddr_in& addr,
                  const char* message)
{
    sendto(sock, message, (int)strlen(message), 0,
           (const sockaddr*)&addr, sizeof(addr));
}

void process_group(SOCKET sock, const sockaddr_in& addr,
                   ClientState& state)
{
    int counts[256] = {};
    int order[256];
    int unique_count = 0;

    for (int i = 0; i < GROUP_SIZE; i++)
    {
        unsigned char ch = (unsigned char)state.group[i];

        if (counts[ch] == 0)
            order[unique_count++] = ch;

        counts[ch]++;
    }

    char outmsg[4096];

    if (unique_count < 3)
    {
        strcpy_s(outmsg, sizeof(outmsg),
            "ERROR: Less than 3 distinct characters. Session ended.");

        send_message(sock, addr, outmsg);

        state.filled = 0;
        state.stopped = 1;

        printf("%s\n", outmsg);
        return;
    }

    strcpy_s(outmsg, sizeof(outmsg), "<");

    for (int i = 0; i < unique_count; i++)
    {
        unsigned char ch = (unsigned char)order[i];
        char piece[64];

        if (ch >= 32 && ch <= 126 && ch != '\'' && ch != '\\')
        {
            sprintf_s(piece, sizeof(piece),
                "%s'%c-%d'",
                i ? ", " : "",
                ch,
                counts[ch]);
        }
        else
        {
            sprintf_s(piece, sizeof(piece),
                "%s0x%02X-%d",
                i ? ", " : "",
                ch,
                counts[ch]);
        }

        strcat_s(outmsg, sizeof(outmsg), piece);
    }

    strcat_s(outmsg, sizeof(outmsg), ">");

    send_message(sock, addr, outmsg);

    printf("Group processed: %s\n", outmsg);

    state.filled = 0;
}

int main()
{
    printf("UDP SERVER\n\n");

    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData))
    {
        printf("WSAStartup error %d\n", WSAGetLastError());
        return 1;
    }

    SOCKET mysocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (mysocket == INVALID_SOCKET)
    {
        printf("Socket error %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    sockaddr_in local_addr = {};

    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(MY_PORT);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(mysocket, (sockaddr*)&local_addr,
             sizeof(local_addr)) == SOCKET_ERROR)
    {
        printf("Bind error %d\n", WSAGetLastError());

        closesocket(mysocket);
        WSACleanup();

        return 1;
    }

    printf("Waiting for UDP datagrams, port %d\n", MY_PORT);

    char buff[BUFFER_SIZE];

    while (true)
    {
        sockaddr_in client_addr = {};
        int client_addr_size = sizeof(client_addr);

        int bytes_recv = recvfrom(
            mysocket,
            buff,
            sizeof(buff),
            0,
            (sockaddr*)&client_addr,
            &client_addr_size
        );

        if (bytes_recv == SOCKET_ERROR)
        {
            printf("recvfrom error %d\n", WSAGetLastError());
            continue;
        }

        std::string key = client_key(client_addr);

        printf("Received from %s: ", key.c_str());

        fwrite(buff, 1, bytes_recv, stdout);
        printf("\n");

        std::string message(buff, bytes_recv);

        if (message == "QUIT")
        {
            clients.erase(key);

            send_message(mysocket, client_addr, "Session ended.");

            printf("Session reset for %s\n", key.c_str());
            continue;
        }

        ClientState& state = clients[key];

        if (state.stopped)
            continue;

        for (int i = 0; i < bytes_recv; i++)
        {
            state.group[state.filled++] = buff[i];

            if (state.filled == GROUP_SIZE)
            {
                process_group(mysocket, client_addr, state);

                if (state.stopped)
                    break;
            }
        }
    }

    closesocket(mysocket);
    WSACleanup();

    return 0;
}
