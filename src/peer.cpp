#include <peer.hpp>

enum class CustomMessageTypes : std::uint32_t {
  Accept,
  Deny,
  Ping,
  Say
};

class CustomPeer : public net::Peer<CustomMessageTypes> {
 public:
  explicit CustomPeer(std::string username)
    : net::Peer<CustomMessageTypes>(std::move(username)) {}

  virtual void ProcessMessage(net::Message<CustomMessageTypes>& msg) override final {
    std::cout << msg;
    int value;
    msg >> value;
    std::cout << " Value : " << value << '\n';
  }

  virtual net::Message<CustomMessageTypes> WriteMessage() override final {    
    net::Message<CustomMessageTypes> msg;
    msg.Header().Id() = CustomMessageTypes::Say;
    msg << 42;
    return msg;
  }
};

int main() {
  std::cout << "Please, enter your username:\n";
  std::string username;
  std::cin >> username;
  CustomPeer peer(username);
  peer.Start(100, 20);
  peer.ProcessUI();
  return 0;
}
