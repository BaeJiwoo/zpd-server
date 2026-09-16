#include "GameWorld.hpp"
#include "ProtobufCodec.hpp"
#include "proto/chat.pb.h"

Packet GameWorld::HandleChatRequest(const Packet& request, const PlayerSession& session,
                                    std::vector<OutboundPacket>& notifications)
{
    Packet response;
    response.code = MessageCode::ChatResponse;
    protocol::ChatRequest message;
    if (!ProtobufCodec::ParsePayload(request, message) || message.text().empty() ||
        message.text().size() > ProtocolLimits::MaxChatTextBytes) {
        response.error = ErrorCode::InvalidPayload;
        return response;
    }
    if (session.state == PlayerState::AwaitingEntry) {
        response.error = ErrorCode::NotEntered;
        return response;
    }
    if (!session.roomId) {
        response.error = ErrorCode::NotInRoom;
        return response;
    }
    const auto room = m_rooms.find(*session.roomId);
    if (session.state != PlayerState::InRoom || room == m_rooms.end() ||
        !room->second.playerIds.contains(session.playerId)) {
        response.error = ErrorCode::InvalidState;
        return response;
    }
    protocol::ChatMessage broadcast;
    broadcast.set_room_id(*session.roomId);
    broadcast.set_player_id(session.playerId);
    broadcast.set_text(message.text());
    Packet notification;
    notification.code = MessageCode::ChatMessage;
    ProtobufCodec::SerializePayload(notification, broadcast);
    for (auto id : room->second.playerIds) {
        const auto connection = m_connectionsByPlayerId.find(id);
        if (connection != m_connectionsByPlayerId.end())
            notifications.push_back({connection->second, notification});
    }
    return response;
}
