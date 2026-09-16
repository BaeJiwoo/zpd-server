#ifndef ZPD_PACKETHANDLER_HPP
#define ZPD_PACKETHANDLER_HPP

#include "Packet.hpp"
#include "ConnectionKey.hpp"
#include "GameWorld.hpp"
#include "ServerLimits.hpp"
#include "ServerEvent.hpp"

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

    // Start와 Stop은 서버를 소유한 스레드에서 호출합니다.
    using DisconnectCallback = std::function<void(ConnectionKey)>;

    bool Start(SendCallback sendPacket, DisconnectCallback disconnect = {});
    void Stop();

    bool ReceiveBytes(ConnectionKey connection, const char* data, std::size_t size);

    bool EnqueueConnected(ConnectionKey connection);

    void EnqueueDisconnected(ConnectionKey connection);

  private:
    void LogicWorker();
    void SendPacket(ConnectionKey connection, const Packet& packet);
    void DisconnectFailedConnection(ConnectionKey connection);
    GameWorld m_gameWorld;

    std::mutex m_mutex;
    std::map<ConnectionKey, std::vector<char>> m_pendingBytesByConnection;
    bool m_logicRunning = false;
    std::queue<ServerEvent> m_eventQueue;
    std::thread m_logicThread;
    SendCallback m_sendPacket;
    DisconnectCallback m_disconnect;
    std::condition_variable m_queueReady;

    // m_mutex로 보호하며, 연결 하나당 종료 이벤트용 자리 하나를 예약합니다.
    std::set<ConnectionKey> m_liveConnections;
};

#endif // ZPD_PACKETHANDLER_HPP
