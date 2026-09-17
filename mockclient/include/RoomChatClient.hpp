#ifndef ZPD_ROOMCHATCLIENT_HPP
#define ZPD_ROOMCHATCLIENT_HPP
#include "Packet.hpp"
#include "PendingRequest.hpp"
#include <WinSock2.h>
#include <atomic>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <array>

class RoomChatClient
{
  public:
    explicit RoomChatClient(SOCKET peer);
    ~RoomChatClient();

    void Start();
    bool IsRunning() const;
    bool HasFailed() const;
    void Stop();

    bool HandleInputLine(const std::string& line);

  private:
    void PrintStatus(const std::string& text);
    void CloseWithError(const std::string& text);
    void ShowMembers();
    bool SendRequest(Packet packet, PendingRequest pending);
    bool ReceiveExact(char* bytes, std::size_t size);
    template <typename Message> Message ParseMessage(const std::vector<char>& payload);
    template <typename Message> void ApplyRoomSnapshot(const std::vector<char>& payload);
    void HandleServerPacket(const PacketHeader& header, const std::vector<char>& payload);
    void ReceivePackets();
    SOCKET m_socket;
    std::atomic<bool> m_running{true};
    std::atomic<bool> m_failed{false};
    std::thread m_receiver;
    std::mutex m_mutex;
    std::uint32_t m_nextRequestId = 1;
    std::map<std::uint32_t, PendingRequest> m_pending;
    std::uint64_t m_playerId = 0;
    std::uint64_t m_roomId = 0;
    std::uint32_t m_capacity = 0;
    std::set<std::uint64_t> m_members;
    std::map<std::uint64_t, std::array<float, 3>> m_positions;
    std::uint64_t m_positionTick = 0;
};
#endif // ZPD_ROOMCHATCLIENT_HPP
