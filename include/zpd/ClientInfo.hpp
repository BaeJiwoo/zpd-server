#ifndef ZPD_CLIENT_INFO_H
#define ZPD_CLIENT_INFO_H

#include "Define.hpp"

#include <atomic>
#include <cstring>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>

class ClientInfo
{
    friend class IOCPServer;
  public:
    explicit ClientInfo(std::uint32_t index) : m_index(index)
    {
        m_receive.m_operation = IOOperation::Receive;
        m_receive.m_buffer.buf = m_receive.m_storage;
        m_receive.m_buffer.len = MAX_BUFFER_SIZE;
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
        if (m_connected || m_pending != 0) {
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

        m_connected.store(true);
        return true;
    }

    bool Receive()
    {
        std::lock_guard lock(m_mutex);
        if (!m_connected)
            return false;
        ZeroMemory(&m_receive.m_overlapped, sizeof(m_receive.m_overlapped));
        m_receive.m_buffer.buf = m_receive.m_storage;
        m_receive.m_buffer.len = MAX_BUFFER_SIZE;

        DWORD flags = 0;
        DWORD received = 0;
        ++m_pending;
        const int result = WSARecv(m_socket, &m_receive.m_buffer, 1, &received, &flags,
                                   &m_receive.m_overlapped, nullptr);
        if (result == 0 || WSAGetLastError() == WSA_IO_PENDING)
            return true;
        CompleteIo();
        return false;
    }

    bool Send(const char* data, std::uint32_t size)
    {
        std::lock_guard lock(m_mutex);
        if (!m_connected.load() || data == nullptr || size == 0 || size > MAX_BUFFER_SIZE) {
            return false;
        }

        
        if (m_sends.size() >= 256)
            return false;
        auto owned = std::make_unique<IOContext>();
        auto* context = owned.get();
        context->m_operation = IOOperation::Send;
        context->m_dataSize = size;
        std::memcpy(context->m_storage, data, size);
        context->m_buffer.buf = context->m_storage;
        context->m_buffer.len = static_cast<ULONG>(size);

        m_sends.push_back(std::move(owned));
        if (m_sends.size() > 1 || PostSend(context))
            return true;
        m_sends.pop_front();
        return false;
    }

    bool ContinueSend(IOContext* context, DWORD transferred)
    {
        std::lock_guard lock(m_mutex);
        if (transferred >= context->m_buffer.len)
            return true;

        context->m_buffer.buf += transferred;
        context->m_buffer.len -= transferred;
        ZeroMemory(&context->m_overlapped, sizeof(context->m_overlapped));

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

        while (m_sends.size() > 1)
            m_sends.pop_back();
        return true;
    }

  private:
    bool PostSend(IOContext* context)
    {
        if (!m_connected)
            return false;
        ++m_pending;
        DWORD sent = 0;
        const int result = WSASend(m_socket, &context->m_buffer, 1, &sent, 0,
                                   &context->m_overlapped, nullptr);
        if (result == 0 || WSAGetLastError() == WSA_IO_PENDING)
            return true;
        CompleteIo();
        return false;
    }

    void CompleteIo()
    {
        if (--m_pending == 0)
            m_idle.notify_all();
    }

    bool FinishSend()
    {
        m_sends.pop_front();
        if (!m_connected) {
            m_sends.clear();
            return true;
        }
        if (m_sends.empty() || PostSend(m_sends.front().get()))
            return true;
        m_sends.clear();
        return false;
    }

    void WaitIdle()
    {
        std::unique_lock lock(m_mutex);
        m_idle.wait(lock, [this] { return m_pending == 0; });
    }

    std::recursive_mutex m_mutex;
    std::condition_variable_any m_idle;
    std::size_t m_pending = 0;
    std::deque<std::unique_ptr<IOContext>> m_sends;
    std::uint32_t m_index;
    SOCKET m_socket = INVALID_SOCKET;
    std::atomic_bool m_connected = false;
    IOContext m_receive;
};

#endif
