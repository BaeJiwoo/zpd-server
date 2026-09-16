#include "GameWorld.hpp"
#include "ProtobufCodec.hpp"
#include "proto/room.pb.h"

namespace {
template <typename Message> void RoomSnapshot(Packet& packet, const RoomSession& room)
{
    Message message;
    message.set_room_id(room.roomId);
    message.set_capacity(room.capacity);
    for (auto id : room.playerIds)
        message.add_player_ids(id);
    ProtobufCodec::SerializePayload(packet, message);
}
}

Packet GameWorld::HandleRoomRequest(const Packet& request, PlayerSession& session,
                                    std::vector<OutboundPacket>& notifications)
{
    Packet response;
    response.code = ResponseCodeFor(request.code);
    protocol::CreateRoomRequest create;
    protocol::JoinRoomRequest join;
    protocol::LeaveRoomRequest leave;
    const bool valid = request.code == MessageCode::CreateRoomRequest
                           ? ProtobufCodec::ParsePayload(request, create)
                       : request.code == MessageCode::JoinRoomRequest
                           ? ProtobufCodec::ParsePayload(request, join)
                           : ProtobufCodec::ParsePayload(request, leave);
    if (!valid) {
        response.error = ErrorCode::InvalidPayload;
        return response;
    }
    if (session.state == PlayerState::AwaitingEntry) {
        response.error = ErrorCode::NotEntered;
        return response;
    }
    if (request.code == MessageCode::LeaveRoomRequest) {
        if (!session.roomId) {
            response.error = ErrorCode::NotInRoom;
            return response;
        }
        protocol::LeaveRoomResponse reply;
        reply.set_room_id(*session.roomId);
        ProtobufCodec::SerializePayload(response, reply);
        RemoveRoomMembership(session, notifications);
        return response;
    }
    if (session.roomId) {
        response.error = ErrorCode::AlreadyInRoom;
        return response;
    }
    if (session.state != PlayerState::Lobby) {
        response.error = ErrorCode::InvalidState;
        return response;
    }
    if (request.code == MessageCode::CreateRoomRequest) {
        if (create.capacity() < ProtocolLimits::MinRoomCapacity ||
            create.capacity() > ProtocolLimits::MaxRoomCapacity) {
            response.error = ErrorCode::InvalidCapacity;
            return response;
        }
        if (m_nextRoomId == 0) {
            response.error = ErrorCode::UnknownError;
            return response;
        }
        RoomSession room;
        room.roomId = m_nextRoomId;
        room.capacity = create.capacity();
        room.playerIds.insert(session.playerId);
        RoomSnapshot<protocol::CreateRoomResponse>(response, room);
        m_rooms.emplace(room.roomId, std::move(room));
        session.roomId = m_nextRoomId++;
        session.state = PlayerState::InRoom;
        return response;
    }
    const auto it = m_rooms.find(join.room_id());
    if (it == m_rooms.end()) {
        response.error = ErrorCode::RoomNotFound;
        return response;
    }
    if (it->second.state != RoomState::Waiting) {
        response.error = ErrorCode::InvalidState;
        return response;
    }
    if (it->second.playerIds.size() >= it->second.capacity) {
        response.error = ErrorCode::RoomFull;
        return response;
    }
    auto updated = it->second;
    updated.playerIds.insert(session.playerId);
    RoomSnapshot<protocol::JoinRoomResponse>(response, updated);
    BuildMembershipNotifications(updated, session.playerId, MembershipChange::Joined,
                                 notifications);
    it->second = std::move(updated);
    session.roomId = it->first;
    session.state = PlayerState::InRoom;
    return response;
}

void GameWorld::BuildMembershipNotifications(const RoomSession& room, std::uint64_t playerId,
                                             MembershipChange change,
                                             std::vector<OutboundPacket>& notifications)
{
    Packet packet;
    if (change == MembershipChange::Joined) {
        packet.code = MessageCode::PlayerJoined;
        protocol::PlayerJoined message;
        message.set_room_id(room.roomId);
        message.set_player_id(playerId);
        ProtobufCodec::SerializePayload(packet, message);
    } else {
        packet.code = MessageCode::PlayerLeft;
        protocol::PlayerLeft message;
        message.set_room_id(room.roomId);
        message.set_player_id(playerId);
        ProtobufCodec::SerializePayload(packet, message);
    }
    for (auto id : room.playerIds) {
        const auto it = m_connectionsByPlayerId.find(id);
        if (id != playerId && it != m_connectionsByPlayerId.end())
            notifications.push_back({it->second, packet});
    }
}

void GameWorld::RemoveRoomMembership(PlayerSession& session,
                                     std::vector<OutboundPacket>& notifications)
{
    if (!session.roomId)
        return;
    const auto it = m_rooms.find(*session.roomId);
    if (it != m_rooms.end()) {
        BuildMembershipNotifications(it->second, session.playerId, MembershipChange::Left,
                                     notifications);
        it->second.playerIds.erase(session.playerId);
        if (it->second.playerIds.empty())
            m_rooms.erase(it);
    }
    session.roomId.reset();
    session.state = PlayerState::Lobby;
}
