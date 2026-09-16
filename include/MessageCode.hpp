#ifndef ZPD_MESSAGECODE_HPP
#define ZPD_MESSAGECODE_HPP

#include <cstdint>

enum class MessageCode : std::uint8_t
{
    EchoRequest = 0x01,
    PingRequest = 0x02,
    EnterRequest = 0x03,
    CreateRoomRequest = 0x04,
    JoinRoomRequest = 0x05,
    LeaveRoomRequest = 0x06,
    ChatRequest = 0x07,

    EchoResponse = 0x81,
    PingResponse = 0x82,
    EnterResponse = 0x83,
    CreateRoomResponse = 0x84,
    JoinRoomResponse = 0x85,
    LeaveRoomResponse = 0x86,
    ChatResponse = 0x87,
    PlayerJoined = 0xc1,
    PlayerLeft = 0xc2,
    ChatMessage = 0xc3,
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
    case MessageCode::CreateRoomRequest:
        return MessageCode::CreateRoomResponse;
    case MessageCode::JoinRoomRequest:
        return MessageCode::JoinRoomResponse;
    case MessageCode::LeaveRoomRequest:
        return MessageCode::LeaveRoomResponse;
    case MessageCode::ChatRequest:
        return MessageCode::ChatResponse;
    default:
        return MessageCode::ErrorResponse;
    }
}

#endif // ZPD_MESSAGECODE_HPP
