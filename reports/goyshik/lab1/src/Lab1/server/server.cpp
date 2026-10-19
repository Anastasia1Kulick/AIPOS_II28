#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <winsock2.h>   // должен быть раньше windows.h
#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#pragma comment(lib, "ws2_32.lib")

#define MY_PORT 666

DWORD WINAPI WorkWithClient(LPVOID client_socket);
int nclients = 0;

// Возвращает строку, в которой слова идут в обратном порядке
std::string ReverseWords(const std::string& s)
{
    std::istringstream iss(s);
    std::vector<std::string> words;
    std::string w;
    while (iss >> w) words.push_back(w);          // разбиваем по пробелам
    std::reverse(words.begin(), words.end());     // разворачиваем порядок слов
    std::string result;
    for (size_t i = 0; i < words.size(); i++)
    {
        if (i) result += ' ';
        result += words[i];
    }
    return result;
}

int main()
{
    char buff[1024];
    printf("TCP SERVER\n");

    // Шаг 1: инициализация библиотеки сокетов
    if (WSAStartup(0x0202, (WSADATA*)&buff[0]))
    {
        printf("Error WSAStartup %d\n", WSAGetLastError());
        return -1;
    }

    // Шаг 2: создание сокета
    SOCKET mysocket = socket(AF_INET, SOCK_STREAM, 0);
    if (mysocket == INVALID_SOCKET)
    {
        printf("Error socket %d\n", WSAGetLastError());
        WSACleanup();
        return -1;
    }

    // Шаг 3: привязка к локальному адресу
    sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(MY_PORT);
    local_addr.sin_addr.s_addr = 0;   // принимать на все адреса
    if (bind(mysocket, (sockaddr*)&local_addr, sizeof(local_addr)))
    {
        printf("Error bind %d\n", WSAGetLastError());
        closesocket(mysocket);
        WSACleanup();
        return -1;
    }

    // Шаг 4: ожидание подключений
    if (listen(mysocket, 0x100))
    {
        printf("Error listen %d\n", WSAGetLastError());
        closesocket(mysocket);
        WSACleanup();
        return -1;
    }
    printf("Waiting for connections...\n");

    // Шаг 5: приём клиентов, каждый обслуживается в своём потоке
    SOCKET client_socket;
    sockaddr_in client_addr;
    int client_addr_size = sizeof(client_addr);
    while ((client_socket = accept(mysocket, (sockaddr*)&client_addr, &client_addr_size)) != INVALID_SOCKET)
    {
        nclients++;
        printf("+ [%s] new connect! Users online: %d\n",
            inet_ntoa(client_addr.sin_addr), nclients);

        // копия сокета в куче, чтобы поток не зависел от переменной цикла
        SOCKET* ps = new SOCKET(client_socket);
        DWORD thID;
        CreateThread(NULL, 0, WorkWithClient, ps, 0, &thID);
    }

    closesocket(mysocket);
    WSACleanup();
    return 0;
}

DWORD WINAPI WorkWithClient(LPVOID client_socket)
{
    SOCKET my_sock = *((SOCKET*)client_socket);
    delete (SOCKET*)client_socket;
    char buff[1024];

    const char hello[] = "Type text ending with '.'; '..' ends the session\r\n";
    send(my_sock, hello, (int)strlen(hello), 0);

    std::string sentence;   // накопленное предложение (telnet шлёт по одному символу)
    bool prev_dot = false;  // предыдущий принятый символ был точкой
    bool finish = false;    // пора завершать сеанс
    int bytes_recv;

    while (!finish && (bytes_recv = recv(my_sock, buff, sizeof(buff), 0)) > 0)
    {
        // одна порция данных может содержать несколько символов, обрабатываем по одному
        for (int i = 0; i < bytes_recv && !finish; i++)
        {
            char c = buff[i];
            if (c == '\0') continue;                 

            if (c == '.')
            {
                if (prev_dot)                        // подряд две точки ".."
                {
                    const char bye[] = "Session finished\r\n";
                    send(my_sock, bye, (int)strlen(bye), 0);
                    finish = true;
                }
                else                                 // одна точка: конец предложения
                {
                    std::string reply = ReverseWords(sentence) + "\r\n";
                    send(my_sock, reply.c_str(), (int)reply.size(), 0);
                    printf("sentence processed: %s", reply.c_str());
                    sentence.clear();
                    prev_dot = true;
                }
            }
            else
            {
                prev_dot = false;
                if (c == '\r' || c == '\n') c = ' '; // перевод строки считаем пробелом
                sentence += c;
            }
        }
    }

    nclients--;
    printf("- disconnect. Users online: %d\n", nclients);
    shutdown(my_sock, SD_BOTH);
    closesocket(my_sock);
    return 0;
}