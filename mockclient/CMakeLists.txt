add_library(chat_client STATIC
    src/ClientApplication.cpp
    src/ConsoleInput.cpp
    src/EchoRequestBuilder.cpp
    src/RoomChatClient.cpp
    src/RoomChatCommands.cpp
    src/RoomChatReceiver.cpp
)
target_include_directories(chat_client PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(chat_client PUBLIC project_options protocol ws2_32)

add_executable(zpd-client src/main.cpp)
target_link_libraries(zpd-client PRIVATE chat_client)
