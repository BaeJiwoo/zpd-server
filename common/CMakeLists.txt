add_library(protocol STATIC
    proto/echo.proto
    proto/session.proto
    proto/room.proto
    proto/chat.proto
    proto/position.proto
    proto/game.proto
)
target_compile_features(protocol PUBLIC cxx_std_20)
target_link_libraries(protocol PUBLIC protobuf::libprotobuf)
target_include_directories(protocol PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}"
)
protobuf_generate(TARGET protocol LANGUAGE cpp)
