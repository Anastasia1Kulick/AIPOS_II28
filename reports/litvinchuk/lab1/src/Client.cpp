#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <conio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define SERVER_IP "127.0.0.1"
#define PORT 2000

string getTime() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char buf[80];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ltm);
    return string(buf);
}

void writeLog(string text) {
    ofstream log("log.txt", ios::app);
    if (log.is_open()) {
        log << "[" << getTime() << "] " << text << endl;
        log.close();
    }
}

int main() {
    setlocale(LC_ALL, "Russian");

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Error WSAStartup" << endl;
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Error socket" << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    cout << "Connecting to server..." << endl;

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Connection failed." << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    cout << "Connected successfully!" << endl;
    writeLog("Start connection with " + string(SERVER_IP) + ":" + to_string(PORT));

    cout << "Enter text (Press PgDn to send, Esc to exit):" << endl;

    string currentLine = "";
    bool connected = true;

    cout << "> ";

    while (true) {
        if (_kbhit()) {
            int ch = _getch();

            if (ch == 27) {
                break;
            }
            else if (ch == 0 || ch == 224) {
                int specKey = _getch();

                if (specKey == 81) {
                    if (!connected) {
                        cout << "\nNot connected." << endl << "> ";
                        continue;
                    }

                    if (currentLine == "disconnect") {
                        cout << "\nDisconnecting..." << endl;
                        writeLog("End connection (disconnect)");
                        closesocket(clientSocket);
                        connected = false;
                        currentLine = "";
                        cout << "> ";
                        continue;
                    }

                    if (currentLine.empty()) {
                        cout << "\nEmpty string." << endl << "> ";
                        continue;
                    }

                    cout << "\n[PgDn] Sending line..." << endl;

                    writeLog("Send string to server: " + currentLine);

                    for (size_t i = 0; i < currentLine.length(); i += 10) {
                        string chunk = currentLine.substr(i, 10);

                        send(clientSocket, chunk.c_str(), chunk.length(), 0);

                        int accumulatedChecksum = 0;
                        int bytesReceived = recv(clientSocket, (char*)&accumulatedChecksum, sizeof(accumulatedChecksum), 0);

                        if (bytesReceived > 0) {
                            cout << "Chunk: " << chunk << " | Total Checksum: " << accumulatedChecksum << endl;
                        }
                        else {
                            cout << "Server disconnected." << endl;
                            connected = false;
                            break;
                        }
                    }

                    currentLine = "";
                    cout << "> ";
                }
            }
            else if (ch == 8) {
                if (!currentLine.empty()) {
                    currentLine.pop_back();
                    cout << "\b \b";
                }
            }
            else if (ch == 13) {
                cout << endl << "> ";
            }
            else {
                currentLine += (char)ch;
                cout << (char)ch;
            }
        }
    }

    if (connected) {
        writeLog("End connection");
        closesocket(clientSocket);
    }

    WSACleanup();
    return 0;
}