#include "ZPDServer.hpp"
#include "proto/echo.pb.h"
#include "proto/session.pb.h"
#include "proto/room.pb.h"
#include "proto/chat.pb.h"

#include <algorithm>
#include <chrono>
#include <conio.h>
#include <future>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace std::chrono_literals;

#include "ClientApplication.hpp"

void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

struct Socket
{
    SOCKET value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Socket()
    {
        Check(value != INVALID_SOCKET, "socket failed");
    }
    ~Socket()
    {
        closesocket(value);
    }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    void Connect(std::uint16_t port)
    {
        DWORD timeout = 3000;
        setsockopt(value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
                   sizeof(timeout));
        setsockopt(value, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout),
                   sizeof(timeout));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        Check(connect(value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0,
              "connect failed");
    }
};

void SendAll(SOCKET peer, const char* data, std::size_t size,
             std::size_t chunkSize = ProtocolLimits::MaxPacketBytes)
{
    std::size_t sent = 0;
    while (sent < size) {
        const int count =
            send(peer, data + sent, static_cast<int>((std::min)(size - sent, chunkSize)), 0);
        Check(count > 0, "send failed");
        sent += static_cast<std::size_t>(count);
    }
}

void ReceiveExact(SOCKET peer, char* data, std::size_t size)
{
    std::size_t received = 0;
    while (received < size) {
        const int count = recv(peer, data + received, static_cast<int>(size - received), 0);
        Check(count > 0, "recv failed or connection closed");
        received += static_cast<std::size_t>(count);
    }
}

Packet ReceivePacket(SOCKET peer)
{
    char bytes[ProtocolLimits::HeaderSize];
    ReceiveExact(peer, bytes, ProtocolLimits::HeaderSize);
    const auto header = PacketHeader::Read(bytes);
    Check(header.packetSize >= ProtocolLimits::HeaderSize &&
              header.packetSize <= ProtocolLimits::MaxPacketBytes,
          "invalid response size");
    Packet packet;
    packet.code = header.code;
    packet.error = header.error;
    packet.requestId = header.requestId;
    packet.payload.resize(header.packetSize - ProtocolLimits::HeaderSize);
    ReceiveExact(peer, packet.payload.data(), packet.payload.size());
    return packet;
}

void CheckResponse(SOCKET peer, MessageCode code, ErrorCode error,
                   const std::vector<char>& payload = {})
{
    const auto response = ReceivePacket(peer);
    Check(response.code == code, "unexpected response message code");
    Check(response.error == error, "unexpected response error code");
    Check(response.payload == payload, "response payload changed");
}

Packet BuildEchoRequest(const char* data, std::size_t available, std::size_t& dataSize)
{
    dataSize = (std::min)(available, ProtocolLimits::MaxPayloadBytes);

    protocol::EchoRequest message;
    do {
        message.set_data(data, dataSize);
        if (message.ByteSizeLong() <= ProtocolLimits::MaxPayloadBytes)
            break;
        --dataSize;
    } while (dataSize != 0);

    std::string encoded;
    Check(message.SerializeToString(&encoded), "EchoRequest serialization failed");

    Packet packet;
    static std::atomic<std::uint32_t> nextId{100};
    packet.requestId = nextId++;
    packet.code = MessageCode::EchoRequest;
    packet.payload.assign(encoded.begin(), encoded.end());
    return packet;
}

std::string ReceiveEchoResponse(SOCKET peer, std::uint32_t expectedId)
{
    const auto packet = ReceivePacket(peer);
    Check(packet.code == MessageCode::EchoResponse, "expected Echo response");
    Check(packet.requestId == expectedId, "Echo request ID changed");
    Check(packet.error == ErrorCode::None, "Echo returned an error");

    protocol::EchoResponse response;
    Check(response.ParseFromArray(packet.payload.data(), static_cast<int>(packet.payload.size())),
          "EchoResponse parsing failed");
    return response.data();
}

