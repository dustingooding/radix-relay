#pragma once

#include <bit>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <chrono>
#include <cstddef>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <radix_relay/async/async_queue.hpp>
#include <radix_relay/concepts/request_tracker.hpp>
#include <radix_relay/concepts/signal_bridge.hpp>
#include <radix_relay/core/events.hpp>
#include <radix_relay/nostr/events.hpp>
#include <radix_relay/nostr/message_handler.hpp>
#include <radix_relay/nostr/protocol.hpp>
#include <radix_relay/transport/uuid_generator.hpp>
#include <spdlog/spdlog.h>
#include <variant>
#include <vector>

namespace radix_relay::core {

template<concepts::signal_bridge Bridge, concepts::request_tracker Tracker>
struct session_orchestrator : public std::enable_shared_from_this<session_orchestrator<Bridge, Tracker>>
{
  session_orchestrator(std::shared_ptr<Bridge> bridge,
    std::shared_ptr<Tracker> tracker,
    std::shared_ptr<boost::asio::io_context> io_context,
    std::shared_ptr<async::async_queue<events::session_orchestrator::in_t>> in_queue,
    std::shared_ptr<async::async_queue<events::transport::in_t>> transport_out_queue,
    std::shared_ptr<async::async_queue<events::transport_event_variant_t>> main_out_queue)
    : bridge_(std::move(bridge)), handler_(bridge_), tracker_(std::move(tracker)), io_context_(std::move(io_context)),
      in_queue_(std::move(in_queue)), transport_out_queue_(std::move(transport_out_queue)),
      main_out_queue_(std::move(main_out_queue))
  {}

  auto run_once() -> boost::asio::awaitable<void>
  {
    auto evt = co_await in_queue_->pop();
    std::visit([&](auto &&event) { handle(std::forward<decltype(event)>(event)); }, evt);
    co_return;
  }

  auto run() -> boost::asio::awaitable<void>
  {
    while (true) { co_await run_once(); }
    co_return;
  }

private:
  static constexpr auto default_timeout = std::chrono::seconds(15);

  std::shared_ptr<Bridge> bridge_;
  nostr::message_handler<Bridge> handler_;
  std::shared_ptr<Tracker> tracker_;
  std::shared_ptr<boost::asio::io_context> io_context_;
  std::shared_ptr<async::async_queue<events::session_orchestrator::in_t>> in_queue_;
  std::shared_ptr<async::async_queue<events::transport::in_t>> transport_out_queue_;
  std::shared_ptr<async::async_queue<events::transport_event_variant_t>> main_out_queue_;

  auto emit_transport_event(events::transport::in_t evt) -> void { transport_out_queue_->push(std::move(evt)); }

  auto emit_main_event(events::transport_event_variant_t evt) -> void { main_out_queue_->push(std::move(evt)); }

  auto handle(const events::send &cmd) -> void
  {
    boost::asio::co_spawn(
      *io_context_,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [self = this->shared_from_this(), cmd]() -> boost::asio::awaitable<void> {
        auto [event_id, bytes] = self->handler_.handle(cmd);

        events::transport::send transport_cmd{ .message_id = transport::uuid_generator::generate(),
          .bytes = std::move(bytes) };
        self->emit_transport_event(transport_cmd);

        try {
          auto ok_response =
            co_await self->tracker_->template async_track<nostr::protocol::ok>(event_id, self->default_timeout);
          self->emit_main_event(events::message_sent{ cmd.peer, event_id, ok_response.accepted });
        } catch (const std::exception &) {
          self->emit_main_event(events::message_sent{ .peer = cmd.peer, .event_id = "", .accepted = false });
        }
      },
      boost::asio::detached);
  }

  auto handle(const events::publish_identity &cmd) -> void
  {
    boost::asio::co_spawn(
      *io_context_,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [self = this->shared_from_this(), cmd]() -> boost::asio::awaitable<void> {
        auto [event_id, bytes] = self->handler_.handle(cmd);

        events::transport::send transport_cmd{ .message_id = transport::uuid_generator::generate(),
          .bytes = std::move(bytes) };
        self->emit_transport_event(transport_cmd);

        try {
          auto ok_response =
            co_await self->tracker_->template async_track<nostr::protocol::ok>(event_id, self->default_timeout);
          self->emit_main_event(events::bundle_published{ .event_id = event_id, .accepted = ok_response.accepted });
        } catch (const std::exception &e) {
          spdlog::warn("[session_orchestrator] OK timeout for event: {} - {}", event_id, e.what());
          self->emit_main_event(events::bundle_published{ .event_id = "", .accepted = false });
        }
      },
      boost::asio::detached);
  }

