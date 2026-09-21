#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <conio.h>
#include <winsock2.h>   // должен быть раньше windows.h
#include <windows.h>
#include <string>
#pragma comment(lib, "ws2_32.lib")

static SOCKET g_sock = INVALID_SOCKET;   // сокет текущего соединения
static volatile bool g_connected = false; // есть ли активное соединение
static HANDLE g_thread = NULL;           // поток приёма данных от сервера
static std::string g_host;               // адрес и порт текущего сервера
static int g_port = 0;
static std::string g_line;               // строка, которую сейчас набирает пользователь
static FILE* g_log = NULL;               // файл протокола событий
static CRITICAL_SECTION g_cs;            // защита консоли и файла от двух потоков

// Текущее время в виде строки
std::string Now()
{
    time_t t = time(NULL);
    tm tmv;
    localtime_s(&tmv, &t);
    char b[32];
    strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &tmv);
    return b;
}

// Запись события в файл протокола со временем
void LogEvent(const std::string& msg)
{
    EnterCriticalSection(&g_cs);
    if (g_log)
    {
        fprintf(g_log, "[%s] %s\n", Now().c_str(), msg.c_str());
        fflush(g_log);
    }
    LeaveCriticalSection(&g_cs);
}

// Вывод сообщения на консоль и повторный вывод приглашения с набранным текстом
void Info(const std::string& msg)
{
    EnterCriticalSection(&g_cs);
    printf("\n%s\n> %s", msg.c_str(), g_line.c_str());
    fflush(stdout);
    LeaveCriticalSection(&g_cs);
}

// Завершение соединения (выполняется один раз, даже если вызвали из двух мест)
void EndConnection(const char* reason)
{
    EnterCriticalSection(&g_cs);
    if (!g_connected)
    {
        LeaveCriticalSection(&g_cs);
        return;
    }
    g_connected = false;
    LeaveCriticalSection(&g_cs);

    LogEvent("Connection closed with " + g_host + ":" + std::to_string(g_port) + " (" + reason + ")");
    shutdown(g_sock, SD_BOTH);   // разбудит поток приёма, если он ждёт в recv
}

// Ждёт завершения потока приёма и закрывает сокет (вызывать, только когда соединения нет)
void CleanupSocket()
{
    if (g_thread)
    {
        WaitForSingleObject(g_thread, INFINITE);
        CloseHandle(g_thread);
        g_thread = NULL;
    }
    if (g_sock != INVALID_SOCKET)
    {
        closesocket(g_sock);
        g_sock = INVALID_SOCKET;
    }
}

// Поток приёма: читает ответы сервера, пишет в протокол и на экран
DWORD WINAPI RecvThread(LPVOID)
{
    SOCKET s = g_sock;
    char buf[1024];
    int n;
    while ((n = recv(s, buf, sizeof(buf) - 1, 0)) > 0)
    {
        buf[n] = '\0';
        std::string data = buf;
        size_t start = 0;
        while (start < data.size())          
        {
            size_t end = data.find('\n', start);
            if (end == std::string::npos) end = data.size();
            std::string line = data.substr(start, end - start);
            while (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty())
            {
                LogEvent("Received: " + line);
                Info("<< " + line);
            }
            start = end + 1;
        }
    }
    // сюда попадаем, если сервер закрыл соединение или оно оборвалось
    if (g_connected)
    {
        EndConnection("closed by server or connection lost");
        Info("Connection closed by server.");
    }
    return 0;
}

