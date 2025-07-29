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
      mEntityPool.push(e);
  }

  Entity RegisterEntity() {
    assert(mNumEntities < MAX_ENTITIES);
    Entity e = mEntityPool.front();
    mEntityPool.pop();
    mNumEntities++;
    return e;
  }
  void UnregisterEntity(Entity e) {
    assert(e < MAX_ENTITIES);
    mEntitySignatures[e].reset();
    mEntityPool.push(e);
    mNumEntities--;
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
  std::queue<Entity> mEntityPool{};
  std::array<Signature, MAX_ENTITIES> mEntitySignatures{};
  size_t mNumEntities = 0;
};
} // namespace vrtr::ECS