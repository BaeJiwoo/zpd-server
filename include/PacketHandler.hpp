#ifndef ZPD_PACKETHANDLER_HPP
#define ZPD_PACKETHANDLER_HPP

#include "Packet.hpp"
#include "Define.hpp"
#include "proto/echo.pb.h"

#include <mutex>
#include <map>
#include <thread>
#include <queue>
#include <functional>
#include <utility>
#include <condition_variable>

class PacketHandler
{
  public:
    using SendCallback =
        std::function<bool(ConnectionKey, const char*, std::uint32_t)>;

    // Call lifecycle functions from the server owner thread.
    bool Run(SendCallback sendPacket);
    void Stop();

    void Reset(ConnectionKey connection)
    {
        std::lock_guard lock(m_mutex);
        m_pending.erase(connection);
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

            auto& pending = m_pending[connection];
            pending.insert(pending.end(), data, data + size);

            std::size_t consumed = 0;
            while (pending.size() - consumed >= PacketHeader::Size) {
                const auto header = PacketHeader::Read(pending.data() + consumed);
                if (header.size < PacketHeader::Size ||
                    header.size > PacketHeader::MaxPacketSize) {
                    m_pending.erase(connection);
                    return false;
                }
                if (pending.size() - consumed < header.size)
                    break;

                const char* payload = pending.data() + consumed + PacketHeader::Size;
                
                Packet request;
                request.request = header.request;
                request.error = header.error;
                request.payload.assign(payload, payload + (header.size - PacketHeader::Size));


                if (m_packetQueue.size() >= 1024) {
                    m_pending.erase(connection);
                    return false;
                }

                m_packetQueue.push({connection, std::move(request)});
                m_queueReady.notify_one();

                consumed += header.size;
            }

            pending.erase(pending.begin(), pending.begin() +
                          static_cast<std::vector<char>::difference_type>(consumed));
            if (pending.empty())
                m_pending.erase(connection);
        }

        return true;
    }

  private:
    struct PendingPacket
    {
        ConnectionKey connection{};
        Packet packet;
    };

    static Packet Dispatch(const Packet& requestPacket)
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
        default: {
            response.error = ErrorCode::UnknownRequest;
            break;
        }
        }
        return response;
    }

    void LogicWorker();
    
    static Packet Echo(const Packet& requestPacket);

    std::mutex m_mutex;
    std::map<ConnectionKey, std::vector<char>> m_pending;
    bool m_logicRunning = false;
    std::queue<PendingPacket> m_packetQueue;
    std::thread m_logicThread;
    SendCallback m_sendPacket;
    std::condition_variable m_queueReady;
};

#endif
