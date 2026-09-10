#include "zpd/winsock.hpp"

#include <cstdlib>
#include <iostream>

int main()
{
    if (!zpd::check_winsock()) {
        return EXIT_FAILURE;
    }

    std::cout << "zpd-server: C++20 / WinSock2 ready.\n";
    return EXIT_SUCCESS;
}
