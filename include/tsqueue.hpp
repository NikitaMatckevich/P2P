#pragma once
#include <common.hpp>

namespace net {

  // message queues are accessed simultaneously by at least 2 trheads (pushing
  // thread and pulling thread), so we need to add synchronization primitives
  // to the ordinary queue object
  template <class T>
  class ThreadSafeQueue {
    std::mutex m_queue_mutex{};
    std::deque<T> m_queue{};

    std::mutex m_cv_mutex{};
    std::condition_variable m_cv{};

   public:
    ThreadSafeQueue() = default;
    ThreadSafeQueue(const ThreadSafeQueue<T>&) = delete;
    ThreadSafeQueue<T>& operator=(const ThreadSafeQueue<T>&) = delete;
    
    void Clear() {
      std::lock_guard<std::mutex> lock(m_queue_mutex);
      m_queue.clear();
    }

    ~ThreadSafeQueue() {
      Clear();
    }

    size_t Size() {
      std::lock_guard<std::mutex> lock(m_queue_mutex);
      return m_queue.size();
    }

    bool Empty() {
      std::lock_guard<std::mutex> lock(m_queue_mutex);
      return m_queue.empty();
    }

    T& Front() {
      std::lock_guard<std::mutex> lock(m_queue_mutex);
      return m_queue.front();
    }

    T& Back() {
      std::lock_guard<std::mutex> lock(m_queue_mutex);
      return m_queue.back();
    }

    void Push(T value) {
      std::lock_guard<std::mutex> lock(m_queue_mutex);
      m_queue.push_front(std::move(value));

      std::unique_lock<std::mutex> unique(m_cv_mutex);
      m_cv.notify_one();
    }

    void Pop() {
      std::lock_guard<std::mutex> lock(m_queue_mutex);
      m_queue.pop_back();
    }

    void Wait() {
      while (Empty()) {
        std::unique_lock<std::mutex> lock(m_cv_mutex);
        m_cv.wait(lock);
      }
    }
  };

}
