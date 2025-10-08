#pragma once

#include <algorithm>
#include <array>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <openssl/ssl.h>
#include <radix_relay/concepts/transport.hpp>
#include <random>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <variant>

namespace radix_relay::nostr {

class Transport
{
private:
  bool connected_ = false;
  std::function<void(std::span<const std::byte>)> message_callback_;

  boost::asio::io_context io_context_;
  boost::asio::ssl::context ssl_context_;
  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket_;
  std::thread io_thread_;
  std::array<char, 8192> read_buffer_;
  std::string partial_message_;

  std::string host_;
  std::string port_;
  std::string path_;

  auto parse_url(const std::string_view address) -> void
  {
    std::string addr_str(address);

    port_ = "443";
    path_ = "/";

    if (addr_str.starts_with("ws://")) {
      throw std::runtime_error("Insecure WebSocket (ws://) not supported. Use wss:// for security.");
    } else if (addr_str.starts_with("wss://")) {
      addr_str = addr_str.substr(6);
      port_ = "443";
    } else if (!addr_str.starts_with("http")) {
      // Default to secure WebSocket
      port_ = "443";
    }

    auto slash_pos = addr_str.find('/');
    if (slash_pos != std::string::npos) {
      host_ = addr_str.substr(0, slash_pos);
      path_ = addr_str.substr(slash_pos);
    } else {
      host_ = addr_str;
    }

    auto colon_pos = host_.find(':');
    if (colon_pos != std::string::npos) {
      port_ = host_.substr(colon_pos + 1);
      host_.resize(colon_pos);
    }
  }

  static auto generate_websocket_key() -> std::string
  {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::string key;
    key.reserve(24);

    for (int i = 0; i < 16; ++i) { key += static_cast<char>(dis(gen)); }

    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;

    for (size_t i = 0; i < key.length(); i += 3) {
      int group = (key[i] << 16);
      if (i + 1 < key.length()) group |= (key[i + 1] << 8);
      if (i + 2 < key.length()) group |= key[i + 2];

      result += chars[(group >> 18) & 63];
      result += chars[(group >> 12) & 63];
      result += chars[(group >> 6) & 63];
      result += chars[group & 63];
    }

    return result;
  }

  auto perform_handshake() -> void
  {
    std::string key = generate_websocket_key();

    std::ostringstream request;
    request << "GET " << path_ << " HTTP/1.1\r\n";
    request << "Host: " << host_ << "\r\n";
    request << "Upgrade: websocket\r\n";
    request << "Connection: Upgrade\r\n";
    request << "Sec-WebSocket-Key: " << key << "\r\n";
    request << "Sec-WebSocket-Version: 13\r\n";
    request << "\r\n";

    std::string request_str = request.str();
    boost::asio::write(ssl_socket_, boost::asio::buffer(request_str));

    boost::asio::streambuf response;
    boost::asio::read_until(ssl_socket_, response, "\r\n\r\n");

    std::istream response_stream(&response);
    std::string http_version;
    unsigned int status_code;
    std::string status_message;

    response_stream >> http_version >> status_code;
    std::getline(response_stream, status_message);

    if (status_code != 101) {
      throw std::runtime_error("WebSocket handshake failed with status: " + std::to_string(status_code));
    }

    connected_ = true;
  }

