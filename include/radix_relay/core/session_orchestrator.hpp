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
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <radix_relay/concepts/request_tracker.hpp>
#include <radix_relay/concepts/signal_bridge.hpp>
#include <radix_relay/core/events.hpp>
#include <radix_relay/nostr/events.hpp>
#include <radix_relay/nostr/message_handler.hpp>
#include <radix_relay/nostr/protocol.hpp>
#include <radix_relay/transport/uuid_generator.hpp>
#include <spdlog/spdlog.h>
#include <vector>

namespace radix_relay::core {

struct strands
{
  const boost::asio::io_context::strand *main;
  const boost::asio::io_context::strand *session;
  const boost::asio::io_context::strand *transport;
};

template<concepts::signal_bridge Bridge, concepts::request_tracker Tracker>
class session_orchestrator : public std::enable_shared_from_this<session_orchestrator<Bridge, Tracker>>
{
public:
  using send_transport_command_fn_t = std::function<void(events::transport::command_variant_t)>;
  using send_event_to_main_fn_t = std::function<void(events::transport_event_variant_t)>;

  session_orchestrator(std::shared_ptr<Bridge> bridge,
    std::shared_ptr<Tracker> tracker,
    strands strands,
    send_transport_command_fn_t send_transport_command,
    send_event_to_main_fn_t send_event_to_main)
    : handler_(bridge), tracker_(std::move(tracker)), main_strand_(strands.main), session_strand_(strands.session),
      transport_strand_(strands.transport), send_transport_command_(std::move(send_transport_command)),
      send_event_to_main_(std::move(send_event_to_main))
  {}

  auto handle_command(const events::send &cmd) -> void
  {
    boost::asio::post(*session_strand_, [self = this->shared_from_this(), cmd]() {
      boost::asio::co_spawn(
        boost::asio::make_strand(self->session_strand_->context()),
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
        [self, cmd]() -> boost::asio::awaitable<void> {
          auto [event_id, bytes] = self->handler_.handle(cmd);

          events::transport::send transport_cmd{ .message_id = transport::uuid_generator::generate(),
            .bytes = std::move(bytes) };
          boost::asio::post(
            *self->transport_strand_, [self, transport_cmd]() { self->send_transport_command_(transport_cmd); });

          try {
            auto ok_response =
              co_await self->tracker_->template async_track<nostr::protocol::ok>(event_id, self->default_timeout);
            boost::asio::post(*self->main_strand_, [self, cmd, event_id, accepted = ok_response.accepted]() {
              self->send_event_to_main_(events::message_sent{ cmd.peer, event_id, accepted });
            });
          } catch (const std::exception &) {
            boost::asio::post(*self->main_strand_, [self, cmd]() {
              self->send_event_to_main_(events::message_sent{ .peer = cmd.peer, .event_id = "", .accepted = false });
            });
          }
        },
        boost::asio::detached);
    });
  }

  auto handle_command(const events::publish_identity &cmd) -> void
  {
    boost::asio::post(*session_strand_, [self = this->shared_from_this(), cmd]() {
      boost::asio::co_spawn(
        boost::asio::make_strand(self->session_strand_->context()),
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
        [self, cmd]() -> boost::asio::awaitable<void> {
          auto [event_id, bytes] = self->handler_.handle(cmd);

          events::transport::send transport_cmd{ .message_id = transport::uuid_generator::generate(),
            .bytes = std::move(bytes) };
          boost::asio::post(
            *self->transport_strand_, [self, transport_cmd]() { self->send_transport_command_(transport_cmd); });

          try {
            auto ok_response =
              co_await self->tracker_->template async_track<nostr::protocol::ok>(event_id, self->default_timeout);
            boost::asio::post(*self->main_strand_, [self, event_id, accepted = ok_response.accepted]() {
              self->send_event_to_main_(events::bundle_published{ .event_id = event_id, .accepted = accepted });
            });
          } catch (const std::exception &e) {
            spdlog::warn("[session_orchestrator] OK timeout for event: {} - {}", event_id, e.what());
            boost::asio::post(*self->main_strand_,
              [self]() { self->send_event_to_main_(events::bundle_published{ .event_id = "", .accepted = false }); });
          }
        },
        boost::asio::detached);
    });
  }

