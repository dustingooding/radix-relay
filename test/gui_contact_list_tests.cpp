#include <async/async_queue.hpp>
#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <core/contact_info.hpp>
#include <core/events.hpp>
#include <gui/processor.hpp>
#include <memory>
#include <slint-testing.h>
#include <string>
#include <vector>

#include "test_doubles/test_double_signal_bridge.hpp"

namespace {
struct slint_test_init
{
  slint_test_init() noexcept { slint::testing::init(); }
};
// NOLINTNEXTLINE(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)
[[maybe_unused]] const slint_test_init slint_init_once;
}// namespace

TEST_CASE("processor calls bridge list_contacts on initialization", "[gui][contact_list]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  auto ui_event_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>>(io_context);
  auto window = radix_relay::gui::make_window();
  auto message_model = radix_relay::gui::make_message_model();

  bridge->contacts_to_return = {
    radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:alice123",
      .nostr_pubkey = "npub_alice",
      .user_alias = "alice",
      .has_active_session = true,
    },
    radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:bob456",
      .nostr_pubkey = "npub_bob",
      .user_alias = "bob",
      .has_active_session = true,
    },
  };

  const radix_relay::gui::processor<radix_relay_test::test_double_signal_bridge> processor(
    "RDX:test123", "hybrid", bridge, command_queue, ui_event_queue, window, message_model);

  CHECK(bridge->was_called("list_contacts"));
}

TEST_CASE("processor populates contact list model with contacts from bridge", "[gui][contact_list]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  auto ui_event_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>>(io_context);
  auto window = radix_relay::gui::make_window();
  auto message_model = radix_relay::gui::make_message_model();

  bridge->contacts_to_return = {
    radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:alice123",
      .nostr_pubkey = "npub_alice",
      .user_alias = "alice",
      .has_active_session = true,
    },
    radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:bob456",
      .nostr_pubkey = "npub_bob",
      .user_alias = "bob",
      .has_active_session = true,
    },
    radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:charlie789",
      .nostr_pubkey = "npub_charlie",
      .user_alias = "charlie",
      .has_active_session = false,
    },
  };

  const radix_relay::gui::processor<radix_relay_test::test_double_signal_bridge> processor(
    "RDX:test123", "hybrid", bridge, command_queue, ui_event_queue, window, message_model);

  auto contact_list_model = processor.get_contact_list_model();
  REQUIRE(contact_list_model != nullptr);
  CHECK(contact_list_model->row_count() == 3);
}

TEST_CASE("processor excludes 'self' contact from contact list", "[gui][contact_list]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  auto ui_event_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>>(io_context);
  auto window = radix_relay::gui::make_window();
  auto message_model = radix_relay::gui::make_message_model();

  bridge->contacts_to_return = {
    radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:test123",
      .nostr_pubkey = "npub_self",
      .user_alias = "self",
      .has_active_session = true,
    },
    radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:alice123",
      .nostr_pubkey = "npub_alice",
      .user_alias = "alice",
      .has_active_session = true,
    },
    radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:bob456",
      .nostr_pubkey = "npub_bob",
      .user_alias = "bob",
      .has_active_session = true,
    },
  };

  const radix_relay::gui::processor<radix_relay_test::test_double_signal_bridge> processor(
    "RDX:test123", "hybrid", bridge, command_queue, ui_event_queue, window, message_model);

  auto contact_list_model = processor.get_contact_list_model();
  REQUIRE(contact_list_model != nullptr);
  CHECK(contact_list_model->row_count() == 2);
}

TEST_CASE("processor handles empty contact list from bridge", "[gui][contact_list]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  auto ui_event_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>>(io_context);
  auto window = radix_relay::gui::make_window();
  auto message_model = radix_relay::gui::make_message_model();

  bridge->contacts_to_return = {};

  const radix_relay::gui::processor<radix_relay_test::test_double_signal_bridge> processor(
    "RDX:test123", "hybrid", bridge, command_queue, ui_event_queue, window, message_model);

  auto contact_list_model = processor.get_contact_list_model();
  REQUIRE(contact_list_model != nullptr);
  CHECK(contact_list_model->row_count() == 0);
}

