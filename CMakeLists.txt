cmake_minimum_required(VERSION 3.2)

project(Network)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Needed for vim code completion plugin

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pthread -DDEBUG -g -Wall -Wextra -pedantic -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -pthread -fomit-frame-pointer")

set(SOURCE ../src/network.cpp)
include_directories(include)

message(STATUS "Current build type: ${CMAKE_BUILD_TYPE}")

add_executable(network ${SOURCE})
