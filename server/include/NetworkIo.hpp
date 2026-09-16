#ifndef ZPD_NETWORKIO_HPP
#define ZPD_NETWORKIO_HPP

#include <WinSock2.h>
#include <Windows.h>
#include "NetworkSettings.hpp"

enum class NetworkIoOperation
{
    Receive,
    Send,
};

struct NetworkIoContext
{
    OVERLAPPED overlapped{};
    WSABUF buffer{};
    NetworkIoOperation operation = NetworkIoOperation::Receive;
    char storage[NetworkSettings::ReceiveBufferBytes]{};
    DWORD dataSize = 0;
};

#endif // ZPD_NETWORKIO_HPP