TEST_CASE("processor handles contact list with only 'self'", "[gui][contact_list]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  auto ui_event_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>>(io_context);
  auto window = radix_relay::gui::make_window();
  auto message_model = radix_relay::gui::make_message_model();

  bridge->contacts_to_return = {
    radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:test123",
      .nostr_pubkey = "npub_self",
      .user_alias = "self",
      .has_active_session = true,
    },
  };

  const radix_relay::gui::processor<radix_relay_test::test_double_signal_bridge> processor(
    "RDX:test123", "hybrid", bridge, command_queue, ui_event_queue, window, message_model);

  auto contact_list_model = processor.get_contact_list_model();
  REQUIRE(contact_list_model != nullptr);
  CHECK(contact_list_model->row_count() == 0);
}

TEST_CASE("contact list model contains structured contact data", "[gui][contact_list]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  auto ui_event_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>>(io_context);
  auto window = radix_relay::gui::make_window();
  auto message_model = radix_relay::gui::make_message_model();

  bridge->contacts_to_return = {
    radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:alice123",
      .nostr_pubkey = "npub_alice",
      .user_alias = "alice",
      .has_active_session = true,
    },
  };

  const radix_relay::gui::processor<radix_relay_test::test_double_signal_bridge> processor(
    "RDX:test123", "hybrid", bridge, command_queue, ui_event_queue, window, message_model);

  auto contact_list_model = processor.get_contact_list_model();
  REQUIRE(contact_list_model != nullptr);
  REQUIRE(contact_list_model->row_count() == 1);

  const auto contact = contact_list_model->row_data(0);
  REQUIRE(contact.has_value());
  if (contact.has_value()) {
    CHECK(std::string(contact->user_alias.data(), contact->user_alias.size()) == "alice");
    CHECK(std::string(contact->rdx_fingerprint.data(), contact->rdx_fingerprint.size()) == "RDX:alice123");
    CHECK(contact->has_active_session == true);
  }
}

TEST_CASE("contact list preserves contact order from bridge", "[gui][contact_list]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  auto ui_event_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>>(io_context);
  auto window = radix_relay::gui::make_window();
  auto message_model = radix_relay::gui::make_message_model();

  bridge->contacts_to_return = {
    radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:zebra",
      .nostr_pubkey = "npub_zebra",
      .user_alias = "zebra",
      .has_active_session = true,
    },
    radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:alpha",
      .nostr_pubkey = "npub_alpha",
      .user_alias = "alpha",
      .has_active_session = true,
    },
    radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:beta",
      .nostr_pubkey = "npub_beta",
      .user_alias = "beta",
      .has_active_session = false,
    },
  };

  const radix_relay::gui::processor<radix_relay_test::test_double_signal_bridge> processor(
    "RDX:test123", "hybrid", bridge, command_queue, ui_event_queue, window, message_model);

  auto contact_list_model = processor.get_contact_list_model();
  REQUIRE(contact_list_model != nullptr);
  REQUIRE(contact_list_model->row_count() == 3);

  auto contact0 = contact_list_model->row_data(0);
  auto contact1 = contact_list_model->row_data(1);
  auto contact2 = contact_list_model->row_data(2);
  REQUIRE(contact0.has_value());
  REQUIRE(contact1.has_value());
  REQUIRE(contact2.has_value());
  if (contact0.has_value() and contact1.has_value() and contact2.has_value()) {
    CHECK(std::string(contact0->user_alias.data(), contact0->user_alias.size()) == "zebra");
    CHECK(std::string(contact1->user_alias.data(), contact1->user_alias.size()) == "alpha");
    CHECK(std::string(contact2->user_alias.data(), contact2->user_alias.size()) == "beta");
  }
}

TEST_CASE("window has contact list property bound to model", "[gui][contact_list]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  auto ui_event_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>>(io_context);
  auto window = radix_relay::gui::make_window();
  auto message_model = radix_relay::gui::make_message_model();

  bridge->contacts_to_return = {
    radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:alice123",
      .nostr_pubkey = "npub_alice",
      .user_alias = "alice",
      .has_active_session = true,
    },
  };

  const radix_relay::gui::processor<radix_relay_test::test_double_signal_bridge> processor(
    "RDX:test123", "hybrid", bridge, command_queue, ui_event_queue, window, message_model);

  auto contacts_property = window->get_contacts();
  CHECK(contacts_property->row_count() == 1);
}
