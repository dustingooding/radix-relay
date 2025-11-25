#pragma once

#include <async/async_queue.hpp>
#include <bit>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <chrono>
#include <concepts/request_tracker.hpp>
#include <concepts/signal_bridge.hpp>
#include <core/events.hpp>
#include <core/uuid_generator.hpp>
#include <cstddef>
#include <memory>
#include <nlohmann/json.hpp>
#include <nostr/events.hpp>
#include <nostr/message_handler.hpp>
#include <nostr/protocol.hpp>
#include <spdlog/spdlog.h>
#include <variant>
#include <vector>

namespace radix_relay::nostr {

struct discovered_bundle
{
  std::string rdx_fingerprint;
  std::string nostr_pubkey;
  std::string bundle_base64;
  std::string event_id;
};

template<concepts::signal_bridge Bridge, concepts::request_tracker Tracker>
struct session_orchestrator : public std::enable_shared_from_this<session_orchestrator<Bridge, Tracker>>
{
  session_orchestrator(const std::shared_ptr<Bridge> &bridge,
    const std::shared_ptr<Tracker> &tracker,
    const std::shared_ptr<boost::asio::io_context> &io_context,
    const std::shared_ptr<async::async_queue<core::events::session_orchestrator::in_t>> &in_queue,
    const std::shared_ptr<async::async_queue<core::events::transport::in_t>> &transport_out_queue,
    const std::shared_ptr<async::async_queue<core::events::presentation_event_variant_t>> &presentation_out_queue,
    std::chrono::milliseconds timeout = std::chrono::seconds(15))
    : bridge_(bridge), handler_(bridge_), tracker_(tracker), request_timeout_(timeout), io_context_(io_context),
      in_queue_(in_queue), transport_out_queue_(transport_out_queue), presentation_out_queue_(presentation_out_queue)
  {}

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto run_once(std::shared_ptr<boost::asio::cancellation_slot> cancel_slot = nullptr) -> boost::asio::awaitable<void>
  {
    auto evt = co_await in_queue_->pop(cancel_slot);
    std::visit([&](auto &&event) { handle(std::forward<decltype(event)>(event)); }, evt);
    co_return;
  }

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto run(std::shared_ptr<boost::asio::cancellation_slot> cancel_slot = nullptr) -> boost::asio::awaitable<void>
  {
    try {
      while (true) { co_await run_once(cancel_slot); }
    } catch (const boost::system::system_error &e) {
      if (e.code() == boost::asio::error::operation_aborted
          or e.code() == boost::asio::experimental::error::channel_cancelled
          or e.code() == boost::asio::experimental::error::channel_closed) {
        spdlog::debug("[session_orchestrator] Cancelled, exiting run loop");
        co_return;
      } else {
        spdlog::error("[session_orchestrator] Unexpected error in run loop: {}", e.what());
        throw;
      }
    }
  }

private:
  [[nodiscard]] auto get_discovered_bundles() const -> const std::vector<discovered_bundle> &
  {
    return discovered_bundles_;
  }

  std::shared_ptr<Bridge> bridge_;
  nostr::message_handler<Bridge> handler_;
  std::shared_ptr<Tracker> tracker_;
  std::chrono::milliseconds request_timeout_;
  std::shared_ptr<boost::asio::io_context> io_context_;
  std::shared_ptr<async::async_queue<core::events::session_orchestrator::in_t>> in_queue_;
  std::shared_ptr<async::async_queue<core::events::transport::in_t>> transport_out_queue_;
  std::shared_ptr<async::async_queue<core::events::presentation_event_variant_t>> presentation_out_queue_;
  std::vector<discovered_bundle> discovered_bundles_;

  auto emit_transport_event(core::events::transport::in_t evt) -> void { transport_out_queue_->push(std::move(evt)); }

  auto emit_presentation_event(core::events::presentation_event_variant_t evt) -> void
  {
    presentation_out_queue_->push(std::move(evt));
  }

  auto handle(const core::events::send &cmd) -> void
  {
    boost::asio::co_spawn(
      *io_context_,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [self = this->shared_from_this(), cmd]() -> boost::asio::awaitable<void> {
        auto [event_id, bytes] = self->handler_.handle(cmd);

        core::events::transport::send transport_cmd{ .message_id = core::uuid_generator::generate(),
          .bytes = std::move(bytes) };
        self->emit_transport_event(transport_cmd);

        try {
          auto ok_response =
            co_await self->tracker_->template async_track<nostr::protocol::ok>(event_id, self->request_timeout_);
          self->emit_presentation_event(core::events::message_sent{ cmd.peer, event_id, ok_response.accepted });
        } catch (const std::exception &) {
          self->emit_presentation_event(
            core::events::message_sent{ .peer = cmd.peer, .event_id = "", .accepted = false });
        }
      },
      boost::asio::detached);
  }

