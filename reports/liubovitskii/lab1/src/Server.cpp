#include <iostream>
#include <fstream>
#include <string>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed." << endl;
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        cout << "Socket creation failed." << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(5555);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "Bind failed." << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        cout << "Listen failed." << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "Server is listening on port 5555..." << endl;

    sockaddr_in clientAddr;
    int clientAddrSize = sizeof(clientAddr);
    SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);

    if (clientSocket != INVALID_SOCKET) {
        cout << "Client connected!" << endl;

        string clientData = "";
        char buf[512];

        while (true) {
            int bytesReceived = recv(clientSocket, buf, sizeof(buf) - 1, 0);
            if (bytesReceived <= 0) {
                cout << "Client disconnected." << endl;
                break;
            }
            buf[bytesReceived] = '\0';
            clientData += buf;

            size_t endPos = clientData.find("#end##");
            if (endPos != string::npos) {

                if (clientData.find("save ") == 0) {
                    size_t nameStart = 5;
                    size_t nameEnd = clientData.find_first_of(" \r\n", nameStart);

                    if (nameEnd != string::npos && nameEnd < endPos) {
                        string filename = clientData.substr(nameStart, nameEnd - nameStart);
                        size_t dataStart = nameEnd + 1;

                        string fileContent = clientData.substr(dataStart, endPos - dataStart);

                        ofstream outFile(filename.c_str(), ios::binary);
                        if (outFile.is_open()) {
                            outFile << fileContent;
                            outFile.close();

                            string msg = "Success: File '" + filename + "' created.\n";
                            send(clientSocket, msg.c_str(), msg.length(), 0);
                            cout << "File " << filename << " saved successfully." << endl;
                        }
                        else {
                            string msg = "Error: Cannot create file.\n";
                            send(clientSocket, msg.c_str(), msg.length(), 0);
                            cout << "Failed to create file " << filename << ". Disconnecting client." << endl;
                            closesocket(clientSocket);
                            break;
                        }
                    }
                }
                clientData.erase(0, endPos + 6);
            }
        }
        closesocket(clientSocket);
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}