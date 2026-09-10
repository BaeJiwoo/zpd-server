#ifndef ZPD_ZPDSERVER_HPP
#define ZPD_ZPDSERVER_HPP

#include "IOCPServer.hpp"
#include <string>

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
                  << " message=\"" << DisplayMessage(data, size) << '"' << std::endl;
        if (!Send(clientId, data, size)) {
            std::cerr << "[send failed] client=" << clientId << std::endl;
            Disconnect(clientId);
        }
    }

    void OnSendCompleted(std::uint32_t clientId, const char* data, DWORD size) override
    {
        std::cout << "[sent] client=" << clientId << " bytes=" << size
                  << " message=\"" << DisplayMessage(data, size) << '"' << std::endl;
    }

    void OnDisconnected(std::uint32_t clientId) override
    {
        std::cout << "[disconnected] client=" << clientId << std::endl;
    }
  private:
    static std::string DisplayMessage(const char* data, DWORD size)
    {
        constexpr char hex[] = "0123456789ABCDEF";
        std::string text;
        for (DWORD i = 0; i < size; ++i) {
            const auto value = static_cast<unsigned char>(data[i]);
            if (value == '\n')
                text += "\\n";
            else if (value == '\r')
                text += "\\r";
            else if (value == '\t')
                text += "\\t";
            else if (value == '"' || value == '\\') {
                text += '\\';
                text += static_cast<char>(value);
            } else if (value < 32 || value == 127) {
                text += "\\x";
                text += hex[value >> 4];
                text += hex[value & 15];
            } else
                text += static_cast<char>(value);
        }
        return text;
    }
};

#endif