  auto handle(const core::events::publish_identity &cmd) -> void
  {
    boost::asio::co_spawn(
      *io_context_,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [self = this->shared_from_this(), cmd]() -> boost::asio::awaitable<void> {
        auto result = self->handler_.handle(cmd);

        core::events::transport::send transport_cmd{ .message_id = core::uuid_generator::generate(),
          .bytes = std::move(result.bytes) };
        self->emit_transport_event(transport_cmd);

        try {
          auto ok_response =
            co_await self->tracker_->template async_track<nostr::protocol::ok>(result.event_id, self->request_timeout_);
          if (ok_response.accepted) {
            self->bridge_->record_published_bundle(
              result.pre_key_id, result.signed_pre_key_id, result.kyber_pre_key_id);
          }
          self->emit_presentation_event(
            core::events::bundle_published{ .event_id = result.event_id, .accepted = ok_response.accepted });
        } catch (const std::exception &e) {
          spdlog::warn("[session_orchestrator] OK timeout for event: {} - {}", result.event_id, e.what());
          self->emit_presentation_event(core::events::bundle_published{ .event_id = "", .accepted = false });
        }
      },
      boost::asio::detached);
  }

  auto handle(const core::events::unpublish_identity &cmd) -> void
  {
    boost::asio::co_spawn(
      *io_context_,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [self = this->shared_from_this(), cmd]() -> boost::asio::awaitable<void> {
        auto [event_id, bytes] = self->handler_.handle(cmd);

        core::events::transport::send transport_cmd{ .message_id = core::uuid_generator::generate(),
          .bytes = std::move(bytes) };
        self->emit_transport_event(transport_cmd);

        try {
          auto ok_response =
            co_await self->tracker_->template async_track<nostr::protocol::ok>(event_id, self->request_timeout_);
          self->emit_presentation_event(
            core::events::bundle_published{ .event_id = event_id, .accepted = ok_response.accepted });
        } catch (const std::exception &e) {
          spdlog::warn("[session_orchestrator] OK timeout for event: {} - {}", event_id, e.what());
          self->emit_presentation_event(core::events::bundle_published{ .event_id = "", .accepted = false });
        }
      },
      boost::asio::detached);
  }

  auto handle(const core::events::trust &cmd) -> void
  {
    bool contact_exists = false;
    try {
      std::ignore = bridge_->lookup_contact(cmd.peer);
      contact_exists = true;
    } catch (const std::exception &e) {
      spdlog::debug("Contact {} not found, will check discovered bundles: {}", cmd.peer, e.what());
    }

    if (contact_exists) {
      if (not cmd.alias.empty()) {
        handler_.handle(cmd);
        spdlog::info("Updated alias for existing contact: {}", cmd.peer);
      }
      return;
    }

    auto bundle_iter = std::ranges::find_if(
      discovered_bundles_, [&cmd](const discovered_bundle &bundle) { return bundle.rdx_fingerprint == cmd.peer; });

    if (bundle_iter != discovered_bundles_.end()) {
      auto session_result =
        handler_.handle(core::events::establish_session{ .bundle_data = bundle_iter->bundle_base64 });
      if (session_result and not cmd.alias.empty()) { handler_.handle(cmd); }
      if (session_result) { emit_presentation_event(*session_result); }
    } else {
      spdlog::error(
        "Cannot establish session with {}: identity not found in discovered bundles and no existing contact", cmd.peer);
    }
  }

  auto handle(const core::events::subscribe &cmd) -> void
  {
    boost::asio::co_spawn(
      *io_context_,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [self = this->shared_from_this(), cmd]() -> boost::asio::awaitable<void> {
        auto [subscription_id, bytes] = self->handler_.handle(cmd);

        core::events::transport::send transport_cmd{ .message_id = core::uuid_generator::generate(),
          .bytes = std::move(bytes) };
        self->emit_transport_event(transport_cmd);

        try {
          auto eose = co_await self->tracker_->template async_track<nostr::protocol::eose>(
            subscription_id, self->request_timeout_);
          self->emit_presentation_event(core::events::subscription_established{ eose.subscription_id });
        } catch (const std::exception &e) {
          spdlog::warn("[session_orchestrator] EOSE timeout for subscription: {} - {}", subscription_id, e.what());
          self->emit_presentation_event(core::events::subscription_established{ "" });
        }
      },
      boost::asio::detached);
  }

  auto handle(const core::events::subscribe_identities & /*cmd*/) -> void
  {
    const auto subscription_id = core::uuid_generator::generate();
    nostr::protocol::validate_subscription_id(subscription_id);

    const auto kind_value = std::to_string(
      static_cast<std::underlying_type_t<nostr::protocol::kind>>(nostr::protocol::kind::bundle_announcement));
    const std::string subscription_json =
      R"(["REQ",")" + subscription_id + R"(",{"kinds":[)" + kind_value + R"(],"#d":["radix_prekey_bundle_v1"]}])";
    handle(core::events::subscribe{ .subscription_json = subscription_json });
  }