void Echo(std::uint16_t port, std::size_t size)
{
    Socket peer;
    peer.Connect(port);
    std::string payload(size, '\0');
    for (std::size_t i = 0; i < size; ++i)
        payload[i] = static_cast<char>(i % 251);
    std::string response;
    std::size_t offset = 0;
    do {
        std::size_t length = 0;
        const auto request = BuildEchoRequest(payload.data() + offset, size - offset, length);
        const auto bytes = request.Serialize();
        SendAll(peer.value, bytes.data(), bytes.size(), 997);
        const auto reply = ReceiveEchoResponse(peer.value, request.requestId);
        Check(reply.size() == length &&
                  std::equal(reply.begin(), reply.end(), payload.data() + offset),
              "Echo packet payload changed");
        response += reply;
        offset += length;
    } while (offset < size);
    Check(response == payload, "echo bytes or order changed");
}

void PacketProtocol(std::uint16_t port)
{
    Socket peer;
    peer.Connect(port);

    const char ping[] = {0, 8, 2, 0, 18, 52, 86, 120};
    SendAll(peer.value, ping, sizeof(ping), 1);
    char pong[sizeof(ping)];
    ReceiveExact(peer.value, pong, sizeof(pong));
    const char expectedPong[] = {0, 8, static_cast<char>(130), 0, 18, 52, 86, 120};
    Check(std::equal(std::begin(expectedPong), std::end(expectedPong), std::begin(pong)),
          "Ping wire response changed");

    const std::string rawEcho{'a', '\0', static_cast<char>(255), 'z'};
    std::size_t encodedDataSize = 0;
    const Packet echo = BuildEchoRequest(rawEcho.data(), rawEcho.size(), encodedDataSize);
    Check(encodedDataSize == rawEcho.size(), "raw Echo request was unexpectedly split");
    const auto echoBytes = echo.Serialize();
    SendAll(peer.value, echoBytes.data(), echoBytes.size(), 1);
    Check(ReceiveEchoResponse(peer.value, echo.requestId) == rawEcho, "raw Echo response changed");

    std::vector<char> combined(echoBytes);
    combined.insert(combined.end(), std::begin(ping), std::end(ping));
    auto secondEcho = echo;
    ++secondEcho.requestId;
    const auto secondEchoBytes = secondEcho.Serialize();
    combined.insert(combined.end(), secondEchoBytes.begin(), secondEchoBytes.end());
    SendAll(peer.value, combined.data(), combined.size());
    Check(ReceiveEchoResponse(peer.value, echo.requestId) == rawEcho,
          "combined Echo response changed");
    CheckResponse(peer.value, MessageCode::PingResponse, ErrorCode::None);
    Check(ReceiveEchoResponse(peer.value, secondEcho.requestId) == rawEcho,
          "combined Echo response changed");

    const auto expectError = [&](Packet request, MessageCode responseCode, ErrorCode expected) {
        request.requestId = 42;
        const auto bytes = request.Serialize();
        SendAll(peer.value, bytes.data(), bytes.size());
        const auto response = ReceivePacket(peer.value);
        Check(response.requestId == request.requestId, "error request ID changed");
        Check(response.code == responseCode && response.error == expected &&
                  response.payload.empty(),
              "invalid error response");
    };
    Packet unknown;
    unknown.code = static_cast<MessageCode>(127);
    expectError(unknown, MessageCode::ErrorResponse, ErrorCode::UnknownRequest);
    for (const auto code :
         {MessageCode::EchoResponse, MessageCode::PingResponse, MessageCode::EnterResponse,
          MessageCode::CreateRoomResponse, MessageCode::JoinRoomResponse,
          MessageCode::LeaveRoomResponse, MessageCode::ChatResponse, MessageCode::ChatMessage,
          MessageCode::PlayerJoined, MessageCode::PlayerLeft, MessageCode::ErrorResponse}) {
        Packet responseAsRequest;
        responseAsRequest.code = code;
        expectError(responseAsRequest, MessageCode::ErrorResponse, ErrorCode::UnknownRequest);
    }
    Packet zeroId;
    zeroId.code = MessageCode::PingRequest;
    const auto zeroBytes = zeroId.Serialize();
    SendAll(peer.value, zeroBytes.data(), zeroBytes.size());
    const auto zeroResponse = ReceivePacket(peer.value);
    Check(zeroResponse.requestId == 0 && zeroResponse.error == ErrorCode::InvalidRequestStatus &&
              zeroResponse.payload.empty(),
          "zero request ID was accepted");
    Packet invalidPing;
    invalidPing.code = MessageCode::PingRequest;
    invalidPing.payload = {'x'};
    expectError(invalidPing, MessageCode::PingResponse, ErrorCode::InvalidPayload);
    Packet invalidStatus;
    invalidStatus.error = ErrorCode::InvalidPayload;
    expectError(invalidStatus, MessageCode::EchoResponse, ErrorCode::InvalidRequestStatus);
    SendAll(peer.value, ping, sizeof(ping));
    CheckResponse(peer.value, MessageCode::PingResponse, ErrorCode::None);

    const char enter[] = {0, 8, 3, 0, 0, 0, 0, 9};
    SendAll(peer.value, enter, sizeof(enter));
    const auto entered = ReceivePacket(peer.value);
    Check(entered.code == MessageCode::EnterResponse, "expected Enter response");
    Check(entered.error == ErrorCode::None, "Enter returned an error");
    protocol::EnterResponse entry;
    Check(entry.ParseFromArray(entered.payload.data(), static_cast<int>(entered.payload.size())),
          "EnterResponse parsing failed");
    Check(entry.player_id() != 0, "Enter returned an invalid player ID");
    SendAll(peer.value, enter, sizeof(enter));
    CheckResponse(peer.value, MessageCode::EnterResponse, ErrorCode::AlreadyEntered);
}

