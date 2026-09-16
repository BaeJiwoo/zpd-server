#ifndef ZPD_CLIENTCONNECTION_HPP
#define ZPD_CLIENTCONNECTION_HPP

#include "NetworkIo.hpp"
#include "ConnectionKey.hpp"

#include <atomic>
#include <cstring>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>

class ClientConnection
{
    friend class IOCPServer;

  public:
    explicit ClientConnection(std::uint32_t index) : m_index(index)
    {
        m_receiveContext.operation = NetworkIoOperation::Receive;
        m_receiveContext.buffer.buf = m_receiveContext.storage;
        m_receiveContext.buffer.len = NetworkSettings::ReceiveBufferBytes;
    }

    std::uint32_t Index() const
    {
        return m_index;
    }

    bool IsConnected() const
    {
        return m_connected.load();
    }

    bool Attach(SOCKET socket, HANDLE iocp)
    {
        std::lock_guard lock(m_mutex);
        if (m_connected || m_pendingIoCount != 0) {
            closesocket(socket);
            return false;
        }
        m_socket = socket;
        if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(m_socket), iocp,
                                   reinterpret_cast<ULONG_PTR>(this), 0) == nullptr) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            return false;
        }

        ++m_generation;

        m_connected.store(true);

        return true;
    }

    bool Receive()
    {
        std::lock_guard lock(m_mutex);
        if (!m_connected)
            return false;
        ZeroMemory(&m_receiveContext.overlapped, sizeof(m_receiveContext.overlapped));
        m_receiveContext.buffer.buf = m_receiveContext.storage;
        m_receiveContext.buffer.len = NetworkSettings::ReceiveBufferBytes;

        DWORD flags = 0;
        DWORD received = 0;
        ++m_pendingIoCount;
        const int result = WSARecv(m_socket, &m_receiveContext.buffer, 1, &received, &flags,
                                   &m_receiveContext.overlapped, nullptr);
        if (result == 0 || WSAGetLastError() == WSA_IO_PENDING)
            return true;
        CompleteIo();
        return false;
    }

    bool Send(const char* data, std::uint32_t size)
    {
        std::lock_guard lock(m_mutex);
        if (!m_connected.load() || data == nullptr || size == 0 ||
            size > NetworkSettings::ReceiveBufferBytes) {
            return false;
        }

        if (m_sendQueue.size() >= NetworkSettings::MaxQueuedSends)
            return false;
        auto owned = std::make_unique<NetworkIoContext>();
        auto* context = owned.get();
        context->operation = NetworkIoOperation::Send;
        context->dataSize = size;
        std::memcpy(context->storage, data, size);
        context->buffer.buf = context->storage;
        context->buffer.len = static_cast<ULONG>(size);

        m_sendQueue.push_back(std::move(owned));
        if (m_sendQueue.size() > 1 || PostSend(context))
            return true;
        m_sendQueue.pop_front();
        return false;
    }

    bool ContinueSend(NetworkIoContext* context, DWORD transferred)
    {
        std::lock_guard lock(m_mutex);
        if (transferred >= context->buffer.len)
            return true;

        context->buffer.buf += transferred;
        context->buffer.len -= transferred;
        ZeroMemory(&context->overlapped, sizeof(context->overlapped));

        return PostSend(context);
    }

    bool Disconnect()
    {
        std::lock_guard lock(m_mutex);
        if (!m_connected.exchange(false))
            return false;

        shutdown(m_socket, SD_BOTH);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;

        while (m_sendQueue.size() > 1)
            m_sendQueue.pop_back();
        return true;
    }

    ConnectionKey Key() const noexcept
    {
        return {m_index, m_generation};
    }

  private:
    bool PostSend(NetworkIoContext* context)
    {
        if (!m_connected)
            return false;
        ++m_pendingIoCount;
        DWORD sent = 0;
        const int result =
            WSASend(m_socket, &context->buffer, 1, &sent, 0, &context->overlapped, nullptr);
        if (result == 0 || WSAGetLastError() == WSA_IO_PENDING)
            return true;
        CompleteIo();
        return false;
    }

    void CompleteIo()
    {
        if (--m_pendingIoCount == 0)
            m_idle.notify_all();
    }

    bool FinishSend()
    {
        m_sendQueue.pop_front();
        if (!m_connected) {
            m_sendQueue.clear();
            return true;
        }
        if (m_sendQueue.empty() || PostSend(m_sendQueue.front().get()))
            return true;
        m_sendQueue.clear();
        return false;
    }

    void WaitIdle()
    {
        std::unique_lock lock(m_mutex);
        m_idle.wait(lock, [this] { return m_pendingIoCount == 0; });
    }

    std::recursive_mutex m_mutex;
    std::condition_variable_any m_idle;
    std::size_t m_pendingIoCount = 0;
    std::deque<std::unique_ptr<NetworkIoContext>> m_sendQueue;
    std::uint32_t m_index;
    SOCKET m_socket = INVALID_SOCKET;
    std::atomic_bool m_connected = false;
    NetworkIoContext m_receiveContext;
    std::uint64_t m_generation = 0;
};

#endif // ZPD_CLIENTCONNECTION_HPP
