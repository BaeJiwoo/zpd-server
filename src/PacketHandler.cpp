#include "PacketHandler.hpp"
#include "proto/echo.pb.h"

#include <exception>
#include <iostream>

bool PacketHandler::Run(SendCallback sendPacket)
{
    if (m_logicRunning || !sendPacket)
        return false;

    m_sendPacket = std::move(sendPacket);

    // TODO: Create the event-processing thread here when the queue is added.
    m_logicRunning = true;

    try {
        m_logicThread = std::thread(&PacketHandler::LogicWorker, this);
    } catch (...) {
        m_logicRunning = false;
        m_sendPacket = {};
        return false;
    }
    return true;
}

void PacketHandler::Stop()
{
    if (!m_logicRunning)
        return;

    // TODO: Signal, wake and join the event-processing thread here.
    {
        {
            std::unique_lock lock(m_mutex);
            m_logicRunning = false;
        }
        m_queueReady.notify_all();

        if(m_logicThread.joinable()) {
            m_logicThread.join();
        }
        
        {
            std::unique_lock lock(m_mutex);
            while (!m_packetQueue.empty()) {
                m_packetQueue.pop();
            }

            m_pending.clear();
            m_sendPacket = {};
        }
    }

}

void PacketHandler::LogicWorker()
{
    while (true) {
        PacketHandlerEvent request;

        {
            std::unique_lock lock(m_mutex);

            m_queueReady.wait(lock, [this] {
                return !m_logicRunning || !m_packetQueue.empty();
            });

            if (!m_logicRunning)
                return;

            request = std::move(m_packetQueue.front());
            m_packetQueue.pop();
        }

        try {
            const Packet response = Dispatch(request.packet);
            const auto bytes = response.Serialize();

            m_sendPacket(
                request.connection,
                bytes.data(),
                static_cast<std::uint32_t>(bytes.size()));

        } catch (const std::exception& error) {
            std::cerr << "[packet processing failed] "
                      << error.what() << '\n';
        } catch (...) {
            std::cerr << "[packet processing failed] unknown error\n";
        }
    }
}

Packet PacketHandler::Echo(const Packet& requestPacket)
{
    Packet response;
    response.request = RequestCode::Echo;

    protocol::EchoRequest request;
    if(!request.ParseFromArray(
            requestPacket.payload.data(),
            static_cast<int>(requestPacket.payload.size()))) {
        response.error = ErrorCode::InvalidPayload;
        return response;
    }

    std::cout << "[Echo received] " << request.data() << '\n';

    protocol::EchoResponse reply;
    reply.set_data(request.data());
    
    if(reply.ByteSizeLong() > PacketHeader::MaxPayloadSize) {
        response.error = ErrorCode::InvalidPayload;
        return response;
    }

    std::string encoded;
    if(!reply.SerializeToString(&encoded)) {
        response.error = ErrorCode::UnknownError;
        return response;
    }

    response.payload.assign(encoded.begin(), encoded.end());
    std::cout << "[Echo response] " << reply.data() << '\n';
    return response;
}
