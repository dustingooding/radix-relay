#pragma once

#include <async/async_queue.hpp>
#include <boost/asio/io_context.hpp>
#include <memory>
#include <string>
#include <vector>

namespace radix_relay_test {

struct simple_event
{
  std::string data;
};

struct output_event
{
  std::string result;
};

struct test_double_standard_handler
{
  using in_queue_t = radix_relay::async::async_queue<simple_event>;

  struct out_queues_t
  {
    std::shared_ptr<radix_relay::async::async_queue<output_event>> output;
  };

  mutable std::vector<std::string> handled_events;

  explicit test_double_standard_handler(const out_queues_t &queues) : output_queue_(queues.output) {}

  auto handle(const simple_event &evt) const -> void
  {
    handled_events.push_back(evt.data);
    if (output_queue_) { output_queue_->push(output_event{ .result = "processed:" + evt.data }); }
  }

  [[nodiscard]] auto get_handle_count() const -> std::size_t { return handled_events.size(); }

  auto clear() const -> void { handled_events.clear(); }

private:
  std::shared_ptr<radix_relay::async::async_queue<output_event>> output_queue_;
};

struct test_double_handler_with_arg
{
  using in_queue_t = radix_relay::async::async_queue<simple_event>;

  struct out_queues_t
  {
    std::shared_ptr<radix_relay::async::async_queue<output_event>> output;
  };

  std::string prefix;
  mutable std::vector<std::string> handled_events;

  explicit test_double_handler_with_arg(const std::string &arg_prefix, const out_queues_t &queues)
    : prefix(arg_prefix), output_queue_(queues.output)
  {}

  auto handle(const simple_event &evt) const -> void
  {
    handled_events.push_back(prefix + ":" + evt.data);
    if (output_queue_) { output_queue_->push(output_event{ .result = prefix + ":" + evt.data }); }
  }

  [[nodiscard]] auto get_handle_count() const -> std::size_t { return handled_events.size(); }

private:
  std::shared_ptr<radix_relay::async::async_queue<output_event>> output_queue_;
};

struct test_double_handler_no_output
{
  using in_queue_t = radix_relay::async::async_queue<simple_event>;
  struct out_queues_t
  {
  };

  mutable std::vector<std::string> handled_events;

  explicit test_double_handler_no_output(const out_queues_t & /*queues*/) {}

  auto handle(const simple_event &evt) const -> void { handled_events.push_back(evt.data); }

  [[nodiscard]] auto get_handle_count() const -> std::size_t { return handled_events.size(); }
};

struct test_double_handler_multi_arg
{
  using in_queue_t = radix_relay::async::async_queue<simple_event>;
  struct out_queues_t
  {
  };

  std::string arg1;
  int arg2;
  bool arg3;
  mutable std::vector<std::string> handled_events;

  explicit test_double_handler_multi_arg(const std::string &a1, int a2, bool a3, const out_queues_t & /*queues*/)
    : arg1(a1), arg2(a2), arg3(a3)
  {}

  auto handle(const simple_event &evt) const -> void
  {
    handled_events.push_back(arg1 + ":" + std::to_string(arg2) + ":" + (arg3 ? "true" : "false") + ":" + evt.data);
  }
};

struct throwing_handler
{
  using in_queue_t = radix_relay::async::async_queue<simple_event>;
  struct out_queues_t
  {
  };

  bool initialized = true;

  explicit throwing_handler(const out_queues_t & /*queues*/) {}

  auto handle(const simple_event & /*evt*/) const -> void
  {
    std::ignore = initialized;
    throw std::runtime_error("Handler error");
  }
};

}// namespace radix_relay_test
