#ifndef ZPD_ZPDSERVER_HPP
#define ZPD_ZPDSERVER_HPP

#include "IOCPServer.hpp"

class ZPDServer final : public IOCPServer
{
  public: 
    ~ZPDServer() override
    {
        Stop();
    }

    void OnConnected(std::uint32_t clientId) override
    {
        std::cout << "[connected] client=" << clientId << std::endl;
    }

    void OnReceived(std::uint32_t clientId, const char* data, DWORD size) override
    {
        std::cout << "[received] client=" << clientId << " bytes=" << size
                  << " message=\"";
        std::cout.write(data, size);
        std::cout << '"' << std::endl;
        if (!Send(clientId, data, size)) {
            std::cerr << "[send failed] client=" << clientId << std::endl;
            Disconnect(clientId);
        }
    }

    void OnSendCompleted(std::uint32_t clientId, const char* data, DWORD size) override
    {
        std::cout << "[sent] client=" << clientId << " bytes=" << size
                  << " message=\"";
        std::cout.write(data, size);
        std::cout << '"' << std::endl;
    }

    void OnDisconnected(std::uint32_t clientId) override
    {
        std::cout << "[disconnected] client=" << clientId << std::endl;
    }
};

#endif
