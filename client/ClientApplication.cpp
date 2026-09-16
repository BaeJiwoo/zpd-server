#include "ClientApplication.hpp"
#include "ClientResources.hpp"
#include "RoomChatClient.hpp"
#include "ConsoleInput.hpp"
#include "ClientSettings.hpp"
#include "NetworkSettings.hpp"
#include <charconv>
#include <iostream>
#include <string_view>

int RunChatClient(int argc, char* argv[])
{
    Utf8ConsoleOutput consoleEncoding;
    unsigned int port = NetworkSettings::DefaultPort;
    if (argc > 2) {
        std::cerr << "Usage: zpd-client [port: 1-65535]\n";
        return 1;
    }
    if (argc == 2) {
        const std::string_view argument(argv[1]);
        const auto [end, error] =
            std::from_chars(argument.data(), argument.data() + argument.size(), port);
        if (error != std::errc{} || end != argument.data() + argument.size() || port == 0 ||
            port > 65535) {
            std::cerr << "Port must be an integer from 1 to 65535.\n";
            return 1;
        }
    }

    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "WinSock2 initialization failed.\n";
        return 1;
    }
    ClientSocketLifetime connection;
    connection.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connection.socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        return 1;
    }

    DWORD timeout = ClientSettings::SendTimeoutMilliseconds;
    DWORD receiveTimeout = ClientSettings::InfiniteReceiveTimeout;
    if (setsockopt(connection.socket, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&receiveTimeout), sizeof(receiveTimeout)) != 0 ||
        setsockopt(connection.socket, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout)) != 0) {
        std::cerr << "Socket timeout configuration failed.\n";
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    if (connect(connection.socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        std::cerr << "Cannot connect to 127.0.0.1:" << port << ". Start zpd-server first.\n";
        return 1;
    }

    std::cout
        << "Connected to 127.0.0.1:" << port << '\n'
        << "Development entry only (no authentication). Commands:\n"
        << ClientSettings::CommandHelp << '\n'
        << "Text sends chat to everyone in your room. Notifications arrive while waiting for input."
        << std::endl;
    RoomChatClient client(connection.socket);
    client.Start();
    RunConsoleInput(client);
    client.Stop();
    std::cout << "Disconnected.\n";
    return client.HasFailed() ? 1 : 0;
}
