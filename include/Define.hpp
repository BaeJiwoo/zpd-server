#ifndef ZPD_DEFINE_HPP
#define ZPD_DEFINE_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <WinSock2.h>
#include <Windows.h>
#include <cstdint>
#include <compare>

constexpr DWORD MaxBufferSize = 4096;

enum class IOOperation
{
    Receive,
    Send,
};

struct IOContext
{
    OVERLAPPED overlapped{};
    WSABUF buffer{};
    IOOperation operation = IOOperation::Receive;
    char storage[MaxBufferSize]{};
    DWORD dataSize = 0;
};

struct ConnectionKey
{
    std::uint32_t clientId;
    std::uint64_t generation;

    bool operator==(const ConnectionKey&) const = default;
    auto operator<=>(const ConnectionKey&) const = default;
};

#endif // ZPD_DEFINE_HPP
