#include <radix_relay/nostr/events.hpp>

#include <nlohmann/json.hpp>
#include <stdexcept>

namespace radix_relay::nostr::events::outgoing {

auto subscription_request::get_subscription_id() const -> std::string
{
  auto json_obj = nlohmann::json::parse(subscription_json);
  if (not json_obj.is_array() or json_obj.size() < 2 or not json_obj[1].is_string()) {
    throw std::runtime_error("Invalid subscription JSON format");
  }
  return json_obj[1].get<std::string>();
}

}// namespace radix_relay::nostr::events::outgoing
