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

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
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

    saveLog("Попытка подключения к " + ip + ":" + std::to_string(port));

    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = inet_addr(ip.c_str());
    srv.sin_port = htons((u_short)port);

    if (connect(sock, (sockaddr*)&srv, sizeof(srv)) == SOCKET_ERROR) {
        std::cout << "Не удалось подключиться к серверу!\n";
        saveLog("Ошибка подключения к серверу.");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "\nГотово! Нажмите клавишу HOME для отправки.\n";
    std::cout << "Для выхода введите exit или нажмите ..\n> ";

    std::string currentInput = "";

    while (true) {
        if (_kbhit()) {
            int ch = _getch();

            if (ch == 224 || ch == 0) {
                int ext = _getch();
                if (ext == 71) { // Клавиша HOME
                    std::cout << std::endl;

                    if (currentInput == "exit") {
                        saveLog("Завершение работы приложения.");
                        break;
                    }

                    if (!currentInput.empty()) {
                        send(sock, currentInput.c_str(), (int)currentInput.length(), 0);

                        char buf[BUFFER_SIZE] = { 0 };
                        int recvBytes = recv(sock, buf, BUFFER_SIZE - 1, 0);
                        if (recvBytes > 0) {
                            std::string answer(buf);

                            time_t now = time(nullptr);
                            char dt[26] = { 0 };
                            ctime_s(dt, sizeof(dt), &now);
                            std::string timeStr(dt);
                            if (!timeStr.empty() && timeStr.back() == '\n') timeStr.pop_back();

                            std::cout << "Ответ от сервера: " << answer << " (Время: " << timeStr << ")\n";

                            saveLog("Получен ответ: \"" + answer + "\"");
                        }
                        else if (recvBytes == 0) {
                            std::cout << "Сервер закрыл соединение.\n";
                            saveLog("Сервер закрыл соединение.");
                            break;
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

    saveLog("Сеанс завершен.");
    closesocket(sock);
    WSACleanup();
    return 0;
}