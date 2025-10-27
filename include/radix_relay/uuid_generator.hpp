#pragma once

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <string>

namespace radix_relay {

class uuid_generator
{
public:
  [[nodiscard]] static auto generate() -> std::string
  {
    static thread_local boost::uuids::random_generator gen;
    return boost::uuids::to_string(gen());
  }
};

}// namespace radix_relay
