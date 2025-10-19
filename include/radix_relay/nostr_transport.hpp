#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <cstddef>
#include <functional>
#include <iostream>
#include <openssl/ssl.h>
#include <radix_relay/concepts/transport.hpp>
#include <random>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace radix_relay::nostr {

class transport
{
public:
  using send_bytes_to_session_fn_t = std::function<void(std::vector<std::byte>)>;

private:
  bool connected_ = false;
  send_bytes_to_session_fn_t send_bytes_to_session_;

  boost::asio::io_context *io_context_;
  static constexpr size_t read_buffer_size = 8192;

  // Bitwise operation constants
  static constexpr size_t shift_18_bits = 18;
  static constexpr size_t shift_16_bits = 16;
  static constexpr size_t shift_12_bits = 12;
  static constexpr size_t shift_8_bits = 8;
  static constexpr size_t shift_6_bits = 6;
  static constexpr unsigned int base64_mask = 63;
  static constexpr unsigned int byte_mask = 0xFF;

  // WebSocket protocol constants
  static constexpr unsigned char ws_fin_text_frame = 0x81;
  static constexpr unsigned char ws_mask_bit = 0x80;
  static constexpr unsigned char ws_payload_16bit = 126;
  static constexpr unsigned char ws_payload_64bit = 127;
  static constexpr unsigned char ws_opcode_mask = 0x0F;
  static constexpr unsigned char ws_opcode_text = 0x1;
  static constexpr unsigned char ws_mask_flag = 0x80;
  static constexpr unsigned char ws_payload_mask = 0x7F;
  static constexpr size_t ws_small_payload_max = 126;
  static constexpr size_t ws_medium_payload_max = 65536;
  static constexpr size_t ws_min_frame_size = 2;
  static constexpr size_t ws_extended_16bit_size = 2;
  static constexpr size_t ws_extended_64bit_size = 8;
  static constexpr size_t ws_mask_size = 4;
  static constexpr int mask_key_size = 4;
  static constexpr int num_64bit_bytes = 7;

  boost::asio::ssl::context ssl_context_;
  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket_;
  std::array<char, read_buffer_size> read_buffer_{};
  std::string partial_message_;

  std::string host_;
  std::string port_;
  std::string path_;

