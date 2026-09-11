#ifndef ZPD_PACKET_HPP
#define ZPD_PACKET_HPP

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "PacketCode.hpp"

struct PacketHeader
{
    static constexpr std::size_t Size = 4;
    static constexpr std::size_t RequestOffset = 2;
    static constexpr std::size_t ErrorOffset = 3;
    static constexpr std::size_t MaxPacketSize = 4096;
    static constexpr std::size_t MaxPayloadSize = MaxPacketSize - Size;

    std::uint16_t size = static_cast<std::uint16_t>(Size);
    RequestCode request = RequestCode::Echo;
    ErrorCode error = ErrorCode::None;

    // The caller must supply at least Size bytes. Never copy a native struct
    // onto the wire: its padding and byte order are implementation-dependent.
    static PacketHeader Read(const char* data) noexcept
    {
        return {
            static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(static_cast<unsigned char>(data[0])) << 8) |
                static_cast<unsigned char>(data[1])),
            static_cast<RequestCode>(static_cast<unsigned char>(data[RequestOffset])),
            static_cast<ErrorCode>(static_cast<unsigned char>(data[ErrorOffset])),
        };
    }
};

struct Packet
{
    RequestCode request = RequestCode::Echo;
    ErrorCode error = ErrorCode::None;
    std::vector<char> payload;

    std::vector<char> Serialize() const
    {
        if (payload.size() > PacketHeader::MaxPayloadSize)
            throw std::length_error("Packet payload exceeds the wire size limit");

        const auto size = static_cast<std::uint16_t>(PacketHeader::Size + payload.size());
        std::vector<char> bytes;
        bytes.reserve(size);
        bytes.push_back(static_cast<char>((size >> 8) & 0xff));
        bytes.push_back(static_cast<char>(size & 0xff));
        bytes.push_back(static_cast<char>(request));
        bytes.push_back(static_cast<char>(error));
        bytes.insert(bytes.end(), payload.begin(), payload.end());
        return bytes;
    }
};

#endif
