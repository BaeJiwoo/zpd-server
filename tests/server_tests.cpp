#include "ZPDServer.hpp"
#include "proto/echo.pb.h"

#include <algorithm>
#include <chrono>
#include <conio.h>
#include <future>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace std::chrono_literals;

int RunEchoClient(int argc, char* argv[]);

void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

struct Socket {
    SOCKET value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Socket() { Check(value != INVALID_SOCKET, "socket failed"); }
    ~Socket() { closesocket(value); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    void Connect(std::uint16_t port)
    {
        DWORD timeout = 3000;
        setsockopt(value, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        setsockopt(value, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        Check(connect(value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0,
              "connect failed");
    }
};

void SendAll(SOCKET peer, const char* data, std::size_t size,
             std::size_t chunkSize = PacketHeader::MaxPacketSize)
{
    std::size_t sent = 0;
    while (sent < size) {
        const int count = send(peer, data + sent,
                               static_cast<int>((std::min)(size - sent, chunkSize)), 0);
        Check(count > 0, "send failed");
        sent += static_cast<std::size_t>(count);
    }
}

void ReceiveExact(SOCKET peer, char* data, std::size_t size)
{
    std::size_t received = 0;
    while (received < size) {
        const int count = recv(peer, data + received,
                               static_cast<int>(size - received), 0);
        Check(count > 0, "recv failed or connection closed");
        received += static_cast<std::size_t>(count);
    }
}

Packet ReceivePacket(SOCKET peer)
{
    char bytes[PacketHeader::Size];
    ReceiveExact(peer, bytes, PacketHeader::Size);
    const auto header = PacketHeader::Read(bytes);
    Check(header.size >= PacketHeader::Size && header.size <= PacketHeader::MaxPacketSize,
          "invalid response size");
    Packet packet;
    packet.request = header.request;
    packet.error = header.error;
    packet.payload.resize(header.size - PacketHeader::Size);
    ReceiveExact(peer, packet.payload.data(), packet.payload.size());
    return packet;
}

void CheckResponse(SOCKET peer, RequestCode request, ErrorCode error,
                   const std::vector<char>& payload = {})
{
    const auto response = ReceivePacket(peer);
    Check(response.request == request, "response request code changed");
    Check(response.error == error, "unexpected response error code");
    Check(response.payload == payload, "response payload changed");
}

Packet BuildEchoRequest(const char* data, std::size_t available, std::size_t& dataSize)
{
    dataSize = (std::min)(available, PacketHeader::MaxPayloadSize);

    protocol::EchoRequest message;
    do {
        message.set_data(data, dataSize);
        if (message.ByteSizeLong() <= PacketHeader::MaxPayloadSize)
            break;
        --dataSize;
    } while (dataSize != 0);

    std::string encoded;
    Check(message.SerializeToString(&encoded), "EchoRequest serialization failed");

    Packet packet;
    packet.request = RequestCode::Echo;
    packet.payload.assign(encoded.begin(), encoded.end());
    return packet;
}

std::string ReceiveEchoResponse(SOCKET peer)
{
    const auto packet = ReceivePacket(peer);
    Check(packet.request == RequestCode::Echo, "expected Echo response");
    Check(packet.error == ErrorCode::None, "Echo returned an error");

    protocol::EchoResponse response;
    Check(response.ParseFromArray(packet.payload.data(),
                                  static_cast<int>(packet.payload.size())),
          "EchoResponse parsing failed");
    return response.data();
}

void Echo(std::uint16_t port, std::size_t size)
{
    Socket peer;
    peer.Connect(port);
    std::string payload(size, '\0');
    for (std::size_t i = 0; i < size; ++i)
        payload[i] = static_cast<char>(i % 251);
    std::string response;
    std::size_t offset = 0;
    do {
        std::size_t length = 0;
        const auto request = BuildEchoRequest(payload.data() + offset, size - offset, length);
        const auto bytes = request.Serialize();
        SendAll(peer.value, bytes.data(), bytes.size(), 997);
        const auto reply = ReceiveEchoResponse(peer.value);
        Check(reply.size() == length &&
              std::equal(reply.begin(), reply.end(), payload.data() + offset),
              "Echo packet payload changed");
        response += reply;
        offset += length;
    } while (offset < size);
    Check(response == payload, "echo bytes or order changed");
}

void PacketProtocol(std::uint16_t port)
{
    Socket peer;
    peer.Connect(port);

    // Literal wire bytes also check the protocol independently of Serialize().
    const char ping[] = {0x00, 0x04, 0x02, 0x00};
    SendAll(peer.value, ping, sizeof(ping), 1);
    char pong[sizeof(ping)];
    ReceiveExact(peer.value, pong, sizeof(pong));
    Check(std::equal(std::begin(ping), std::end(ping), std::begin(pong)),
          "Ping wire response changed");

    const std::string rawEcho{'a', '\0', static_cast<char>(0xff), 'z'};
    std::size_t encodedDataSize = 0;
    const Packet echo = BuildEchoRequest(rawEcho.data(), rawEcho.size(), encodedDataSize);
    Check(encodedDataSize == rawEcho.size(), "raw Echo request was unexpectedly split");
    const auto echoBytes = echo.Serialize();
    SendAll(peer.value, echoBytes.data(), echoBytes.size(), 1);
    Check(ReceiveEchoResponse(peer.value) == rawEcho, "raw Echo response changed");

    // Write several frames together; read each response by its header length.
    std::vector<char> combined(echoBytes);
    combined.insert(combined.end(), std::begin(ping), std::end(ping));
    combined.insert(combined.end(), echoBytes.begin(), echoBytes.end());
    SendAll(peer.value, combined.data(), combined.size());
    Check(ReceiveEchoResponse(peer.value) == rawEcho, "combined Echo response changed");
    CheckResponse(peer.value, RequestCode::Ping, ErrorCode::None);
    Check(ReceiveEchoResponse(peer.value) == rawEcho, "combined Echo response changed");

    const auto expectError = [&](const Packet& request, ErrorCode expected) {
        const auto bytes = request.Serialize();
        SendAll(peer.value, bytes.data(), bytes.size());
        CheckResponse(peer.value, request.request, expected);
    };
    Packet unknown;
    unknown.request = static_cast<RequestCode>(0xff);
    expectError(unknown, ErrorCode::UnknownRequest);
    Packet invalidPing;
    invalidPing.request = RequestCode::Ping;
    invalidPing.payload = {'x'};
    expectError(invalidPing, ErrorCode::InvalidPayload);
    Packet invalidStatus;
    invalidStatus.error = ErrorCode::InvalidPayload;
    expectError(invalidStatus, ErrorCode::InvalidRequestStatus);
    SendAll(peer.value, ping, sizeof(ping));
    CheckResponse(peer.value, RequestCode::Ping, ErrorCode::None);
}

void InvalidPacketSize(std::uint16_t port, std::uint16_t size)
{
    Socket peer;
    peer.Connect(port);
    const char bytes[] = {static_cast<char>(size >> 8), static_cast<char>(size & 0xff),
                          static_cast<char>(RequestCode::Echo), 0};
    SendAll(peer.value, bytes, sizeof(bytes));
    char response;
    const int count = recv(peer.value, &response, 1, 0);
    const int error = count == SOCKET_ERROR ? WSAGetLastError() : 0;
    Check(count == 0 || (count == SOCKET_ERROR && error == WSAECONNRESET),
          "invalid packet size did not close the connection");
}

int Finish(int result, bool pause)
{
    if (pause) {
        std::cout << "Press any key to exit..." << std::flush;
        DWORD mode = 0;
        if (GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode))
            static_cast<void>(_getch());
        else
            static_cast<void>(std::cin.get());
        std::cout << '\n';
    }
    return result;
}

int main(int argc, char* argv[])
{
    if (!(argc == 2 && std::string_view(argv[1]) == "--no-pause"))
        return RunEchoClient(argc, argv);
    const bool pause = !(argc == 2 && std::string_view(argv[1]) == "--no-pause");
    if (argc > 2 || (argc == 2 && pause)) {
        std::cerr << "Usage: zpd-server-tests [--no-pause]\n";
        return Finish(1, pause);
    }
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "WinSock2 initialization failed.\n";
        return Finish(1, pause);
    }
    int result = 0;
    std::cout << "Running integration tests with embedded servers on temporary ports.\n"
              << "Server logs appear below. No external server is required." << std::endl;
    try {
        ZPDServer server;
        Check(!server.Start(0, 0), "zero capacity accepted");
        Check(server.Start(0, 16, 4), "start failed");
        Check(!server.Start(0, 16, 4), "double start accepted");
        {
            ZPDServer conflicting;
            Check(!conflicting.Start(server.Port(), 1, 1), "occupied port accepted");
            Check(conflicting.Start(0, 1, 1), "start after bind failure failed");
            Echo(conflicting.Port(), 32);
        }
        PacketProtocol(server.Port());
        InvalidPacketSize(server.Port(), 0);
        InvalidPacketSize(server.Port(), 3);
        InvalidPacketSize(server.Port(), 4097);
        Echo(server.Port(), 0);
        Echo(server.Port(), 1);
        Echo(server.Port(), PacketHeader::MaxPayloadSize);
        Echo(server.Port(), 4096);
        Echo(server.Port(), 256 * 1024);
        std::vector<std::future<void>> clients;
        for (int i = 0; i < 8; ++i)
            clients.push_back(std::async(std::launch::async, [&server] {
                Echo(server.Port(), 64 * 1024);
            }));
        for (auto& client : clients)
            client.get();
        for (int i = 0; i < 100; ++i)
            Echo(server.Port(), 257);

        Socket active;
        active.Connect(server.Port());
        const std::string pendingData(PacketHeader::MaxPayloadSize, 'x');
        std::size_t pendingDataSize = 0;
        const Packet pending = BuildEchoRequest(pendingData.data(), pendingData.size(),
                                                pendingDataSize);
        const auto pendingBytes = pending.Serialize();
        SendAll(active.value, pendingBytes.data(), pendingBytes.size());
        server.Stop();
        server.Stop();
        Check(server.Start(0, 1, 2), "restart failed");
        Echo(server.Port(), 8192);
        server.Stop();

        Check(server.Start(0, 1, 4), "single-slot start failed");
        for (int i = 0; i < 30; ++i) {
            bool completed = false;
            for (int attempt = 0; attempt < 100 && !completed; ++attempt) {
                try {
                    Echo(server.Port(), 1024);
                    completed = true;
                } catch (const std::runtime_error&) {
                    std::this_thread::sleep_for(2ms);
                }
            }
            Check(completed, "single slot did not become reusable");
        }
        server.Stop();

        Socket aliveDuringDestruction;
        {
            ZPDServer scoped;
            Check(scoped.Start(0, 2, 2), "scoped start failed");
            aliveDuringDestruction.Connect(scoped.Port());
            std::this_thread::sleep_for(20ms);
        }
        std::cout << "All server integration checks passed.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    WSACleanup();
    return Finish(result, pause);
}
