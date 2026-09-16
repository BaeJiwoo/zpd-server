#ifndef ZPD_PLAYERSESSION_HPP
#define ZPD_PLAYERSESSION_HPP

#include "ConnectionKey.hpp"

#include <cstdint>
#include <optional>

enum class PlayerState
{
    AwaitingEntry,
    Lobby,
    InRoom
};

struct PlayerSession
{
    std::uint64_t playerId = 0;
    ConnectionKey connection;
    PlayerState state = PlayerState::AwaitingEntry;
    std::optional<std::uint64_t> roomId;
};

#endif // ZPD_PLAYERSESSION_HPP
