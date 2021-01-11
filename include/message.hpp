#pragma once
#include <common.hpp>

namespace net {

  template <class T>
  class Connection;

  template <class T>
  class MessageHeader {
    T id{};
    std::uint32_t size{sizeof(MessageHeader<T>)};
   public:
    T  Id() const noexcept { return id; }
    T& Id() noexcept { return id; }
    std::uint32_t  Size() const noexcept { return size; }
    std::uint32_t& Size() noexcept { return size; }
  };

  template <class T>
  class Message {

    std::shared_ptr<Connection<T>> m_sender;
    MessageHeader<T> m_header{};
    std::vector<std::uint8_t> m_body{};

   public:

    explicit Message(std::shared_ptr<Connection<T>> sender = nullptr)
      : m_sender(std::move(sender)) {}
  
    size_t Size() const noexcept {
      return m_body.size() + sizeof(MessageHeader<T>);
    }

    const std::shared_ptr<Connection<T>>& Sender() const noexcept {
      return m_sender;
    }
    std::shared_ptr<Connection<T>>& Sender() noexcept {
      return m_sender;
    }
    const MessageHeader<T>& Header() const noexcept {
      return m_header;
    }
    MessageHeader<T>& Header() noexcept {
      return m_header;
    }
    const std::vector<std::uint8_t>& Body() const noexcept {
      return m_body;
    }
    std::vector<std::uint8_t>& Body() noexcept {
      return m_body;
    }
    
    template <class DataType>
    Message<T>& operator<<(const DataType& data) {
      static_assert(std::is_standard_layout<DataType>::value,
        "Data is too complex to be parsed");
      size_t prev = m_body.size();
      m_body.resize(prev + sizeof(DataType));
      std::memcpy(m_body.data() + prev, &data, sizeof(DataType));
      m_header.Size() = m_body.size();
      return *this;
    }
    
    template <class DataType>
    Message<T>& operator>>(DataType& data) {
      static_assert(std::is_standard_layout<DataType>::value,
        "Data is too complex to be parsed");
      size_t next = m_body.size() - sizeof(DataType);
      std::memcpy(&data, m_body.data() + next, sizeof(DataType));
      m_body.resize(next);
      m_header.Size() = m_body.size();
      return *this;
    }
  };

  template <class T>
  std::ostream& operator<<(std::ostream& out, const Message<T>& msg) {
    out << "Id: " << std::uint32_t(msg.Header().Id()) << "  Size: " << msg.Header().Size();
    return out;
  }

  enum class ServerMessageTypes : std::uint32_t {
    EstablishConnection,
    AskUserAddress
  };

  using ServerMessage = Message<ServerMessageTypes>;
}
