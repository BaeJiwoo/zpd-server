#include "RoomChatClient.hpp"
#include "ClientSettings.hpp"
#include "proto/echo.pb.h"
#include "proto/session.pb.h"
#include "proto/room.pb.h"
#include "proto/chat.pb.h"
#include <iostream>
#include <stdexcept>

template <typename Message> Message RoomChatClient::ParseMessage(const std::vector<char>& payload)
{
    Message message;
    if (!message.ParseFromArray(payload.data(), static_cast<int>(payload.size())))
        throw std::runtime_error("Response/notification parsing failed.");
    return message;
}

template <typename Message> void RoomChatClient::ApplyRoomSnapshot(const std::vector<char>& payload)
{
    const auto message = ParseMessage<Message>(payload);
    std::set<std::uint64_t> members(message.player_ids().begin(), message.player_ids().end());
    if (!message.room_id() || message.capacity() < ProtocolLimits::MinRoomCapacity ||
        message.capacity() > ProtocolLimits::MaxRoomCapacity ||
        members.size() > message.capacity() || members.contains(0) ||
        !members.contains(m_playerId) ||
        members.size() != static_cast<std::size_t>(message.player_ids_size()))
        throw std::runtime_error("Invalid room snapshot.");
    m_roomId = message.room_id();
    m_capacity = message.capacity();
    m_members = std::move(members);
    ShowMembers();
}

void RoomChatClient::HandleServerPacket(const PacketHeader& header,
                                        const std::vector<char>& payload)
{
    std::lock_guard lock(m_mutex);
    if (header.code == MessageCode::ChatMessage) {
        const auto message = ParseMessage<protocol::ChatMessage>(payload);
        if (header.requestId != 0 || header.error != ErrorCode::None || !m_roomId ||
            message.room_id() != m_roomId || !m_members.contains(message.player_id()) ||
            message.text().empty() || message.text().size() > ProtocolLimits::MaxChatTextBytes)
            throw std::runtime_error("Invalid chat notification.");
        std::cout << "[Room " << message.room_id() << "][Player " << message.player_id() << "] "
                  << message.text() << std::endl;
        return;
    }
    if (header.code == MessageCode::PlayerJoined || header.code == MessageCode::PlayerLeft) {
        if (header.requestId != 0 || header.error != ErrorCode::None)
            throw std::runtime_error("Invalid notification header.");
        std::uint64_t roomId, playerId;
        const bool joined = header.code == MessageCode::PlayerJoined;
        if (joined) {
            const auto message = ParseMessage<protocol::PlayerJoined>(payload);
            roomId = message.room_id();
            playerId = message.player_id();
        } else {
            const auto message = ParseMessage<protocol::PlayerLeft>(payload);
            roomId = message.room_id();
            playerId = message.player_id();
        }
        if (!playerId || !roomId || roomId != m_roomId || playerId == m_playerId)
            throw std::runtime_error("Invalid notification membership.");
        if (joined) {
            if (m_members.size() >= m_capacity || !m_members.insert(playerId).second)
                throw std::runtime_error("Invalid join notification.");
        } else if (m_members.erase(playerId) != 1)
            throw std::runtime_error("Invalid leave notification.");
        std::cout << (joined ? "PlayerJoined: " : "PlayerLeft: ") << playerId << '\n';
        ShowMembers();
        return;
    }
    const auto it = m_pending.find(header.requestId);
    if (it == m_pending.end() || header.requestId == 0 ||
        header.code != ResponseCodeFor(it->second.code))
        throw std::runtime_error("Unknown response message or request ID.");
    const auto pending = it->second;
    if (header.error != ErrorCode::None) {
        if (!payload.empty())
            throw std::runtime_error("Error response body must be empty.");
        std::cout << "Request " << header.requestId << " failed: server error "
                  << static_cast<unsigned int>(header.error);
        if (header.error == ErrorCode::NotEntered)
            std::cout << " (use /enter first)";
        else if (header.error == ErrorCode::NotInRoom)
            std::cout << " (use /create <capacity> or /join <roomId> first)";
        else if (header.error == ErrorCode::RoomNotFound)
            std::cout << " (room not found; use the Room ID, not capacity or player ID)";
        std::cout << std::endl;
        m_pending.erase(it);
        return;
    }
    switch (header.code) {
    case MessageCode::EchoResponse: {
        const auto reply = ParseMessage<protocol::EchoResponse>(payload);
        if (reply.data() != pending.expectedEchoText)
            throw std::runtime_error("Echo payload mismatch.");
        pending.batch->response += reply.data();
        if (--pending.batch->remainingResponses == 0)
            std::cout << "Echo: " << pending.batch->response << std::endl;
        break;
    }
    case MessageCode::ChatResponse:
        if (!payload.empty())
            throw std::runtime_error("Unexpected Chat response body.");
        break;
    case MessageCode::PingResponse:
        if (!payload.empty())
            throw std::runtime_error("Unexpected Ping body.");
        std::cout << "Pong" << std::endl;
        break;
    case MessageCode::EnterResponse:
        m_playerId = ParseMessage<protocol::EnterResponse>(payload).player_id();
        if (!m_playerId)
            throw std::runtime_error("Invalid player ID.");
        std::cout << "Entered lobby. Temporary player ID: " << m_playerId << std::endl;
        break;
    case MessageCode::CreateRoomResponse:
        ApplyRoomSnapshot<protocol::CreateRoomResponse>(payload);
        break;
    case MessageCode::JoinRoomResponse:
        ApplyRoomSnapshot<protocol::JoinRoomResponse>(payload);
        break;
    case MessageCode::LeaveRoomResponse:
        if (ParseMessage<protocol::LeaveRoomResponse>(payload).room_id() != m_roomId || !m_roomId)
            throw std::runtime_error("Invalid leave response.");
        m_roomId = 0;
        m_capacity = 0;
        m_members.clear();
        std::cout << "Left room. Back in lobby." << std::endl;
        break;
    default:
        throw std::runtime_error("Unknown server message.");
    }
    m_pending.erase(it);
}

void RoomChatClient::ReceivePackets()
{
    try {
        while (m_running) {
            char bytes[ProtocolLimits::HeaderSize];
            if (!ReceiveExact(bytes, sizeof(bytes))) {
                if (m_running)
                    CloseWithError("Server disconnected.");
                break;
            }
            const auto header = PacketHeader::Read(bytes);
            if (header.packetSize < ProtocolLimits::HeaderSize ||
                header.packetSize > ProtocolLimits::MaxPacketBytes)
                throw std::runtime_error("Invalid server packet length.");
            std::vector<char> payload(header.packetSize - ProtocolLimits::HeaderSize);
            if (!ReceiveExact(payload.data(), payload.size()))
                throw std::runtime_error("Server disconnected during a packet.");
            HandleServerPacket(header, payload);
        }
    } catch (const std::exception& error) {
        CloseWithError(error.what());
    }
    m_running = false;
    std::lock_guard lock(m_mutex);
    for (const auto& [id, request] : m_pending)
        std::cout << "Request " << id << " failed: connection closed." << '\n';
    m_pending.clear();
    m_members.clear();
    m_roomId = 0;
}
