#pragma once

#include <bitset>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace vrtr::ECS {
using ComponentSignature = uint32_t; // To indicate the component type.
constexpr ComponentSignature MAX_COMPONENTS = 64;
using Signature = std::bitset<MAX_COMPONENTS>;

struct TransformComponent {
  glm::vec3 position;
  glm::quat rotation;
  glm::vec3 scale;
};
} // namespace vrtr::ECS