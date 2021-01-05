#pragma once
#include <message.hpp>
#include <tsqueue.hpp>
#include <connection.hpp>

namespace {
  template <class T>
  T UIFactory(std::string_view name) {
    T value;
    std::cout << "Please enter the " << name << '\n';
    std::cin >> value;
    return value;
  }
}

namespace net {
 
  template <class T>
  class Peer {
    
    ThreadSafeQueue<Message<T>> m_queue{};
    std::thread m_asio{};
    std::thread m_proc{};
    std::vector<std::shared_ptr<Connection<T>>> m_connections{};
    asio::io_context m_context{};
    asio::ip::tcp::acceptor m_acceptor;
    asio::ip::tcp::socket m_socket;

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

    void Update(size_t freqReading = -1, size_t tooMuchConnections = 1) {
      asio::post(m_context, [this, tooMuchConnections]() {
        if (m_connections.size() >= tooMuchConnections)
          EraseConnections();
      });
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
    
    explicit Peer(std::uint16_t port)
      : m_acceptor(m_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
      , m_socket(m_context) {}
    
    virtual ~Peer() { Stop(); }

    ThreadSafeQueue<Message<T>>& Incoming() noexcept { return m_queue; }
    const ThreadSafeQueue<Message<T>>& Incoming() const noexcept { return m_queue; }

    bool Start(size_t freqReading = -1, size_t tooMuchConnections = 1) {
      try {
        std::cout << "Peer started\n";
        WaitForConnections();
        m_asio = std::thread([this]() {
          this->m_context.run();
        });
        m_proc = std::thread([this, freqReading, tooMuchConnections]() {
          while (true) {
            this->Update(freqReading, tooMuchConnections);
          }
        });
      }
      catch (std::exception& e) {
        std::cerr <<  "Peer exception : " << e.what() << '\n';
        return false;
      }
      return true;
    }

    void Stop() {
      for (auto& peer : m_connections) {
        peer->Disconnect();
        peer.reset();
      }
      m_context.stop();
      if (m_proc.joinable())
        m_proc.join();
      if (m_asio.joinable())
        m_asio.join();
      m_connections.clear();
    }
            
    void WaitForConnections() {
      m_acceptor.async_accept(
        [this](std::error_code ec, asio::ip::tcp::socket socket) {
          if (!ec) {
            std::cout << "New peer trying to connect :\n" << socket.remote_endpoint() << '\n';
            auto connection = std::make_shared<Connection<T>>(m_context, std::move(socket), m_queue);
            if (AcceptConnection(connection)) {
              NewConnection(std::move(connection))->Accept();
            } else {
              std::cout << "Connection denied\n";
            }
          } else {
            std::cout << "Connection error : " << ec.message() << '\n';
          }
          WaitForConnections();
        });
    }
    
    void ConnectTo(const std::string& host, std::uint16_t port) {
      asio::post(m_context,
        [this, host, port]() {
          asio::error_code ec;
          auto connection = std::make_shared<Connection<T>>(m_context,
            asio::ip::tcp::socket(m_context),
            m_queue);
          asio::ip::tcp::resolver resolver(m_context);
          auto endpoints = resolver.resolve(host, std::to_string(port), ec);
          if (!ec) {
            NewConnection(std::move(connection))->Propose(endpoints);
          } else {
            std::cerr << "Connection failed : " << ec.message() << '\n';
          }
        });
    }

    void DisconnectFrom(size_t peer_id) {
      asio::post(m_context,
        [this, peer_id]() {
          if (peer_id < m_connections.size()) {
            auto& peer = m_connections.at(peer_id);
            if (peer && peer->IsConnected()) {
              peer->Disconnect();
            }
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
            auto& peer = m_connections.at(reciever_id);
            if (peer && peer->IsConnected()) {
              peer->SendMessage(msg);
            } else {
              ProcessDisconnection(peer);
            }
          } else {
            std::cerr << "Connection #" << reciever_id << " doesn't exist\n";
          }
      });
    }
    
    virtual void ProcessUI(char command = 'h') {
      do {
        switch (command) {
        case 'h': std::cout << "Command list:\n";
                  std::cout << "\t h \t - display this message\n";
                  std::cout << "\t c \t - connect to other peer\n";
                  std::cout << "\t s \t - send some message\n";
                  std::cout << "\t x \t - exit\n";
                  break;
        case 'c': ConnectTo(UIFactory<std::string>("user id"),
                            UIFactory<std::uint16_t>("port number"));
                  break;
        case 's': SendMessage(UIFactory<size_t>("reciever local id"),
                              WriteMessage());
                  break;
        default:  std::cout << "Invalid command\n";
                  break;
        }
      } while (std::cin >> command);
    }
  };

}
