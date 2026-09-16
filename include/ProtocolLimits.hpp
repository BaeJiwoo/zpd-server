#ifndef ZPD_PROTOCOLLIMITS_HPP
#define ZPD_PROTOCOLLIMITS_HPP

#include <cstddef>
#include <cstdint>

namespace ProtocolLimits {
inline constexpr std::size_t HeaderSize = 8;
inline constexpr std::size_t RequestIdOffset = 4;
inline constexpr std::size_t MessageCodeOffset = 2;
inline constexpr std::size_t ErrorCodeOffset = 3;
inline constexpr std::size_t MaxPacketBytes = 4096;
inline constexpr std::size_t MaxPayloadBytes = MaxPacketBytes - HeaderSize;
inline constexpr std::size_t MaxChatTextBytes = 1024;
inline constexpr std::uint32_t MinRoomCapacity = 1;
inline constexpr std::uint32_t MaxRoomCapacity = 16;
} // namespace ProtocolLimits

#endif // ZPD_PROTOCOLLIMITS_HPP
