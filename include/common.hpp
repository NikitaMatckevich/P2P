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

  inline std::uint64_t GenRandomKey() {
    std::random_device rd;
    std::mt19937_64 mt(rd());
    std::uint64_t key = mt() ^
      std::uint64_t(std::chrono::system_clock::now().
        time_since_epoch().count());
    return key;
  }

  inline std::uint64_t Encrypt(std::uint64_t key) {
    std::uint64_t out = (key & 0xFACEDEADBEEFCAFE) << 5 |
                        (key & 0xC0DE1DEC0DE1C0DE) >> 3 |
                        (key ^ 0xF0F0F0F0F0F0F0F0) ;
    return out ^ (key * key - 12345678);
  }

  enum class ServerMsgTypes : std::uint32_t {
    Connect,
    AskAddress
  };

  enum class PeerMsgTypes : std::uint32_t {
    Connect,
    Say
  };
}