  auto parse_url(const std::string_view address) -> void
  {
    std::string addr_str(address);

    port_ = "443";
    path_ = "/";

    static constexpr size_t wss_prefix_length = 6;

    if (addr_str.starts_with("ws://")) {
      throw std::runtime_error("Insecure WebSocket (ws://) not supported. Use wss:// for security.");
    }
    if (addr_str.starts_with("wss://")) {
      addr_str = addr_str.substr(wss_prefix_length);
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
    static constexpr int max_byte_value = 255;
    static constexpr int key_byte_count = 16;
    static constexpr int base64_reserve_size = 24;
    static constexpr int bits_per_group = 3;

    std::random_device random_dev;
    std::mt19937 gen(random_dev());
    std::uniform_int_distribution<> dis(0, max_byte_value);

    std::string key;
    key.reserve(base64_reserve_size);

    for (int i = 0; i < key_byte_count; ++i) { key += static_cast<char>(dis(gen)); }

    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;

    for (size_t i = 0; i < key.length(); i += bits_per_group) {
      auto byte0 = std::bit_cast<unsigned char>(key[i]);
      unsigned int group = static_cast<unsigned int>(byte0) << shift_16_bits;
      if (i + 1 < key.length()) {
        auto byte1 = std::bit_cast<unsigned char>(key[i + 1]);
        group |= static_cast<unsigned int>(byte1) << shift_8_bits;
      }
      if (i + 2 < key.length()) {
        auto byte2 = std::bit_cast<unsigned char>(key[i + 2]);
        group |= byte2;
      }

      result += chars[(group >> shift_18_bits) & base64_mask];
      result += chars[(group >> shift_12_bits) & base64_mask];
      result += chars[(group >> shift_6_bits) & base64_mask];
      result += chars[group & base64_mask];
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

    static constexpr unsigned int http_switching_protocols = 101;

    std::istream response_stream(&response);
    std::string http_version;
    unsigned int status_code{};
    std::string status_message;

    response_stream >> http_version >> status_code;
    std::getline(response_stream, status_message);

    if (status_code != http_switching_protocols) {
      throw std::runtime_error("WebSocket handshake failed with status: " + std::to_string(status_code));
    }

    connected_ = true;
  }

  static auto encode_websocket_frame(const std::span<const std::byte> payload) -> std::string
  {

    std::string frame;

    frame.push_back(std::bit_cast<char>(ws_fin_text_frame));

    size_t payload_len = payload.size();

    if (payload_len < ws_small_payload_max) {
      auto len_byte = static_cast<unsigned char>(payload_len);
      auto header_byte = static_cast<unsigned char>(ws_mask_bit | len_byte);
      frame.push_back(std::bit_cast<char>(header_byte));
    } else if (payload_len < ws_medium_payload_max) {
      auto header_byte = static_cast<unsigned char>(ws_mask_bit | ws_payload_16bit);
      frame.push_back(std::bit_cast<char>(header_byte));
      auto byte0 = static_cast<unsigned char>((payload_len >> shift_8_bits) & byte_mask);
      auto byte1 = static_cast<unsigned char>(payload_len & byte_mask);
      frame.push_back(std::bit_cast<char>(byte0));
      frame.push_back(std::bit_cast<char>(byte1));
    } else {
      auto header_byte = static_cast<unsigned char>(ws_mask_bit | ws_payload_64bit);
      frame.push_back(std::bit_cast<char>(header_byte));
      for (size_t i = num_64bit_bytes; i <= num_64bit_bytes; --i) {
        auto byte = static_cast<unsigned char>((payload_len >> (i * shift_8_bits)) & byte_mask);
        frame.push_back(std::bit_cast<char>(byte));
      }
    }

    std::random_device random_dev;
    std::mt19937 gen(random_dev());
    std::array<char, mask_key_size> mask_key{};
    std::ranges::generate(mask_key, [&gen]() -> char {
      auto byte = static_cast<unsigned char>(gen() & byte_mask);
      return std::bit_cast<char>(byte);
    });
    frame.append(mask_key.data(), 4);

    for (size_t i = 0; i < payload_len; ++i) {
      auto mask_index = i % mask_key_size;
      auto payload_byte = std::to_integer<unsigned char>(payload[i]);
      auto mask_byte = std::bit_cast<unsigned char>(mask_key.at(mask_index));
      auto masked = static_cast<unsigned char>(payload_byte ^ mask_byte);
      frame.push_back(std::bit_cast<char>(masked));
    }

    return frame;
  }

  auto decode_websocket_frame(const std::string &data) -> void
  {

    if (data.length() < ws_min_frame_size) { return; }

    size_t pos = 0;

    while (pos + 1 < data.length()) {
      auto first_byte = std::bit_cast<unsigned char>(data[pos++]);
      auto second_byte = std::bit_cast<unsigned char>(data[pos++]);

      unsigned int opcode = first_byte & ws_opcode_mask;
      bool masked = (second_byte & ws_mask_flag) != 0;
      size_t payload_len = second_byte & ws_payload_mask;

      if (payload_len == ws_payload_16bit) {
        if (pos + ws_extended_16bit_size > data.length()) { return; }
        auto byte0 = std::bit_cast<unsigned char>(data[pos]);
        auto byte1 = std::bit_cast<unsigned char>(data[pos + 1]);
        payload_len = (static_cast<size_t>(byte0) << shift_8_bits) | byte1;
        pos += ws_extended_16bit_size;
      } else if (payload_len == ws_payload_64bit) {
        if (pos + ws_extended_64bit_size > data.length()) { return; }
        payload_len = 0;
        for (size_t i = 0; i < ws_extended_64bit_size; ++i) {
          auto byte = std::bit_cast<unsigned char>(data[pos + i]);
          payload_len = (payload_len << shift_8_bits) | byte;
        }
        pos += ws_extended_64bit_size;
      }

      if (masked) { pos += ws_mask_size; }

      if (pos + payload_len > data.length()) { return; }

      if (opcode == ws_opcode_text) {
        std::string payload = data.substr(pos, payload_len);
        partial_message_ += payload;

        static constexpr unsigned char ws_fin_flag = 0x80;
        bool fin = (first_byte & ws_fin_flag) != 0;
        if (fin && send_bytes_to_session_) {
          std::vector<std::byte> bytes;
          bytes.resize(partial_message_.size());
          std::ranges::transform(
            partial_message_, bytes.begin(), [](char character) { return std::bit_cast<std::byte>(character); });
          send_bytes_to_session_(std::move(bytes));
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
  transport(boost::asio::io_context *io_context, send_bytes_to_session_fn_t send_bytes_to_session)
    : send_bytes_to_session_(std::move(send_bytes_to_session)), io_context_(io_context),
      ssl_context_(boost::asio::ssl::context::sslv23), ssl_socket_(*io_context_, ssl_context_)
  {
    ssl_context_.set_default_verify_paths();
    ssl_context_.set_verify_mode(boost::asio::ssl::verify_peer);
  }

  ~transport() { disconnect(); }

  transport(const transport &) = delete;
  auto operator=(const transport &) -> transport & = delete;
  transport(transport &&) = delete;
  auto operator=(transport &&) -> transport & = delete;

  auto connect(const std::string_view address) -> void
  {
    try {
      parse_url(address);

      boost::asio::ip::tcp::resolver resolver(*io_context_);
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

    } catch (const std::exception &e) {
      throw std::runtime_error("Failed to connect to " + std::string(address) + ": " + e.what());
    }
  }

  auto send(const std::span<const std::byte> payload) -> void
  {
    if (!connected_) { throw std::runtime_error("Not connected"); }

    std::string frame = encode_websocket_frame(payload);

    boost::asio::async_write(ssl_socket_,
      boost::asio::buffer(frame),
      [](const boost::system::error_code & /*error*/, std::size_t /*bytes_transferred*/) {});
  }

  auto disconnect() -> void
  {
    if (connected_) {
      connected_ = false;

      if (ssl_socket_.lowest_layer().is_open()) { ssl_socket_.lowest_layer().close(); }
    }
  }
};

// TODO: Update transport concept for strand-based architecture
// static_assert(radix_relay::concepts::transport<transport>);

}// namespace radix_relay::nostr
