#ifndef ZPD_ZPDSERVER_HPP
#define ZPD_ZPDSERVER_HPP

#include "IOCPServer.hpp"
#include "PacketHandler.hpp"

class ZPDServer final : public IOCPServer
{
  public: 
    ~ZPDServer() override
    {
        Stop();
    }

    void OnConnected(ConnectionKey connection) override
    {
        m_packetHandler.Reset(connection);
        std::cout << "[connected] client=" << connection.clientId << std::endl;
    }

    void OnReceived(ConnectionKey connection, const char* data, DWORD size) override
    {
        
        auto clientId = connection.clientId;
        try {
            if (m_packetHandler.Handle(connection, data, size,
                    [&](const char* bytes, std::uint32_t length) {
                        return Send(connection, bytes, length);
                    })) {
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
        m_packetHandler.Reset(connection);
        std::cout << "[disconnected] client=" << connection.clientId << std::endl;
    }

  private:
    static_assert(PacketHeader::MaxPacketSize <= MAX_BUFFER_SIZE);
    PacketHandler m_packetHandler;
};

#endif
