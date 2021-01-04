#pragma once
#include <message.hpp>
#include <tsqueue.hpp>

namespace net {
  
  template <class Id>
  class Connection : public std::enable_shared_from_this<Connection<Id>> {

    asio::io_context& m_context;
    asio::ip::tcp::socket m_socket;

    ThreadSafeQueue<Message<Id>>& m_recv;
    ThreadSafeQueue<Message<Id>>  m_send{};
    Message<Id> m_temporary{};

    void DumpEndpoints() const {
      std::cout << "Local endpoint:\n";
      std::cout << m_socket.local_endpoint() << '\n';
      std::cout << "Remote endpoint:\n";
      std::cout << m_socket.remote_endpoint() << '\n';
    }

    void AddToInbox() {
      m_recv.Push(m_temporary);
      m_recv.Front().Sender() = this->shared_from_this();
      ReadHeader();
    }

    void ReadHeader() {
      asio::async_read(m_socket, asio::buffer(&m_temporary.Header(), sizeof(MessageHeader<Id>)),
        [this](std::error_code ec, size_t length) {
          if (!ec) {
            if (m_temporary.Header().Size() > 0) {
              m_temporary.Body().resize(m_temporary.Header().Size());
              ReadBody();
            } else {
              AddToInbox();
            }
          } else {
            std::cout << "Read Header fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }

    void ReadBody() {
      asio::async_read(m_socket, asio::buffer(m_temporary.Body().data(), m_temporary.Header().Size()),
        [this](std::error_code ec, size_t length) {
          if (!ec) {
            AddToInbox();
          } else {
            std::cout << "Read Body fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }

    void WriteHeader() {
      asio::async_write(m_socket, asio::buffer(&m_send.Back().Header(), sizeof(MessageHeader<Id>)),
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
            std::cout << "Read Body fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }

    void WriteBody() {
      asio::async_write(m_socket, asio::buffer(m_send.Back().Body().data(), sizeof(MessageHeader<Id>)),
        [this](std::error_code ec, size_t length) {
          if (!ec) {
            m_send.Pop();
            if (!m_send.Empty()) {
              WriteHeader();
            }
          } else {
            std::cout << "Read Body fail\n";
            DumpEndpoints();
            m_socket.close();
          }
        });
    }

   public:
    Connection(asio::io_context& context_reference,
               asio::ip::tcp::socket&& socket,
               ThreadSafeQueue<Message<Id>>& owner_inbox_queue)
      : m_context(context_reference)
      , m_socket(std::move(socket))
      , m_recv(owner_inbox_queue) {}

    void Propose(const asio::ip::tcp::resolver::results_type& endpoints) {
      asio::async_connect(m_socket, endpoints,
        [this](std::error_code ec, asio::ip::tcp::endpoint endpoint) {
          if (!ec) {
            ReadHeader();
          } else {
            std::cout << "Proposition denied\n";
          }
        });
    }

    void Accept() {
      if (m_socket.is_open()) {
        ReadHeader();
      }
    }

    void Disconnect() {
      if (IsConnected()) {
        asio::post(m_context,
          [this](){
            this->m_socket.close();
          });
      }
    }

    bool IsConnected() const {
      return m_socket.is_open();
    }

    void SendMessage(const Message<Id>& msg) {
      asio::post(m_context,
        [this, msg]() {
          bool isWritingNow = !(this->m_send.Empty());
          this->m_send.Push(msg);
          if (!isWritingNow) {
            this->WriteHeader();
          }
        });
    }
  };

}
