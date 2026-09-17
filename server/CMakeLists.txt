add_library(server_core STATIC src/PacketHandler.cpp)
target_include_directories(server_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(server_core PUBLIC project_options protocol ws2_32)

add_executable(zpd-server src/main.cpp src/winsock.cpp)
target_link_libraries(zpd-server PRIVATE server_core)

if(BUILD_TESTING)
    add_executable(zpd-server-tests tests/server_tests.cpp)
    target_link_libraries(zpd-server-tests PRIVATE server_core)
    add_test(NAME server-integration COMMAND zpd-server-tests)
    set_tests_properties(server-integration PROPERTIES TIMEOUT 15)
endif()
