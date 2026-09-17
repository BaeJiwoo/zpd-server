#ifndef ZPD_CLIENTSETTINGS_HPP
#define ZPD_CLIENTSETTINGS_HPP
#include <chrono>
#include <cstdint>
namespace ClientSettings {
inline constexpr std::uint32_t SendTimeoutMilliseconds = 10000;
inline constexpr std::uint32_t InfiniteReceiveTimeout = 0;
inline constexpr auto InputPollInterval = std::chrono::milliseconds(20);
inline constexpr const char* CommandHelp =
    "/enter /create <capacity> /join <roomId> /leave /members /move <x> <y> <z> /positions /game <command> [payload] /ping /say <text> /echo <text> /quit";
}
#endif // ZPD_CLIENTSETTINGS_HPP
