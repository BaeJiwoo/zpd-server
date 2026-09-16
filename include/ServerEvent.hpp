#ifndef ZPD_SERVEREVENT_HPP
#define ZPD_SERVEREVENT_HPP

#include "ConnectionKey.hpp"
#include "Packet.hpp"

enum class ServerEventType
{
    Connected,
    PacketReceived,
    Disconnected
};

struct ServerEvent
{
    ServerEventType type{};
    ConnectionKey connection{};
    Packet packet; // PacketReceived 이벤트에서만 사용
};

#endif // ZPD_SERVEREVENT_HPP
