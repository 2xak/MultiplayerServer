cmake_minimum_required(VERSION 3.20)
project(gameserver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # lets clangd/clang-tidy work

add_library(common STATIC common/socket.cpp common/Timing.hpp)
target_include_directories(common PUBLIC common)
target_compile_options(common PUBLIC -Wall -Wextra -Wpedantic)

add_executable(server src/main.cpp)
target_link_libraries(server PRIVATE common Threads::Threads)

add_executable(bot bot/main.cpp)
target_link_libraries(bot PRIVATE common Threads::Threads)

find_package(Threads REQUIRED)