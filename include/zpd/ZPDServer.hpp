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

    void OnConnected(std::uint32_t clientId) override
    {
        m_packetHandler.Reset(clientId);
        std::cout << "[connected] client=" << clientId << std::endl;
    }

    void OnReceived(std::uint32_t clientId, const char* data, DWORD size) override
    {
        try {
            if (m_packetHandler.Handle(clientId, data, size,
                    [this, clientId](const char* bytes, std::uint32_t length) {
                        return Send(clientId, bytes, length);
                    })) {
                return;
            }
            std::cerr << "[packet or send failed] client=" << clientId << std::endl;
        } catch (...) {
            // Exceptions must not escape an IOCP callback.
            std::cerr << "[packet handling failed] client=" << clientId << std::endl;
        }
        Disconnect(clientId);
    }

    void OnSendCompleted(std::uint32_t clientId, const char*, DWORD size) override
    {
        std::cout << "[sent] client=" << clientId << " bytes=" << size << std::endl;
    }

    void OnDisconnected(std::uint32_t clientId) override
    {
        m_packetHandler.Reset(clientId);
        std::cout << "[disconnected] client=" << clientId << std::endl;
    }

  private:
    static_assert(PacketHeader::MaxPacketSize <= MAX_BUFFER_SIZE);
    PacketHandler m_packetHandler;
};

#endif
