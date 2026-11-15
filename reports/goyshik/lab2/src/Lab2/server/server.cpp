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

    SOCKET server = socket(AF_INET, SOCK_DGRAM, 0);
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

    std::cout << "Сервер запущен и ждет сообщения...\n";

    char buf[BUFFER_SIZE] = { 0 };
    sockaddr_in clientAddr{};
    int clientLen = sizeof(clientAddr);

    while (true) {
        ZeroMemory(buf, BUFFER_SIZE);
        int bytes = recvfrom(server, buf, BUFFER_SIZE - 1, 0, (sockaddr*)&clientAddr, &clientLen);
        if (bytes == SOCKET_ERROR) continue;

        std::string msg(buf);
        std::cout << "Получено: " << msg << std::endl;

        if (msg == "..") {
            std::cout << "Получен сигнал завершения сеанса.\n";
            std::string stopMsg = "Окончание сеанса.";
            sendto(server, stopMsg.c_str(), (int)stopMsg.length(), 0, (sockaddr*)&clientAddr, clientLen);
            break;
        }

        std::string response = reverseWords(msg);
        sendto(server, response.c_str(), (int)response.length(), 0, (sockaddr*)&clientAddr, clientLen);
    }

    closesocket(server);
    WSACleanup();
    return 0;
}