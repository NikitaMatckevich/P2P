#pragma once
#include <memory>
#include <thread>
#include <mutex>
#include <deque>
#include <vector>
#include <iostream>
#include <string_view>
#include <charconv>
#include <chrono>
#include <random>

#ifdef _WIN_32
  #define _WIN_32_WINNT 0x0A00
#endif

#define ASIO_STANDALONE
#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>

namespace net {
  template <class T>
  T UIFactory(std::string_view name) {
    T value;
    std::cout << "Please enter the " << name << '\n';
    std::cin >> value;
    return value;
  }
}