void InvalidPacketSize(std::uint16_t port, std::uint16_t size)
{
    Socket peer;
    peer.Connect(port);
    const char bytes[] = {static_cast<char>(size >> 8),
                          static_cast<char>(size & 255),
                          static_cast<char>(MessageCode::EchoRequest),
                          0,
                          0,
                          0,
                          0,
                          1};
    SendAll(peer.value, bytes, sizeof(bytes));
    char response;
    const int count = recv(peer.value, &response, 1, 0);
    const int error = count == SOCKET_ERROR ? WSAGetLastError() : 0;
    Check(count == 0 || (count == SOCKET_ERROR && error == WSAECONNRESET),
          "invalid packet size did not close the connection");
}

template <typename Message> Packet Request(MessageCode code, const Message& message)
{
    static std::atomic<std::uint32_t> nextId{1000};
    Packet packet;
    packet.code = code;
    packet.requestId = nextId++;
    std::string encoded;
    Check(message.SerializeToString(&encoded), "request serialization failed");
    packet.payload.assign(encoded.begin(), encoded.end());
    return packet;
}

Packet Exchange(SOCKET peer, const Packet& request, ErrorCode error = ErrorCode::None)
{
    const auto bytes = request.Serialize();
    SendAll(peer, bytes.data(), bytes.size());
    const auto response = ReceivePacket(peer);
    Check(response.code == ResponseCodeFor(request.code), "unexpected response code");
    Check(response.requestId == request.requestId, "response request ID mismatch");
    Check(response.error == error, "unexpected server error");
    if (error != ErrorCode::None)
        Check(response.payload.empty(), "error body is not empty");
    return response;
}

template <typename Message> Message Parse(const Packet& packet)
{
    Message message;
    Check(message.ParseFromArray(packet.payload.data(), static_cast<int>(packet.payload.size())),
          "message parsing failed");
    return message;
}

std::uint64_t Enter(SOCKET peer)
{
    const auto id =
        Parse<protocol::EnterResponse>(
            Exchange(peer, Request(MessageCode::EnterRequest, protocol::EnterRequest{})))
            .player_id();
    Check(id != 0, "invalid player ID");
    return id;
}

