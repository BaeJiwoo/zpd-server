#ifndef ZPD_MESSAGECODE_HPP
#define ZPD_MESSAGECODE_HPP

#include <cstdint>

enum class MessageCode : std::uint8_t
{
    EchoRequest = 1,
    PingRequest = 2,
    EnterRequest = 3,
    CreateRoomRequest = 4,
    JoinRoomRequest = 5,
    LeaveRoomRequest = 6,
    ChatRequest = 7,
    PositionUpdateRequest = 8,
    GameCommandRequest = 9,

    EchoResponse = 129,
    PingResponse = 130,
    EnterResponse = 131,
    CreateRoomResponse = 132,
    JoinRoomResponse = 133,
    LeaveRoomResponse = 134,
    ChatResponse = 135,
    PositionUpdateResponse = 136,
    GameCommandResponse = 137,
    PlayerJoined = 193,
    PlayerLeft = 194,
    ChatMessage = 195,
    RoomPositions = 196,
    GameEvent = 197,
    ErrorResponse = 255,
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
    case MessageCode::PositionUpdateRequest:
        return MessageCode::PositionUpdateResponse;
    case MessageCode::GameCommandRequest:
        return MessageCode::GameCommandResponse;
    default:
        return MessageCode::ErrorResponse;
    }
}

#endif // ZPD_MESSAGECODE_HPP
