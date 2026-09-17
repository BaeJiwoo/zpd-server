#include "RoomChatClient.hpp"
#include "ClientSettings.hpp"
#include "proto/echo.pb.h"
#include "proto/session.pb.h"
#include "proto/room.pb.h"
#include "proto/chat.pb.h"
#include <iostream>
#include <stdexcept>
#include "EchoRequestBuilder.hpp"
#include <charconv>
#include <sstream>
#include <cmath>
#include "proto/position.pb.h"
#include "proto/game.pb.h"
#include "ProtobufCodec.hpp"

bool RoomChatClient::HandleInputLine(const std::string& line)
{
    if (line == "/quit")
        return false;
    if (line == "/positions") {
        std::lock_guard lock(m_mutex);
        std::cout << "Position tick: " << m_positionTick << '\n';
        for (const auto& [id, position] : m_positions)
            std::cout << "Player " << id << " position: " << position[0] << ' '
                      << position[1] << ' ' << position[2] << '\n';
        std::cout << std::flush;
        return true;
    }
    if (line == "/members") {
        std::lock_guard lock(m_mutex);
        ShowMembers();
        return true;
    }
    Packet packet;
    if (line.starts_with("/game ")) {
        const auto input = std::string_view(line).substr(6);
        const auto split = input.find(' ');
        const auto number = input.substr(0, split);
        std::uint32_t command = 0;
        const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), command);
        const auto payload = split == std::string_view::npos ? std::string_view{} : input.substr(split + 1);
        if (error != std::errc{} || end != number.data() + number.size() || !command ||
            payload.size() > ProtocolLimits::MaxGamePayloadBytes) {
            PrintStatus("Invalid game command. Use /game <command> [payload].");
            return true;
        }
        protocol::GameCommandRequest message;
        {
            std::lock_guard lock(m_mutex);
            if (!m_roomId) {
                std::cout << "Join a room before game commands." << std::endl;
                return true;
            }
            message.set_room_id(m_roomId);
        }
        message.set_command(command);
        message.set_payload(std::string(payload));
        packet.code = MessageCode::GameCommandRequest;
        ProtobufCodec::SerializePayload(packet, message);
    } else if (line.starts_with("/move ")) {
        std::istringstream input(line.substr(6));
        float x, y, z;
        std::string extra;
        if (!(input >> x >> y >> z) || (input >> extra) || !std::isfinite(x) ||
            !std::isfinite(y) || !std::isfinite(z) ||
            std::abs(x) > ProtocolLimits::MaxPositionCoordinate ||
            std::abs(y) > ProtocolLimits::MaxPositionCoordinate ||
            std::abs(z) > ProtocolLimits::MaxPositionCoordinate) {
            PrintStatus("Invalid position. Use /move <x> <y> <z> within +/-1000000.");
            return true;
        }
        protocol::PositionUpdateRequest message;
        {
            std::lock_guard lock(m_mutex);
            if (!m_roomId) {
                std::cout << "Join a room before moving." << std::endl;
                return true;
            }
            message.set_room_id(m_roomId);
        }
        message.mutable_position()->set_x(x);
        message.mutable_position()->set_y(y);
        message.mutable_position()->set_z(z);
        packet.code = MessageCode::PositionUpdateRequest;
        ProtobufCodec::SerializePayload(packet, message);
    } else if (line == "/enter")
        packet.code = MessageCode::EnterRequest;
    else if (line == "/ping")
        packet.code = MessageCode::PingRequest;
    else if (line == "/leave")
        packet.code = MessageCode::LeaveRoomRequest;
    else if (line.starts_with("/create ") || line.starts_with("/join ")) {
        const bool create = line.starts_with("/create ");
        const auto argument = std::string_view(line).substr(create ? 8 : 6);
        std::uint64_t value = 0;
        const auto [end, error] =
            std::from_chars(argument.data(), argument.data() + argument.size(), value);
        if (error != std::errc{} || end != argument.data() + argument.size() || value == 0 ||
            (create && value > ProtocolLimits::MaxRoomCapacity)) {
            PrintStatus(
                "Invalid argument. /create accepts 1-16; /join requires a positive room ID.");
            return true;
        }
        std::string encoded;
        if (create) {
            packet.code = MessageCode::CreateRoomRequest;
            protocol::CreateRoomRequest message;
            message.set_capacity(static_cast<std::uint32_t>(value));
            message.SerializeToString(&encoded);
        } else {
            packet.code = MessageCode::JoinRoomRequest;
            protocol::JoinRoomRequest message;
            message.set_room_id(value);
            message.SerializeToString(&encoded);
        }
        packet.payload.assign(encoded.begin(), encoded.end());
    } else if (line != "/echo" && !line.starts_with("/echo ")) {
        if (line.starts_with('/') && !line.starts_with("/say ")) {
            PrintStatus(ClientSettings::CommandHelp);
            return true;
        }
        const auto text = line.starts_with("/say ") ? line.substr(5) : line;
        if (text.empty() || text.size() > ProtocolLimits::MaxChatTextBytes) {
            PrintStatus("Chat must contain 1-" + std::to_string(ProtocolLimits::MaxChatTextBytes) +
                        " UTF-8 bytes.");
            return true;
        }
        protocol::ChatRequest message;
        message.set_text(text);
        std::string encoded;
        if (!message.SerializeToString(&encoded)) {
            PrintStatus("Chat serialization failed.");
            return true;
        }
        packet.code = MessageCode::ChatRequest;
        packet.payload.assign(encoded.begin(), encoded.end());
    } else {
        const auto echoText = line == "/echo" ? std::string{} : line.substr(6);
        std::vector<std::pair<Packet, std::string>> chunks;
        std::size_t offset = 0;
        do {
            Packet chunk;
            std::size_t length = 0;
            if (!BuildEchoRequest(echoText, offset, chunk, length)) {
                CloseWithError("Echo serialization failed.");
                return false;
            }
            chunks.emplace_back(std::move(chunk), echoText.substr(offset, length));
            offset += length;
        } while (offset < echoText.size());
        auto batch = std::make_shared<EchoResponseBatch>();
        batch->remainingResponses = chunks.size();
        for (auto& [chunk, expected] : chunks)
            if (!SendRequest(std::move(chunk),
                             {MessageCode::EchoRequest, std::move(expected), batch}))
                return false;
        return true;
    }
    return SendRequest(packet, {packet.code, {}, {}});
}
