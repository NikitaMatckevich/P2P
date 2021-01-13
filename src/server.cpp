#include <server.hpp>

int main () {
  net::Server server;
  server.Start();
  server.ProcessUI();
  return 0;
}
