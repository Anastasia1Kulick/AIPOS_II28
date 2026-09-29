#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <string>
#include <winsock2.h>
#include <conio.h>
#include <ctime>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

void logEvent(const string& eventType, const string& data) {
    ofstream logFile("client_protocol.txt", ios::app);
    if (logFile.is_open()) {
        time_t now = time(0);
        char* dt = ctime(&now);
        string timeStr(dt);
        timeStr.erase(timeStr.length() - 1);
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

    while (true) {
        cin >> cmd >> ip >> port;
        if (cmd == "connect") {
            cin.ignore(10000, '\n');
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

    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);

    string inputBuffer = "";

    while (true) {
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

        if (_kbhit()) {
            int ch = _getch();
            if (ch == 0 || ch == 224) {
                int extCh = _getch();
                if (extCh == 73) {
                    if (!inputBuffer.empty()) {
                        send(sock, inputBuffer.c_str(), inputBuffer.length(), 0);
                        logEvent("Sent", inputBuffer);
                        cout << "\n[Sent]: " << inputBuffer << endl << "> ";
                        inputBuffer = "";
                    }
                }
            }
            else if (ch == '\r') {
                inputBuffer += '\n';
                cout << endl;
            }
            else if (ch == '\b') {
                if (!inputBuffer.empty()) {
                    inputBuffer.erase(inputBuffer.length() - 1);
                    cout << "\b \b";
                }
            }
            else {
                inputBuffer += (char)ch;
                cout << (char)ch;
            }
        }
        Sleep(20);
    }
    closesocket(sock);
    WSACleanup();
    return 0;
}