  static auto encode_websocket_frame(const std::span<const std::byte> payload) -> std::string
  {
    std::string frame;

    frame.push_back(static_cast<char>(0x81));

    size_t payload_len = payload.size();

    if (payload_len < 126) {
      frame.push_back(static_cast<char>(0x80 | payload_len));
    } else if (payload_len < 65536) {
      frame.push_back(static_cast<char>(0x80 | 126));
      frame.push_back(static_cast<char>((payload_len >> 8) & 0xFF));
      frame.push_back(static_cast<char>(payload_len & 0xFF));
    } else {
      frame.push_back(static_cast<char>(0x80 | 127));
      for (int i = 7; i >= 0; --i) { frame.push_back(static_cast<char>((payload_len >> (i * 8)) & 0xFF)); }
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::array<char, 4> mask_key;
    std::generate(mask_key.begin(), mask_key.end(), [&gen]() { return static_cast<char>(gen() & 0xFF); });
    frame.append(mask_key.data(), 4);

    for (size_t i = 0; i < payload_len; ++i) {
      char masked_byte = static_cast<char>(payload[i]) ^ mask_key[i % 4];
      frame.push_back(masked_byte);
    }

    return frame;
  }

  auto decode_websocket_frame(const std::string &data) -> void
  {
    if (data.length() < 2) return;

    size_t pos = 0;

    while (pos + 1 < data.length()) {
      char first_byte = data[pos++];
      char second_byte = data[pos++];

      int opcode = first_byte & 0x0F;
      bool masked = (second_byte & 0x80) != 0;
      size_t payload_len = second_byte & 0x7F;

      if (payload_len == 126) {
        if (pos + 2 > data.length()) return;
        payload_len = (static_cast<size_t>(static_cast<unsigned char>(data[pos])) << 8)
                      | static_cast<size_t>(static_cast<unsigned char>(data[pos + 1]));
        pos += 2;
      } else if (payload_len == 127) {
        if (pos + 8 > data.length()) return;
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
          payload_len =
            (payload_len << 8) | static_cast<size_t>(static_cast<unsigned char>(data[pos + static_cast<size_t>(i)]));
        }
        pos += 8;
      }

      if (masked) { pos += 4; }

      if (pos + payload_len > data.length()) return;

      if (opcode == 0x1) {
        std::string payload = data.substr(pos, payload_len);
        partial_message_ += payload;

        bool fin = (first_byte & 0x80) != 0;
        if (fin && message_callback_) {
          auto bytes = reinterpret_cast<const std::byte *>(partial_message_.data());
          std::span<const std::byte> byte_span(bytes, partial_message_.size());
          message_callback_(byte_span);
          partial_message_.clear();
        }
      }

      pos += payload_len;
    }
  }

  auto start_read() -> void
  {
    ssl_socket_.async_read_some(
      boost::asio::buffer(read_buffer_), [this](const boost::system::error_code &error, std::size_t bytes_transferred) {
        handle_read(error, bytes_transferred);
      });
  }

  auto handle_read(const boost::system::error_code &error, std::size_t bytes_transferred) -> void
  {
    if (!error) {
      std::string received_data(read_buffer_.data(), bytes_transferred);
      decode_websocket_frame(received_data);
      start_read();
    } else {
      connected_ = false;
    }
  }

public:
  Transport() : ssl_context_(boost::asio::ssl::context::sslv23), ssl_socket_(io_context_, ssl_context_), read_buffer_{}
  {
    ssl_context_.set_default_verify_paths();
    ssl_context_.set_verify_mode(boost::asio::ssl::verify_peer);
  }

  ~Transport() { disconnect(); }

  [[nodiscard]] auto io_context() -> boost::asio::io_context & { return io_context_; }

  auto connect(const std::string_view address) -> void
  {
    try {
      parse_url(address);

      boost::asio::ip::tcp::resolver resolver(io_context_);
      auto endpoints = resolver.resolve(host_, port_);

      // Connect the underlying TCP socket
      boost::asio::connect(ssl_socket_.lowest_layer(), endpoints);

      // Set SNI hostname for SSL verification
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
      if (!SSL_set_tlsext_host_name(ssl_socket_.native_handle(), host_.c_str())) {
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
        throw std::runtime_error("Failed to set SNI hostname");
      }

      // Perform SSL handshake
      ssl_socket_.handshake(boost::asio::ssl::stream_base::client);

      // Perform WebSocket handshake
      perform_handshake();

      start_read();

      io_thread_ = std::thread([this]() { io_context_.run(); });

    } catch (const std::exception &e) {
      throw std::runtime_error("Failed to connect to " + std::string(address) + ": " + e.what());
    }
  }

  auto send(const std::span<const std::byte> payload) -> void
  {
    if (!connected_) { throw std::runtime_error("Not connected"); }

    std::string frame = encode_websocket_frame(payload);

    boost::asio::post(io_context_, [this, frame = std::move(frame)]() {
      boost::asio::async_write(ssl_socket_,
        boost::asio::buffer(frame),
        [](const boost::system::error_code & /*error*/, std::size_t /*bytes_transferred*/) {});
    });
  }

  auto register_message_callback(std::function<void(std::span<const std::byte>)> callback) -> void
  {
    message_callback_ = std::move(callback);
  }

  auto disconnect() -> void
  {
    if (connected_) {
      connected_ = false;
      io_context_.stop();

      if (io_thread_.joinable()) { io_thread_.join(); }

      if (ssl_socket_.lowest_layer().is_open()) { ssl_socket_.lowest_layer().close(); }
    }
  }
};

static_assert(radix_relay::concepts::Transport<Transport>);

}// namespace radix_relay::nostr
