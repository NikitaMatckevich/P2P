#pragma once
#include <message.hpp>
#include <tsqueue.hpp>
#include <connection.hpp>

namespace net {
 
  template <class PeerProtocol, class ServerProtocol>
  class Peer {
    
   public:

    using PConnection = PeerConnection<PeerProtocol>;
    using PConnectionActive = ActiveConnection<PeerProtocol, true, PeerConnection>;
    using PConnectionPassive = PassiveConnection<PeerProtocol, true, PeerConnection>;
    using SConnection = ClientConnection<ServerProtocol>;
    using PMessage = Message<PeerProtocol>;
    using SMessage = Message<ServerProtocol>;

   private:

    ThreadSafeQueue<PMessage> m_queue{};

    std::thread m_asio{};
    std::thread m_proc{};

    std::vector<std::shared_ptr<PConnection>> m_connections{};
    size_t m_connections_till_check{1};

    asio::io_context m_context{};
    asio::ip::tcp::acceptor m_acceptor;

    std::string m_username;
    std::string m_host;
    std::uint16_t m_port;

    std::shared_ptr<PConnection> NewConnection(std::shared_ptr<PConnection> peer) {
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

    void EraseConnections(std::shared_ptr<PConnection> peer = nullptr) {
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
     
    std::tuple<std::string, std::uint16_t> GetIPFromServer(std::string username) {
      auto [socket, ec] = ConnectToServer();
      std::string host;
      std::uint16_t port = 0;
      if (!ec) {
        using namespace asio;
        std::array<std::uint8_t, 4> ip;
        asio::read(socket, buffer(ip), transfer_exactly(4), ec);
        asio::read(socket, buffer(&port, 2), transfer_exactly(2), ec);
        if (!ec) {
          host = std::to_string(ip[0]);
          for (size_t i = 1; i < 4; i++) {
            host.push_back('.');
            host += std::to_string(ip[i]);
          }
        } else {
          std::cerr << "Cannot parse IP of " << username << " : ";
          std::cerr << ec.message() << '\n';
        }
      }
      socket.close();
      return std::make_tuple(std::move(host), std::move(port));
    }
    
   protected:

    virtual PMessage PeerMessage() = 0;
    virtual SMessage ServerMessage() = 0;
    virtual void ProcessMessage(PMessage& msg) = 0;
    
    virtual bool AcceptConnection(std::shared_ptr<PConnection> peer) {
      std::cout << "Attention: this function accepts any connection\n";
      return true;
    }
    virtual void ProcessDisconnection(std::shared_ptr<PConnection> peer) {
      std::cout << "Disconnecting\n";
    }
    
   public:
    
    explicit Peer(std::string username, SMessage hiToServer)
      : m_acceptor(m_context)
      , m_username(std::move(username))
    {
      auto conn = std::make_unique<SConnection>(m_context, asio::ip::tcp::socket(m_context));
      asio::error_code ec;
      asio::ip::tcp::resolver resolver;
      auto endpoints = resolver.resolve("127.0.0.1", "60000", ec);
      if (!ec) {
        conn->Request(endpoints, std::move(hiToServer));
        conn->GetMyIP(m_host, m_port);
        conn->Disconnect();
        asio::ip::tcp::endpoint endpoint(asio::ip::make_address(m_host, ec), m_port);
        if (!ec) {
          m_acceptor.open(endpoint.protocol());
          m_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
          m_acceptor.bind(endpoint);
          m_acceptor.listen();
        }
      }
      if (ec) {
        std::cerr << "Error while creating server connection : ";
        std::cerr << ec.message() << '\n';
      }
    }

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

    bool Start(size_t freqReading = -1, bool waitIncoming = true) {
      try {
        std::cout << "Peer started\n";
        WaitForConnections();
        m_asio = std::thread([this]() {
          m_context.run();
        });
        m_proc = std::thread([this, freqReading, waitIncoming]() {
          while (!m_context.stopped()) {
            Update(freqReading, waitIncoming);
          }
        });
      } catch (const std::exception& e) {
        std::cerr <<  "Peer cannot start : " << e.what() << '\n';
        return false;
      }
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
            auto conn = std::make_shared<PConnectionPassive>(m_context, std::move(socket), m_queue);
            if (AcceptConnection(conn)) {
              AddToVector(conn);
              conn->Accept();
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
      asio::post(m_context,
        [this, username]() {
          asio::error_code ec;
          auto conn = std::make_shared<PConnectionActive>(m_context,
            asio::ip::tcp::socket(m_context),
            m_queue);
          std::string host;
          std::uint16_t port;
          conn->GetFriendIP(username, host, port, ec);
          asio::ip::tcp::resolver resolver(m_context);
          auto endpoints = resolver.resolve(host, std::to_string(port), ec);
          asio::ip::tcp::resolver::iterator it = endpoints;
          asio::ip::tcp::resolver::iterator end;
          if (!ec) {
            AddToVector(conn);
            conn->Request(endpoints, ConnectionMessage());
          } else {
            std::cerr << "Connection failed : " << ec.message() << '\n';
          }
        });
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