  auto handle(const events::trust &cmd) -> void { handler_.handle(cmd); }

  auto handle(const events::subscribe &cmd) -> void
  {
    boost::asio::co_spawn(
      *io_context_,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [self = this->shared_from_this(), cmd]() -> boost::asio::awaitable<void> {
        auto [subscription_id, bytes] = self->handler_.handle(cmd);

        events::transport::send transport_cmd{ .message_id = transport::uuid_generator::generate(),
          .bytes = std::move(bytes) };
        self->emit_transport_event(transport_cmd);

        try {
          auto eose = co_await self->tracker_->template async_track<nostr::protocol::eose>(
            subscription_id, self->default_timeout);
          self->emit_main_event(events::subscription_established{ eose.subscription_id });
        } catch (const std::exception &e) {
          spdlog::warn("[session_orchestrator] EOSE timeout for subscription: {} - {}", subscription_id, e.what());
          self->emit_main_event(events::subscription_established{ "" });
        }
      },
      boost::asio::detached);
  }

  auto handle(const events::subscribe_identities &cmd) -> void
  {
    const auto kind_value = std::to_string(
      static_cast<std::underlying_type_t<nostr::protocol::kind>>(nostr::protocol::kind::bundle_announcement));
    const std::string subscription_json =
      R"(["REQ",")" + cmd.subscription_id + R"(",{"kinds":[)" + kind_value + R"(],"#d":["radix_prekey_bundle_v1"]}])";
    handle(events::subscribe{ .subscription_json = subscription_json });
  }

  auto handle(const events::subscribe_messages &cmd) -> void
  {
    auto subscription_json = bridge_->create_subscription_for_self(cmd.subscription_id);
    handle(events::subscribe{ .subscription_json = subscription_json });
  }

