#pragma once
#include <message.hpp>
#include <tsqueue.hpp>
#include <connection.hpp>

namespace net {

  // P2P TCP connection object
  template <class T>
  class Connection : public ActiveConnection<CustomConnectionModes>
                   , public PassiveConnection<CustomConnectionModes>
                   , public std::enable_shared_from_this<Connection<T>>
  {

    // common inbox queue for all connections of the peer
    ThreadSafeQueue<Message<T>>& m_recv;
    ThreadSafeQueue<Message<T>>  m_send{};
    Message<T> m_temp{};

    void AddToInbox() {
      m_recv.Push(m_temp);
      m_recv.Front().Sender() = this->shared_from_this();
      ReadHeader();
    }

    // ASYNC functions to read/write messages in P2P mode
    void ReadHeader() {
      asio::async_read(m_socket, asio::buffer(&m_temp.Header(), sizeof(MessageHeader<T>)),
        [this](std::error_code ec, size_t length) {
          if (!ec) {
            if (m_temp.Header().Size() > 0) {
              m_temp.Body().resize(m_temp.Header().Size());
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
      asio::async_read(m_socket, asio::buffer(m_temp.Body().data(), m_temp.Header().Size()),
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
 
   protected:
    
    virtual bool OnAccept(CustomConnectionModes type) override final {
      ReadHeader();
      return true;
    }
    virtual void OnRequest(CustomConnectionModes type) override final {
      ReadHeader();
    }

   public:

    Connection(asio::io_context& context,
               asio::ip::tcp::socket&& socket,
               ThreadSafeQueue<Message<T>>& inbox)
      : BaseConnection(context, std::move(socket))
      , m_recv(inbox) {}

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

  // Short connection with server
  class ClientConnection : public ActiveConnection<CustomConnectionModes> {

    std::string m_friend_name;
    std::optional<IP> m_remote{};
    std::condition_variable m_connection{};
    std::mutex m_connection_mutex{};

   protected:

    // Whenever peer requests connection, it recieves some data from server and
    // immediately disconnects
    virtual void OnRequest(CustomConnectionModes type) override final {
      {
        std::unique_lock<std::mutex> lock(m_connection_mutex);
        m_connection.notify_one();
      }
      if (type == CustomConnectionModes::AskAddress) {
        m_active.Clear();
        for (auto c : m_friend_name)
          m_active << c;
        asio::async_write(m_socket, asio::buffer(&m_active.Header(), sizeof(MessageHeader<CustomConnectionModes>)),
          [this](std::error_code ec, size_t length) {
            if (!ec && m_active.Header().Size() > 0) {
              asio::async_write(m_socket, asio::buffer(m_active.Body().data(), m_active.Body().size()),
                [this](std::error_code ec, size_t length) {
                  if (!ec) {
                    m_remote = IP{};
                    asio::async_read(m_socket, asio::buffer(m_remote.value().first),
                      [this](std::error_code ec, size_t length) {
                        asio::async_read(m_socket, asio::buffer(&m_remote.value().second, 2),
                          [this](std::error_code ec, size_t length) {
                            Disconnect(); 
                          });
                    });
                  } else {
                    std::cout << "Read fail\n";
                    DumpEndpoints();
                    m_socket.close();
                  }
                });
            } else {
              std::cout << "Ask address " << m_friend_name << " fail\n";
              DumpEndpoints();
              m_socket.close();
            }
          });
      } else {
        Disconnect(); 
      }
    }

   public:

    ClientConnection(asio::io_context& context,
                     asio::ip::tcp::socket&& socket,
                     std::string friend_name = "")
      : BaseConnection(context, std::move(socket))
      , m_friend_name{std::move(friend_name)} {}
    
    // Server is used primarily to get IPs of other users
    void GetFriendIP(std::string& host, std::uint16_t& port) { 
      if (!IsConnected() && m_remote.has_value()) {
        port = m_remote.value().second;
        host = StringFromIP(m_remote.value().first);
      }
    }
  
    void GetMyIP(std::string& host, std::uint16_t& port) {
      while (!IsConnected()) {
        std::unique_lock<std::mutex> lock(m_connection_mutex);
        m_connection.wait(lock);
      }
      port = m_socket.local_endpoint().port();
      host = m_socket.local_endpoint().address().to_v4().to_string();
    }
  };

  template <class T>
  class Peer {

    using work_guard_t = asio::executor_work_guard<asio::io_context::executor_type>; 

    ThreadSafeQueue<Message<T>> m_queue{};

    std::thread m_asio{};
    std::thread m_proc{};

    std::vector<std::shared_ptr<Connection<T>>> m_connections{};
    size_t m_connections_till_check{1};

    asio::io_context m_context{};

    asio::ip::tcp::acceptor m_acceptor;
    std::unique_ptr<work_guard_t> m_work_guard{nullptr};
    std::string m_username;
    std::string m_host;
    std::uint16_t m_port;

    std::shared_ptr<Connection<T>> NewConnection(std::shared_ptr<Connection<T>> peer) {
      auto it = std::find(m_connections.begin(), m_connections.end(), nullptr);
      if (it != m_connections.end()) {
        *it = std::move(peer);
        return *it;
      }
      else {
        m_connections.push_back(std::move(peer));
        return m_connections.back();
      }
    }

    void EraseConnections(std::shared_ptr<Connection<T>> peer = nullptr) {
      m_connections.erase(
        std::remove(m_connections.begin(), m_connections.end(), peer),
        m_connections.end()
      );
    }

    void Update(size_t freqReading = -1, bool waitIncoming = true) {
      if (waitIncoming) {
        Incoming().Wait();
      }
      for (size_t i = 0; i < freqReading && !m_queue.Empty(); i++) {
        ProcessMessage(m_queue.Back());
        m_queue.Pop();
      }
    }
         
   protected:

    virtual Message<T> WriteMessage() = 0;
    virtual void ProcessMessage(Message<T>& msg) = 0;
    
    virtual bool AcceptConnection(std::shared_ptr<Connection<T>> peer) {
      std::cout << "Attention: this function accepts any connection\n";
      return true;
    }
    virtual void ProcessDisconnection(std::shared_ptr<Connection<T>> peer) {
      std::cout << "Disconnecting\n";
    }
    
   public:
    
    explicit Peer(std::string username)
      : m_acceptor(m_context)
      , m_username(std::move(username)) {}

    virtual ~Peer() { Stop(); }

    ThreadSafeQueue<Message<T>>& Incoming() noexcept { return m_queue; }
    const ThreadSafeQueue<Message<T>>& Incoming() const noexcept { return m_queue; }
    std::string& Username() noexcept { return m_username; }
    const std::string& Username() const noexcept { return m_username; }
    void SetNumConnectionsTillCheck(size_t num) {
      asio::post(m_context,
        [this, num]() {
          m_connections_till_check = num;
        });
    }
    
    // When started, peer firstly connects to server and tells its IP address.
    // Then it binds its acceptor object to the endpoint used during the very
    // first connection. In such a way other users could find him in the
    // network
    bool Start(size_t freqReading = -1, bool waitIncoming = true) {
      std::cout << "Peer started\n";
      Message<CustomConnectionModes> hiToServer;
      hiToServer.Header().Id() = CustomConnectionModes::Initialize;
      for (auto c : m_username)
        hiToServer << c;
      auto conn = std::make_unique<ClientConnection>(m_context, asio::ip::tcp::socket(m_context));
      asio::error_code ec;
      asio::ip::tcp::resolver resolver(m_context);
      auto endpoints = resolver.resolve("127.0.0.1", "60000", ec);
      if (ec) {
        return false;
      }
      conn->Request(endpoints, std::move(hiToServer));
      m_work_guard = std::make_unique<work_guard_t>(m_context.get_executor());
      m_asio = std::thread([this]() {
        m_context.run();
      });
      conn->GetMyIP(m_host, m_port);
      std::cout << m_host << m_port << '\n';
      conn->Wait();
      asio::ip::tcp::endpoint endpoint(asio::ip::make_address(m_host, ec), m_port);
      if (ec) {
        return false;
      }
      m_acceptor.open(endpoint.protocol());
      m_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
      m_acceptor.bind(endpoint);
      m_acceptor.listen();
      WaitForConnections();
      m_proc = std::thread([this, freqReading, waitIncoming]() {
        while (!m_context.stopped()) {
          Update(freqReading, waitIncoming);
        }
      });
      return true;
    }

    void Stop() {
      try {
        m_context.stop();
        if (m_asio.joinable())
          m_asio.join();
        if (m_proc.joinable())
          m_proc.join();
        for (auto& peer : m_connections) {
          peer->Disconnect();
          peer.reset();
        }
        m_connections.clear();
      } catch (const std::exception& e) {
        std::cerr << "Peer cannot be stopped correctly :" << e.what() << '\n';
      }
    }
            
    void WaitForConnections() { 
      m_acceptor.async_accept(
        [this](std::error_code ec, asio::ip::tcp::socket socket) {
          if (!ec) {
            std::cout << "New peer trying to connect :\n" << socket.remote_endpoint() << '\n';
            auto peer = std::make_shared<Connection<T>>(m_context, std::move(socket), m_queue);
            if (AcceptConnection(peer)) {
              NewConnection(std::move(peer))->Accept();
            } else {
              std::cout << "Connection denied\n";
            }
          } else {
            std::cout << "Connection error : " << ec.message() << '\n';
          }
          WaitForConnections();
        });
    }
    
    void ConnectTo(std::string username) {
      Message<CustomConnectionModes> hi;
      hi.Header().Id() = CustomConnectionModes::AskAddress;
      for (auto c : m_username)
        hi << c;
      auto conn = std::make_unique<ClientConnection>(m_context, asio::ip::tcp::socket(m_context), username);
      asio::error_code ec;
      asio::ip::tcp::resolver resolver(m_context);
      auto endpoints = resolver.resolve("127.0.0.1", "60000", ec);
      std::string host;
      std::uint16_t port; 
      if (!ec) {
        conn->Request(endpoints, std::move(hi));
        conn->Wait();
        conn->GetFriendIP(host, port);
      }
      conn.reset();
      auto peer = std::make_shared<Connection<T>>(m_context, asio::ip::tcp::socket(m_context), m_queue);
      endpoints = resolver.resolve(host, std::to_string(port), ec);
      if (!ec) {
        asio::post(m_context,
          [this, peer, endpoints, hi]() {
            NewConnection(peer)->Request(endpoints, std::move(hi));
          });
      }
    }

    void DisconnectFrom(size_t peer_id) {
      asio::post(m_context,
        [this, peer_id]() {
          if (peer_id < m_connections.size()) {
            auto& peer = m_connections[peer_id];
            if (peer && peer->IsConnected()) {
              peer->Disconnect();
            }
            if (m_connections.size() >= m_connections_till_check)
              EraseConnections();
            ProcessDisconnection(peer);
          } else {
            std::cerr << "Connection #" << peer_id << " doesn't exist\n";
          }
      });
    }

    void SendMessage(const size_t reciever_id, const Message<T>& msg) {
      asio::post(m_context,
        [this, reciever_id, msg]() {
          if (reciever_id < m_connections.size()) {
            auto& peer = m_connections[reciever_id];
            if (peer && peer->IsConnected()) {
              peer->SendMessage(msg);
            } else {
              if (m_connections.size() >= m_connections_till_check)
                EraseConnections();
              ProcessDisconnection(peer);
            }
          } else {
            std::cerr << "Connection #" << reciever_id << " doesn't exist\n";
          }
      });
    }

    //  User can change this if needed
    virtual void ProcessUI(char command = 'h') {
      while (std::cin >> command) {
        switch (command) {
        case 'h': std::cout << "Command list:\n";
                  std::cout << "\t h \t - display this message\n";
                  std::cout << "\t c \t - connect to other peer\n";
                  std::cout << "\t s \t - send some message\n";
                  std::cout << "\t x \t - exit\n";
                  break;
        case 'c': ConnectTo(UIFactory<std::string>("user name"));
                  break;
        case 'd': DisconnectFrom(UIFactory<size_t>("local peer id"));
                  break;
        case 's': SendMessage(UIFactory<size_t>("reciever local id"),
                              WriteMessage());
                  break;
        case 'x': Stop();
                  std::exit(EXIT_SUCCESS);
                  break;
        default:  std::cout << "Invalid command\n";
                  break;
        }
      }
    }
  };

}
