#ifndef ZPD_SERVERLIMITS_HPP
#define ZPD_SERVERLIMITS_HPP

#include <cstddef>
namespace ServerLimits {
// A live connection reserves one additional slot for its disconnect event.
inline constexpr std::size_t MaxQueuedEvents = 1024;
} // namespace ServerLimits

#endif // ZPD_SERVERLIMITS_HPP
