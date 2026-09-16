#include "RoomChatClient.hpp"
#include <iostream>
#include <utility>

RoomChatClient::RoomChatClient(SOCKET peer) : m_socket(peer)
{
}

RoomChatClient::~RoomChatClient()
{
    Stop();
}

void RoomChatClient::Start()
{
    m_receiver = std::thread([this] { ReceivePackets(); });
}

bool RoomChatClient::IsRunning() const
{
    return m_running;
}

bool RoomChatClient::HasFailed() const
{
    return m_failed;
}

void RoomChatClient::Stop()
{
    m_running = false;
    shutdown(m_socket, SD_BOTH);
    if (m_receiver.joinable())
        m_receiver.join();
}

void RoomChatClient::PrintStatus(const std::string& text)
{
    std::lock_guard lock(m_mutex);
    std::cout << text << std::endl;
}

void RoomChatClient::CloseWithError(const std::string& text)
{
    m_failed = true;
    m_running = false;
    shutdown(m_socket, SD_BOTH);
    PrintStatus(text);
}

void RoomChatClient::ShowMembers()
{
    std::cout << "Room " << m_roomId << " (" << m_members.size() << '/' << m_capacity << "):";
    for (auto id : m_members)
        std::cout << ' ' << id;
    std::cout << std::endl;
}

bool RoomChatClient::SendRequest(Packet packet, PendingRequest pending)
{
    {
        std::lock_guard lock(m_mutex);
        if (!m_running)
            return false;
        // IDs may wrap, but must never collide with an outstanding request.
        do {
            packet.requestId = m_nextRequestId++;
        } while (packet.requestId == 0 || m_pending.contains(packet.requestId));
        m_pending.emplace(packet.requestId, std::move(pending));
    }
    const auto bytes = packet.Serialize();
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        const int count =
            send(m_socket, bytes.data() + sent, static_cast<int>(bytes.size() - sent), 0);
        if (count <= 0) {
            CloseWithError("Send failed. Connection closed.");
            return false;
        }
        sent += static_cast<std::size_t>(count);
    }
    return true;
}

bool RoomChatClient::ReceiveExact(char* bytes, std::size_t size)
{
    std::size_t offset = 0;
    while (offset < size) {
        const int count = recv(m_socket, bytes + offset, static_cast<int>(size - offset), 0);
        if (count <= 0)
            return false;
        offset += static_cast<std::size_t>(count);
    }
    return true;
}
