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

  virtual bool AcceptConnection(std::shared_ptr<net::Connection<CustomMessage>> peer) override final {
    std::cout << "Accepting this guy!\n";
    return true;
  }
  virtual void ProcessDisconnection(std::shared_ptr<net::Connection<CustomMessage>> peer) override final {
    std::cout << "This guy disconnected!\n";
    peer->Disconnect();
    peer.reset();
  }
  virtual void ProcessMessage(const net::Message<CustomMessage>& msg) override final {
    if (msg.Sender() && msg.Sender()->IsConnected()) {
      msg.Sender()->SendMessage(msg); // resend back
    }
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
    return std::move(msg);
  }
};

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << "Please specify exactly one entry port\n";
    return -1;
  }
  CustomPeer peer(std::stoi(argv[1]));
  peer.Start();
  while (true) {
    peer.Update();
  }
  return 0;
}
