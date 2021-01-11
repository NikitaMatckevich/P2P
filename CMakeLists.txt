cmake_minimum_required(VERSION 3.2)

project(P2P)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Needed for vim code completion plugin

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pthread -DDEBUG -g -Wall -Wextra -pedantic -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -pthread -fomit-frame-pointer")

message(STATUS "Current build type: ${CMAKE_BUILD_TYPE}")

include_directories(./include/)

add_executable(peer ../src/peer.cpp)
add_executable(server ../src/server.cpp)
