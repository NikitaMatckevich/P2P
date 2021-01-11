#include <common.hpp>

namespace net {

  class CustomServerConnection : public ServerConnection<ServerMsgTypes> {
    protected:
    
  };

  using UserMap = std::unordered_map<
    std::string,
    std::pair< std::array<std::uint8_t, 4>, std::uint16_t >
  >; 
  
  class Server {
   
    UserMap m_users{};
    std::thread m_asio{};
    asio::io_context m_context{};
    asio::ip::tcp::acceptor m_acceptor;
    asio::ip::tcp::socket m_socket; 
    
   public:
    
    explicit Server()
      : m_acceptor(m_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 60000))
      , m_socket(m_context) {}
  
    virtual ~Server() { Stop(); }

    UserMap& Users() noexcept { return m_users; }
    const UserMap& Users() const noexcept { return m_users; }

    bool Start() {
      try {
        std::cout << "Peer started\n";
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

    std::array<std::uint8_t, 4> ParseIP(const asio::ip::tcp::endpoint& ep) {
      std::array<std::uint8_t, 4> res;
      std::string address = ep.address().to_v4().to_string();
      std::string_view view(address);
      for (size_t i = 0; i < 4; i++) {
        std::uint8_t val = 0;
        auto [ptr, ec] = std::from_chars(view.begin(), view.end(), val);
        if (ec == std::errc()) {
          res[i] = val;
          if (ptr != view.end())
            view = std::string_view(ptr + 1);
        }
        else {
          std::cerr << "Server cannot parse the address " << address << '\n';
          break;
        }
      }
      for (int i = 0; i < 4; i++) 
        std::cout << (int)res[i] << '.';
      std::cout << '\n';
      return res;
    }
            
    void WaitForConnections() {
      m_acceptor.async_accept(
        [this](std::error_code ec, asio::ip::tcp::socket socket) {
          if (!ec) {
            auto conn = std::unique_ptr<ServerConnection<T>>(m_context, std::move(socket), this);
            if (conn->IsConnected()) {
              conn->Accept();
              conn->Disconnect();
            }
          }
          WaitForConnections();
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

}

int main() {        
  net::Server server;
  server.Start();
  server.ProcessUI();
  return 0;
}
