#pragma once

namespace zpd {

// Initializes WinSock2, creates a TCP socket, and releases the resources.
[[nodiscard]] bool check_winsock();

} // namespace zpd
