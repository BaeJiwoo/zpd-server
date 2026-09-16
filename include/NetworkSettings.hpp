#ifndef ZPD_NETWORKSETTINGS_HPP
#define ZPD_NETWORKSETTINGS_HPP

#include <cstddef>
#include <cstdint>

namespace NetworkSettings {
inline constexpr std::uint16_t DefaultPort = 20000;
inline constexpr std::uint32_t ReceiveBufferBytes = 4096;
inline constexpr std::size_t MaxQueuedSends = 256;
inline constexpr std::uint32_t DefaultMaxConnections = 1000;
}

#endif // ZPD_NETWORKSETTINGS_HPP
