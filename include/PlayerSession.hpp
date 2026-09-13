#ifndef ZPD_PLAYERSESSION_HPP
#define ZPD_PLAYERSESSION_HPP

#include "Define.hpp"

#include <cstdint>
#include <optional>

enum class PlayerState
{
    Connected,
    Lobby
};

struct PlayerSession
{
    std::uint64_t playerId = 0;
    ConnectionKey connection;
    PlayerState state = PlayerState::Connected;
    std::optional<std::uint64_t> roomId;
};

#endif // ZPD_PLAYERSESSION_HPP
