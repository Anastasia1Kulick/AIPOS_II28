#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <fstream>
#include <set>
#include <sstream>
#include <string>

#pragma comment(lib, "ws2_32.lib")

namespace {

const int kListenPort = 8080;
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

std::string PeerKey(const sockaddr_in& addr)
{
    char ip[32];
    const char* dotted = inet_ntoa(addr.sin_addr);
    sprintf(ip, "%s:%u", dotted ? dotted : "?", static_cast<unsigned>(ntohs(addr.sin_port)));
    return ip;
}

bool SendTo(SOCKET sock, const sockaddr_in& to, int toLen, const std::string& text)
{
    int n = sendto(sock, text.c_str(), static_cast<int>(text.size()), 0,
                   reinterpret_cast<const sockaddr*>(&to), toLen);
    return n != SOCKET_ERROR;
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

} // namespace

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        printf("socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in bindAddr;
    memset(&bindAddr, 0, sizeof(bindAddr));
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindAddr.sin_port = htons(static_cast<u_short>(kListenPort));

    if (bind(sock, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR)
    {
        printf("bind failed: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("UDP server, variant 7, port %d\n", kListenPort);
    printf("Command: find <string> <filename>  -> number of matching lines\n");
    printf("If the string is missing, the session is closed.\n");

    std::set<std::string> alive;
    char chunk[kBufSize];

    for (;;)
    {
        sockaddr_in remote;
        int remoteLen = sizeof(remote);
        int got = recvfrom(sock, chunk, sizeof(chunk), 0,
                           reinterpret_cast<sockaddr*>(&remote), &remoteLen);
        if (got == SOCKET_ERROR)
        {
            printf("recvfrom failed: %d\n", WSAGetLastError());
            continue;
        }

        std::string who = PeerKey(remote);
        std::string packet(chunk, got);
        if (!packet.empty() && packet.back() == '\0')
            packet.pop_back();

        std::string acc = packet;
        if (acc.find('\n') == std::string::npos)
            acc.push_back('\n');

        while (!acc.empty())
        {
            size_t nl = acc.find('\n');
            std::string line;
            if (nl == std::string::npos)
            {
                line.swap(acc);
            }
            else
            {
                line = acc.substr(0, nl);
                acc.erase(0, nl + 1);
            }
            TrimInPlace(line);

            bool isNew = alive.insert(who).second;
            if (isNew)
            {
                printf("[%s] client %s connected\n", Stamp().c_str(), who.c_str());
                if (!SendTo(sock, remote, remoteLen, "Hello, Student!\n"))
                    printf("sendto failed: %d\n", WSAGetLastError());
            }

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
                    SendTo(sock, remote, remoteLen, "Usage: find <string> <filename>\n");
                    continue;
                }

                int hits = CountMatchingLines(filename, needle);
                if (hits <= 0)
                {
                    SendTo(sock, remote, remoteLen, "Session finished\n");
                    printf("[%s] %s : string not found, session closed\n",
                           Stamp().c_str(), who.c_str());
                    alive.erase(who);
                    continue;
                }

                char answer[64];
                sprintf(answer, "%d\n", hits);
                SendTo(sock, remote, remoteLen, answer);
                continue;
            }

            SendTo(sock, remote, remoteLen, line + "\n");
        }
    }
}
