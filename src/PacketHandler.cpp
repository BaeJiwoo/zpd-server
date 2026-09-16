#include "PacketHandler.hpp"
#include "proto/echo.pb.h"
#include "PacketCode.hpp"
#include "Packet.hpp"

#include <exception>
#include <iostream>
#include <set>

bool PacketHandler::Run(SendCallback sendPacket)
{
    if (m_logicRunning || !sendPacket)
        return false;

    m_sendPacket = std::move(sendPacket);

    m_logicRunning = true;

    try {
        m_logicThread = std::thread(&PacketHandler::LogicWorker, this);
    } catch (...) {
        m_logicRunning = false;
        m_sendPacket = {};
        return false;
    }
    return true;
}

void PacketHandler::Stop()
{
    if (!m_logicRunning)
        return;

    {
        {
            std::unique_lock lock(m_mutex);
            m_logicRunning = false;
        }
        m_queueReady.notify_all();

        if (m_logicThread.joinable()) {
            m_logicThread.join();
        }

        {
            std::unique_lock lock(m_mutex);
            while (!m_eventQueue.empty()) {
                m_eventQueue.pop();
            }

            m_pendingBytesByConnection.clear();
            m_liveConnections.clear();
            m_sendPacket = {};
        }
    }
}

void PacketHandler::LogicWorker()
{
    std::map<ConnectionKey, PlayerSession> sessions;
    std::map<std::uint64_t, ConnectionKey> connectionsByPlayerId;
    std::uint64_t nextPlayerId = 1;

    while (true) {
        PacketHandlerEvent event;

        {
            std::unique_lock lock(m_mutex);

            m_queueReady.wait(lock, [this] { return !m_logicRunning || !m_eventQueue.empty(); });

            if (!m_logicRunning)
                return;

            event = std::move(m_eventQueue.front());
            m_eventQueue.pop();
        }

        try {
            switch (event.type) {
            case PacketHandlerEventType::Connected: {
                PlayerSession session;
                session.connection = event.connection;

                sessions.try_emplace(event.connection, session);
                break;
            }

            case PacketHandlerEventType::PacketReceived: {
                if (!sessions.contains(event.connection))
                    break;

                {
                    std::lock_guard lock(m_mutex);
                    if (!m_liveConnections.contains(event.connection)) {
                        break;
                    }
                }

                Packet response = Dispatch(event.packet, sessions.at(event.connection),
                                           nextPlayerId, connectionsByPlayerId);
                response.requestId = event.packet.requestId;
                const auto bytes = response.Serialize();

                m_sendPacket(event.connection, bytes.data(),
                             static_cast<std::uint32_t>(bytes.size()));
                break;
            }

            case PacketHandlerEventType::Disconnected: {
                const auto sessionIt = sessions.find(event.connection);
                if (sessionIt == sessions.end())
                    break;

                const auto playerId = sessionIt->second.playerId;

                if (playerId != 0)
                    connectionsByPlayerId.erase(playerId);

                sessions.erase(sessionIt);
                break;
            }
            }

        } catch (const std::exception& error) {
            std::cerr << "[packet processing failed] " << error.what() << '\n';
        } catch (...) {
            std::cerr << "[packet processing failed] unknown error\n";
        }
    }
}

Packet PacketHandler::Echo(const Packet& requestPacket)
{
    Packet response;
    response.code = MessageCode::EchoResponse;

    protocol::EchoRequest request;
    if (!request.ParseFromArray(requestPacket.payload.data(),
                                static_cast<int>(requestPacket.payload.size()))) {
        response.error = ErrorCode::InvalidPayload;
        return response;
    }

    std::cout << "[Echo received] " << request.data() << '\n';

    protocol::EchoResponse reply;
    reply.set_data(request.data());

    if (reply.ByteSizeLong() > PacketHeader::MaxPayloadSize) {
        response.error = ErrorCode::InvalidPayload;
        return response;
    }

    std::string encoded;
    if (!reply.SerializeToString(&encoded)) {
        response.error = ErrorCode::UnknownError;
        return response;
    }

    response.payload.assign(encoded.begin(), encoded.end());
    std::cout << "[Echo response] " << reply.data() << '\n';
    return response;
}

bool PacketHandler::EnqueueConnected(ConnectionKey connection)
{
    std::lock_guard lock(m_mutex);

    if (!m_logicRunning) {
        return false;
    }

    if (m_liveConnections.contains(connection)) {
        return true;
    }

    const auto used = m_eventQueue.size() + m_liveConnections.size();

    if (used + 2 > MaxEventQueueSize) {
        return false;
    }

    m_liveConnections.insert(connection);

    try {
        m_eventQueue.push({PacketHandlerEventType::Connected, connection, Packet{}});
    } catch (...) {
        m_liveConnections.erase(connection);
        throw;
    }

    m_queueReady.notify_one();
    return true;
}

void PacketHandler::EnqueueDisconnected(ConnectionKey connection)
{
    {
        std::lock_guard lock(m_mutex);

        m_pendingBytesByConnection.erase(connection);

        if (!m_liveConnections.contains(connection)) {
            return;
        }

        if (!m_logicRunning) {
            m_liveConnections.erase(connection);
            return;
        }

        m_eventQueue.push({PacketHandlerEventType::Disconnected, connection, Packet{}});

        m_liveConnections.erase(connection);
    }

    m_queueReady.notify_one();
    return;
}

Packet PacketHandler::EnterSession(const Packet& requestPacket, PlayerSession& session,
                                   std::uint64_t& nextPlayerId,
                                   std::map<std::uint64_t, ConnectionKey>& connectionsByPlayerId)
{
    Packet response;
    response.code = MessageCode::EnterResponse;

    protocol::EnterRequest request;
    if (!request.ParseFromArray(requestPacket.payload.data(),
                                static_cast<int>(requestPacket.payload.size()))) {
        response.error = ErrorCode::InvalidPayload;
        return response;
    }

    if (session.state != PlayerState::Connected) {
        response.error = ErrorCode::AlreadyEntered;
        return response;
    }

    // 0은 입장 전 ID이며, 카운터가 소진돼도 재사용하지 않습니다.
    if (nextPlayerId == 0) {
        response.error = ErrorCode::UnknownError;
        return response;
    }

    protocol::EnterResponse reply;
    reply.set_player_id(nextPlayerId);

    std::string encoded;
    if (!reply.SerializeToString(&encoded)) {
        response.error = ErrorCode::UnknownError;
        return response;
    }

    response.payload.assign(encoded.begin(), encoded.end());

    connectionsByPlayerId.emplace(nextPlayerId, session.connection);

    session.playerId = nextPlayerId++;
    session.state = PlayerState::Lobby;

    return response;
}
