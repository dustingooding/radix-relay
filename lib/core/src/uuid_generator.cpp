#include <core/uuid_generator.hpp>

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace radix_relay::core {

auto uuid_generator::generate() -> std::string
{
  static thread_local boost::uuids::random_generator gen;
  return boost::uuids::to_string(gen());
}

}// namespace radix_relay::core
