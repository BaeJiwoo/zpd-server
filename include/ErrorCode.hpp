#ifndef ZPD_ERRORCODE_HPP
#define ZPD_ERRORCODE_HPP

#include <cstdint>

enum class ErrorCode : std::uint8_t
{
    None = 0x00,
    UnknownRequest = 0x01,
    InvalidPayload = 0x02,
    InvalidRequestStatus = 0x03,
    AlreadyEntered = 0x11,
    NotEntered = 0x12,
    RoomNotFound = 0x13,
    RoomFull = 0x14,
    AlreadyInRoom = 0x15,
    NotInRoom = 0x16,
    InvalidState = 0x17,
    InvalidCapacity = 0x18,
    UnknownError = 0x99
};

#endif // ZPD_ERRORCODE_HPP
