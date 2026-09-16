#ifndef ZPD_CONNECTIONKEY_HPP
#define ZPD_CONNECTIONKEY_HPP

#include <cstdint>
#include <compare>

struct ConnectionKey
{
    std::uint32_t slotIndex = 0;
    std::uint64_t generation = 0;

    bool operator==(const ConnectionKey&) const = default;
    auto operator<=>(const ConnectionKey&) const = default;
};

#endif // ZPD_CONNECTIONKEY_HPP
