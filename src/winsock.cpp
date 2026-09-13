#include "winsock.hpp"

#include <winsock2.h>

#include <iostream>

bool zpd::CheckWinsock()
{
    WSADATA data{};
    const int startupResult = WSAStartup(MAKEWORD(2, 2), &data);
    if (startupResult != 0) {
        std::cerr << "WSAStartup failed: " << startupResult << '\n';
        return false;
    }

    const SOCKET socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socketHandle == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << '\n';
        WSACleanup();
        return false;
    }

    const int closeResult = closesocket(socketHandle);
    if (closeResult == SOCKET_ERROR) {
        std::cerr << "Socket close failed: " << WSAGetLastError() << '\n';
    }
    const int cleanupResult = WSACleanup();
    if (cleanupResult == SOCKET_ERROR) {
        std::cerr << "WSACleanup failed: " << WSAGetLastError() << '\n';
    }
    if (closeResult == SOCKET_ERROR || cleanupResult == SOCKET_ERROR) {
        return false;
    }

    return true;
}
