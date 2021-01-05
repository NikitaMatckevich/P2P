#include <peer.hpp>

enum class CustomMessage : uint32_t {
  Accept,
  Deny,
  Ping,
  Say
};

class CustomPeer : public net::Peer<CustomMessage> {
 public:
  explicit CustomPeer(std::uint16_t port) : net::Peer<CustomMessage>(port) {} 

  virtual void ProcessMessage(net::Message<CustomMessage>& msg) override final {
    std::cout << msg;
    int value;
    msg >> value;
    std::cout << " Value : " << value << '\n';
  }
  virtual net::Message<CustomMessage> WriteMessage() override final {
    net::Message<CustomMessage> msg;
    std::cout << "Please choose the message type\n";
    std::string type;
    std::cin >> type;
    if (type == "say") {
      msg.Header().Id() = CustomMessage::Say;
      msg << 42;
    }
    return msg;
  }
};

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << "Please specify exactly one entry port\n";
    return -1;
  }
  CustomPeer peer(std::stoi(argv[1]));
  peer.Start(100, 20);
  peer.ProcessUI();
  return 0;
}
