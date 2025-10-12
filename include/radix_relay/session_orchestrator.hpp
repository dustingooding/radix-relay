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
#include <nlohmann/json.hpp>
#include <radix_relay/concepts/request_tracker.hpp>
#include <radix_relay/events/events.hpp>
#include <radix_relay/events/nostr_events.hpp>
#include <radix_relay/nostr_message_handler.hpp>
#include <radix_relay/nostr_protocol.hpp>
#include <ranges>
#include <signal_bridge_cxx/lib.h>
#include <vector>

namespace radix_relay {

template<concepts::RequestTracker Tracker> class SessionOrchestrator
{
public:
  using SendBytesToTransportFn = std::function<void(std::vector<std::byte>)>;
  using SendEventToMainFn = std::function<void(events::TransportEventVariant)>;

  SessionOrchestrator(rust::Box<SignalBridge> &bridge,
    Tracker &tracker,
    const boost::asio::io_context::strand &main_strand,
    const boost::asio::io_context::strand &session_strand,
    const boost::asio::io_context::strand &transport_strand,
    SendBytesToTransportFn send_bytes_to_transport,
    SendEventToMainFn send_event_to_main)
    : handler_(bridge), bridge_(bridge), tracker_(tracker), main_strand_(main_strand), session_strand_(session_strand),
      transport_strand_(transport_strand), send_bytes_to_transport_(std::move(send_bytes_to_transport)),
      send_event_to_main_(std::move(send_event_to_main))
  {}

  auto handle_command(const events::send &cmd) -> void
  {
    boost::asio::post(session_strand_, [this, cmd]() {
      boost::asio::co_spawn(
        boost::asio::make_strand(session_strand_.context()),
        [this, cmd]() -> boost::asio::awaitable<void> {
          auto [event_id, bytes] = handler_.handle(cmd);

          boost::asio::post(
            transport_strand_, [this, msg_bytes = std::move(bytes)]() { send_bytes_to_transport_(msg_bytes); });

          try {
            auto ok = co_await tracker_.template async_track<nostr::protocol::ok>(event_id, default_timeout);
            boost::asio::post(main_strand_, [this, cmd, event_id, accepted = ok.accepted]() {
              send_event_to_main_(events::message_sent{ cmd.peer, event_id, accepted });
            });
          } catch (const std::exception &) {
            boost::asio::post(
              main_strand_, [this, cmd]() { send_event_to_main_(events::message_sent{ cmd.peer, "", false }); });
          }
        },
        boost::asio::detached);
    });
  }

  auto handle_command(const events::publish_identity &cmd) -> void
  {
    boost::asio::post(session_strand_, [this, cmd]() {
      boost::asio::co_spawn(
        boost::asio::make_strand(session_strand_.context()),
        [this, cmd]() -> boost::asio::awaitable<void> {
          auto [event_id, bytes] = handler_.handle(cmd);

          boost::asio::post(
            transport_strand_, [this, bundle_bytes = std::move(bytes)]() { send_bytes_to_transport_(bundle_bytes); });

          try {
            auto ok = co_await tracker_.template async_track<nostr::protocol::ok>(event_id, default_timeout);
            boost::asio::post(main_strand_, [this, event_id, accepted = ok.accepted]() {
              send_event_to_main_(events::bundle_published{ event_id, accepted });
            });
          } catch (const std::exception &) {
            boost::asio::post(main_strand_, [this]() { send_event_to_main_(events::bundle_published{ "", false }); });
          }
        },
        boost::asio::detached);
    });
  }

  auto handle_command(const events::trust &cmd) -> void
  {
    boost::asio::post(session_strand_, [this, cmd]() { handler_.handle(cmd); });
  }

  auto handle_command(const events::subscribe &cmd) -> void
  {
    boost::asio::post(session_strand_, [this, cmd]() {
      boost::asio::co_spawn(
        boost::asio::make_strand(session_strand_.context()),
        [this, cmd]() -> boost::asio::awaitable<void> {
          auto [subscription_id, bytes] = handler_.handle(cmd);

          boost::asio::post(
            transport_strand_, [this, sub_bytes = std::move(bytes)]() { send_bytes_to_transport_(sub_bytes); });

          try {
            auto eose = co_await tracker_.template async_track<nostr::protocol::eose>(subscription_id, default_timeout);
            boost::asio::post(main_strand_, [this, sub_id = eose.subscription_id]() {
              send_event_to_main_(events::subscription_established{ sub_id });
            });
          } catch (const std::exception &) {
            boost::asio::post(main_strand_, [this]() { send_event_to_main_(events::subscription_established{ "" }); });
          }
        },
        boost::asio::detached);
    });
  }

  auto handle_bytes_from_transport(const std::vector<std::byte> &bytes) -> void
  {
    boost::asio::post(session_strand_, [this, bytes]() {
      std::string json_str;
      json_str.resize(bytes.size());
      std::ranges::transform(bytes, json_str.begin(), [](std::byte byte) { return std::bit_cast<char>(byte); });

      try {
        auto parsed = nlohmann::json::parse(json_str);
        if (!parsed.is_array() || parsed.empty() || !parsed[0].is_string()) {
          nostr::events::incoming::unknown_protocol evt{ json_str };
          handler_.handle(evt);
          return;
        }

        const auto msg_type = parsed[0].get<std::string>();

        if (msg_type == "OK") {
          auto ok_msg = nostr::protocol::ok::deserialize(json_str);
          if (ok_msg) {
            tracker_.resolve(ok_msg->event_id, *ok_msg);
            nostr::events::incoming::ok evt{ *ok_msg };
            handler_.handle(evt);
          } else {
            nostr::events::incoming::unknown_protocol evt{ json_str };
            handler_.handle(evt);
          }
        } else if (msg_type == "EOSE") {
          auto eose_msg = nostr::protocol::eose::deserialize(json_str);
          if (eose_msg) {
            tracker_.resolve(eose_msg->subscription_id, *eose_msg);
            nostr::events::incoming::eose evt{ *eose_msg };
            handler_.handle(evt);
          } else {
            nostr::events::incoming::unknown_protocol evt{ json_str };
            handler_.handle(evt);
          }
        } else if (msg_type == "EVENT" && parsed.size() >= 3) {
          auto event_data = parsed[2];

          if (event_data["kind"] == 40001) {
            nostr::events::incoming::encrypted_message evt{ nostr::protocol::event_data{ .id = event_data["id"],
              .pubkey = event_data["pubkey"],
              .created_at = event_data["created_at"],
              .kind = event_data["kind"],
              .tags = event_data["tags"],
              .content = event_data["content"],
              .sig = event_data["sig"] } };

            if (auto result = handler_.handle(evt)) {
              boost::asio::post(main_strand_, [this, result = *result]() { send_event_to_main_(result); });
            }
          } else if (event_data["kind"] == 30078) {
            nostr::events::incoming::bundle_announcement evt{ nostr::protocol::event_data{ .id = event_data["id"],
              .pubkey = event_data["pubkey"],
              .created_at = event_data["created_at"],
              .kind = event_data["kind"],
              .tags = event_data["tags"],
              .content = event_data["content"],
              .sig = event_data["sig"] } };

            if (auto result = handler_.handle(evt)) {
              boost::asio::post(main_strand_, [this, result = *result]() { send_event_to_main_(result); });
            }
          } else if (event_data["kind"] == 40002) {
            nostr::events::incoming::identity_announcement evt{ nostr::protocol::event_data{ .id = event_data["id"],
              .pubkey = event_data["pubkey"],
              .created_at = event_data["created_at"],
              .kind = event_data["kind"],
              .tags = event_data["tags"],
              .content = event_data["content"],
              .sig = event_data["sig"] } };

            handler_.handle(evt);
          } else if (event_data["kind"] == 40003) {
            nostr::events::incoming::session_request evt{ nostr::protocol::event_data{ .id = event_data["id"],
              .pubkey = event_data["pubkey"],
              .created_at = event_data["created_at"],
              .kind = event_data["kind"],
              .tags = event_data["tags"],
              .content = event_data["content"],
              .sig = event_data["sig"] } };

            handler_.handle(evt);
          } else if (event_data["kind"] == 40004) {
            nostr::events::incoming::node_status evt{ nostr::protocol::event_data{ .id = event_data["id"],
              .pubkey = event_data["pubkey"],
              .created_at = event_data["created_at"],
              .kind = event_data["kind"],
              .tags = event_data["tags"],
              .content = event_data["content"],
              .sig = event_data["sig"] } };

            handler_.handle(evt);
          } else {
            nostr::events::incoming::unknown_message evt{ nostr::protocol::event_data{ .id = event_data["id"],
              .pubkey = event_data["pubkey"],
              .created_at = event_data["created_at"],
              .kind = event_data["kind"],
              .tags = event_data["tags"],
              .content = event_data["content"],
              .sig = event_data["sig"] } };

            handler_.handle(evt);
          }
        } else {
          nostr::events::incoming::unknown_protocol evt{ json_str };
          handler_.handle(evt);
        }
      } catch (const std::exception &) {
        nostr::events::incoming::unknown_protocol evt{ json_str };
        handler_.handle(evt);
      }
    });
  }

private:
  static constexpr auto default_timeout = std::chrono::seconds(5);

  NostrMessageHandler handler_;
  [[maybe_unused]] rust::Box<SignalBridge> &bridge_;
  Tracker &tracker_;
  const boost::asio::io_context::strand &main_strand_;
  const boost::asio::io_context::strand &session_strand_;
  const boost::asio::io_context::strand &transport_strand_;
  SendBytesToTransportFn send_bytes_to_transport_;
  SendEventToMainFn send_event_to_main_;
};

}// namespace radix_relay
