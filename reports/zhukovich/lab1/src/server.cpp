#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <fstream>
#include <sstream>
#include <string>

#pragma comment(lib, "ws2_32.lib")

namespace {

const int kListenPort = 8080;
const int kBacklog = 8;
const int kBufSize = 2048;

void TrimInPlace(std::string& s)
{
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
        ++i;
    if (i)
        s.erase(0, i);
}

bool SendAll(SOCKET sock, const std::string& text)
{
    const char* p = text.c_str();
    int left = static_cast<int>(text.size());
    while (left > 0)
    {
        int n = send(sock, p, left, 0);
        if (n == SOCKET_ERROR)
            return false;
        p += n;
        left -= n;
    }
    return true;
}

std::string Stamp()
{
    time_t now = time(NULL);
    tm local;
    localtime_s(&local, &now);
    char buf[40];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local);
    return buf;
}

int CountMatchingLines(const std::string& filename, const std::string& needle)
{
    std::ifstream in(filename.c_str());
    if (!in)
        return -1;

    int hits = 0;
    std::string line;
    while (std::getline(in, line))
    {
        if (line.find(needle) != std::string::npos)
            ++hits;
    }
    return hits;
}

struct PeerInfo
{
    SOCKET sock;
    std::string who;
};

unsigned __stdcall ServeClient(void* arg)
{
    PeerInfo* peer = static_cast<PeerInfo*>(arg);
    SOCKET sock = peer->sock;
    std::string who = peer->who;
    delete peer;

    printf("[%s] client %s connected\n", Stamp().c_str(), who.c_str());

    if (!SendAll(sock, "Hello, Student!\n"))
    {
        closesocket(sock);
        return 0;
    }

    std::string acc;
    char chunk[kBufSize];
    int got;

    while ((got = recv(sock, chunk, sizeof(chunk), 0)) > 0)
    {
        acc.append(chunk, got);

        for (;;)
        {
            size_t nl = acc.find('\n');
            if (nl == std::string::npos)
                break;

            std::string line = acc.substr(0, nl);
            acc.erase(0, nl + 1);
            TrimInPlace(line);
            if (line.empty())
                continue;

            printf("[%s] %s >> %s\n", Stamp().c_str(), who.c_str(), line.c_str());

            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;

            if (cmd == "find")
            {
                std::string needle;
                std::string filename;
                iss >> needle >> filename;
                if (needle.empty() || filename.empty())
                {
                    if (!SendAll(sock, "Usage: find <string> <filename>\n"))
                    {
                        closesocket(sock);
                        return 0;
                    }
                    continue;
                }

                int hits = CountMatchingLines(filename, needle);
                if (hits <= 0)
                {
                    SendAll(sock, "Session finished\n");
                    printf("[%s] %s : string not found, session closed\n",
                           Stamp().c_str(), who.c_str());
                    closesocket(sock);
                    return 0;
                }

                char answer[64];
                sprintf(answer, "%d\n", hits);
                if (!SendAll(sock, answer))
                {
                    closesocket(sock);
                    return 0;
                }
                continue;
            }

            if (!SendAll(sock, line + "\n"))
            {
                closesocket(sock);
                return 0;
            }
        }
    }

    printf("[%s] client %s disconnected\n", Stamp().c_str(), who.c_str());
    closesocket(sock);
    return 0;
}

}

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET)
    {
        printf("socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in bindAddr;
    memset(&bindAddr, 0, sizeof(bindAddr));
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindAddr.sin_port = htons(static_cast<u_short>(kListenPort));

    if (bind(listener, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR)
    {
        printf("bind failed: %d\n", WSAGetLastError());
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    if (listen(listener, kBacklog) == SOCKET_ERROR)
    {
        printf("listen failed: %d\n", WSAGetLastError());
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    printf("TCP server, variant 7, port %d\n", kListenPort);
    printf("Command: find <string> <filename>  -> number of matching lines\n");
    printf("If the string is missing, the session is closed.\n");

    for (;;)
    {
        sockaddr_in remote;
        int remoteLen = sizeof(remote);
        SOCKET client = accept(listener, reinterpret_cast<sockaddr*>(&remote), &remoteLen);
        if (client == INVALID_SOCKET)
        {
            printf("accept failed: %d\n", WSAGetLastError());
            continue;
        }

        PeerInfo* info = new PeerInfo;
        info->sock = client;
        char ip[32];
        const char* dotted = inet_ntoa(remote.sin_addr);
        sprintf(ip, "%s:%u", dotted ? dotted : "?", static_cast<unsigned>(ntohs(remote.sin_port)));
        info->who = ip;

        uintptr_t th = _beginthreadex(NULL, 0, ServeClient, info, 0, NULL);
        if (th)
            CloseHandle(reinterpret_cast<HANDLE>(th));
        else
        {
            printf("cannot spawn worker thread\n");
            closesocket(client);
            delete info;
        }
    }
}
