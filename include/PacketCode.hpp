#ifndef ZPD_PACKETCODE_HPP
#define ZPD_PACKETCODE_HPP

#include <cstdint>

enum class MessageCode : std::uint8_t
{
    EchoRequest = 0x01,
    PingRequest = 0x02,
    EnterRequest = 0x03,

    EchoResponse = 0x81,
    PingResponse = 0x82,
    EnterResponse = 0x83,
    ErrorResponse = 0xff,
};

constexpr MessageCode ResponseCodeFor(MessageCode request) noexcept
{
    switch (request) {
    case MessageCode::EchoRequest:
        return MessageCode::EchoResponse;
    case MessageCode::PingRequest:
        return MessageCode::PingResponse;
    case MessageCode::EnterRequest:
        return MessageCode::EnterResponse;
    default:
        return MessageCode::ErrorResponse;
    }
}

enum class ErrorCode : std::uint8_t
{
    None = 0x00,
    UnknownRequest = 0x01,
    InvalidPayload = 0x02,
    InvalidRequestStatus = 0x03,
    AlreadyEntered = 0x11,
    UnknownError = 0x99
};

#endif // ZPD_PACKETCODE_HPP
