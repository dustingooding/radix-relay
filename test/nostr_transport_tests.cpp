#include "test_doubles/test_double_websocket_stream.hpp"
#include <async/async_queue.hpp>
#include <core/events.hpp>
#include <nostr/transport.hpp>

#include <boost/asio.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <variant>
#include <vector>

namespace radix_relay::nostr::test {

TEST_CASE("Transport processes connect command and establishes connection", "[nostr][transport][queue]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(io_context);

  auto in_queue = std::make_shared<async::async_queue<core::events::transport::in_t>>(io_context);
  auto out_queue = std::make_shared<async::async_queue<core::events::session_orchestrator::in_t>>(io_context);

  transport<radix_relay::test::test_double_websocket_stream> transport(fake, io_context, in_queue, out_queue);

  in_queue->push(core::events::transport::connect{ .url = "wss://relay.damus.io" });

  boost::asio::co_spawn(*io_context, transport.run_once(), boost::asio::detached);

  io_context->run();

  const auto &connections = fake->get_connections();
  REQUIRE(connections.size() == 1);
  CHECK(connections[0].host == "relay.damus.io");
  CHECK(connections[0].port == "443");
  CHECK(connections[0].path == "/");
}

TEST_CASE("Transport pushes connected event with correct URL after connection", "[nostr][transport][queue]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(io_context);

  auto in_queue = std::make_shared<async::async_queue<core::events::transport::in_t>>(io_context);
  auto out_queue = std::make_shared<async::async_queue<core::events::session_orchestrator::in_t>>(io_context);

  transport<radix_relay::test::test_double_websocket_stream> transport(fake, io_context, in_queue, out_queue);

  in_queue->push(core::events::transport::connect{ .url = "wss://relay.damus.io" });

  boost::asio::co_spawn(*io_context, transport.run_once(), boost::asio::detached);

  io_context->run();

  CHECK(out_queue->size() == 1);

  io_context->restart();
  auto future = boost::asio::co_spawn(*io_context, out_queue->pop(), boost::asio::use_future);
  io_context->run();
  auto event = future.get();

  CHECK(std::holds_alternative<core::events::transport::connected>(event));
  const auto &connected_evt = std::get<core::events::transport::connected>(event);
  CHECK(connected_evt.url == "wss://relay.damus.io");
  CHECK(connected_evt.type == core::events::transport_type::internet);
}

TEST_CASE("Transport emits bytes_received event when WebSocket receives data", "[nostr][transport][queue]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(io_context);

  auto in_queue = std::make_shared<async::async_queue<core::events::transport::in_t>>(io_context);
  auto out_queue = std::make_shared<async::async_queue<core::events::session_orchestrator::in_t>>(io_context);

  transport<radix_relay::test::test_double_websocket_stream> transport(fake, io_context, in_queue, out_queue);

  in_queue->push(core::events::transport::connect{ .url = "wss://relay.damus.io" });
  boost::asio::co_spawn(*io_context, transport.run_once(), boost::asio::detached);
  io_context->run();
  io_context->restart();

  CHECK(out_queue->size() == 1);
  auto clear_future = boost::asio::co_spawn(*io_context, out_queue->pop(), boost::asio::use_future);
  io_context->run();
  clear_future.get();

  constexpr std::byte test_byte_1{ 0xAB };
  constexpr std::byte test_byte_2{ 0xCD };
  constexpr std::byte test_byte_3{ 0xEF };
  std::vector<std::byte> incoming_data{ test_byte_1, test_byte_2, test_byte_3 };
  fake->set_read_data(incoming_data);

  io_context->restart();
  io_context->run();

  CHECK(out_queue->size() == 1);

  io_context->restart();
  auto future = boost::asio::co_spawn(*io_context, out_queue->pop(), boost::asio::use_future);
  io_context->run();
  auto event = future.get();

  CHECK(std::holds_alternative<core::events::transport::bytes_received>(event));
  const auto &received_evt = std::get<core::events::transport::bytes_received>(event);
  CHECK(received_evt.bytes == incoming_data);
}

TEST_CASE("Transport processes send command and writes data to WebSocket", "[nostr][transport][queue]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(io_context);

  auto in_queue = std::make_shared<async::async_queue<core::events::transport::in_t>>(io_context);
  auto out_queue = std::make_shared<async::async_queue<core::events::session_orchestrator::in_t>>(io_context);

  transport<radix_relay::test::test_double_websocket_stream> transport(fake, io_context, in_queue, out_queue);

  in_queue->push(core::events::transport::connect{ .url = "wss://relay.damus.io" });
  boost::asio::co_spawn(*io_context, transport.run_once(), boost::asio::detached);
  io_context->run();
  io_context->restart();

  CHECK(out_queue->size() == 1);
  auto clear_future2 = boost::asio::co_spawn(*io_context, out_queue->pop(), boost::asio::use_future);
  io_context->run();
  clear_future2.get();

  const std::vector<std::byte> data{ std::byte{ 0x01 }, std::byte{ 0x02 }, std::byte{ 0x03 } };
  in_queue->push(core::events::transport::send{ .message_id = "test-msg-id", .bytes = data });

  boost::asio::co_spawn(*io_context, transport.run_once(), boost::asio::detached);

  io_context->restart();
  io_context->run();

  const auto &writes = fake->get_writes();
  REQUIRE(writes.size() == 1);
  CHECK(writes[0].data == data);
}

TEST_CASE("Transport pushes sent event with message ID after sending data", "[nostr][transport][queue]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(io_context);

  auto in_queue = std::make_shared<async::async_queue<core::events::transport::in_t>>(io_context);
  auto out_queue = std::make_shared<async::async_queue<core::events::session_orchestrator::in_t>>(io_context);

  transport<radix_relay::test::test_double_websocket_stream> transport(fake, io_context, in_queue, out_queue);

  in_queue->push(core::events::transport::connect{ .url = "wss://relay.damus.io" });
  boost::asio::co_spawn(*io_context, transport.run_once(), boost::asio::detached);
  io_context->run();
  io_context->restart();

  CHECK(out_queue->size() == 1);
  auto clear_future2 = boost::asio::co_spawn(*io_context, out_queue->pop(), boost::asio::use_future);
  io_context->run();
  clear_future2.get();

  const std::vector<std::byte> data{ std::byte{ 0x01 }, std::byte{ 0x02 }, std::byte{ 0x03 } };
  in_queue->push(core::events::transport::send{ .message_id = "test-msg-id", .bytes = data });

  boost::asio::co_spawn(*io_context, transport.run_once(), boost::asio::detached);

  io_context->restart();
  io_context->run();

  CHECK(out_queue->size() == 1);

  io_context->restart();
  auto future = boost::asio::co_spawn(*io_context, out_queue->pop(), boost::asio::use_future);
  io_context->run();
  auto event = future.get();

  CHECK(std::holds_alternative<core::events::transport::sent>(event));
  const auto &sent_evt = std::get<core::events::transport::sent>(event);
  CHECK(sent_evt.message_id == "test-msg-id");
}

TEST_CASE("Transport pushes disconnected event after processing disconnect command", "[nostr][transport][queue]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(io_context);

  auto in_queue = std::make_shared<async::async_queue<core::events::transport::in_t>>(io_context);
  auto out_queue = std::make_shared<async::async_queue<core::events::session_orchestrator::in_t>>(io_context);

  transport<radix_relay::test::test_double_websocket_stream> transport(fake, io_context, in_queue, out_queue);

  in_queue->push(core::events::transport::connect{ .url = "wss://relay.damus.io" });
  boost::asio::co_spawn(*io_context, transport.run_once(), boost::asio::detached);
  io_context->run();
  io_context->restart();

  CHECK(out_queue->size() == 1);
  auto clear_future3 = boost::asio::co_spawn(*io_context, out_queue->pop(), boost::asio::use_future);
  io_context->run();
  clear_future3.get();

  in_queue->push(core::events::transport::disconnect{});

  boost::asio::co_spawn(*io_context, transport.run_once(), boost::asio::detached);

  io_context->restart();
  io_context->run();

  CHECK(out_queue->size() == 1);

  io_context->restart();
  auto future = boost::asio::co_spawn(*io_context, out_queue->pop(), boost::asio::use_future);
  io_context->run();
  auto event = future.get();

  CHECK(std::holds_alternative<core::events::transport::disconnected>(event));
}

TEST_CASE("Transport respects cancellation signal while waiting", "[nostr][transport][cancellation]")
{
  struct test_state
  {
    std::atomic<bool> coroutine_done{ false };
  };

  auto io_context = std::make_shared<boost::asio::io_context>();
  auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(io_context);
  auto in_queue = std::make_shared<async::async_queue<core::events::transport::in_t>>(io_context);
  auto out_queue = std::make_shared<async::async_queue<core::events::session_orchestrator::in_t>>(io_context);
  auto cancel_signal = std::make_shared<boost::asio::cancellation_signal>();
  auto cancel_slot = std::make_shared<boost::asio::cancellation_slot>(cancel_signal->slot());
  auto trans =
    std::make_shared<transport<radix_relay::test::test_double_websocket_stream>>(fake, io_context, in_queue, out_queue);

  auto state = std::make_shared<test_state>();

  boost::asio::co_spawn(
    *io_context,
    [](std::shared_ptr<transport<radix_relay::test::test_double_websocket_stream>> transport_ptr,
      std::shared_ptr<test_state> test_state_ptr,
      std::shared_ptr<boost::asio::cancellation_slot> c_slot) -> boost::asio::awaitable<void> {
      try {
        co_await transport_ptr->run(c_slot);
      } catch (const boost::system::system_error &err) {
        if (err.code() != boost::asio::error::operation_aborted
            and err.code() != boost::asio::experimental::error::channel_cancelled
            and err.code() != boost::asio::experimental::error::channel_closed) {
          throw;
        }
      }
      test_state_ptr->coroutine_done = true;
    }(trans, state, cancel_slot),
    boost::asio::detached);

  io_context->poll();

  cancel_signal->emit(boost::asio::cancellation_type::terminal);
  io_context->run();

  CHECK(state->coroutine_done);
}

}// namespace radix_relay::nostr::test