// Команда connect <адрес> <порт>
void DoConnect(const std::string& host, int port)
{
    if (g_connected)
    {
        Info("Already connected. Use disconnect first.");
        return;
    }
    CleanupSocket();   

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    unsigned long ip = inet_addr(host.c_str());
    if (ip == INADDR_NONE)                    // возможно, задано имя (например, localhost)
    {
        hostent* h = gethostbyname(host.c_str());
        if (!h)
        {
            Info("Cannot resolve host: " + host);
            return;
        }
        memcpy(&addr.sin_addr, h->h_addr_list[0], h->h_length);
    }
    else
        addr.sin_addr.s_addr = ip;

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET)
    {
        Info("socket error " + std::to_string(WSAGetLastError()));
        return;
    }
    if (connect(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        Info("Connect failed, error " + std::to_string(WSAGetLastError()));
        closesocket(s);
        return;
    }

    g_sock = s;
    g_host = host;
    g_port = port;
    g_connected = true;
    LogEvent("Connection established with " + host + ":" + std::to_string(port));
    Info("Connected to " + host + ":" + std::to_string(port));

    DWORD id;
    g_thread = CreateThread(NULL, 0, RecvThread, NULL, 0, &id);
}

// Команда disconnect <адрес> <порт>
void DoDisconnect(const std::string& host, int port)
{
    if (!g_connected)
    {
        Info("Not connected.");
        return;
    }
    if (host != g_host || port != g_port)
    {
        Info("Not connected to " + host + ":" + std::to_string(port) +
            " (current: " + g_host + ":" + std::to_string(g_port) + ")");
        return;
    }
    EndConnection("disconnect command");
    CleanupSocket();
    Info("Disconnected.");
}

// Обработка введённой строки (вызывается по нажатию Home)
void HandleLine(const std::string& line)
{
    if (line.empty())
    {
        EnterCriticalSection(&g_cs);
        printf("\n> ");
        fflush(stdout);
        LeaveCriticalSection(&g_cs);
        return;
    }

    char cmd[32] = "", host[128] = "";
    int port = 0;
    int n = sscanf(line.c_str(), "%31s %127s %d", cmd, host, &port);

    if (n >= 1 && (strcmp(cmd, "connect") == 0 || strcmp(cmd, "disconnect") == 0))
    {
        if (n != 3)
        {
            Info(std::string("Usage: ") + cmd + " <address> <port>");
            return;
        }
        if (strcmp(cmd, "connect") == 0) DoConnect(host, port);
        else DoDisconnect(host, port);
        return;
    }

    // обычная строка: отправляем серверу
    if (!g_connected)
    {
        Info("Not connected. Use: connect <address> <port>");
        return;
    }
    if (send(g_sock, line.c_str(), (int)line.size(), 0) == SOCKET_ERROR)
        Info("send error " + std::to_string(WSAGetLastError()));
    else
        Info("(sent)");
}

int main()
{
    InitializeCriticalSection(&g_cs);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa))
    {
        printf("Error WSAStartup %d\n", WSAGetLastError());
        return -1;
    }
    g_log = fopen("client_log.txt", "a");
    if (!g_log) printf("Warning: cannot open client_log.txt\n");

    printf("TCP CLIENT\n");
    printf("Type a line and press Home to submit it.\n");
    printf("Commands: connect <address> <port>, disconnect <address> <port>\n");
    printf("Esc - exit\n\n> ");

    for (;;)
    {
        int c = _getch();
        if (c == 27) break;                      // Esc: выход

        if (c == 0 || c == 224)                  // расширенная клавиша: второй код - какая именно
        {
            int k = _getch();
            if (k == 71)                         // Home
            {
                std::string line = g_line;
                EnterCriticalSection(&g_cs);
                g_line.clear();
                LeaveCriticalSection(&g_cs);
                HandleLine(line);
            }
            continue;
        }

        EnterCriticalSection(&g_cs);
        if (c == 8)                              // Backspace
        {
            if (!g_line.empty())
            {
                g_line.pop_back();
                _putch(8); _putch(' '); _putch(8);
            }
        }
        else if (c >= 32)                        // обычный символ
        {
            g_line += (char)c;
            _putch(c);
        }
        LeaveCriticalSection(&g_cs);
    }

    if (g_connected) EndConnection("client exit");
    CleanupSocket();
    if (g_log) fclose(g_log);
    WSACleanup();
    DeleteCriticalSection(&g_cs);
    return 0;
}