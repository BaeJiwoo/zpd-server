#ifndef ZPD_PENDINGREQUEST_HPP
#define ZPD_PENDINGREQUEST_HPP
#include "MessageCode.hpp"
#include <cstddef>
#include <memory>
#include <string>

struct EchoResponseBatch
{
    std::size_t remainingResponses = 0;
    std::string response;
};

struct PendingRequest
{
    MessageCode code;
    std::string expectedEchoText;
    std::shared_ptr<EchoResponseBatch> batch;
};

#endif // ZPD_PENDINGREQUEST_HPP
