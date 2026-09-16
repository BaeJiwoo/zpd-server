#ifndef ZPD_CLIENTRESOURCES_HPP
#define ZPD_CLIENTRESOURCES_HPP
#include <WinSock2.h>
#include <Windows.h>

struct Utf8ConsoleOutput
{
    UINT previousCodePage = GetConsoleOutputCP();
    Utf8ConsoleOutput()
    {
        if (previousCodePage)
            SetConsoleOutputCP(CP_UTF8);
    }
    ~Utf8ConsoleOutput()
    {
        if (previousCodePage)
            SetConsoleOutputCP(previousCodePage);
    }
};

struct ClientSocketLifetime
{
    SOCKET socket = INVALID_SOCKET;
    ClientSocketLifetime() = default;
    ClientSocketLifetime(const ClientSocketLifetime&) = delete;
    ClientSocketLifetime& operator=(const ClientSocketLifetime&) = delete;
    ~ClientSocketLifetime()
    {
        if (socket != INVALID_SOCKET)
            closesocket(socket);
        WSACleanup();
    }
};

#endif // ZPD_CLIENTRESOURCES_HPP
