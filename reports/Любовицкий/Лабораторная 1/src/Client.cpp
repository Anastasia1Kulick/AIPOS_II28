#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <string>
#include <winsock2.h>
#include <conio.h> // Для _kbhit() и _getch()
#include <ctime>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// Функция для записи событий в протокол-файл
void logEvent(const string& eventType, const string& data) {
    ofstream logFile("client_protocol.txt", ios::app);
    if (logFile.is_open()) {
        time_t now = time(0);
        char* dt = ctime(&now);
        string timeStr(dt);
        timeStr.erase(timeStr.length() - 1); // Убираем перенос строки от ctime
        logFile << "[" << timeStr << "] " << eventType << ": " << data << endl;
        logFile.close();
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed." << endl;
        return 1;
    }

    string cmd, ip;
    int port;

    cout << "Enter command to connect (format: connect <IP> <PORT>)" << endl;
    cout << "Example: connect 127.0.0.1 5555" << endl << "> ";

    // Ожидание команды connect
    while (true) {
        cin >> cmd >> ip >> port;
        if (cmd == "connect") {
            cin.ignore(10000, '\n'); // Очищаем буфер после cin
            break;
        }
        else {
            cout << "Invalid command. Try again: > ";
            cin.clear();
            cin.ignore(10000, '\n');
        }
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());
    serverAddr.sin_port = htons(port);

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "Connection failed!" << endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    cout << "Connected to server!" << endl;
    cout << "Type 'save <filename> <content>#end##' to test." << endl;
    cout << "Press 'PgUp' to send the entered string." << endl;

    // Переводим сокет в неблокирующий режим для одновременного чтения сети и клавиатуры
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);

    string inputBuffer = "";

    while (true) {
        // 1. Проверяем наличие входящих данных от сервера
        char recvBuf[512];
        int bytesReceived = recv(sock, recvBuf, sizeof(recvBuf) - 1, 0);
        if (bytesReceived > 0) {
            recvBuf[bytesReceived] = '\0';
            string receivedStr(recvBuf);
            cout << "\n[Server]: " << receivedStr << "> " << inputBuffer;
            logEvent("Received", receivedStr);
        }
        else if (bytesReceived == 0 || (bytesReceived == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)) {
            cout << "\nDisconnected by server." << endl;
            break;
        }

        // 2. Проверяем нажатия клавиатуры
        if (_kbhit()) {
            int ch = _getch();

            // Обработка спец-клавиш (PgUp возвращает 0xE0 или 0, а затем 73)
            if (ch == 0 || ch == 224) {
                int extCh = _getch();
                if (extCh == 73) { // Код клавиши PgUp
                    if (!inputBuffer.empty()) {
                        send(sock, inputBuffer.c_str(), inputBuffer.length(), 0);
                        logEvent("Sent", inputBuffer);
                        cout << "\n[Sent]: " << inputBuffer << endl << "> ";
                        inputBuffer = ""; // Очищаем буфер после отправки
                    }
                }
            }
            else if (ch == '\r') { // Нажатие Enter
                inputBuffer += '\n';
                cout << endl;
            }
            else if (ch == '\b') { // Backspace
                if (!inputBuffer.empty()) {
                    inputBuffer.erase(inputBuffer.length() - 1);
                    cout << "\b \b";
                }
            }
            else {
                inputBuffer += (char)ch;
                cout << (char)ch; // Эхо-вывод
            }
        }
        Sleep(20); // Небольшая пауза, чтобы не нагружать процессор на 100%
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}