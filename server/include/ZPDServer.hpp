#ifndef ZPD_ZPDSERVER_HPP
#define ZPD_ZPDSERVER_HPP

#include "IOCPServer.hpp"
#include "PacketHandler.hpp"

class ZPDServer final : public IOCPServer
{
  public:
    bool Start(std::uint16_t port, std::uint32_t maxClients, std::uint16_t workerCount = 0)
    {
        if (!m_packetHandler.Start(
                [this](ConnectionKey connection, const char* data, std::uint32_t size) {
                    return Send(connection, data, size);
                },
                [this](ConnectionKey connection) { Disconnect(connection); }))
            return false;

        if (!IOCPServer::Start(port, maxClients, workerCount)) {
            m_packetHandler.Stop();
            return false;
        }
        return true;
    }

    void Stop()
    {
        m_packetHandler.Stop();
        IOCPServer::Stop();
    }

    ~ZPDServer() override
    {
        Stop();
    }

    void OnConnected(ConnectionKey connection) override
    {
        if (!m_packetHandler.EnqueueConnected(connection)) {
            Disconnect(connection);
            return;
        }
        std::cout << "[connected] client=" << connection.slotIndex << std::endl;
    }

    void OnReceived(ConnectionKey connection, const char* data, DWORD size) override
    {

        auto slotIndex = connection.slotIndex;
        try {
            if (m_packetHandler.ReceiveBytes(connection, data, size)) {
                return;
            }
            std::cerr << "[packet receive failed] client=" << slotIndex << std::endl;
        } catch (...) {
            std::cerr << "[packet handling failed] client=" << slotIndex << std::endl;
        }
        Disconnect(connection);
    }

    void OnSendCompleted(ConnectionKey connection, const char*, DWORD size) override
    {
        std::cout << "[sent] client=" << connection.slotIndex << " bytes=" << size << std::endl;
    }

    void OnDisconnected(ConnectionKey connection) override
    {
        m_packetHandler.EnqueueDisconnected(connection);
        std::cout << "[disconnected] client=" << connection.slotIndex << std::endl;
    }

  private:
    static_assert(ProtocolLimits::MaxPacketBytes <= NetworkSettings::ReceiveBufferBytes);
    PacketHandler m_packetHandler;
};

#endif // ZPD_ZPDSERVER_HPP
