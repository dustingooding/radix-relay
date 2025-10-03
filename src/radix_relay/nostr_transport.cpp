#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <iomanip>
#include <radix_relay/nostr_transport.hpp>
#include <random>
#include <sstream>
#include <stdexcept>

namespace radix_relay {

NostrTransport::NostrTransport() : socket_(io_context_) {}

NostrTransport::~NostrTransport() { disconnect(); }

auto NostrTransport::parse_url(const std::string_view address) -> void
{
  // Parse WebSocket URL like "ws://relay.damus.io" or "wss://relay.damus.io"
  std::string addr_str(address);

  // Default values
  port_ = "80";
  path_ = "/";

  // Check for ws:// or wss://
  if (addr_str.starts_with("ws://")) {
    addr_str = addr_str.substr(5);
    port_ = "80";
  } else if (addr_str.starts_with("wss://")) {
    addr_str = addr_str.substr(6);
    port_ = "443";// TODO: Add SSL support later
    throw std::runtime_error("WSS (secure WebSocket) not yet supported");
  } else if (!addr_str.starts_with("http")) {
    // Assume plain WebSocket if no protocol specified
    port_ = "80";
  }

  // Split host and path
  auto slash_pos = addr_str.find('/');
  if (slash_pos != std::string::npos) {
    host_ = addr_str.substr(0, slash_pos);
    path_ = addr_str.substr(slash_pos);
  } else {
    host_ = addr_str;
  }

  // Check for port in host
  auto colon_pos = host_.find(':');
  if (colon_pos != std::string::npos) {
    port_ = host_.substr(colon_pos + 1);
    host_ = host_.substr(0, colon_pos);
  }
}

auto NostrTransport::generate_websocket_key() -> std::string
{
  // Generate random 16-byte key for WebSocket handshake
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 255);

  std::string key;
  key.reserve(24);// Base64 of 16 bytes = 24 chars

  for (int i = 0; i < 16; ++i) { key += static_cast<char>(dis(gen)); }

  // Simple base64 encoding (for WebSocket key)
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

auto NostrTransport::perform_handshake() -> void
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
  boost::asio::write(socket_, boost::asio::buffer(request_str));

  // Read response
  boost::asio::streambuf response;
  boost::asio::read_until(socket_, response, "\r\n\r\n");

  std::istream response_stream(&response);
  std::string http_version;
  unsigned int status_code;
  std::string status_message;

  response_stream >> http_version >> status_code;
  std::getline(response_stream, status_message);

  if (status_code != 101) {
    throw std::runtime_error("WebSocket handshake failed with status: " + std::to_string(status_code));
  }

  // TODO: Verify Sec-WebSocket-Accept header
  connected_ = true;
}

auto NostrTransport::encode_websocket_frame(const std::span<const std::byte> payload) -> std::string
{
  std::string frame;

  // WebSocket frame format (RFC 6455)
  // FIN=1, RSV=000, Opcode=0001 (text frame)
  frame.push_back(0x81);

  size_t payload_len = payload.size();

  if (payload_len < 126) {
    // Mask bit = 1, payload length
    frame.push_back(0x80 | static_cast<char>(payload_len));
  } else if (payload_len < 65536) {
    frame.push_back(0x80 | 126);
    frame.push_back((payload_len >> 8) & 0xFF);
    frame.push_back(payload_len & 0xFF);
  } else {
    frame.push_back(0x80 | 127);
    for (int i = 7; i >= 0; --i) { frame.push_back((payload_len >> (i * 8)) & 0xFF); }
  }

  // Masking key (4 bytes)
  std::random_device rd;
  std::mt19937 gen(rd());
  std::array<char, 4> mask_key;
  for (auto &byte : mask_key) { byte = static_cast<char>(gen() & 0xFF); }
  frame.append(mask_key.data(), 4);

  // Masked payload
  for (size_t i = 0; i < payload_len; ++i) {
    char masked_byte = static_cast<char>(payload[i]) ^ mask_key[i % 4];
    frame.push_back(masked_byte);
  }

  return frame;
}

