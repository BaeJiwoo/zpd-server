#include "zpd/Define.hpp"
#include "zpd/Packet.hpp"

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
              << "Type a message and press Enter. /ping sends Ping; /quit exits."
              << std::endl;
    std::string line;
    while (std::cout << "> " << std::flush, std::getline(std::cin, line)) {
        if (line == "/quit")
            break;
        const bool ping = line == "/ping";
        std::string response;
        std::size_t offset = 0;
        do {
            const auto length = ping ? std::size_t{0} :
                (std::min)(line.size() - offset, PacketHeader::MaxPayloadSize);
            Packet request;
            request.request = ping ? RequestCode::Ping : RequestCode::Echo;
            request.payload.assign(line.data() + offset, line.data() + offset + length);
            const auto bytes = request.Serialize();
            std::size_t sent = 0;
            while (sent < bytes.size()) {
                const int count = send(connection.peer, bytes.data() + sent,
                                       static_cast<int>(bytes.size() - sent), 0);
                if (count <= 0) {
                    std::cerr << "Send failed or timed out.\n";
                    return 1;
                }
                sent += static_cast<std::size_t>(count);
            }
            const auto receiveExact = [&](char* destination, std::size_t count) {
                std::size_t received = 0;
                while (received < count) {
                    const int chunk = recv(connection.peer, destination + received,
                                           static_cast<int>(count - received), 0);
                    if (chunk <= 0)
                        return false;
                    received += static_cast<std::size_t>(chunk);
                }
                return true;
            };
            char headerBytes[PacketHeader::Size];
            if (!receiveExact(headerBytes, PacketHeader::Size)) {
                std::cerr << "Server disconnected or receive timed out.\n";
                return 1;
            }
            const auto header = PacketHeader::Read(headerBytes);
            if (header.size < PacketHeader::Size ||
                header.size > PacketHeader::MaxPacketSize ||
                header.request != request.request) {
                std::cerr << "Invalid response header.\n";
                return 1;
            }
            std::string payload(header.size - PacketHeader::Size, '\0');
            if (!receiveExact(payload.data(), payload.size())) {
                std::cerr << "Server disconnected or receive timed out.\n";
                return 1;
            }
            if (header.error != ErrorCode::None) {
                std::cerr << "Server returned error code "
                          << static_cast<unsigned int>(header.error) << ".\n";
                break;
            }
            if (payload.size() != request.payload.size() ||
                !std::equal(payload.begin(), payload.end(), request.payload.begin())) {
                std::cerr << "Response payload does not match the request.\n";
                return 1;
            }
            response += payload;
            offset += length;
            if (ping || offset == line.size())
                std::cout << (ping ? "Pong" : "Echo: " + response) << std::endl;
        } while (!ping && offset < line.size());
    }
    std::cout << "Disconnected.\n";
    return 0;
}
