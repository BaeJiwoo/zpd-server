#ifndef ZPD_ZPDSERVER_HPP
#define ZPD_ZPDSERVER_HPP

#include "IOCPServer.hpp"
#include "PacketHandler.hpp"

class ZPDServer final : public IOCPServer
{
  public:
    bool Start(std::uint16_t port, std::uint32_t maxClients, std::uint16_t workerCount = 0)
    {
        if (!m_packetHandler.Run(
                [this](ConnectionKey connection, const char* bytes, std::uint32_t length) {
                    if (Send(connection, bytes, length))
                        return true;
                    Disconnect(connection);
                    return false;
                }))
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
        std::cout << "[connected] client=" << connection.clientId << std::endl;
    }

    void OnReceived(ConnectionKey connection, const char* data, DWORD size) override
    {

        auto clientId = connection.clientId;
        try {
            if (m_packetHandler.Handle(connection, data, size)) {
                return;
            }
            std::cerr << "[packet or send failed] client=" << clientId << std::endl;
        } catch (...) {
            std::cerr << "[packet handling failed] client=" << clientId << std::endl;
        }
        Disconnect(connection);
    }

    void OnSendCompleted(ConnectionKey connection, const char*, DWORD size) override
    {
        std::cout << "[sent] client=" << connection.clientId << " bytes=" << size << std::endl;
    }

    void OnDisconnected(ConnectionKey connection) override
    {
        m_packetHandler.EnqueueDisconnected(connection);
        std::cout << "[disconnected] client=" << connection.clientId << std::endl;
    }

  private:
    static_assert(PacketHeader::MaxPacketSize <= MaxBufferSize);
    PacketHandler m_packetHandler;
};

#endif // ZPD_ZPDSERVER_HPP
