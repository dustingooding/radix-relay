// Integration test for websocket_stream with real network connections
// Not included in default test suite - build manually when needed
//
// Usage:
//   cmake --build out/build/unixlike-clang-debug --target beast_websocket_integration_test
//   ./out/build/unixlike-clang-debug/test/beast_websocket_integration_test
//
// This test connects to wss://echo.websocket.org which echoes back any message sent

#include <radix_relay/transport/websocket_stream.hpp>

#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <span>
#include <string>
#include <thread>
#include <vector>

TEST_CASE("Beast WebSocket can connect to echo server", "[integration][network]")
{
  boost::asio::io_context io_context;
  auto work_guard = boost::asio::make_work_guard(io_context);
  std::thread io_thread([&io_context]() { io_context.run(); });

  // RAII guard to ensure thread cleanup even on test failure
  struct cleanup_guard
  {
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> &work;
    boost::asio::io_context &context;
    std::thread &thread;

    cleanup_guard(boost::asio::executor_work_guard<boost::asio::io_context::executor_type> &wrk,
      boost::asio::io_context &ctx,
      std::thread &thr)
      : work(wrk), context(ctx), thread(thr)
    {}

    cleanup_guard(const cleanup_guard &) = delete;
    auto operator=(const cleanup_guard &) -> cleanup_guard & = delete;
    cleanup_guard(cleanup_guard &&) = delete;
    auto operator=(cleanup_guard &&) -> cleanup_guard & = delete;

    ~cleanup_guard()
    {
      work.reset();
      context.stop();
      if (thread.joinable()) { thread.join(); }
    }
  };
  const cleanup_guard guard{ work_guard, io_context, io_thread };

  auto websocket = std::make_shared<radix_relay::transport::websocket_stream>(io_context);

  SECTION("Connect to wss://echo.websocket.org")
  {
    bool connected = false;
    bool has_error = false;

    std::string error_message;
    websocket->async_connect({ .host = "echo.websocket.org", .port = "443", .path = "/" },
      [&connected, &has_error, &error_message](const boost::system::error_code &error, std::size_t /*bytes*/) {
        connected = !error;
        has_error = static_cast<bool>(error);
        if (error) { error_message = error.message(); }
      });

    // Wait for connection
    constexpr auto wait_interval = std::chrono::milliseconds(10);
    constexpr int max_wait = 500;// 5 seconds
    int wait_count = 0;
    while (!connected && !has_error && wait_count < max_wait) {
      std::this_thread::sleep_for(wait_interval);
      ++wait_count;
    }

    INFO("Connection error: " << error_message);
    REQUIRE(connected);
    REQUIRE_FALSE(has_error);

    SECTION("Send and receive echo")
    {
      const std::string test_message = "Hello WebSocket!";
      std::vector<std::byte> send_data;
      send_data.reserve(test_message.size());
      std::ranges::transform(test_message, std::back_inserter(send_data), [](const char character) {
        return static_cast<std::byte>(character);
      });

      bool write_done = false;
      auto send_span = std::span<const std::byte>(send_data);
      websocket->async_write(send_span,
        [&write_done](const boost::system::error_code &error, std::size_t /*bytes*/) { write_done = !error; });

      wait_count = 0;
      while (!write_done && wait_count < max_wait) {
        std::this_thread::sleep_for(wait_interval);
        ++wait_count;
      }

      REQUIRE(write_done);

      constexpr std::size_t buffer_size = 1024;
      std::array<std::byte, buffer_size> read_buffer{};

      // First message - server identification
      bool first_read_done = false;
      std::size_t first_bytes_read = 0;

      websocket->async_read(boost::asio::buffer(read_buffer),
        [&first_read_done, &first_bytes_read](const boost::system::error_code &error, std::size_t bytes) {
          if (!error) {
            first_read_done = true;
            first_bytes_read = bytes;
          }
        });

      wait_count = 0;
      while (!first_read_done && wait_count < max_wait) {
        std::this_thread::sleep_for(wait_interval);
        ++wait_count;
      }

      REQUIRE(first_read_done);

      std::string first_message;
      for (std::size_t idx = 0; idx < first_bytes_read; ++idx) {
        first_message.push_back(static_cast<char>(read_buffer.at(idx)));
      }
      INFO("First message (server ID): '" << first_message << "' (" << first_bytes_read << " bytes)");

      // Second message - echo of our message
      bool second_read_done = false;
      std::size_t second_bytes_read = 0;

      websocket->async_read(boost::asio::buffer(read_buffer),
        [&second_read_done, &second_bytes_read](const boost::system::error_code &error, std::size_t bytes) {
          if (!error) {
            second_read_done = true;
            second_bytes_read = bytes;
          }
        });

      wait_count = 0;
      while (!second_read_done && wait_count < max_wait) {
        std::this_thread::sleep_for(wait_interval);
        ++wait_count;
      }

      REQUIRE(second_read_done);

      std::string received;
      for (std::size_t idx = 0; idx < second_bytes_read; ++idx) {
        received.push_back(static_cast<char>(read_buffer.at(idx)));
      }

      INFO("Expected echo: '" << test_message << "' (" << test_message.size() << " bytes)");
      INFO("Received echo: '" << received << "' (" << second_bytes_read << " bytes)");

      CHECK(second_bytes_read == test_message.size());
      CHECK(received == test_message);
    }

    websocket->async_close([](const boost::system::error_code & /*error*/, std::size_t /*bytes*/) {});
  }
}
