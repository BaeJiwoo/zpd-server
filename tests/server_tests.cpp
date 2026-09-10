#include "zpd/ZPDServer.hpp"

#include <chrono>
#include <conio.h>
#include <future>
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

void Echo(std::uint16_t port, std::size_t size)
{
    Socket peer;
    peer.Connect(port);
    std::string payload(size, '\0');
    for (std::size_t i = 0; i < size; ++i)
        payload[i] = static_cast<char>(i % 251);
    std::size_t sent = 0;
    while (sent < size) {
        const int count = send(peer.value, payload.data() + sent,
                               static_cast<int>((std::min)(size - sent, std::size_t{997})), 0);
        Check(count > 0, "send failed");
        sent += static_cast<std::size_t>(count);
    }
    std::string response(size, '\0');
    std::size_t received = 0;
    while (received < size) {
        const int count = recv(peer.value, response.data() + received,
                               static_cast<int>(size - received), 0);
        Check(count > 0, "recv failed or connection closed");
        received += static_cast<std::size_t>(count);
    }
    Check(response == payload, "echo bytes or order changed");
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
        Echo(server.Port(), 1);
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
        const std::string pending(4096, 'x');
        send(active.value, pending.data(), static_cast<int>(pending.size()), 0);
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
