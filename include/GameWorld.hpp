#ifndef ZPD_GAMEWORLD_HPP
#define ZPD_GAMEWORLD_HPP
#include "OutboundPacket.hpp"
#include "PlayerSession.hpp"
#include "RoomSession.hpp"
#include <map>
#include <vector>

class GameWorld
{
  public:
    void AddConnection(ConnectionKey connection);
    std::vector<OutboundPacket> RemoveConnection(ConnectionKey connection);
    std::vector<OutboundPacket> HandleRequest(ConnectionKey connection, const Packet& request);

  private:
    Packet DispatchRequest(const Packet& request, PlayerSession& session,
                           std::vector<OutboundPacket>& notifications);
    Packet HandleChatRequest(const Packet& request, const PlayerSession& session,
                             std::vector<OutboundPacket>& notifications);
    Packet HandleRoomRequest(const Packet& request, PlayerSession& session,
                             std::vector<OutboundPacket>& notifications);
    void RemoveRoomMembership(PlayerSession& session, std::vector<OutboundPacket>& notifications);
    void BuildMembershipNotifications(const RoomSession& room, std::uint64_t playerId,
                                      MembershipChange change,
                                      std::vector<OutboundPacket>& notifications);
    static Packet HandleEchoRequest(const Packet& requestPacket);

    Packet HandleEnterRequest(const Packet& requestPacket, PlayerSession& session);

    std::map<ConnectionKey, PlayerSession> m_sessions;
    std::map<std::uint64_t, ConnectionKey> m_connectionsByPlayerId;
    std::map<std::uint64_t, RoomSession> m_rooms;
    std::uint64_t m_nextPlayerId = 1;
    std::uint64_t m_nextRoomId = 1;
};
#endif // ZPD_GAMEWORLD_HPP
