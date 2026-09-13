#ifndef ZPD_PACKETHANDLEREVENT_HPP
#define ZPD_PACKETHANDLEREVENT_HPP

#include "Define.hpp"
#include "Packet.hpp"

enum class PacketHandlerEventType
{
    Connected,
    PacketReceived,
    Disconnected
};

struct PacketHandlerEvent
{
    PacketHandlerEventType type{};
    ConnectionKey connection{};
    Packet packet; // PacketReceived 이벤트에서만 사용
};

#endif // ZPD_PACKETHANDLEREVENT_HPP
