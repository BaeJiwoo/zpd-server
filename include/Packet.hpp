#ifndef ZPD_PACKET_HPP
#define ZPD_PACKET_HPP

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "PacketCode.hpp"

struct PacketHeader
{
    static constexpr std::size_t Size = 8;
    static constexpr std::size_t RequestIdOffset = 4;
    static constexpr std::size_t CodeOffset = 2;
    static constexpr std::size_t ErrorOffset = 3;
    static constexpr std::size_t MaxPacketSize = 4096;
    static constexpr std::size_t MaxPayloadSize = MaxPacketSize - Size;

    std::uint16_t size = static_cast<std::uint16_t>(Size);
    MessageCode code = MessageCode::EchoRequest;
    std::uint32_t requestId = 0;
    ErrorCode error = ErrorCode::None;

    // 최소 Size 바이트가 필요합니다. 구조체 패딩과 무관하게 네트워크 바이트 순서로 읽습니다.
    static PacketHeader Read(const char* data) noexcept
    {
        PacketHeader header;
        header.size = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(static_cast<unsigned char>(data[0])) << 8) |
            static_cast<unsigned char>(data[1]));
        header.code = static_cast<MessageCode>(static_cast<unsigned char>(data[CodeOffset]));
        header.error = static_cast<ErrorCode>(static_cast<unsigned char>(data[ErrorOffset]));
        header.requestId =
            (static_cast<std::uint32_t>(static_cast<unsigned char>(data[RequestIdOffset])) << 24) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(data[RequestIdOffset + 1])) << 16) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(data[RequestIdOffset + 2])) << 8) |
            static_cast<std::uint32_t>(static_cast<unsigned char>(data[RequestIdOffset + 3]));
        return header;
    }
};

struct Packet
{
    MessageCode code = MessageCode::EchoRequest;
    std::uint32_t requestId = 0;
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
        bytes.push_back(static_cast<char>(code));
        bytes.push_back(static_cast<char>(error));
        bytes.push_back(static_cast<char>((requestId >> 24) & 0xff));
        bytes.push_back(static_cast<char>((requestId >> 16) & 0xff));
        bytes.push_back(static_cast<char>((requestId >> 8) & 0xff));
        bytes.push_back(static_cast<char>(requestId & 0xff));
        bytes.insert(bytes.end(), payload.begin(), payload.end());
        return bytes;
    }
};

#endif // ZPD_PACKET_HPP