Packet Create(std::uint32_t capacity)
{
    protocol::CreateRoomRequest message;
    message.set_capacity(capacity);
    return Request(MessageCode::CreateRoomRequest, message);
}

Packet Join(std::uint64_t roomId)
{
    protocol::JoinRoomRequest message;
    message.set_room_id(roomId);
    return Request(MessageCode::JoinRoomRequest, message);
}

Packet Leave()
{
    return Request(MessageCode::LeaveRoomRequest, protocol::LeaveRoomRequest{});
}
Packet Ping()
{
    return Request(MessageCode::PingRequest, protocol::PingRequest{});
}

void Notification(SOCKET peer, bool joined, std::uint64_t roomId, std::uint64_t playerId)
{
    const auto packet = ReceivePacket(peer);
    Check(packet.code == (joined ? MessageCode::PlayerJoined : MessageCode::PlayerLeft) &&
              packet.requestId == 0 && packet.error == ErrorCode::None,
          "invalid notification header");
    if (joined) {
        const auto message = Parse<protocol::PlayerJoined>(packet);
        Check(message.room_id() == roomId && message.player_id() == playerId,
              "wrong joined player");
    } else {
        const auto message = Parse<protocol::PlayerLeft>(packet);
        Check(message.room_id() == roomId && message.player_id() == playerId, "wrong left player");
    }
}

Packet ChatRequest(const std::string& text)
{
    protocol::ChatRequest message;
    message.set_text(text);
    return Request(MessageCode::ChatRequest, message);
}

void ChatNotice(SOCKET peer, std::uint64_t roomId, std::uint64_t playerId, const std::string& text)
{
    const auto packet = ReceivePacket(peer);
    Check(packet.code == MessageCode::ChatMessage && packet.requestId == 0 &&
              packet.error == ErrorCode::None,
          "invalid chat notification header");
    const auto message = Parse<protocol::ChatMessage>(packet);
    Check(message.room_id() == roomId && message.player_id() == playerId && message.text() == text,
          "chat text or sender changed");
}

