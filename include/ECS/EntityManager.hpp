#pragma once

#include "utils/log.hpp"
#include "ECS/Entity.hpp"
#include "ECS/Components.hpp"
#include <queue>
#include <array>

namespace vrtr::ECS {
class EntityManager {
public:
  EntityManager() {
    LOGI("entity manager");
    for (Entity e = 0; e < MAX_ENTITIES; e++)
      mAvailableEntities.push(e);
  }

  Entity RegisterEntity() {
    assert(mNumRegisteredEntities < MAX_ENTITIES);
    Entity e = mAvailableEntities.front();
    mAvailableEntities.pop();
    mNumRegisteredEntities++;
    return e;
  }
  void UnregisterEntity(Entity e) {
    assert(e < MAX_ENTITIES);
    mEntitySignatures[e].reset();
    mAvailableEntities.push(e);
    mNumRegisteredEntities--;
  }

  void SetSignature(Entity e, Signature signature) {
    assert(e < MAX_ENTITIES);
    mEntitySignatures[e] = signature;
  }

  Signature GetSignature(Entity e) {
    assert(e < MAX_ENTITIES);
    return mEntitySignatures[e];
  }

private:
  std::queue<Entity> mAvailableEntities{};
  std::array<Signature, MAX_ENTITIES> mEntitySignatures{};
  uint32_t mNumRegisteredEntities = 0;
};
} // namespace vrtr::ECS