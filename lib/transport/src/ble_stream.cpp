#include <transport/ble_stream.hpp>

#include <algorithm>

namespace radix_relay::transport {

namespace {
  constexpr std::size_t default_mtu = 20;
  constexpr int scan_duration_ms = 5000;
  constexpr std::size_t max_mtu = 512;
}// namespace

ble_stream::ble_stream(const std::shared_ptr<boost::asio::io_context> &io_context)
  : io_context_(io_context), strand_(boost::asio::make_strand(*io_context))
{}

auto ble_stream::find_adapter() -> std::optional<SimpleBLE::Adapter>
{
  try {
    auto adapters = SimpleBLE::Adapter::get_adapters();
    if (adapters.empty()) { return std::nullopt; }
    return adapters[0];
  } catch (...) {
    return std::nullopt;
  }
}

auto ble_stream::find_peripheral(const std::string &address) -> std::optional<SimpleBLE::Peripheral>
{
  if (not adapter_) { return std::nullopt; }

  std::optional<SimpleBLE::Peripheral> found_peripheral;

  adapter_->set_callback_on_scan_found([&found_peripheral, &address](SimpleBLE::Peripheral peripheral) {
    if (peripheral.address() == address) { found_peripheral = peripheral; }
  });

  adapter_->scan_for(scan_duration_ms);

  return found_peripheral;
}

auto ble_stream::setup_notification_callback() -> void
{
  if (not peripheral_ or not peripheral_->is_connected()) { return; }

  peripheral_->notify(service_uuid_, rx_characteristic_uuid_, [this](const SimpleBLE::ByteArray &data) {
    read_buffer_.clear();
    read_buffer_.reserve(data.size());
    std::ranges::transform(
      data, std::back_inserter(read_buffer_), [](auto byte) { return static_cast<std::byte>(byte); });

    if (pending_read_handler_) {
      auto handler = std::move(pending_read_handler_);
      pending_read_handler_ = nullptr;

      boost::asio::post(
        strand_, [handler = std::move(handler), size = data.size()]() { handler(boost::system::error_code{}, size); });
    }
  });
}

auto ble_stream::async_connect(ble_connection_params params,
  std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void
{
  service_uuid_ = std::string(params.service_uuid);
  tx_characteristic_uuid_ = std::string(params.characteristic_uuid);
  rx_characteristic_uuid_ = std::string(params.characteristic_uuid);

  boost::asio::post(strand_, [this, params, handler = std::move(handler)]() mutable {
    auto make_error = [](const std::string & /*message*/) {
      return boost::system::error_code{ boost::asio::error::connection_refused };
    };

    adapter_ = find_adapter();
    if (not adapter_) {
      handler(make_error("No BLE adapter found"), 0);
      return;
    }

    auto device_address = std::string(params.device_address);
    peripheral_ = find_peripheral(device_address);
    if (not peripheral_) {
      handler(make_error("Device not found"), 0);
      return;
    }

    try {
      peripheral_->connect();
      connected_ = peripheral_->is_connected();

      if (not connected_) {
        handler(make_error("Failed to connect"), 0);
        return;
      }

      auto services = peripheral_->services();
      if (services.empty()) {
        handler(make_error("No services found"), 0);
        return;
      }

      setup_notification_callback();

      mtu_ = default_mtu;
      handler(boost::system::error_code{}, 0);
    } catch (const std::exception &e) {
      connected_ = false;
      handler(make_error(std::string("Connection failed: ") + e.what()), 0);
    }
  });
}

auto ble_stream::async_write(std::span<const std::byte> data,
  const std::function<void(const boost::system::error_code &, std::size_t)> &handler) -> void
{
  if (not connected_ or not peripheral_) {
    boost::asio::post(
      strand_, [handler]() { handler(boost::system::error_code{ boost::asio::error::not_connected }, 0); });
    return;
  }

  if (data.size() > mtu_) {
    boost::asio::post(
      strand_, [handler]() { handler(boost::system::error_code{ boost::asio::error::message_size }, 0); });
    return;
  }

  SimpleBLE::ByteArray ble_data;
  ble_data.reserve(data.size());
  std::ranges::transform(
    data, std::back_inserter(ble_data), [](auto byte) { return static_cast<char>(static_cast<unsigned char>(byte)); });

  boost::asio::post(strand_, [this, ble_data = std::move(ble_data), handler, size = data.size()]() {
    try {
      peripheral_->write_request(service_uuid_, tx_characteristic_uuid_, ble_data);
      handler(boost::system::error_code{}, size);
    } catch (const std::exception &) {
      handler(boost::system::error_code{ boost::asio::error::operation_aborted }, 0);
    }
  });
}

auto ble_stream::async_read(const boost::asio::mutable_buffer &buffer,
  const std::function<void(const boost::system::error_code &, std::size_t)> &handler) -> void
{
  if (not connected_ or not peripheral_) {
    boost::asio::post(
      strand_, [handler]() { handler(boost::system::error_code{ boost::asio::error::not_connected }, 0); });
    return;
  }

  if (not read_buffer_.empty()) {
    const auto bytes_to_copy = std::min(boost::asio::buffer_size(buffer), read_buffer_.size());
    std::copy_n(read_buffer_.begin(), bytes_to_copy, static_cast<std::byte *>(buffer.data()));
    read_buffer_.clear();

    boost::asio::post(strand_, [handler, bytes_to_copy]() { handler(boost::system::error_code{}, bytes_to_copy); });
    return;
  }

  pending_read_handler_ = handler;
}

auto ble_stream::async_close(std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void
{
  boost::asio::post(strand_, [this, handler = std::move(handler)]() {
    try {
      if (peripheral_ and peripheral_->is_connected()) { peripheral_->disconnect(); }
      connected_ = false;
      peripheral_.reset();
      adapter_.reset();
      handler(boost::system::error_code{}, 0);
    } catch (const std::exception &) {
      handler(boost::system::error_code{ boost::asio::error::operation_aborted }, 0);
    }
  });
}

auto ble_stream::get_mtu() const -> std::size_t { return mtu_; }

}// namespace radix_relay::transport
