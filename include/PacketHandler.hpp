#ifndef ZPD_PACKETHANDLER_HPP
#define ZPD_PACKETHANDLER_HPP

#include "Packet.hpp"
#include "Define.hpp"
#include "proto/echo.pb.h"

#include <mutex>
#include <map>

class PacketHandler
{
  public:
    void Reset(ConnectionKey connection)
    {
        std::lock_guard lock(m_mutex);
        m_pending.erase(connection);
    }

    template <typename SendPacket>
    bool Handle(ConnectionKey connection, const char* data, std::size_t size,
                SendPacket&& sendPacket)
    {
        if (size == 0)
            return true;
        if (data == nullptr || size > PacketHeader::MaxPacketSize)
            return false;

        std::vector<Packet> requests;
        {
            std::lock_guard lock(m_mutex);
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
                
                //requests.push_back(Dispatch(header, payload));
                
                Packet request;
                request.request = header.request;
                request.error = header.error;
                request.payload.assign(payload, payload + (header.size - PacketHeader::Size));

                requests.push_back(request);

                consumed += header.size;
            }

            pending.erase(pending.begin(), pending.begin() +
                          static_cast<std::vector<char>::difference_type>(consumed));
            if (pending.empty())
                m_pending.erase(connection);
        }

        for (const auto& request : requests) {
            const Packet response = Dispatch(request);
            const auto bytes = response.Serialize();

            if (!sendPacket(bytes.data(), static_cast<std::uint32_t>(bytes.size())))
                return false;
        }
        return true;
    }

  private:
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
            protocol::EchoRequest request;

            if (!request.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
                response.error = ErrorCode::InvalidPayload;
                break;
            }

            protocol::EchoResponse reply;
            reply.set_data(request.data());

            if (reply.ByteSizeLong() > PacketHeader::MaxPayloadSize) {
                response.error = ErrorCode::InvalidPayload;
                break;
            }

            std::string encoded;
            if (!reply.SerializeToString(&encoded)) {
                response.error = ErrorCode::UnknownError;
                break;
            }

            response.payload.assign(encoded.begin(), encoded.end());
            break;
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

    std::mutex m_mutex;
    std::map<ConnectionKey, std::vector<char>> m_pending;
};

#endif