void RoomProtocol(std::uint16_t port)
{
    Socket first, second, third, isolated;
    first.Connect(port);
    second.Connect(port);
    third.Connect(port);
    isolated.Connect(port);
    Exchange(first.value, ChatRequest("hello"), ErrorCode::NotEntered);
    Exchange(first.value, Create(2), ErrorCode::NotEntered);
    Exchange(first.value, Join(1), ErrorCode::NotEntered);
    Exchange(first.value, Leave(), ErrorCode::NotEntered);
    const auto firstId = Enter(first.value);
    const auto secondId = Enter(second.value);
    const auto thirdId = Enter(third.value);
    Enter(isolated.value);
    Check(firstId != secondId && secondId != thirdId, "player IDs overlap");
    Exchange(first.value, Request(MessageCode::EnterRequest, protocol::EnterRequest{}),
             ErrorCode::AlreadyEntered);
    Exchange(first.value, ChatRequest("hello"), ErrorCode::NotInRoom);
    Exchange(first.value, Create(0), ErrorCode::InvalidCapacity);
    Exchange(first.value, Create(17), ErrorCode::InvalidCapacity);
    Exchange(first.value, Join(999999), ErrorCode::RoomNotFound);
    Exchange(first.value, Leave(), ErrorCode::NotInRoom);
    auto malformed = Create(2);
    malformed.payload = {static_cast<char>(128)};
    Exchange(first.value, malformed, ErrorCode::InvalidPayload);
    const auto created = Parse<protocol::CreateRoomResponse>(Exchange(first.value, Create(2)));
    const auto roomId = created.room_id();
    Check(roomId && created.capacity() == 2 && created.player_ids_size() == 1 &&
              created.player_ids(0) == firstId,
          "invalid creator snapshot");
    const auto other = Parse<protocol::CreateRoomResponse>(Exchange(isolated.value, Create(1)));
    Check(other.room_id() != roomId, "room IDs overlap");
    Exchange(isolated.value, Join(roomId), ErrorCode::AlreadyInRoom);
    const auto joined = Parse<protocol::JoinRoomResponse>(Exchange(second.value, Join(roomId)));
    Check(joined.room_id() == roomId && joined.capacity() == 2 && joined.player_ids_size() == 2 &&
              std::set<std::uint64_t>(joined.player_ids().begin(), joined.player_ids().end()) ==
                  std::set<std::uint64_t>{firstId, secondId},
          "invalid joined snapshot");
    Notification(first.value, true, roomId, secondId);
    for (const auto& text :
         {std::string("hello"), std::string("안녕하세요 👋"), std::string(1024, 'x')}) {
        const auto accepted = Exchange(first.value, ChatRequest(text));
        Check(accepted.payload.empty(), "chat acknowledgement should be empty");
        ChatNotice(first.value, roomId, firstId, text);
        ChatNotice(second.value, roomId, firstId, text);
    }
    Exchange(second.value, ChatRequest("reply"));
    ChatNotice(first.value, roomId, secondId, "reply");
    ChatNotice(second.value, roomId, secondId, "reply");
    Exchange(first.value, ChatRequest(""), ErrorCode::InvalidPayload);
    Exchange(first.value, ChatRequest(std::string(1025, 'x')), ErrorCode::InvalidPayload);
    auto badChat = ChatRequest("valid");
    badChat.payload = {static_cast<char>(128)};
    Exchange(first.value, badChat, ErrorCode::InvalidPayload);
    badChat.payload = {10, 1, static_cast<char>(255)};
    Exchange(first.value, badChat, ErrorCode::InvalidPayload);

    Exchange(third.value, Join(roomId), ErrorCode::RoomFull);
    Exchange(second.value, Join(roomId), ErrorCode::AlreadyInRoom);
    Exchange(second.value, Create(2), ErrorCode::AlreadyInRoom);
    Exchange(first.value, Ping());
    Exchange(second.value, Ping());
    Exchange(isolated.value, Ping());
    const auto left = Parse<protocol::LeaveRoomResponse>(Exchange(first.value, Leave()));
    Check(left.room_id() == roomId, "wrong leave room ID");
    Notification(second.value, false, roomId, firstId);
    Exchange(first.value, ChatRequest("left"), ErrorCode::NotInRoom);
    Exchange(first.value, Leave(), ErrorCode::NotInRoom);
    Exchange(third.value, Join(roomId));
    Notification(second.value, true, roomId, thirdId);
    linger reset{1, 0};
    setsockopt(third.value, SOL_SOCKET, SO_LINGER, reinterpret_cast<const char*>(&reset),
               sizeof(reset));
    closesocket(third.value);
    third.value = INVALID_SOCKET;
    Notification(second.value, false, roomId, thirdId);
    Exchange(second.value, Ping());
    Exchange(second.value, Leave());
    Exchange(first.value, Join(roomId), ErrorCode::RoomNotFound);
    Exchange(first.value, Ping());
    Exchange(isolated.value, Ping());
    Exchange(isolated.value, Leave());
    Exchange(first.value, Join(other.room_id()), ErrorCode::RoomNotFound);

    const auto raceRoom =
        Parse<protocol::CreateRoomResponse>(Exchange(first.value, Create(2))).room_id();
    auto a = std::async(std::launch::async, [&] {
        const auto bytes = Join(raceRoom).Serialize();
        SendAll(second.value, bytes.data(), bytes.size());
        return ReceivePacket(second.value);
    });
    auto b = std::async(std::launch::async, [&] {
        const auto bytes = Join(raceRoom).Serialize();
        SendAll(isolated.value, bytes.data(), bytes.size());
        return ReceivePacket(isolated.value);
    });
    const auto ar = a.get(), br = b.get();
    Check((ar.error == ErrorCode::None && br.error == ErrorCode::RoomFull) ||
              (br.error == ErrorCode::None && ar.error == ErrorCode::RoomFull),
          "room capacity race");
    const auto winner = Parse<protocol::JoinRoomResponse>(ar.error == ErrorCode::None ? ar : br);
    Check(winner.player_ids_size() == 2, "race exceeded capacity");
    const auto notice = ReceivePacket(first.value);
    Check(notice.code == MessageCode::PlayerJoined && notice.requestId == 0,
          "race join notification missing");
    Exchange(first.value, Ping());
}

