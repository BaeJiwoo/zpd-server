#ifndef ZPD_IOCPSERVER_HPP
#define ZPD_IOCPSERVER_HPP

#include "ClientInfo.hpp"

#include <algorithm>
#include <atomic>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

class IOCPServer
{
  public:
    virtual ~IOCPServer()
    {
        Stop();
    }

    std::uint16_t Port() const
    {
        return m_port;
    }

    bool Start(std::uint16_t port, std::uint32_t maxClients, std::uint16_t workerCount = 0)
    {
        if (m_running.load() || maxClients == 0)
            return false;

        if (WSAStartup(MAKEWORD(2, 2), &m_wsaData) != 0)
            return false;

        m_winsockStarted = true;

        m_listenSocket =
            WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
        if (m_listenSocket == INVALID_SOCKET)
            return FailStart();

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) ==
                SOCKET_ERROR ||
            listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR)
            return FailStart();

        int addressSize = sizeof(address);
        if (getsockname(m_listenSocket, reinterpret_cast<sockaddr*>(&address), &addressSize) ==
            SOCKET_ERROR)
            return FailStart();
        m_port = ntohs(address.sin_port);

        m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        if (m_iocp == nullptr)
            return FailStart();

        try {
            m_clients.reserve(maxClients);
            for (std::uint32_t i = 0; i < maxClients; ++i)
                m_clients.push_back(std::make_unique<ClientInfo>(i));

            m_running.store(true);

            if (workerCount == 0)
                workerCount = static_cast<std::uint16_t>(
                    std::clamp(std::thread::hardware_concurrency(), 1u, 65535u));

            for (std::uint16_t i = 0; i < workerCount; ++i)
                m_workerThreads.emplace_back([this] { WorkerLoop(); });

            m_acceptThread = std::thread([this] { AcceptLoop(); });
        } catch (...) {
            Stop();
            return false;
        }

        std::cout << "Listening on port " << m_port << '\n';
        return true;
    }

    void Stop()
    {
        if (!m_running.exchange(false)) {
            CleanupHandles();
            return;
        }

        closesocket(m_listenSocket);

        if (m_acceptThread.joinable())
            m_acceptThread.join();
        m_listenSocket = INVALID_SOCKET;

        for (auto& client : m_clients)
            DisconnectClient(client.get());

        for (auto& client : m_clients)
            client->WaitIdle();

        for (std::size_t i = 0; i < m_workerThreads.size(); ++i)
            PostQueuedCompletionStatus(m_iocp, 0, 0, nullptr);

        for (auto& worker : m_workerThreads)
            if (worker.joinable())
                worker.join();

        m_workerThreads.clear();
        m_clients.clear();
        CleanupHandles();
    }

  protected:
    bool Send(ConnectionKey connection, const char* data, std::uint32_t size)
    {
        if (data == nullptr || size == 0 || size > MaxBufferSize ||
            connection.clientId >= m_clients.size()) {
            return false;
        }

        ClientInfo* client = m_clients[connection.clientId].get();
        std::lock_guard lock(client->m_mutex);

        if (!client->IsConnected() || client->m_generation != connection.generation) {
            return false;
        }

        return client->Send(data, size);
    }

    void Disconnect(ConnectionKey connection)
    {
        if (connection.clientId >= m_clients.size()) {
            return;
        }

        ClientInfo* client = m_clients[connection.clientId].get();
        std::lock_guard lock(client->m_mutex);

        if (!client->IsConnected() || client->m_generation != connection.generation) {
            return;
        }

        DisconnectClient(client);
    }

    virtual void OnConnected(ConnectionKey connection) = 0;
    virtual void OnReceived(ConnectionKey connection, const char* data, DWORD size) = 0;
    virtual void OnSendCompleted(ConnectionKey connection, const char*, DWORD size) = 0;
    virtual void OnDisconnected(ConnectionKey connection) = 0;

  private:
    bool FailStart()
    {
        CleanupHandles();
        return false;
    }

    void CleanupHandles()
    {
        m_port = 0;
        m_clients.clear();
        if (m_listenSocket != INVALID_SOCKET) {
            closesocket(m_listenSocket);
            m_listenSocket = INVALID_SOCKET;
        }
        if (m_iocp != nullptr) {
            CloseHandle(m_iocp);
            m_iocp = nullptr;
        }
        if (m_winsockStarted) {
            WSACleanup();
            m_winsockStarted = false;
        }
    }

    void DisconnectClient(ClientInfo* client)
    {
        std::lock_guard lock(client->m_mutex);
        if (client->Disconnect()) {
            OnDisconnected(client->Key());
        }
    }

    void AcceptLoop()
    {
        while (m_running.load()) {
            SOCKET socket = accept(m_listenSocket, nullptr, nullptr);
            if (socket == INVALID_SOCKET)
                continue;
            if (!m_running.load()) {
                closesocket(socket);
                break;
            }

            ClientInfo* client = nullptr;
            for (auto& candidate : m_clients) {
                std::lock_guard lock(candidate->m_mutex);
                if (!candidate->IsConnected() && candidate->m_pendingIoCount == 0) {
                    client = candidate.get();
                    break;
                }
            }

            if (client == nullptr) {
                closesocket(socket);
                continue;
            }
            std::lock_guard lock(client->m_mutex);
            if (!client->Attach(socket, m_iocp)) {
                continue;
            }

            OnConnected(client->Key());
            if (!client->Receive())
                DisconnectClient(client);
        }
    }

    void WorkerLoop()
    {
        while (true) {
            DWORD transferred = 0;
            ULONG_PTR key = 0;
            OVERLAPPED* overlapped = nullptr;
            const BOOL succeeded =
                GetQueuedCompletionStatus(m_iocp, &transferred, &key, &overlapped, INFINITE);
            if (overlapped == nullptr)
                return;

            auto* client = reinterpret_cast<ClientInfo*>(key);
            auto* context = reinterpret_cast<IOContext*>(overlapped);
            std::lock_guard lock(client->m_mutex);

            struct CompletionGuard
            {
                ClientInfo* client;
                ~CompletionGuard()
                {
                    client->CompleteIo();
                }
            } completion{client};

            if (!succeeded || transferred == 0 || !client->IsConnected()) {
                DisconnectClient(client);
                if (context->operation == IOOperation::Send)
                    client->FinishSend();
                continue;
            }

            if (context->operation == IOOperation::Receive) {
                OnReceived(client->Key(), context->storage, transferred);
                if (client->IsConnected() && !client->Receive()) {
                    DisconnectClient(client);
                }
                continue;
            }

            if (transferred < context->buffer.len) {
                if (!client->ContinueSend(context, transferred)) {
                    DisconnectClient(client);
                    client->FinishSend();
                }
                continue;
            }

            OnSendCompleted(client->Key(), context->storage, context->dataSize);
            if (!client->FinishSend())
                DisconnectClient(client);
        }
    }

    WSADATA m_wsaData{};
    std::uint16_t m_port = 0;
    bool m_winsockStarted = false;
    SOCKET m_listenSocket = INVALID_SOCKET;
    HANDLE m_iocp = nullptr;
    std::atomic_bool m_running = false;
    std::vector<std::unique_ptr<ClientInfo>> m_clients;
    std::thread m_acceptThread;
    std::vector<std::thread> m_workerThreads;
};

#endif // ZPD_IOCPSERVER_HPP
