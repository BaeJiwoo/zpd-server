#ifndef ZPD_ERRORCODE_HPP
#define ZPD_ERRORCODE_HPP

#include <cstdint>

enum class ErrorCode : std::uint8_t
{
    None = 0,
    UnknownRequest = 1,
    InvalidPayload = 2,
    InvalidRequestStatus = 3,
    AlreadyEntered = 17,
    NotEntered = 18,
    RoomNotFound = 19,
    RoomFull = 20,
    AlreadyInRoom = 21,
    NotInRoom = 22,
    InvalidState = 23,
    InvalidCapacity = 24,
    UnknownError = 153
};

#endif // ZPD_ERRORCODE_HPP
