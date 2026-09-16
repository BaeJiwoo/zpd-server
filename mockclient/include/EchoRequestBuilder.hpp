#ifndef ZPD_ECHOREQUESTBUILDER_HPP
#define ZPD_ECHOREQUESTBUILDER_HPP
#include "Packet.hpp"
#include <string_view>
bool BuildEchoRequest(std::string_view source, std::size_t offset, Packet& packet,
                      std::size_t& dataSize);
#endif // ZPD_ECHOREQUESTBUILDER_HPP
