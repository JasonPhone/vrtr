#pragma once

#include "ECS/Entity.hpp"

#include <unordered_set>

namespace vrtr::ECS {
class System {
public:
  std::unordered_set<Entity> mEntities;
};


} // namespace vrtr::ECS