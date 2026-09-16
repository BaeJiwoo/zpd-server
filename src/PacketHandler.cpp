#include "PacketHandler.hpp"
#include "MessageCode.hpp"
#include "Packet.hpp"

#include <exception>
#include <iostream>
#include <set>

bool PacketHandler::Start(SendCallback sendPacket, DisconnectCallback disconnect)
{
    if (m_logicRunning || !sendPacket)
        return false;

    m_sendPacket = std::move(sendPacket);
    m_disconnect = std::move(disconnect);

    m_logicRunning = true;

    try {
        m_logicThread = std::thread(&PacketHandler::LogicWorker, this);
    } catch (...) {
        m_logicRunning = false;
        m_sendPacket = {};
        m_disconnect = {};
        return false;
    }
    return true;
}

void PacketHandler::Stop()
{
    {
        std::lock_guard lock(m_mutex);
        if (!m_logicRunning)
            return;
        m_logicRunning = false;
    }
    m_queueReady.notify_all();
    if (m_logicThread.joinable())
        m_logicThread.join();

    std::lock_guard lock(m_mutex);
    m_eventQueue = {};
    m_pendingBytesByConnection.clear();
    m_liveConnections.clear();
    m_sendPacket = {};
    m_disconnect = {};
    m_gameWorld = GameWorld{};
}

void PacketHandler::LogicWorker()
{

    while (true) {
        ServerEvent event;

        {
            std::unique_lock lock(m_mutex);

            m_queueReady.wait(lock, [this] { return !m_logicRunning || !m_eventQueue.empty(); });

            if (!m_logicRunning)
                return;

            event = std::move(m_eventQueue.front());
            m_eventQueue.pop();
        }

        try {
            std::vector<OutboundPacket> outgoing;
            switch (event.type) {
            case ServerEventType::Connected:
                m_gameWorld.AddConnection(event.connection);
                break;
            case ServerEventType::PacketReceived: {
                {
                    std::lock_guard lock(m_mutex);
                    if (!m_liveConnections.contains(event.connection))
                        break;
                }
                outgoing = m_gameWorld.HandleRequest(event.connection, event.packet);
                break;
            }
            case ServerEventType::Disconnected:
                outgoing = m_gameWorld.RemoveConnection(event.connection);
                break;
            }
            for (const auto& outgoingPacket : outgoing)
                SendPacket(outgoingPacket.recipient, outgoingPacket.packet);

        } catch (const std::exception& error) {
            std::cerr << "[packet processing failed] " << error.what() << '\n';
            DisconnectFailedConnection(event.connection);
        } catch (...) {
            std::cerr << "[packet processing failed] unknown error\n";
            DisconnectFailedConnection(event.connection);
        }
    }
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

    if (used + 2 > ServerLimits::MaxQueuedEvents) {
        return false;
    }

    m_liveConnections.insert(connection);

    try {
        m_eventQueue.push({ServerEventType::Connected, connection, Packet{}});
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

        m_eventQueue.push({ServerEventType::Disconnected, connection, Packet{}});

        m_liveConnections.erase(connection);
    }

    m_queueReady.notify_one();
    return;
}

void PacketHandler::SendPacket(ConnectionKey connection, const Packet& packet)
{
    {
        std::lock_guard lock(m_mutex);
        if (!m_logicRunning || !m_liveConnections.contains(connection))
            return;
    }
    // The transport checks generation under the connection lock.
    // Keep each recipient independent, including callbacks which throw.
    try {
        const auto bytes = packet.Serialize();
        if (!m_sendPacket(connection, bytes.data(), static_cast<std::uint32_t>(bytes.size())))
            DisconnectFailedConnection(connection);
    } catch (...) {
        DisconnectFailedConnection(connection);
    }
}

void PacketHandler::DisconnectFailedConnection(ConnectionKey connection)
{
    if (m_disconnect) {
        try {
            m_disconnect(connection);
        } catch (...) {
            // Still schedule logic cleanup if a custom transport callback fails.
        }
    }
    EnqueueDisconnected(connection);
}

bool PacketHandler::ReceiveBytes(ConnectionKey connection, const char* data, std::size_t size)
{
    if (size == 0)
        return true;
    if (data == nullptr || size > ProtocolLimits::MaxPacketBytes)
        return false;

    {
        std::lock_guard lock(m_mutex);

        if (!m_logicRunning || !m_liveConnections.contains(connection))
            return false;

        auto& pending = m_pendingBytesByConnection[connection];
        pending.insert(pending.end(), data, data + size);

        std::size_t consumed = 0;
        while (pending.size() - consumed >= ProtocolLimits::HeaderSize) {
            const auto header = PacketHeader::Read(pending.data() + consumed);
            if (header.packetSize < ProtocolLimits::HeaderSize ||
                header.packetSize > ProtocolLimits::MaxPacketBytes) {
                m_pendingBytesByConnection.erase(connection);
                return false;
            }
            if (pending.size() - consumed < header.packetSize)
                break;

            const char* payload = pending.data() + consumed + ProtocolLimits::HeaderSize;

            Packet request;
            request.code = header.code;
            request.error = header.error;
            request.requestId = header.requestId;
            request.payload.assign(payload,
                                   payload + (header.packetSize - ProtocolLimits::HeaderSize));

            if (m_eventQueue.size() + m_liveConnections.size() >= ServerLimits::MaxQueuedEvents) {
                m_pendingBytesByConnection.erase(connection);
                return false;
            }

            m_eventQueue.push({ServerEventType::PacketReceived, connection, std::move(request)});

            m_queueReady.notify_one();

            consumed += header.packetSize;
        }

        pending.erase(pending.begin(),
                      pending.begin() + static_cast<std::vector<char>::difference_type>(consumed));
        if (pending.empty())
            m_pendingBytesByConnection.erase(connection);
    }

    return true;
}