auto NostrTransport::decode_websocket_frame(const std::string &data) -> void
{
  if (data.length() < 2) return;

  size_t pos = 0;

  while (pos + 1 < data.length()) {
    // Parse frame header
    char first_byte = data[pos++];
    char second_byte = data[pos++];

    bool fin = (first_byte & 0x80) != 0;
    int opcode = first_byte & 0x0F;
    bool masked = (second_byte & 0x80) != 0;
    size_t payload_len = second_byte & 0x7F;

    // Handle extended payload length
    if (payload_len == 126) {
      if (pos + 2 > data.length()) return;
      payload_len = (static_cast<unsigned char>(data[pos]) << 8) | static_cast<unsigned char>(data[pos + 1]);
      pos += 2;
    } else if (payload_len == 127) {
      if (pos + 8 > data.length()) return;
      payload_len = 0;
      for (int i = 0; i < 8; ++i) { payload_len = (payload_len << 8) | static_cast<unsigned char>(data[pos + i]); }
      pos += 8;
    }

    // Skip masking key if present (server->client frames shouldn't be masked)
    if (masked) { pos += 4; }

    // Extract payload
    if (pos + payload_len > data.length()) return;

    if (opcode == 0x1) {// Text frame
      std::string payload = data.substr(pos, payload_len);
      partial_message_ += payload;

      if (fin && message_callback_) {
        // Convert string to byte span for callback
        auto bytes = reinterpret_cast<const std::byte *>(partial_message_.data());
        std::span<const std::byte> byte_span(bytes, partial_message_.size());
        message_callback_(byte_span);
        partial_message_.clear();
      }
    }
    // TODO: Handle other frame types (close, ping, pong)

    pos += payload_len;
  }
}

auto NostrTransport::start_read() -> void
{
  socket_.async_read_some(
    boost::asio::buffer(read_buffer_), [this](const boost::system::error_code &error, std::size_t bytes_transferred) {
      handle_read(error, bytes_transferred);
    });
}

auto NostrTransport::handle_read(const boost::system::error_code &error, std::size_t bytes_transferred) -> void
{
  if (!error) {
    std::string received_data(read_buffer_.data(), bytes_transferred);
    decode_websocket_frame(received_data);
    start_read();// Continue reading
  } else {
    // Connection closed or error occurred
    connected_ = false;
  }
}

auto NostrTransport::connect(const std::string_view address) -> void
{
  try {
    parse_url(address);

    // Resolve host
    boost::asio::ip::tcp::resolver resolver(io_context_);
    auto endpoints = resolver.resolve(host_, port_);

    // Connect to server
    boost::asio::connect(socket_, endpoints);

    // Perform WebSocket handshake
    perform_handshake();

    // Start async read loop
    start_read();

    // Start IO context in separate thread
    io_thread_ = std::thread([this]() { io_context_.run(); });

  } catch (const std::exception &e) {
    throw std::runtime_error("Failed to connect to " + std::string(address) + ": " + e.what());
  }
}

auto NostrTransport::send(const std::span<const std::byte> payload) -> void
{
  if (!connected_) { throw std::runtime_error("Not connected"); }

  std::string frame = encode_websocket_frame(payload);

  boost::asio::post(io_context_, [this, frame = std::move(frame)]() {
    boost::asio::async_write(socket_,
      boost::asio::buffer(frame),
      [](const boost::system::error_code & /*error*/, std::size_t /*bytes_transferred*/) {
        // TODO: Handle write errors
      });
  });
}

auto NostrTransport::register_message_callback(std::function<void(std::span<const std::byte>)> callback) -> void
{
  message_callback_ = std::move(callback);
}

auto NostrTransport::disconnect() -> void
{
  if (connected_) {
    connected_ = false;
    io_context_.stop();

    if (io_thread_.joinable()) { io_thread_.join(); }

    if (socket_.is_open()) { socket_.close(); }
  }
}

}// namespace radix_relay
