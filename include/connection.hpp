#pragma once
#include <message.hpp>
#include <tsqueue.hpp>

namespace net {
  
  // Connection hierarchy
  class BaseConnection {
 
    std::mutex m_work_mutex{};
    std::condition_variable m_work{};

   protected:

    asio::io_context& m_context;
    asio::ip::tcp::socket m_socket; 
    std::uint64_t m_handshake_recv = 0;
    std::uint64_t m_handshake_send = 0;
    
   public:

    BaseConnection(asio::io_context& context,
                   asio::ip::tcp::socket&& socket)
      : m_context(context)
      , m_socket(std::move(socket)) {}

    virtual ~BaseConnection() = default;

    void DumpEndpoints() const {
      std::cout << "Local endpoint:\n";
      std::cout << m_socket.local_endpoint() << '\n';
      std::cout << "Remote endpoint:\n";
      std::cout << m_socket.remote_endpoint() << '\n';
    }
 
    void Disconnect() {
      if (IsConnected()) {
        DumpEndpoints();
        asio::post(m_context,
          [this]() {   
            std::unique_lock lock(m_work_mutex);
            m_socket.close();
            m_work.notify_one();
          });
      }
    }

    bool IsConnected() const {
      return m_socket.is_open();
    }

    void Wait() {
      while (IsConnected()) {
        std::unique_lock lock(m_work_mutex);
        m_work.wait(lock);
      }
    }
  };

  // Some objects can propose other objects to connect (they are active)
  template <class T>
  class ActiveConnection : virtual public BaseConnection {
   protected:
   
    Message<T> m_active{}; 

    void WriteHello() {
      asio::async_write(m_socket, asio::buffer(&m_active.Header(), sizeof(MessageHeader<T>)),
        [this](std::error_code ec, size_t length) {
          if (!ec && m_active.Header().Size() > 0) {
            std::cout << 2 << "... \n";
            WriteMyName();
          } else {
            std::cout << "Write hello fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }

    void WriteMyName() {
      asio::async_write(m_socket, asio::buffer(m_active.Body().data(), m_active.Body().size()),
        [this](std::error_code ec, size_t length) {
          if (!ec) {
            std::cout << 3 << "... \n";
            OnRequest(m_active.Header().Id());
          } else {
            std::cout << "Write my name fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }
 
   protected:

    virtual void OnRequest(T type) = 0;

   public:

    void Request(const asio::ip::tcp::resolver::results_type& endpoints, Message<T> msg) { 
      m_active = std::move(msg);
      asio::async_connect(m_socket, endpoints,
        [this](std::error_code ec, asio::ip::tcp::endpoint endpoint) {
          if (!ec) {
            std::cout << 1 << "... \n";
            WriteHello();
          } else {
            std::cout << "Proposition denied\n";
          }
        });
    }
  };

  // Some objects can only wait for connections (they are passive)
  template <class T>
  class PassiveConnection : virtual public BaseConnection {
   protected:

    Message<T> m_passive{};

    void ReadHello() {
      std::cout << "Trying to read hello\n";
      asio::async_read(m_socket, asio::buffer(&m_passive.Header(), sizeof(MessageHeader<T>)),
        [this](std::error_code ec, size_t length) {
          std::cout << "Already in callback\n";
          if (!ec && m_passive.Header().Size() > 0) {
            m_passive.Body().resize(m_passive.Header().Size());
            std::cout << 2 << "... \n";
            ReadRemoteName();
          } else {
            std::cout << "Read hello fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }

    void ReadRemoteName() {
      asio::async_read(m_socket, asio::buffer(m_passive.Body().data(), m_passive.Body().size()),
        [this](std::error_code ec, size_t length) {
          if (ec || !OnAccept(m_passive.Header().Id())) {
            std::cout << "Accept fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }
 
   protected:

    virtual bool OnAccept(T type) = 0;

   public:

    void Accept() {
      if (IsConnected()) {
        std::cout << 1 << "... \n";
        ReadHello();
      }
    }
  };

  // We need some simple protocol for peer/server short interactions
  enum class CustomConnectionModes : std::uint32_t {
    Initialize,
    AskAddress
  };

  // IP version IPv4
  using IP = std::pair<std::array<std::uint8_t, 4>, std::uint16_t>;
  
  inline std::array<std::uint8_t, 4> IPFromString(const std::string& s) {
    std::array<std::uint8_t, 4> res;
    std::string_view view{s};
    for (size_t i = 0; i < 4; i++) {
      std::uint8_t val = 0;
      auto [ptr, ec] = std::from_chars(view.begin(), view.end(), val);
      if (ec == std::errc()) {
        res[i] = val;
        if (ptr != view.end())
          view = std::string_view(ptr + 1);
      }
      else {
        std::cerr << "Cannot parse the address " << s << '\n';
        break;
      }
    }
    for (int i = 0; i < 4; i++) 
      std::cout << (int)res[i] << '.';
    std::cout << '\n';
    return res;
  }

  inline std::string StringFromIP(const std::array<std::uint8_t, 4>& a) {
    std::string res = std::to_string(a[0]);
    for (size_t i = 1; i < 4; i++) {
      res.push_back('.');
      res += std::to_string(a[i]);
    }
    return res;
  }
 
}
