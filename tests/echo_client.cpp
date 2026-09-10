#include "zpd/Define.hpp"

#include <algorithm>
#include <charconv>
#include <iostream>
#include <string>
#include <string_view>

int RunEchoClient(int argc, char* argv[])
{
    unsigned int port = 9000;
    if (argc > 2) {
        std::cerr << "Usage: zpd-server-tests [port: 1-65535]\n";
        return 1;
    }
    if (argc == 2) {
        const std::string_view argument(argv[1]);
        const auto [end, error] = std::from_chars(argument.data(),
                                                 argument.data() + argument.size(), port);
        if (error != std::errc{} || end != argument.data() + argument.size() ||
            port == 0 || port > 65535) {
            std::cerr << "Port must be an integer from 1 to 65535.\n";
            return 1;
        }
    }

    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "WinSock2 initialization failed.\n";
        return 1;
    }
    struct Cleanup {
        SOCKET peer = INVALID_SOCKET;
        ~Cleanup()
        {
            if (peer != INVALID_SOCKET)
                closesocket(peer);
            WSACleanup();
        }
    } connection;
    connection.peer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connection.peer == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        return 1;
    }

    DWORD timeout = 10000;
    if (setsockopt(connection.peer, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout)) != 0 ||
        setsockopt(connection.peer, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout)) != 0) {
        std::cerr << "Socket timeout configuration failed.\n";
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    if (connect(connection.peer, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        std::cerr << "Cannot connect to 127.0.0.1:" << port
                  << ". Start zpd-server first.\n";
        return 1;
    }

    std::cout << "Connected to 127.0.0.1:" << port << '\n'
              << "Type a message and press Enter. Type /quit to exit." << std::endl;
    std::string line;
    while (std::cout << "> " << std::flush, std::getline(std::cin, line)) {
        if (line == "/quit")
            break;
        line += '\n';
        std::string response;
        for (std::size_t offset = 0; offset < line.size();) {
            const auto length = (std::min)(line.size() - offset, std::size_t{MAX_BUFFER_SIZE});
            std::size_t sent = 0;
            while (sent < length) {
                const int count = send(connection.peer, line.data() + offset + sent,
                                       static_cast<int>(length - sent), 0);
                if (count <= 0) {
                    std::cerr << "Send failed or timed out.\n";
                    return 1;
                }
                sent += static_cast<std::size_t>(count);
            }
            std::size_t received = 0;
            while (received < length) {
                char buffer[MAX_BUFFER_SIZE];
                const int count = recv(connection.peer, buffer,
                                       static_cast<int>(length - received), 0);
                if (count <= 0) {
                    std::cerr << "Server disconnected or receive timed out.\n";
                    return 1;
                }
                response.append(buffer, static_cast<std::size_t>(count));
                received += static_cast<std::size_t>(count);
            }
            offset += length;
        }
        std::cout << "Echo: " << response << std::flush;
    }
    std::cout << "Disconnected.\n";
    return 0;
}
