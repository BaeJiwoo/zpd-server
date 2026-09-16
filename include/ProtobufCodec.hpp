#ifndef ZPD_PROTOBUFCODEC_HPP
#define ZPD_PROTOBUFCODEC_HPP
#include "Packet.hpp"
#include <stdexcept>
#include <string>
namespace ProtobufCodec {
template <typename Message> bool ParsePayload(const Packet& packet, Message& message)
{
    return message.ParseFromArray(packet.payload.data(), static_cast<int>(packet.payload.size()));
}

template <typename Message> void SerializePayload(Packet& packet, const Message& message)
{
    std::string bytes;
    if (message.ByteSizeLong() > ProtocolLimits::MaxPayloadBytes ||
        !message.SerializeToString(&bytes))
        throw std::runtime_error("Message serialization failed");
    packet.payload.assign(bytes.begin(), bytes.end());
}

} // namespace ProtobufCodec
#endif // ZPD_PROTOBUFCODEC_HPP
