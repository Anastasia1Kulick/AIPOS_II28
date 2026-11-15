#include <iostream>
#include <winsock2.h>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

constexpr int PORT = 666;
constexpr int BUFFER_SIZE = 1024;

static std::string reverseWords(const std::string& text) {
    if (!text.empty() && text.back() == '.') {
        std::string core = text.substr(0, text.length() - 1);
        std::stringstream ss(core);
        std::string word;
        std::vector<std::string> words;

        while (ss >> word) words.push_back(word);
        std::reverse(words.begin(), words.end());

        std::string result = "";
        for (size_t i = 0; i < words.size(); ++i) {
            result += words[i] + (i + 1 < words.size() ? " " : "");
        }
        return result + ".";
    }
    return text;
}

int main() {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    WSADATA wsa = { 0 };
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return 1;
    }

    // Изменено: SOCK_DGRAM заменен на SOCK_STREAM (TCP)
    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(server);
        WSACleanup();
        return 1;
    }

    // Добавлено для TCP: перевод сокета в режим прослушивания входящих подключений
    if (listen(server, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(server);
        WSACleanup();
        return 1;
    }

    std::cout << "TCP-сервер запущен и ждет подключения клиента...\n";

    sockaddr_in clientAddr{};
    int clientLen = sizeof(clientAddr);

    // Добавлено для TCP: принятие соединения от клиента
    SOCKET clientSocket = accept(server, (sockaddr*)&clientAddr, &clientLen);
    if (clientSocket == INVALID_SOCKET) {
        std::cout << "Ошибка принятия подключения!\n";
        closesocket(server);
        WSACleanup();
        return 1;
    }

    std::cout << "Клиент успешно подключен!\n";

    char buf[BUFFER_SIZE] = { 0 };

    while (true) {
        ZeroMemory(buf, BUFFER_SIZE);
        // Изменено: для TCP используется recv вместо recvfrom
        int bytes = recv(clientSocket, buf, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) {
            std::cout << "Клиент отключился.\n";
            break;
        }

        std::string msg(buf);
        std::cout << "Получено: " << msg << std::endl;

        if (msg == "..") {
            std::cout << "Получен сигнал завершения сеанса.\n";
            std::string stopMsg = "Окончание сеанса.";
            // Изменено: для TCP используется send вместо sendto
            send(clientSocket, stopMsg.c_str(), (int)stopMsg.length(), 0);
            break;
        }

        std::string response = reverseWords(msg);
        send(clientSocket, response.c_str(), (int)response.length(), 0);
    }

    closesocket(clientSocket);
    closesocket(server);
    WSACleanup();
    return 0;
}