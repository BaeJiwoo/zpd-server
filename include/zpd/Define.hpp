#ifndef ZPD_DEFINE_H
#define ZPD_DEFINE_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <WinSock2.h>
#include <Windows.h>
#include <cstdint>

constexpr DWORD MAX_BUFFER_SIZE = 4096;

enum class IOOperation
{
    Receive,
    Send,
};

struct IOContext
{
    OVERLAPPED m_overlapped{};
    WSABUF m_buffer{};
    IOOperation m_operation = IOOperation::Receive;
    char m_storage[MAX_BUFFER_SIZE]{};
    DWORD m_dataSize = 0;
};

#endif
