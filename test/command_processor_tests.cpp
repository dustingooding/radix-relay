#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <radix_relay/async/async_queue.hpp>
#include <radix_relay/core/command_processor.hpp>
#include <radix_relay/core/events.hpp>
#include <string>
#include <vector>

using namespace radix_relay;
using namespace radix_relay::core;

struct mock_handler
{
  std::vector<std::string> received_commands;

  auto handle(const events::raw_command &cmd) -> void { received_commands.push_back(cmd.input); }
};

struct command_processor_fixture
{
  std::shared_ptr<boost::asio::io_context> io_context;
  std::shared_ptr<async::async_queue<events::raw_command>> in_queue;
  std::shared_ptr<mock_handler> handler;
  std::shared_ptr<command_processor<mock_handler>> processor;

  command_processor_fixture()
    : io_context(std::make_shared<boost::asio::io_context>()),
      in_queue(std::make_shared<async::async_queue<events::raw_command>>(io_context)),
      handler(std::make_shared<mock_handler>()),
      processor(std::make_shared<command_processor<mock_handler>>(io_context, in_queue, handler))
  {}
};

TEST_CASE("command_processor construction", "[command_processor]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto in_queue = std::make_shared<async::async_queue<events::raw_command>>(io_context);

  auto handler = std::make_shared<mock_handler>();

  REQUIRE_NOTHROW([&]() { const command_processor<mock_handler> processor(io_context, in_queue, handler); }());
}

TEST_CASE("command_processor run_once processes single command", "[command_processor]")
{
  const command_processor_fixture fixture;

  fixture.in_queue->push(events::raw_command{ "test command" });

  boost::asio::co_spawn(*fixture.io_context, fixture.processor->run_once(), boost::asio::detached);

  fixture.io_context->run();

  REQUIRE(fixture.handler->received_commands.size() == 1);
  CHECK(fixture.handler->received_commands[0] == "test command");
}

TEST_CASE("command_processor run_once processes multiple commands sequentially", "[command_processor]")
{
  const command_processor_fixture fixture;

  fixture.in_queue->push(events::raw_command{ "first" });
  fixture.in_queue->push(events::raw_command{ "second" });
  fixture.in_queue->push(events::raw_command{ "third" });

  boost::asio::co_spawn(
    *fixture.io_context,
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
    [&fixture]() -> boost::asio::awaitable<void> {
      co_await fixture.processor->run_once();
      co_await fixture.processor->run_once();
      co_await fixture.processor->run_once();
    },
    boost::asio::detached);

  fixture.io_context->run();

  REQUIRE(fixture.handler->received_commands.size() == 3);
  CHECK(fixture.handler->received_commands[0] == "first");
  CHECK(fixture.handler->received_commands[1] == "second");
  CHECK(fixture.handler->received_commands[2] == "third");
}

struct stopping_handler
{
  std::vector<std::string> received_commands;
  std::shared_ptr<boost::asio::io_context> io_context;

  explicit stopping_handler(std::shared_ptr<boost::asio::io_context> ctx) : io_context(std::move(ctx)) {}

  auto handle(const events::raw_command &cmd) -> void
  {
    received_commands.push_back(cmd.input);
    if (received_commands.size() == 3) { io_context->stop(); }
  }
};

TEST_CASE("command_processor run processes commands continuously", "[command_processor]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto in_queue = std::make_shared<async::async_queue<events::raw_command>>(io_context);
  auto handler = std::make_shared<stopping_handler>(io_context);
  auto processor = std::make_shared<command_processor<stopping_handler>>(io_context, in_queue, handler);

  in_queue->push(events::raw_command{ "cmd1" });
  in_queue->push(events::raw_command{ "cmd2" });
  in_queue->push(events::raw_command{ "cmd3" });

  boost::asio::co_spawn(*io_context, processor->run(), boost::asio::detached);

  io_context->run();

  REQUIRE(handler->received_commands.size() == 3);
  CHECK(handler->received_commands[0] == "cmd1");
  CHECK(handler->received_commands[1] == "cmd2");
  CHECK(handler->received_commands[2] == "cmd3");
}
