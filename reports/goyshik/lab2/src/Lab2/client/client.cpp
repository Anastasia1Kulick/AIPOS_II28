#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <winsock2.h>
#include <string>
#include <fstream>
#include <conio.h>
#include <ctime>

#pragma comment(lib, "ws2_32.lib")

constexpr const char* LOG_FILE = "client_log.txt";
constexpr int BUFFER_SIZE = 1024;

static void saveLog(const std::string& text) {
    std::ofstream log(LOG_FILE, std::ios::app);
    if (log.is_open()) {
        time_t now = time(nullptr);
        char dt[26] = { 0 };
        ctime_s(dt, sizeof(dt), &now);
        std::string timeStr(dt);
        if (!timeStr.empty() && timeStr.back() == '\n') timeStr.pop_back();

        log << "[" << timeStr << "] " << text << std::endl;
    }
}

int main() {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    WSADATA wsa = { 0 };
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    std::string ip = "";
    int port = 0;
    std::cout << "Введите IP сервера (например, 127.0.0.1): ";
    std::cin >> ip;
    std::cout << "Введите порт (например, 666): ";
    std::cin >> port;

    saveLog("Начало соединения с " + ip + ":" + std::to_string(port));

    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = inet_addr(ip.c_str());
    srv.sin_port = htons((u_short)port);

    std::cout << "\nГотово! Вводи текст и нажимай клавишу HOME для отправки.\n";
    std::cout << "Для выхода напиши exit или отправь ..\n> ";

    std::string currentInput = "";

    while (true) {
        if (_kbhit()) {
            int ch = _getch();

            if (ch == 224 || ch == 0) {
                int ext = _getch();
                if (ext == 71) { // Клавиша HOME
                    std::cout << std::endl;

                    if (currentInput == "exit") {
                        saveLog("Соединение закрыто пользователем.");
                        break;
                    }

                    if (!currentInput.empty()) {
                        sendto(sock, currentInput.c_str(), (int)currentInput.length(), 0, (sockaddr*)&srv, sizeof(srv));

                        char buf[BUFFER_SIZE] = { 0 };
                        sockaddr_in tempAddr{};
                        int tempLen = sizeof(tempAddr);

                        int recvBytes = recvfrom(sock, buf, BUFFER_SIZE - 1, 0, (sockaddr*)&tempAddr, &tempLen);
                        if (recvBytes != SOCKET_ERROR) {
                            std::string answer(buf);

                            time_t now = time(nullptr);
                            char dt[26] = { 0 };
                            ctime_s(dt, sizeof(dt), &now);
                            std::string timeStr(dt);
                            if (!timeStr.empty() && timeStr.back() == '\n') timeStr.pop_back();

                            std::cout << "Ответ от сервера: " << answer << " (время: " << timeStr << ")\n";

                            saveLog("Получена строка: \"" + answer + "\"");
                        }
                    }

                    currentInput.clear();
                    std::cout << "> ";
                }
            }
            else if (ch == '\b') { // Backspace
                if (!currentInput.empty()) {
                    currentInput.pop_back();
                    std::cout << "\b \b";
                }
            }
            else if (ch >= 32 && ch <= 126) {
                currentInput += (char)ch;
                std::cout << (char)ch;
            }
        }
    }

    saveLog("Конец соединения.");
    closesocket(sock);
    WSACleanup();
    return 0;
}