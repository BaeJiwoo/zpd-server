#include "GameWorld.hpp"
#include "proto/echo.pb.h"
#include "proto/session.pb.h"
#include <stdexcept>
#include <utility>

void GameWorld::AddConnection(ConnectionKey connection)
{
    PlayerSession session;
    session.connection = connection;
    m_sessions.try_emplace(connection, session);
}

std::vector<OutboundPacket> GameWorld::RemoveConnection(ConnectionKey connection)
{
    std::vector<OutboundPacket> notifications;
    const auto session = m_sessions.find(connection);
    if (session == m_sessions.end())
        return notifications;
    RemoveRoomMembership(session->second, notifications);
    m_connectionsByPlayerId.erase(session->second.playerId);
    m_sessions.erase(session);
    return notifications;
}

std::vector<OutboundPacket> GameWorld::HandleRequest(ConnectionKey connection,
                                                     const Packet& request)
{
    const auto session = m_sessions.find(connection);
    if (session == m_sessions.end())
        return {};
    std::vector<OutboundPacket> notifications;
    Packet response = DispatchRequest(request, session->second, notifications);
    response.requestId = request.requestId;
    notifications.insert(notifications.begin(), {connection, std::move(response)});
    return notifications;
}

Packet GameWorld::HandleEchoRequest(const Packet& requestPacket)
{
    Packet response;
    response.code = MessageCode::EchoResponse;

    protocol::EchoRequest request;
    if (!request.ParseFromArray(requestPacket.payload.data(),
                                static_cast<int>(requestPacket.payload.size()))) {
        response.error = ErrorCode::InvalidPayload;
        return response;
    }

    protocol::EchoResponse reply;
    reply.set_data(request.data());

    if (reply.ByteSizeLong() > ProtocolLimits::MaxPayloadBytes) {
        response.error = ErrorCode::InvalidPayload;
        return response;
    }

    std::string encoded;
    if (!reply.SerializeToString(&encoded)) {
        response.error = ErrorCode::UnknownError;
        return response;
    }

    response.payload.assign(encoded.begin(), encoded.end());

    return response;
}

Packet GameWorld::HandleEnterRequest(const Packet& requestPacket, PlayerSession& session)
{
    Packet response;
    response.code = MessageCode::EnterResponse;

    protocol::EnterRequest request;
    if (!request.ParseFromArray(requestPacket.payload.data(),
                                static_cast<int>(requestPacket.payload.size()))) {
        response.error = ErrorCode::InvalidPayload;
        return response;
    }

    if (session.state != PlayerState::AwaitingEntry) {
        response.error = ErrorCode::AlreadyEntered;
        return response;
    }

    // 0은 입장 전 ID이며, 카운터가 소진돼도 재사용하지 않습니다.
    if (m_nextPlayerId == 0) {
        response.error = ErrorCode::UnknownError;
        return response;
    }

    protocol::EnterResponse reply;
    reply.set_player_id(m_nextPlayerId);

    std::string encoded;
    if (!reply.SerializeToString(&encoded)) {
        response.error = ErrorCode::UnknownError;
        return response;
    }

    response.payload.assign(encoded.begin(), encoded.end());

    m_connectionsByPlayerId.emplace(m_nextPlayerId, session.connection);

    session.playerId = m_nextPlayerId++;
    session.state = PlayerState::Lobby;

    return response;
}

Packet GameWorld::DispatchRequest(const Packet& request, PlayerSession& session,
                                  std::vector<OutboundPacket>& notifications)
{
    Packet response;
    response.code = ResponseCodeFor(request.code);
    if (response.code == MessageCode::ErrorResponse) {
        response.error = ErrorCode::UnknownRequest;
        return response;
    }
    if (request.error != ErrorCode::None || request.requestId == 0) {
        response.error = ErrorCode::InvalidRequestStatus;
        return response;
    }
    switch (request.code) {
    case MessageCode::EchoRequest:
        return HandleEchoRequest(request);
    case MessageCode::PingRequest:
        if (!request.payload.empty())
            response.error = ErrorCode::InvalidPayload;
        return response;
    case MessageCode::ChatRequest:
        return HandleChatRequest(request, session, notifications);
    case MessageCode::EnterRequest:
        return HandleEnterRequest(request, session);
    default:
        return HandleRoomRequest(request, session, notifications);
    }
}
