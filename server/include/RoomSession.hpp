#ifndef ZPD_ROOMSESSION_HPP
#define ZPD_ROOMSESSION_HPP

#include <cstdint>
#include <set>

enum class RoomState
{
    Waiting
};

enum class MembershipChange
{
    Joined,
    Left
};

struct RoomSession
{

    std::uint64_t roomId = 0;
    std::uint32_t capacity = 0;
    RoomState state = RoomState::Waiting;
    std::set<std::uint64_t> playerIds;
};

#endif // ZPD_ROOMSESSION_HPP
