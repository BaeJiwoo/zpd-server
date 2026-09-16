#ifndef ZPD_PACKET_HPP
#define ZPD_PACKET_HPP

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "PacketHeader.hpp"

struct Packet
{
    MessageCode code = MessageCode::EchoRequest;
    std::uint32_t requestId = 0;
    ErrorCode error = ErrorCode::None;
    std::vector<char> payload;

    std::vector<char> Serialize() const
    {
        if (payload.size() > ProtocolLimits::MaxPayloadBytes)
            throw std::length_error("Packet payload exceeds the wire size limit");

        const auto size = static_cast<std::uint16_t>(ProtocolLimits::HeaderSize + payload.size());
        std::vector<char> bytes;
        bytes.reserve(size);
        bytes.push_back(static_cast<char>((size >> 8) & 255));
        bytes.push_back(static_cast<char>(size & 255));
        bytes.push_back(static_cast<char>(code));
        bytes.push_back(static_cast<char>(error));
        bytes.push_back(static_cast<char>((requestId >> 24) & 255));
        bytes.push_back(static_cast<char>((requestId >> 16) & 255));
        bytes.push_back(static_cast<char>((requestId >> 8) & 255));
        bytes.push_back(static_cast<char>(requestId & 255));
        bytes.insert(bytes.end(), payload.begin(), payload.end());
        return bytes;
    }
};

#endif // ZPD_PACKET_HPP
