#ifndef ZPD_PACKETHANDLER_HPP
#define ZPD_PACKETHANDLER_HPP

#include "Packet.hpp"
#include "proto/echo.pb.h"

#include <mutex>
#include <unordered_map>

class PacketHandler
{
  public:
    void Reset(std::uint32_t clientId)
    {
        std::lock_guard lock(m_mutex);
        m_pending.erase(clientId);
    }

    template <typename SendPacket>
    bool Handle(std::uint32_t clientId, const char* data, std::size_t size,
                SendPacket&& sendPacket)
    {
        if (size == 0)
            return true;
        if (data == nullptr || size > PacketHeader::MaxPacketSize)
            return false;

        std::vector<Packet> responses;
        {
            std::lock_guard lock(m_mutex);
            auto& pending = m_pending[clientId];
            pending.insert(pending.end(), data, data + size);

            std::size_t consumed = 0;
            while (pending.size() - consumed >= PacketHeader::Size) {
                const auto header = PacketHeader::Read(pending.data() + consumed);
                if (header.size < PacketHeader::Size ||
                    header.size > PacketHeader::MaxPacketSize) {
                    m_pending.erase(clientId);
                    return false;
                }
                if (pending.size() - consumed < header.size)
                    break;

                const char* payload = pending.data() + consumed + PacketHeader::Size;
                responses.push_back(Dispatch(header, payload));
                consumed += header.size;
            }

            pending.erase(pending.begin(), pending.begin() +
                          static_cast<std::vector<char>::difference_type>(consumed));
            if (pending.empty())
                m_pending.erase(clientId);
        }

        for (const auto& response : responses) {
            const auto bytes = response.Serialize();
            if (!sendPacket(bytes.data(), static_cast<std::uint32_t>(bytes.size())))
                return false;
        }
        return true;
    }

  private:
    static Packet Dispatch(const PacketHeader& header, const char* payload)
    {
        Packet response;
        response.request = header.request;
        if (header.error != ErrorCode::None) {
            response.error = ErrorCode::InvalidRequestStatus;
            return response;
        }

        const std::size_t payloadSize = header.size - PacketHeader::Size;
        switch (header.request) {
        case RequestCode::Echo: {
            protocol::EchoRequest request;

            if (!request.ParseFromArray(payload, static_cast<int>(payloadSize))) {
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
            if (payloadSize != 0)
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
    std::unordered_map<std::uint32_t, std::vector<char>> m_pending;
};

#endif
