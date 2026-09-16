#include "EchoRequestBuilder.hpp"
#include "proto/echo.pb.h"
#include <algorithm>

bool BuildEchoRequest(std::string_view source, std::size_t offset, Packet& packet,
                      std::size_t& dataSize)
{
    dataSize = (std::min)(source.size() - offset, ProtocolLimits::MaxPayloadBytes);

    protocol::EchoRequest message;
    do {
        message.set_data(source.data() + offset, dataSize);
        if (message.ByteSizeLong() <= ProtocolLimits::MaxPayloadBytes)
            break;
        --dataSize;
    } while (dataSize != 0);

    std::string encoded;
    if (!message.SerializeToString(&encoded))
        return false;

    packet.code = MessageCode::EchoRequest;
    packet.payload.assign(encoded.begin(), encoded.end());
    return true;
}
