#pragma once
#include <message.hpp>
#include <tsqueue.hpp>
#include <connection.hpp>

namespace net {
 
  template <class T>
  class Peer {
    
    ThreadSafeQueue<Message<T>> m_queue;
    std::thread m_AsioThread{};
    std::thread m_UserThread{};
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

   protected:

    virtual bool AcceptConnection(std::shared_ptr<Connection<T>> peer) = 0;
    virtual void ProcessDisconnection(std::shared_ptr<Connection<T>> peer) = 0;
    virtual void ProcessMessage(const Message<T>& msg) = 0;

   public:
    
    explicit Peer(std::uint16_t port)
      : m_acceptor(m_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
      , m_socket(m_context)
    {}
    
    virtual ~Peer() {
      Stop();
    }

    ThreadSafeQueue<Message<T>>& Incoming() noexcept {
      return m_queue;
    }
    const ThreadSafeQueue<Message<T>>& Incoming() const noexcept {
      return m_queue;
    }

    bool Start() {
      try {
        std::cout << "Client started, press Y to connect to other users\n";
        WaitForConnections();
        m_AsioThread = std::thread([this]() {
          this->m_context.run();
        });
        m_UserThread = std::thread([this]() {
          char buttonPressed;
          while (std::cin >> buttonPressed) {
            if (buttonPressed == 'Y') {
              std::string name;
              std::cout << "Please enter the ID of the user you wish to call\n";
              std::cin >> name;
              std::uint16_t port;
              std::cout << "Please enter the port number\n";
              std::cin >> port;
              ConnectTo(name, port);
            }
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
      m_connections.clear();
      m_context.stop();
      if (m_AsioThread.joinable())
        m_AsioThread.join();
      if (m_UserThread.joinable())
        m_UserThread.join();
    }

    void Update(size_t maxMsg = -1) {
      for (size_t i = 0; i < maxMsg && !m_queue.Empty(); i++) {
        ProcessMessage(m_queue.Back());
        m_queue.Pop();
      }
      EraseConnections();
    }
        
    void WaitForConnections() {
      m_acceptor.async_accept(
        [this](std::error_code ec, asio::ip::tcp::socket socket) {
          if (!ec) {
            std::cout << "New peer trying to connect :\n" << socket.remote_endpoint() << '\n';
            auto connection = std::make_shared<Connection<T>>(m_context, std::move(socket), m_queue);
            if (AcceptConnection(connection)) {
              NewConnection(std::move(connection))->Accept();
              std::cout << "Connection with\n" << socket.remote_endpoint() << "\nstarted\n";
            } else {
              std::cout << "Connection with\n" << socket.remote_endpoint() << "\ndenied\n";
            }
          } else {
            std::cout << "Connection error : " << ec.message() << '\n';
          }
          WaitForConnections();
        });
    }
    
    bool ConnectTo(const std::string& host, const std::uint16_t port) {
      try {
        auto connection = std::make_shared<Connection<T>>(m_context,
          asio::ip::tcp::socket(m_context),
          m_queue);
        asio::ip::tcp::resolver resolver(m_context);
        auto endpoints = resolver.resolve(host, std::to_string(port));
        NewConnection(std::move(connection))->Propose(endpoints);
      }
      catch (std::exception& e) {
        std::cerr << "Peer exception : " << e.what() << std::endl;
        return false;
      }
      return true;
    }

    void DisconnectFrom(size_t peer_id) {
      auto& peer = m_connections.at(peer_id);
      if (peer && peer->IsConnected()) {
        peer->Disconnect();
      }
      ProcessDisconnection(std::move(peer));
    }

    void SendMessage(const size_t reciever_id, const Message<T>& msg) {
      auto& peer = m_connections.at(reciever_id);
      if (peer && peer->IsConnected()) {
        peer->SendMessage(msg);
      } else {
        ProcessDisconnection(std::move(peer));
      }
    }
  };

}
