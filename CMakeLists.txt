cmake_minimum_required(VERSION 3.25)
project(zpd-server VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "zpd-server requires Windows and WinSock2.")
endif()

find_package(Protobuf CONFIG REQUIRED)

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_20)
target_compile_definitions(project_options INTERFACE WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
    target_compile_options(project_options INTERFACE /W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(protocol STATIC
    proto/echo.proto
    proto/session.proto
    proto/room.proto
    proto/chat.proto
)
target_compile_features(protocol PUBLIC cxx_std_20)
target_link_libraries(protocol PUBLIC protobuf::libprotobuf)
target_include_directories(protocol PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
protobuf_generate(TARGET protocol LANGUAGE cpp)

add_library(server_core STATIC
    src/PacketHandler.cpp
    src/GameWorld.cpp
    src/RoomRequests.cpp
    src/ChatRequests.cpp
)
target_include_directories(server_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(server_core PUBLIC project_options protocol ws2_32)

add_executable(zpd-server src/main.cpp src/winsock.cpp)
target_link_libraries(zpd-server PRIVATE server_core)

add_library(chat_client STATIC
    client/ClientApplication.cpp
    client/ConsoleInput.cpp
    client/EchoRequestBuilder.cpp
    client/RoomChatClient.cpp
    client/RoomChatCommands.cpp
    client/RoomChatReceiver.cpp
)
target_include_directories(chat_client PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/client"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(chat_client PUBLIC project_options protocol ws2_32)
add_executable(zpd-client client/main.cpp)
target_link_libraries(zpd-client PRIVATE chat_client)

include(CTest)
if(BUILD_TESTING)
    add_executable(zpd-server-tests tests/server_tests.cpp)
    # Retain the legacy interactive entry point while zpd-client is the dedicated client.
    target_link_libraries(zpd-server-tests PRIVATE server_core chat_client)
    add_test(NAME server-integration COMMAND zpd-server-tests --no-pause)
    set_tests_properties(server-integration PROPERTIES TIMEOUT 45)
endif()