  auto handle_command(const events::trust &cmd) -> void
  {
    boost::asio::post(*session_strand_, [this, cmd]() { handler_.handle(cmd); });
  }

  auto handle_command(const events::subscribe &cmd) -> void
  {
    boost::asio::post(*session_strand_, [self = this->shared_from_this(), cmd]() {
      boost::asio::co_spawn(
        boost::asio::make_strand(self->session_strand_->context()),
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
        [self, cmd]() -> boost::asio::awaitable<void> {
          auto [subscription_id, bytes] = self->handler_.handle(cmd);

          events::transport::send transport_cmd{ .message_id = transport::uuid_generator::generate(),
            .bytes = std::move(bytes) };
          boost::asio::post(
            *self->transport_strand_, [self, transport_cmd]() { self->send_transport_command_(transport_cmd); });

          try {
            auto eose = co_await self->tracker_->template async_track<nostr::protocol::eose>(
              subscription_id, self->default_timeout);
            boost::asio::post(*self->main_strand_, [self, sub_id = eose.subscription_id]() {
              self->send_event_to_main_(events::subscription_established{ sub_id });
            });
          } catch (const std::exception &e) {
            spdlog::warn("[session_orchestrator] EOSE timeout for subscription: {} - {}", subscription_id, e.what());
            boost::asio::post(
              *self->main_strand_, [self]() { self->send_event_to_main_(events::subscription_established{ "" }); });
          }
        },
        boost::asio::detached);
    });
  }

  auto handle_event(const events::transport::bytes_received &evt) noexcept -> void
  {
    try {
      boost::asio::post(*session_strand_, [this, bytes = evt.bytes]() {
        std::string json_str;
        json_str.resize(bytes.size());
        std::ranges::transform(bytes, json_str.begin(), [](std::byte byte) { return std::bit_cast<char>(byte); });

        try {
          auto parsed = nlohmann::json::parse(json_str);
          if (!parsed.is_array() || parsed.empty() || !parsed[0].is_string()) {
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
          } else if (msg_type == "EVENT" && parsed.size() >= 3) {
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

              if (auto result = handler_.handle(evt_inner)) {
                boost::asio::post(*main_strand_, [this, result = *result]() { send_event_to_main_(result); });
              }
              break;
            }
            case nostr::protocol::kind::bundle_announcement: {
              nostr::events::incoming::bundle_announcement evt_inner{ nostr::protocol::event_data{
                .id = event_data["id"],
                .pubkey = event_data["pubkey"],
                .created_at = event_data["created_at"],
                .kind = event_data["kind"],
                .tags = event_data["tags"],
                .content = event_data["content"],
                .sig = event_data["sig"] } };

              if (auto result = handler_.handle(evt_inner)) {
                boost::asio::post(*main_strand_, [this, result = *result]() { send_event_to_main_(result); });
              }
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
      });
    } catch (const std::bad_alloc &e) {
      // Cannot post event due to memory exhaustion - log and continue
      spdlog::error("[session_orchestrator] Failed to post bytes_received event: {}", e.what());
    }
  }

private:
  static constexpr auto default_timeout = std::chrono::seconds(15);

  nostr::message_handler<Bridge> handler_;
  std::shared_ptr<Tracker> tracker_;
  const boost::asio::io_context::strand *main_strand_;
  const boost::asio::io_context::strand *session_strand_;
  const boost::asio::io_context::strand *transport_strand_;
  send_transport_command_fn_t send_transport_command_;
  send_event_to_main_fn_t send_event_to_main_;
};

}// namespace radix_relay::core
