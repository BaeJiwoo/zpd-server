#ifndef ZPD_PACKETHANDLER_HPP
#define ZPD_PACKETHANDLER_HPP

#include "Packet.hpp"
#include "Define.hpp"
#include "proto/echo.pb.h"
#include "PlayerSession.hpp"
#include "PacketHandlerEvent.hpp"

#include <mutex>
#include <map>
#include <thread>
#include <queue>
#include <functional>
#include <utility>
#include <condition_variable>
#include <set>

class PacketHandler
{
  public:
    using SendCallback = std::function<bool(ConnectionKey, const char*, std::uint32_t)>;

    // Run과 Stop은 서버를 소유한 스레드에서 호출합니다.
    bool Run(SendCallback sendPacket);
    void Stop();

    void Reset(ConnectionKey connection)
    {
        std::lock_guard lock(m_mutex);
        m_pendingBytesByConnection.erase(connection);
    }

    bool Handle(ConnectionKey connection, const char* data, std::size_t size)
    {
        if (size == 0)
            return true;
        if (data == nullptr || size > PacketHeader::MaxPacketSize)
            return false;

        {
            std::lock_guard lock(m_mutex);

            if (!m_logicRunning)
                return false;

            auto& pending = m_pendingBytesByConnection[connection];
            pending.insert(pending.end(), data, data + size);

            std::size_t consumed = 0;
            while (pending.size() - consumed >= PacketHeader::Size) {
                const auto header = PacketHeader::Read(pending.data() + consumed);
                if (header.size < PacketHeader::Size || header.size > PacketHeader::MaxPacketSize) {
                    m_pendingBytesByConnection.erase(connection);
                    return false;
                }
                if (pending.size() - consumed < header.size)
                    break;

                const char* payload = pending.data() + consumed + PacketHeader::Size;

                Packet request;
                request.request = header.request;
                request.error = header.error;
                request.payload.assign(payload, payload + (header.size - PacketHeader::Size));

                if (m_eventQueue.size() + m_liveConnections.size() >= MaxEventQueueSize) {
                    m_pendingBytesByConnection.erase(connection);
                    return false;
                }

                m_eventQueue.push(
                    {PacketHandlerEventType::PacketReceived, connection, std::move(request)});

                m_queueReady.notify_one();

                consumed += header.size;
            }

            pending.erase(pending.begin(),
                          pending.begin() +
                              static_cast<std::vector<char>::difference_type>(consumed));
            if (pending.empty())
                m_pendingBytesByConnection.erase(connection);
        }

        return true;
    }

    bool EnqueueConnected(ConnectionKey connection);

    void EnqueueDisconnected(ConnectionKey connection);

  private:
    static constexpr std::size_t MaxEventQueueSize = 1024;

    static Packet Dispatch(const Packet& requestPacket, PlayerSession& session,
                           std::uint64_t& nextPlayerId, std::map<std::uint64_t, ConnectionKey>& connectionsByPlayerId)
    {
        Packet response;
        response.request = requestPacket.request;

        if (requestPacket.error != ErrorCode::None) {
            response.error = ErrorCode::InvalidRequestStatus;
            return response;
        }

        const auto& payload = requestPacket.payload;
        switch (requestPacket.request) {
        case RequestCode::Echo: {
            return Echo(requestPacket);
        }
        case RequestCode::Ping: {
            if (!payload.empty())
                response.error = ErrorCode::InvalidPayload;
            break;
        }
        case RequestCode::Enter: {
            return EnterSession(requestPacket, session, nextPlayerId, connectionsByPlayerId);
        }
        default: {
            response.error = ErrorCode::UnknownRequest;
            break;
        }
        }
        return response;
    }

    void LogicWorker();

    static Packet Echo(const Packet& requestPacket);

    static Packet EnterSession(const Packet& requestPacket, PlayerSession& session,
                               std::uint64_t& nextPlayerId, std::map<std::uint64_t, ConnectionKey>& connectionsByPlayerId);

    std::mutex m_mutex;
    std::map<ConnectionKey, std::vector<char>> m_pendingBytesByConnection;
    bool m_logicRunning = false;
    std::queue<PacketHandlerEvent> m_eventQueue;
    std::thread m_logicThread;
    SendCallback m_sendPacket;
    std::condition_variable m_queueReady;

    // m_mutex로 보호하며, 연결 하나당 종료 이벤트용 자리 하나를 예약합니다.
    std::set<ConnectionKey> m_liveConnections;
};

#endif // ZPD_PACKETHANDLER_HPP
