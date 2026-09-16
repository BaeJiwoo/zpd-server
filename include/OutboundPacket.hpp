#ifndef ZPD_OUTBOUNDPACKET_HPP
#define ZPD_OUTBOUNDPACKET_HPP
#include "ConnectionKey.hpp"
#include "Packet.hpp"
struct OutboundPacket
{
    ConnectionKey recipient;
    Packet packet;
};
#endif // ZPD_OUTBOUNDPACKET_HPP
