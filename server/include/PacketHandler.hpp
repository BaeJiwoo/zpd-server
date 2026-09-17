#ifndef ZPD_PACKETHANDLER_HPP
#define ZPD_PACKETHANDLER_HPP

#include "Packet.hpp"
#include "ConnectionKey.hpp"

#include "ServerLimits.hpp"
#include "ServerEvent.hpp"

#include <functional>
#include <mutex>
#include <map>
#include <thread>
#include <queue>

#include <utility>
#include <condition_variable>
#include <set>

class PacketHandler
{
  public:
    ~PacketHandler() { Stop(); }
    using SendCallback = std::function<bool(ConnectionKey, const char*, std::uint32_t)>;
    using DisconnectCallback = std::function<void(ConnectionKey)>;

    bool Start(SendCallback sendPacket, DisconnectCallback disconnect);
    void Stop();

    bool ReceiveBytes(ConnectionKey connection, const char* data, std::size_t size);

    bool EnqueueConnected(ConnectionKey connection);

    void EnqueueDisconnected(ConnectionKey connection);

  private:
    void LogicWorker();
    void HandleConnected(ConnectionKey connection);
    void HandlePacketReceived(ConnectionKey connection, const Packet& packet);
    void HandleDisconnected(ConnectionKey connection);
    void DisconnectFailedConnection(ConnectionKey connection);

    SendCallback m_sendPacket;
    DisconnectCallback m_disconnect;

    std::mutex m_mutex;
    std::map<ConnectionKey, std::vector<char>> m_pendingBytesByConnection;
    bool m_logicRunning = false;
    std::queue<ServerEvent> m_eventQueue;
    std::thread m_logicThread;
    std::condition_variable m_queueReady;

    std::set<ConnectionKey> m_liveConnections;
};

#endif // ZPD_PACKETHANDLER_HPP
