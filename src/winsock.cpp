#include "zpd/winsock.hpp"

#include <winsock2.h>

#include <iostream>

bool zpd::check_winsock()
{
    WSADATA data{};
    const int startup_result = WSAStartup(MAKEWORD(2, 2), &data);
    if (startup_result != 0) {
        std::cerr << "WSAStartup failed: " << startup_result << '\n';
        return false;
    }

    const SOCKET socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_handle == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << '\n';
        WSACleanup();
        return false;
    }

    const int close_result = closesocket(socket_handle);
    if (close_result == SOCKET_ERROR) {
        std::cerr << "Socket close failed: " << WSAGetLastError() << '\n';
    }
    const int cleanup_result = WSACleanup();
    if (cleanup_result == SOCKET_ERROR) {
        std::cerr << "WSACleanup failed: " << WSAGetLastError() << '\n';
    }
    if (close_result == SOCKET_ERROR || cleanup_result == SOCKET_ERROR) {
        return false;
    }

    return true;
}
