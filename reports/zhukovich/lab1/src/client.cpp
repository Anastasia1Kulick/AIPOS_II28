#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <string>

#pragma comment(lib, "ws2_32.lib")

namespace {

SOCKET gSocket = INVALID_SOCKET;
volatile LONG gOnline = 0;
HANDLE gRecvThread = NULL;
std::string gPeerHost;
int gPeerPort = 0;
std::string gDraft;
FILE* gJournal = NULL;
CRITICAL_SECTION gLock;

const int kEndScan = 79;
const int kEsc = 27;

std::string NowText()
{
    time_t t = time(NULL);
    tm local;
    localtime_s(&local, &t);
    char buf[40];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local);
    return buf;
}


void Journal(const char* direction, const std::string& payload)
{
    EnterCriticalSection(&gLock);
    if (gJournal)
    {
        fprintf(gJournal, "%s  %s  %s\n", NowText().c_str(), direction, payload.c_str());
        fflush(gJournal);
    }
    LeaveCriticalSection(&gLock);
}

void Show(const std::string& text)
{
    EnterCriticalSection(&gLock);
    printf("\n%s\n> %s", text.c_str(), gDraft.c_str());
    fflush(stdout);
    LeaveCriticalSection(&gLock);
}

void MarkOfflineAndShutdown()
{
    if (InterlockedCompareExchange(&gOnline, 0, 1) != 1)
        return;
    if (gSocket != INVALID_SOCKET)
        shutdown(gSocket, SD_BOTH);
}

void JoinRecvAndCloseSocket()
{
    if (gRecvThread)
    {
        WaitForSingleObject(gRecvThread, INFINITE);
        CloseHandle(gRecvThread);
        gRecvThread = NULL;
    }
    if (gSocket != INVALID_SOCKET)
    {
        closesocket(gSocket);
        gSocket = INVALID_SOCKET;
    }
}

DWORD WINAPI PumpIncoming(LPVOID)
{
    SOCKET sock = gSocket;
    char buf[1024];
    int n;
    std::string rest;

    while ((n = recv(sock, buf, sizeof(buf), 0)) > 0)
    {
        rest.append(buf, n);
        size_t pos;
        while ((pos = rest.find('\n')) != std::string::npos)
        {
            std::string line = rest.substr(0, pos);
            rest.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty())
                continue;
            Journal("RECV", line);
            Show(std::string("<< ") + line);
        }
    }

    if (InterlockedCompareExchange(&gOnline, 0, 1) == 1)
        Show("Server closed the connection.");
    return 0;
}

bool ResolveIPv4(const std::string& host, unsigned long* ipOut)
{
    unsigned long ip = inet_addr(host.c_str());
    if (ip != INADDR_NONE)
    {
        *ipOut = ip;
        return true;
    }
    hostent* he = gethostbyname(host.c_str());
    if (!he || !he->h_addr_list[0])
        return false;
    memcpy(ipOut, he->h_addr_list[0], sizeof(unsigned long));
    return true;
}

void CmdConnect(const std::string& host, int port)
{
    if (InterlockedCompareExchange(&gOnline, 0, 0) != 0)
    {
        Show("Already connected. Use disconnect first.");
        return;
    }
    JoinRecvAndCloseSocket();

    unsigned long ip = 0;
    if (!ResolveIPv4(host, &ip))
    {
        Show(std::string("Cannot resolve ") + host);
        return;
    }

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET)
    {
        Show(std::string("socket() error ") + std::to_string(WSAGetLastError()));
        return;
    }

    sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(static_cast<u_short>(port));
    dest.sin_addr.s_addr = ip;

    if (connect(s, reinterpret_cast<sockaddr*>(&dest), sizeof(dest)) == SOCKET_ERROR)
    {
        Show(std::string("connect() failed, WSA=") + std::to_string(WSAGetLastError()));
        closesocket(s);
        return;
    }

    gSocket = s;
    gPeerHost = host;
    gPeerPort = port;
    InterlockedExchange(&gOnline, 1);
    Show(std::string("Connected to ") + host + ":" + std::to_string(port));

    DWORD tid = 0;
    gRecvThread = CreateThread(NULL, 0, PumpIncoming, NULL, 0, &tid);
    if (!gRecvThread)
        Show("Warning: receive thread was not created.");
}

void CmdDisconnect(const std::string& host, int port)
{
    if (InterlockedCompareExchange(&gOnline, 0, 0) == 0)
    {
        Show("Not connected.");
        return;
    }
    if (host != gPeerHost || port != gPeerPort)
    {
        Show(std::string("Not connected to ") + host + ":" + std::to_string(port) +
             " (current: " + gPeerHost + ":" + std::to_string(gPeerPort) + ")");
        return;
    }
    MarkOfflineAndShutdown();
    JoinRecvAndCloseSocket();
    Show("Disconnected.");
}

void SubmitDraft(const std::string& line)
{
    if (line.empty())
    {
        EnterCriticalSection(&gLock);
        printf("\n> ");
        fflush(stdout);
        LeaveCriticalSection(&gLock);
        return;
    }

    char verb[24] = "";
    char host[128] = "";
    int port = 0;
    int parsed = sscanf(line.c_str(), "%23s %127s %d", verb, host, &port);

    if (parsed >= 1 && (!strcmp(verb, "connect") || !strcmp(verb, "disconnect")))
    {
        if (parsed != 3)
        {
            Show(std::string("Syntax: ") + verb + " <address> <port>");
            return;
        }
        if (!strcmp(verb, "connect"))
            CmdConnect(host, port);
        else
            CmdDisconnect(host, port);
        return;
    }

    if (InterlockedCompareExchange(&gOnline, 0, 0) == 0)
    {
        Show("Not connected. Use: connect <address> <port>");
        return;
    }

    std::string payload = line + "\n";
    if (send(gSocket, payload.c_str(), static_cast<int>(payload.size()), 0) == SOCKET_ERROR)
        Show(std::string("send() error ") + std::to_string(WSAGetLastError()));
    else
    {
        Journal("SENT", line);
        Show("(sent)");
    }
}

} 

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);
    InitializeCriticalSection(&gLock);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    gJournal = fopen("zhukovich_lab1.log", "a");
    if (!gJournal)
        printf("Note: cannot write zhukovich_lab1.log\n");

    printf("TCP client, variant 3\n");
    printf("Send a line with End. Esc — exit.\n");
    printf("connect <address> <port>\n");
    printf("disconnect <address> <port>\n");
    printf("Server: find <string> <filename>\n\n> ");

    for (;;)
    {
        int ch = _getch();
        if (ch == kEsc)
            break;

        if (ch == 0 || ch == 224)
        {
            int extra = _getch();
            if (extra == kEndScan)
            {
                std::string copy;
                EnterCriticalSection(&gLock);
                copy.swap(gDraft);
                LeaveCriticalSection(&gLock);
                SubmitDraft(copy);
            }
            continue;
        }

        EnterCriticalSection(&gLock);
        if (ch == 8)
        {
            if (!gDraft.empty())
            {
                gDraft.pop_back();
                _putch(8);
                _putch(' ');
                _putch(8);
            }
        }
        else if (ch >= 32 && ch < 127)
        {
            gDraft += static_cast<char>(ch);
            _putch(ch);
        }
        LeaveCriticalSection(&gLock);
    }

    MarkOfflineAndShutdown();
    JoinRecvAndCloseSocket();
    if (gJournal)
        fclose(gJournal);
    WSACleanup();
    DeleteCriticalSection(&gLock);
    return 0;
}