void LogicLifecycle()
{
    PacketHandler handler;
    std::mutex mutex;
    std::condition_variable ready;
    std::map<ConnectionKey, std::queue<Packet>> inbox;
    std::optional<ConnectionKey> fail;
    const auto sender = [&](ConnectionKey key, const char* bytes, std::uint32_t size) {
        std::lock_guard lock(mutex);
        if (fail == key)
            return false;
        const auto header = PacketHeader::Read(bytes);
        Packet packet;
        packet.code = header.code;
        packet.error = header.error;
        packet.requestId = header.requestId;
        packet.payload.assign(bytes + ProtocolLimits::HeaderSize, bytes + size);
        inbox[key].push(std::move(packet));
        ready.notify_all();
        return true;
    };
    const auto take = [&](ConnectionKey key) {
        std::unique_lock lock(mutex);
        Check(ready.wait_for(lock, 3s, [&] { return !inbox[key].empty(); }),
              "logic response timeout");
        auto packet = std::move(inbox[key].front());
        inbox[key].pop();
        return packet;
    };
    const auto submit = [&](ConnectionKey key, const Packet& packet) {
        const auto bytes = packet.Serialize();
        Check(handler.ReceiveBytes(key, bytes.data(), bytes.size()), "logic Handle failed");
    };
    struct StopGuard
    {
        PacketHandler& handler;
        ~StopGuard()
        {
            handler.Stop();
        }
    } guard{handler};
    Check(handler.Start(sender), "logic start failed");
    const ConnectionKey old{0, 1}, current{0, 2}, observer{1, 1}, joining{2, 1};
    Check(handler.EnqueueConnected(old), "old connect failed");
    submit(old, Request(MessageCode::EnterRequest, protocol::EnterRequest{}));
    take(old);
    submit(old, Create(3));
    const auto oldRoom = Parse<protocol::CreateRoomResponse>(take(old)).room_id();
    handler.EnqueueDisconnected(old);
    Check(handler.EnqueueConnected(current), "new generation connect failed");
    submit(current, Request(MessageCode::EnterRequest, protocol::EnterRequest{}));
    take(current);
    handler.EnqueueDisconnected(old);
    const auto stale = Ping().Serialize();
    Check(!handler.ReceiveBytes(old, stale.data(), stale.size()), "stale key was accepted");
    submit(current, Join(oldRoom));
    Check(take(current).error == ErrorCode::RoomNotFound, "disconnect leaked empty room");
    submit(current, Create(3));
    const auto roomId = Parse<protocol::CreateRoomResponse>(take(current)).room_id();
    for (auto key : {observer, joining}) {
        Check(handler.EnqueueConnected(key), "connect failed");
        submit(key, Request(MessageCode::EnterRequest, protocol::EnterRequest{}));
        take(key);
    }
    submit(observer, Join(roomId));
    take(observer);
    take(current);
    {
        std::lock_guard lock(mutex);
        fail = current;
    }
    submit(joining, Join(roomId));
    Check(take(joining).error == ErrorCode::None, "join failed");
    Check(take(observer).code == MessageCode::PlayerJoined, "failed recipient stopped broadcast");
    Check(take(observer).code == MessageCode::PlayerLeft,
          "failed send did not clean up membership");
    Check(take(joining).code == MessageCode::PlayerLeft, "failed send leave notification missing");
    handler.EnqueueDisconnected(current);
    submit(observer, Ping());
    Check(take(observer).code == MessageCode::PingResponse, "duplicate leave notification");
    handler.Stop();
    {
        std::lock_guard lock(mutex);
        fail.reset();
        inbox.clear();
    }
    Check(handler.Start(sender), "logic restart failed");
    Check(handler.EnqueueConnected(current), "restarted connect failed");
    submit(current, Request(MessageCode::EnterRequest, protocol::EnterRequest{}));
    take(current);
    submit(current, Join(roomId));
    Check(take(current).error == ErrorCode::RoomNotFound, "restart retained room state");
}