  auto handle(const events::transport::bytes_received &evt) noexcept -> void
  {
    try {
      std::string json_str;
      json_str.resize(evt.bytes.size());
      std::ranges::transform(evt.bytes, json_str.begin(), [](std::byte byte) { return std::bit_cast<char>(byte); });

      try {
        auto parsed = nlohmann::json::parse(json_str);
        if (not parsed.is_array() or parsed.empty() or not parsed[0].is_string()) {
          nostr::events::incoming::unknown_protocol evt_inner{ json_str };
          handler_.handle(evt_inner);
          return;
        }

        const auto msg_type = parsed[0].get<std::string>();

        if (msg_type == "OK") {
          auto ok_msg = nostr::protocol::ok::deserialize(json_str);
          if (ok_msg) {
            tracker_->resolve(ok_msg->event_id, *ok_msg);
            nostr::events::incoming::ok evt_inner{ *ok_msg };
            handler_.handle(evt_inner);
          } else {
            nostr::events::incoming::unknown_protocol evt_inner{ json_str };
            handler_.handle(evt_inner);
          }
        } else if (msg_type == "EOSE") {
          auto eose_msg = nostr::protocol::eose::deserialize(json_str);
          if (eose_msg) {
            tracker_->resolve(eose_msg->subscription_id, *eose_msg);
            nostr::events::incoming::eose evt_inner{ *eose_msg };
            handler_.handle(evt_inner);
          } else {
            nostr::events::incoming::unknown_protocol evt_inner{ json_str };
            handler_.handle(evt_inner);
          }
        } else if (msg_type == "EVENT" and parsed.size() >= 3) {
          auto event_data = parsed[2];
          auto kind_value = event_data["kind"].get<std::uint32_t>();

          switch (static_cast<nostr::protocol::kind>(kind_value)) {
          case nostr::protocol::kind::encrypted_message: {
            nostr::events::incoming::encrypted_message evt_inner{ nostr::protocol::event_data{ .id = event_data["id"],
              .pubkey = event_data["pubkey"],
              .created_at = event_data["created_at"],
              .kind = event_data["kind"],
              .tags = event_data["tags"],
              .content = event_data["content"],
              .sig = event_data["sig"] } };

            if (auto result = handler_.handle(evt_inner)) { emit_main_event(*result); }
            break;
          }
          case nostr::protocol::kind::bundle_announcement: {
            nostr::events::incoming::bundle_announcement evt_inner{ nostr::protocol::event_data{ .id = event_data["id"],
              .pubkey = event_data["pubkey"],
              .created_at = event_data["created_at"],
              .kind = event_data["kind"],
              .tags = event_data["tags"],
              .content = event_data["content"],
              .sig = event_data["sig"] } };

            if (auto result = handler_.handle(evt_inner)) { emit_main_event(*result); }
            break;
          }
          case nostr::protocol::kind::identity_announcement: {
            nostr::events::incoming::identity_announcement evt_inner{ nostr::protocol::event_data{
              .id = event_data["id"],
              .pubkey = event_data["pubkey"],
              .created_at = event_data["created_at"],
              .kind = event_data["kind"],
              .tags = event_data["tags"],
              .content = event_data["content"],
              .sig = event_data["sig"] } };

            handler_.handle(evt_inner);
            break;
          }
          case nostr::protocol::kind::session_request: {
            nostr::events::incoming::session_request evt_inner{ nostr::protocol::event_data{ .id = event_data["id"],
              .pubkey = event_data["pubkey"],
              .created_at = event_data["created_at"],
              .kind = event_data["kind"],
              .tags = event_data["tags"],
              .content = event_data["content"],
              .sig = event_data["sig"] } };

            handler_.handle(evt_inner);
            break;
          }
          case nostr::protocol::kind::node_status: {
            nostr::events::incoming::node_status evt_inner{ nostr::protocol::event_data{ .id = event_data["id"],
              .pubkey = event_data["pubkey"],
              .created_at = event_data["created_at"],
              .kind = event_data["kind"],
              .tags = event_data["tags"],
              .content = event_data["content"],
              .sig = event_data["sig"] } };

            handler_.handle(evt_inner);
            break;
          }
          default: {
            nostr::events::incoming::unknown_message evt_inner{ nostr::protocol::event_data{ .id = event_data["id"],
              .pubkey = event_data["pubkey"],
              .created_at = event_data["created_at"],
              .kind = event_data["kind"],
              .tags = event_data["tags"],
              .content = event_data["content"],
              .sig = event_data["sig"] } };

            handler_.handle(evt_inner);
            break;
          }
          }
        } else {
          nostr::events::incoming::unknown_protocol evt_inner{ json_str };
          handler_.handle(evt_inner);
        }
      } catch (const std::exception &e) {
        spdlog::warn("[session_orchestrator] Failed to parse message: {} - Raw: {}", e.what(), json_str);
        nostr::events::incoming::unknown_protocol evt_inner{ json_str };
        handler_.handle(evt_inner);
      }
    } catch (const std::bad_alloc &e) {
      spdlog::error("[session_orchestrator] Failed to process bytes_received event: {}", e.what());
    }
  }

  auto handle(const events::transport::connected & /*evt*/) -> void
  {
    spdlog::debug("[session_orchestrator] Transport connected");
    std::ignore = bridge_;
  }

  auto handle(const events::transport::connect_failed & /*evt*/) -> void
  {
    spdlog::debug("[session_orchestrator] Transport connect failed");
    std::ignore = bridge_;
  }

  auto handle(const events::transport::sent & /*evt*/) -> void
  {
    spdlog::debug("[session_orchestrator] Transport sent");
    std::ignore = bridge_;
  }

  auto handle(const events::transport::send_failed & /*evt*/) -> void
  {
    spdlog::debug("[session_orchestrator] Transport send failed");
    std::ignore = bridge_;
  }

  auto handle(const events::transport::disconnected & /*evt*/) -> void
  {
    spdlog::debug("[session_orchestrator] Transport disconnected");
    std::ignore = bridge_;
  }
};

}// namespace radix_relay::core
