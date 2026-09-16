#ifndef ZPD_PACKETHEADER_HPP
#define ZPD_PACKETHEADER_HPP

#include "MessageCode.hpp"
#include "ErrorCode.hpp"
#include "ProtocolLimits.hpp"

struct PacketHeader
{
    std::uint16_t packetSize = static_cast<std::uint16_t>(ProtocolLimits::HeaderSize);
    MessageCode code = MessageCode::EchoRequest;
    std::uint32_t requestId = 0;
    ErrorCode error = ErrorCode::None;

    static PacketHeader Read(const char* data) noexcept
    {
        PacketHeader header;
        header.packetSize = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(static_cast<unsigned char>(data[0])) << 8) |
            static_cast<unsigned char>(data[1]));
        header.code = static_cast<MessageCode>(
            static_cast<unsigned char>(data[ProtocolLimits::MessageCodeOffset]));
        header.error = static_cast<ErrorCode>(
            static_cast<unsigned char>(data[ProtocolLimits::ErrorCodeOffset]));
        header.requestId =
            (static_cast<std::uint32_t>(
                 static_cast<unsigned char>(data[ProtocolLimits::RequestIdOffset]))
             << 24) |
            (static_cast<std::uint32_t>(
                 static_cast<unsigned char>(data[ProtocolLimits::RequestIdOffset + 1]))
             << 16) |
            (static_cast<std::uint32_t>(
                 static_cast<unsigned char>(data[ProtocolLimits::RequestIdOffset + 2]))
             << 8) |
            static_cast<std::uint32_t>(
                static_cast<unsigned char>(data[ProtocolLimits::RequestIdOffset + 3]));
        return header;
    }
};

#endif // ZPD_PACKETHEADER_HPP
