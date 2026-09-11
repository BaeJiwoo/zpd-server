#ifndef ZPD_PACKET_CODE_HPP
#define ZPD_PACKET_CODE_HPP

#include <cstdint>

enum class RequestCode : std::uint8_t
{
    Echo = 0x01,
    Ping = 0x02,
};

enum class ErrorCode : std::uint8_t
{
    None = 0x00,
    UnknownRequest = 0x01,
    InvalidPayload = 0x02,
    InvalidRequestStatus = 0x03,
    UnknownError = 0x10
};

#endif
