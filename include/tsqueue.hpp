#pragma once
#include <common.hpp>

namespace net {
  
  template <class T>
  class ThreadSafeQueue {
    std::mutex m_mutex;
    std::deque<T> m_queue;
   public:
    ThreadSafeQueue() = default;
    ThreadSafeQueue(const ThreadSafeQueue<T>&) = delete;
    ThreadSafeQueue<T>& operator=(const ThreadSafeQueue<T>&) = delete;
    
    void Clear() {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_queue.clear();
    }

    ~ThreadSafeQueue() {
      Clear();
    }

    size_t Size() {
      std::lock_guard<std::mutex> lock(m_mutex);
      return m_queue.size();
    }

    bool Empty() {
      std::lock_guard<std::mutex> lock(m_mutex);
      return m_queue.empty();
    }

    T& Front() {
      std::lock_guard<std::mutex> lock(m_mutex);
      return m_queue.front();
    }

    T& Back() {
      std::lock_guard<std::mutex> lock(m_mutex);
      return m_queue.back();
    }

    void Push(T value) {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_queue.push_front(std::move(value));
    }

    void Pop() {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_queue.pop_back();
    }
  };

}
