#pragma once
#include <connection.hpp>

namespace net {

  using UserMap = std::unordered_map<std::string, IP>; 
 
  class Server;
  
  // Server only waits for connections and provides info about user IPs
  // (extremely unsafe!!!)
  class ServerConnection : public PassiveConnection<CustomConnectionModes> {

    Server * m_server;

    std::string GetTempString() {
      std::string result;
      for (auto c : m_passive.Body())
        result.push_back(c);
      return result;
    }
    
   protected:

    virtual bool OnAccept(CustomConnectionModes type) override final;

   public:

    ServerConnection(asio::io_context& context,
                     asio::ip::tcp::socket&& socket,
                     Server * server)
      : BaseConnection(context, std::move(socket))
      , m_server(server) {}
  };

  class Server {
   
    UserMap m_users{};
    std::thread m_asio{};
    asio::io_context m_context{};
    std::unique_ptr<ServerConnection> m_connection{nullptr};
    asio::ip::tcp::acceptor m_acceptor;

   public:
    
    explicit Server()
      : m_acceptor(m_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 60000)) {}
  
    virtual ~Server() { Stop(); }

    UserMap& Users() noexcept { return m_users; }
    const UserMap& Users() const noexcept { return m_users; }

    bool Start() {
      try {
        std::cout << "Server started\n";
        WaitForConnections();
        m_asio = std::thread([this]() {
          m_context.run();
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
      } catch (const std::exception& e) {
        std::cerr << "Peer cannot be stopped correctly :" << e.what() << '\n';
      }
    }

    void WaitForConnections() {
      m_acceptor.async_accept(
        [this](std::error_code ec, asio::ip::tcp::socket socket) {
          if (!ec) {
            m_connection = std::make_unique<ServerConnection>(m_context, std::move(socket), this);
            m_connection->Accept();
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
        case 'x': Stop();
                  std::exit(EXIT_SUCCESS);
                  break;
        default:  std::cout << "Invalid command\n";
                  break;
        }
      }
    }
  };

  inline bool ServerConnection::OnAccept(CustomConnectionModes type) {
    bool ok = true;
    std::string username = GetTempString();
    auto it = m_server->Users().find(username);
    if (type == CustomConnectionModes::AskAddress) {
      if (it == m_server->Users().end()) {
        std::cerr << "User with name " << username << " is unregistered\n";
        ok = false;
      } else {
        asio::async_read(m_socket, asio::buffer(&m_passive.Header(), sizeof(MessageHeader<CustomConnectionModes>)),
          [this, it](std::error_code ec, size_t length) {
            if (!ec && m_passive.Header().Size() > 0) {
              asio::async_read(m_socket, asio::buffer(m_passive.Body().data(), m_passive.Body().size()),
                [this, it](std::error_code ec, size_t length) {
                  if (!ec) {
                    auto it = m_server->Users().find(GetTempString());
                    if (it == m_server->Users().end()) {
                      std::cerr << "User is unregistered\n";
                    } else {
                      asio::async_write(m_socket, asio::buffer(it->second.first),
                        [this, it](std::error_code ec, size_t length) {
                          asio::async_write(m_socket, asio::buffer(&(it->second.second), 2),
                            [this](std::error_code ec, size_t length) {
                              m_server->WaitForConnections();
                            });
                        });
                    }
                  } else {
                    std::cout << "Accept fail\n";
                    DumpEndpoints();
                    m_socket.close();
                  }
                });
            } else {
              std::cout << "Accept fail\n";
              DumpEndpoints();
              m_socket.close();
            }
          });
      }
    } else if (type == CustomConnectionModes::Initialize) {
      if (it != m_server->Users().end()) {
        std::cerr << "User with name " << username << " is already registered\n";
        ok = false;
      } else {
        std::cout << "New user : " << username << '\n';
        m_server->Users().emplace(std::make_pair(
          std::move(username),
          std::make_pair(
            IPFromString(m_socket.remote_endpoint().address().to_v4().to_string()),
            std::uint16_t(m_socket.remote_endpoint().port())
          )
        ));
      }
      m_server->WaitForConnections();
    } else {
      ok = false;
      m_server->WaitForConnections();
    }
    return ok;
  }

}