  auto handle(const core::events::bundle_announcement_received &event) -> void
  {
    auto rdx_fingerprint = bridge_->extract_rdx_from_bundle_base64(event.bundle_content);

    auto existing = std::ranges::find_if(
      discovered_bundles_, [&event](const auto &bundle) { return bundle.nostr_pubkey == event.pubkey; });

    if (existing != discovered_bundles_.end()) {
      existing->rdx_fingerprint = rdx_fingerprint;
      existing->bundle_base64 = event.bundle_content;
      existing->event_id = event.event_id;
    } else {
      discovered_bundles_.push_back(discovered_bundle{ .rdx_fingerprint = rdx_fingerprint,
        .nostr_pubkey = event.pubkey,
        .bundle_base64 = event.bundle_content,
        .event_id = event.event_id });
    }
  }

  auto handle(const core::events::bundle_announcement_removed &event) -> void
  {
    std::erase_if(discovered_bundles_, [&event](const auto &bundle) { return bundle.nostr_pubkey == event.pubkey; });
  }

  auto handle(const core::events::list_identities & /*cmd*/) -> void
  {
    std::vector<core::events::discovered_identity> identities;
    identities.reserve(discovered_bundles_.size());
    std::ranges::transform(discovered_bundles_, std::back_inserter(identities), [](const auto &bundle) {
      return core::events::discovered_identity{
        .rdx_fingerprint = bundle.rdx_fingerprint, .nostr_pubkey = bundle.nostr_pubkey, .event_id = bundle.event_id
      };
    });
    emit_presentation_event(core::events::identities_listed{ .identities = std::move(identities) });
  }

  auto handle(const core::events::subscribe_messages & /*cmd*/) -> void
  {
    const auto subscription_id = core::uuid_generator::generate();
    nostr::protocol::validate_subscription_id(subscription_id);

    auto subscription_json = bridge_->create_subscription_for_self(subscription_id, 0);
    handle(core::events::subscribe{ .subscription_json = subscription_json });
  }

  auto handle(const core::events::transport::bytes_received &evt) noexcept -> void
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

            if (auto result = handler_.handle(evt_inner)) {
              emit_presentation_event(*result);
              if (result->should_republish_bundle) { handle(core::events::publish_identity{}); }
            }
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

            if (auto result = handler_.handle(evt_inner)) {
              std::visit(
                [this](auto &&inner_evt) {
                  handle(inner_evt);
                  emit_presentation_event(inner_evt);
                },
                *result);
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
        std::string error_msg(e.what());
        if (error_msg.find("old counter") != std::string::npos
            or error_msg.find("message with old") != std::string::npos) {
          try {
            auto parsed = nlohmann::json::parse(json_str);
            if (parsed.is_array() and parsed.size() >= 3 and parsed[2].contains("pubkey")
                and parsed[2].contains("id")) {
              spdlog::debug("[session_orchestrator] Ignored duplicate message from {} (event: {})",
                parsed[2]["pubkey"].get<std::string>().substr(0, 16),
                parsed[2]["id"].get<std::string>().substr(0, 16));
            } else {
              spdlog::debug("[session_orchestrator] Ignored duplicate message: {}", error_msg);
            }
          } catch (...) {
            spdlog::debug("[session_orchestrator] Ignored duplicate message: {}", error_msg);
          }
        } else {
          spdlog::warn("[session_orchestrator] Failed to parse message: {} - Raw: {}", error_msg, json_str);
          nostr::events::incoming::unknown_protocol evt_inner{ json_str };
          handler_.handle(evt_inner);
        }
      }
    } catch (const std::bad_alloc &e) {
      spdlog::error("[session_orchestrator] Failed to process bytes_received event: {}", e.what());
    }
  }

  auto handle(const core::events::connect &evt) -> void
  {
    spdlog::info("[session_orchestrator] Connecting to relay: {}", evt.relay);
    emit_transport_event(core::events::transport::connect{ .url = evt.relay });
  }

  auto handle(const core::events::transport::connected & /*evt*/) -> void
  {
    spdlog::info("[session_orchestrator] Transport connected, performing key maintenance");
    auto maintenance_result = bridge_->perform_key_maintenance();

    if (maintenance_result.signed_pre_key_rotated or maintenance_result.kyber_pre_key_rotated) {
      spdlog::info("[session_orchestrator] Keys rotated, republishing bundle");
      handle(core::events::publish_identity{});
    }

    spdlog::info("[session_orchestrator] Subscribing to identities and messages");
    handle(core::events::subscribe_identities{});
    handle(core::events::subscribe_messages{});
  }

  auto handle(const core::events::transport::connect_failed &evt) -> void
  {
    spdlog::error("[session_orchestrator] Transport connect failed: {}", evt.error_message);
    std::ignore = bridge_;
  }

  auto handle(const core::events::transport::sent & /*evt*/) -> void
  {
    spdlog::debug("[session_orchestrator] Transport sent");
    std::ignore = bridge_;
  }

  auto handle(const core::events::transport::send_failed &evt) -> void
  {
    spdlog::error("[session_orchestrator] Transport send failed: {}", evt.error_message);
    std::ignore = bridge_;
  }

  auto handle(const core::events::transport::disconnected & /*evt*/) -> void
  {
    spdlog::info("[session_orchestrator] Transport disconnected");
    std::ignore = bridge_;
  }
};

}// namespace radix_relay::nostr
