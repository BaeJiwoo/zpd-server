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

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
include(CTest)
add_subdirectory(common)
add_subdirectory(mockclient)
add_subdirectory(server)
