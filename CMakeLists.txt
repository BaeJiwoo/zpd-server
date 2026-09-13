cmake_minimum_required(VERSION 3.25)

project(zpd-server VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "zpd-server requires Windows and WinSock2.")
endif()

find_package(Protobuf CONFIG REQUIRED)

add_library(protocol STATIC
    proto/echo.proto
)

target_compile_features(protocol PUBLIC cxx_std_20)
target_link_libraries(protocol PUBLIC protobuf::libprotobuf)

target_include_directories(protocol
    PUBLIC "${CMAKE_CURRENT_BINARY_DIR}"
)

protobuf_generate(
    TARGET protocol
    LANGUAGE cpp
)

add_executable(zpd-server
    src/main.cpp
    src/winsock.cpp
    src/PacketHandler.cpp
    include/winsock.hpp
    include/ZPDServer.hpp
    include/Packet.hpp
    include/PacketHandlerEvent.hpp
)
target_include_directories(zpd-server PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(zpd-server PRIVATE cxx_std_20)
set_target_properties(zpd-server PROPERTIES CXX_EXTENSIONS OFF)
target_compile_definitions(zpd-server PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(zpd-server
    PRIVATE
        ws2_32
        protocol
)

if(MSVC)
    target_compile_options(zpd-server PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(zpd-server-tests
        tests/server_tests.cpp
        tests/echo_client.cpp
        src/PacketHandler.cpp
    )
    target_include_directories(zpd-server-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_compile_features(zpd-server-tests PRIVATE cxx_std_20)
    set_target_properties(zpd-server-tests PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_definitions(zpd-server-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(zpd-server-tests
        PRIVATE
            ws2_32
            protocol
    )

    if(MSVC)
        target_compile_options(zpd-server-tests PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
    endif()
    add_test(NAME server-integration COMMAND zpd-server-tests --no-pause)
    set_tests_properties(server-integration PROPERTIES TIMEOUT 45)
endif()
