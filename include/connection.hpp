#pragma once
#include <message.hpp>
#include <tsqueue.hpp>

namespace net {

  class BaseConnection : public std::enable_shared_from_this<BaseConnection> {
   
   protected:

    asio::io_context& m_context;
    asio::ip::tcp::socket m_socket; 
    std::uint64_t m_handshake_recv = 0;
    std::uint64_t m_handshake_send = 0;
   
    BaseConnection(asio::io_context& context_reference,
                   asio::ip::tcp::socket&& socket)
      : AbstractConenction()
      , m_context(context_reference)
      , m_socket(std::move(socket)) {}

   public:

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
            m_socket.close();
          });
      }
    }

    bool IsConnected() const {
      return m_socket.is_open();
    }
  };

  template <class T>
  class PeerConnection : public BaseConnection {

    ThreadSafeQueue<Message<T>>& m_recv;
    ThreadSafeQueue<Message<T>>  m_send{};

    void AddToInbox() {
      m_recv.Push(m_temporary);
      m_recv.Front().Sender() = this->shared_from_this();
      ReadHeader();
    }

    void ReadHeader() {
      asio::async_read(m_socket, asio::buffer(&m_temporary.Header(), sizeof(MessageHeader<T>)),
        [this](std::error_code ec, size_t length) {
          if (!ec) {
            if (m_temporary.Header().Size() > 0) {
              m_temporary.Body().resize(m_temporary.Header().Size());
              ReadBody();
            } else {
              AddToInbox();
            }
          } else {
            std::cout << "Read header fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }

    void ReadBody() {
      asio::async_read(m_socket, asio::buffer(m_temporary.Body().data(), m_temporary.Header().Size()),
        [this](std::error_code ec, size_t length) {
          if (!ec) {
            AddToInbox();
          } else {
            std::cout << "Read body fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }

    void WriteHeader() {
      asio::async_write(m_socket, asio::buffer(&m_send.Back().Header(), sizeof(MessageHeader<T>)),
        [this](std::error_code ec, size_t length) {
          if (!ec) {
            if (m_send.Back().Header().Size() > 0) {
              WriteBody();
            } else {
              m_send.Pop();
              if (!m_send.Empty()) {
                WriteHeader();
              }
            }
          } else {
            std::cout << "Write header fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }

    void WriteBody() {
      asio::async_write(m_socket, asio::buffer(m_send.Back().Body().data(), m_send.Back().Body().size()),
        [this](std::error_code ec, size_t length) {
          if (!ec) {
            m_send.Pop();
            if (!m_send.Empty()) {
              WriteHeader();
            }
          } else {
            std::cout << "Write body fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }

   public:
    PeerConnection(asio::io_context& context_reference,
               asio::ip::tcp::socket&& socket,
               ThreadSafeQueue<Message<T>>& owner_inbox_queue)
      : BaseConnection(context_reference, std::move(socket))
      , m_recv(owner_inbox_queue) {}
    
    void SendMessage(const Message<T>& msg) {
      asio::post(m_context,
        [this, msg]() {
          bool isWritingNow = !(m_send.Empty());
          m_send.Push(msg);
          if (!isWritingNow) {
            WriteHeader();
          }
        });
    }
  };

  template <class T, bool FromDerived = false, class DerivedConnection = BaseConnection>
  class PassiveConnection
  : public std::conditional<FromDerived && std::is_convertible_v<BaseConnection*, DerivedConnection*>,
    DerivedConnection, BaseConnection>::type
  {
    Message<T> m_temporary{};

    void ReadHello() {
      asio::async_read(m_socket, asio::buffer(m_temporary.Header(), sizeof(MessageHeader<T>)),
        [this](std::error_code ec, size_t length) {
          if (!ec && m_temporary.Header().Size() > 0) {
            ReadRemoteName();
          } else {
            std::cout << "Read hello fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }

    void ReadRemoteName() {
      asio::async_read(m_socket, asio::buffer(m_temporary.Body().data(), m_temporary.Body().size()),
        [this](std::error_code ec, size_t length) {
          if (!ec) {
            ReadKey();
          } else {
            std::cout << "Read remote name fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }

    void WriteKey() {
      m_handshake_send = GenRandomKey();
      asio::async_write(m_socket, asio::buffer(&m_handshake_send, 8),
        [this](std::error_code ec, size_t length) {
          if (!ec) {
            ValidateKey();
          } else {
            std::cout << "Write key fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }

    void ValidateKey() {
      asio::async_read(m_socket, asio::buffer(&m_handshake_recv, 8),
        [this](std::error_code ec, size_t length) {
          if (ec || m_handshake_recv != Encrypt(m_handshake_send) ||
              !OnAccept(m_temporary.Header().Id()))
          {
            std::cout << "Key validation fail\n";
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
        ReadHello();
      }
    }
  };

  template <class T, bool FromDerived = false, class DerivedConnection = BaseConnection>
  class ActiveConnection
  : public std::conditional<FromDerived && std::is_convertible_v<BaseConnection*, DerivedConnection*>,
    DerivedConnection, BaseConnection>::type
  {

    Message<T> m_temporary{};

    void WriteHello() {
      asio::async_write(m_socket, asio::buffer(m_temporary.Header(), sizeof(MessageHeader<T>)),
        [this](std::error_code ec, size_t length) {
          if (!ec && m_temporary.Header().Size() > 0) {
            WriteMyName();
          } else {
            std::cout << "Write hello fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }

    void WriteMyName() {
      asio::async_write(m_socket, asio::buffer(m_temporary.Body().data(), m_temporary.Body().size()),
        [this](std::error_code ec, size_t length) {
          if (!ec) {
            ReadKey();
          } else {
            std::cout << "Write my name fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }

    void ReadKey() {
      asio::async_read(m_socket, asio::buffer(&m_handshake_recv, 8),
        [this](std::error_code ec, size_t length) {
          if (!ec) {
            m_handshake_send = Encrypt(m_handshake_recv);
            WriteKeyBack();
          } else {
            std::cout << "Read Key fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }

    void WriteKeyBack() {
      asio::async_write(m_socket, asio::buffer(&m_handshake_send, 8),
        [this](std::error_code ec, size_t length) {
          if (!ec) {
            OnRequest(m_temporary.Header().Id());
          } else {
            std::cout << "Encryption fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }

   protected:
    
    virtual void OnRequest(T type) = 0;

   public:

    void Request(const asio::ip::tcp::resolver::results_type& endpoints, Message<T> msg) {
      m_temporary = std::move(msg);
      asio::async_connect(m_socket, endpoints,
        [this](std::error_code ec, asio::ip::tcp::endpoint endpoint) {
          if (!ec) {
            WriteHello();
          } else {
            std::cout << "Proposition denied\n";
          }
        });
    }
  };

  using IP = std::pair<std::string, std::uint16_t>>;
  
  template <class T>
  class Server;

  template <class T>
  class ServerConnection : public PassiveConnection<T> {

    Server<T> * m_server;

   public:

    ServerConnection(asio::io_context& context_reference,
                     asio::ip::tcp::socket&& socket, Server<T> * server)
      : BaseConnection(context_reference, std::move(socket))
      , m_server(server) {}
  };

  template <class T>
  class ClientConnection : public ActiveConnection<T> {

    std::optional<IP> m_remote{};
    std::mutex m_mutex{};
    std::condition_variable m_cv{};

   public:

    ClientConnection(asio::io_context& context_reference,
                     asio::ip::tcp::socket&& socket)
      : BaseConnection(context_reference, std::move(socket)) {}  

    void GetFriendIP(std::string& host, std::uint16_t& port) {
      while (!m_remote) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock);
      }
      std::unique_lock<std::mutex> lock(m_mutex);
      port = m_remote.value().second;
      host = std::move(m_remote.value().first);
    }

    void GetMyIP(std::string& host, std::uint16_t& port) const {
      if (IsConnected()) {
        port = m_socket.local_endpoint().port();
        host = m_socket.local_endpoint().address().to_v4().to_string();
      } else {
        std::cerr << "Disconnected while reading name\n";
      }
    }
  };
 
}
