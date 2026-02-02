#pragma once

#include <concepts/transport_stream.hpp>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <simpleble/SimpleBLE.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace radix_relay::transport {

struct ble_connection_params
{
  std::string_view device_address;
  std::string_view service_uuid;
  std::string_view characteristic_uuid;
};

class ble_stream
{
public:
  using connection_params_t = ble_connection_params;

  explicit ble_stream(const std::shared_ptr<boost::asio::io_context> &io_context);

  auto async_connect(ble_connection_params params,
    std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void;

  auto async_write(std::span<const std::byte> data,
    const std::function<void(const boost::system::error_code &, std::size_t)> &handler) -> void;

  auto async_read(const boost::asio::mutable_buffer &buffer,
    const std::function<void(const boost::system::error_code &, std::size_t)> &handler) -> void;

  auto async_close(std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void;

  [[nodiscard]] auto get_mtu() const -> std::size_t;

private:
  std::shared_ptr<boost::asio::io_context> io_context_;
  boost::asio::strand<boost::asio::io_context::executor_type> strand_;
  std::size_t mtu_{ 20 };
  bool connected_{ false };

  std::optional<SimpleBLE::Adapter> adapter_;
  std::optional<SimpleBLE::Peripheral> peripheral_;

  std::string tx_characteristic_uuid_;
  std::string rx_characteristic_uuid_;
  std::string service_uuid_;

  std::vector<std::byte> read_buffer_;
  std::function<void(const boost::system::error_code &, std::size_t)> pending_read_handler_;

  static auto find_adapter() -> std::optional<SimpleBLE::Adapter>;
  auto find_peripheral(const std::string &address) -> std::optional<SimpleBLE::Peripheral>;
  auto setup_notification_callback() -> void;
};

static_assert(concepts::transport_stream<ble_stream>);

}// namespace radix_relay::transport
