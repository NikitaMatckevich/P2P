#pragma once
#include <memory>
#include <thread>
#include <mutex>
#include <deque>
#include <vector>
#include <iostream>

#ifdef _WIN_32
  #define _WIN_32_WINNT 0x0A00
#endif

#define ASIO_STANDALONE
#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>

