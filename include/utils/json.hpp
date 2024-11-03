#pragma once

#include <nlohmann/json.hpp>
#include "utils/log.hpp"
using Json = nlohmann::json;

namespace vrtr {
template <typename TargetType>
TargetType fetchOptional(const Json &json, const std::string &field_name,
                         TargetType default_val) {
  if (!json.contains(field_name))
    return default_val;
  return json[field_name].get<TargetType>();
}

template <typename TargetType>
TargetType fetchRequired(const Json &json, const std::string &field_name) {
  if (!json.contains(field_name)) {
    LOGE("{} is required but not found in given json object!", field_name);
    // std::exit(1);
  }
  return json[field_name].get<TargetType>();
}
}; // namespace vrtr