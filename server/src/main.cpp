#include "ZPDServer.hpp"
#include "NetworkSettings.hpp"


#include <charconv>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

int main(int argc, char* argv[])
{
    unsigned int port = NetworkSettings::DefaultPort;
    if (argc > 2) {
        std::cerr << "Usage: zpd-server [port: 1-65535]\n";
        return EXIT_FAILURE;
    }
    if (argc == 2) {
        const std::string_view argument(argv[1]);
        const auto [end, error] =
            std::from_chars(argument.data(), argument.data() + argument.size(), port);
        if (error != std::errc{} || end != argument.data() + argument.size() || port == 0 ||
            port > 65535) {
            std::cerr << "Port must be an integer from 1 to 65535.\n";
            return EXIT_FAILURE;
        }
    }

    ZPDServer server;
    if (!server.Start(static_cast<std::uint16_t>(port), NetworkSettings::DefaultMaxConnections)) {
        std::cerr << "Server startup failed. Check the port and Windows socket resources.\n";
        return EXIT_FAILURE;
    }

    std::cout << "Packet server ready. Press Enter to stop.\n";
    std::string line;
    std::getline(std::cin, line);
    server.Stop();
    return EXIT_SUCCESS;
}