int Finish(int result, bool pause)
{
    if (pause) {
        std::cout << "Press any key to exit..." << std::flush;
        DWORD mode = 0;
        if (GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode))
            static_cast<void>(_getch());
        else
            static_cast<void>(std::cin.get());
        std::cout << '\n';
    }
    return result;
}

int main(int argc, char* argv[])
{
    if (!(argc == 2 && std::string_view(argv[1]) == "--no-pause"))
        return RunChatClient(argc, argv);
    const bool pause = !(argc == 2 && std::string_view(argv[1]) == "--no-pause");
    if (argc > 2 || (argc == 2 && pause)) {
        std::cerr << "Usage: zpd-server-tests [--no-pause]\n";
        return Finish(1, pause);
    }
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "WinSock2 initialization failed.\n";
        return Finish(1, pause);
    }
    int result = 0;
    std::cout << "Running integration tests with embedded servers on temporary ports.\n"
              << "Server logs appear below. No external server is required." << std::endl;
    try {
        ZPDServer server;
        Check(!server.Start(0, 0), "zero capacity accepted");
        Check(server.Start(0, 16, 4), "start failed");
        Check(!server.Start(0, 16, 4), "double start accepted");
        {
            ZPDServer conflicting;
            Check(!conflicting.Start(server.Port(), 1, 1), "occupied port accepted");
            Check(conflicting.Start(0, 1, 1), "start after bind failure failed");
            Echo(conflicting.Port(), 32);
        }
        LogicLifecycle();
        PacketProtocol(server.Port());
        RoomProtocol(server.Port());
        InvalidPacketSize(server.Port(), 0);
        InvalidPacketSize(server.Port(), 3);
        InvalidPacketSize(server.Port(), 7);
        InvalidPacketSize(server.Port(), 4097);
        Echo(server.Port(), 0);
        Echo(server.Port(), 1);
        Echo(server.Port(), ProtocolLimits::MaxPayloadBytes);
        Echo(server.Port(), 4096);
        Echo(server.Port(), 256 * 1024);
        std::vector<std::future<void>> clients;
        for (int i = 0; i < 8; ++i)
            clients.push_back(
                std::async(std::launch::async, [&server] { Echo(server.Port(), 64 * 1024); }));
        for (auto& client : clients)
            client.get();
        for (int i = 0; i < 100; ++i)
            Echo(server.Port(), 257);

        Socket active;
        active.Connect(server.Port());
        const std::string pendingData(ProtocolLimits::MaxPayloadBytes, 'x');
        std::size_t pendingDataSize = 0;
        const Packet pending =
            BuildEchoRequest(pendingData.data(), pendingData.size(), pendingDataSize);
        const auto pendingBytes = pending.Serialize();
        SendAll(active.value, pendingBytes.data(), pendingBytes.size());
        server.Stop();
        server.Stop();
        Check(server.Start(0, 1, 2), "restart failed");
        Echo(server.Port(), 8192);
        server.Stop();

        Check(server.Start(0, 1, 4), "single-slot start failed");
        for (int i = 0; i < 30; ++i) {
            bool completed = false;
            for (int attempt = 0; attempt < 100 && !completed; ++attempt) {
                try {
                    Echo(server.Port(), 1024);
                    completed = true;
                } catch (const std::runtime_error&) {
                    std::this_thread::sleep_for(2ms);
                }
            }
            Check(completed, "single slot did not become reusable");
        }
        server.Stop();

        Socket aliveDuringDestruction;
        {
            ZPDServer scoped;
            Check(scoped.Start(0, 2, 2), "scoped start failed");
            aliveDuringDestruction.Connect(scoped.Port());
            std::this_thread::sleep_for(20ms);
        }
        std::cout << "All server integration checks passed.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    WSACleanup();
    return Finish(result, pause);
}
