/// An array to store the same type of components' data in a tight layout.
#pragma once
#include <array>
#include <unordered_map>
#include "ECS/Entity.hpp"
#include "utils/log.hpp"

/// When a component is added to an entity,
/// we append it to the end of array.

/// When a component is removed from an entity,
/// we move last one in the array to replace its position,
/// so it is still tight.

namespace vrtr::ECS {

/// For component manager to use.
class IComponentArray {
public:
  virtual ~IComponentArray() = 0;
  virtual void EntityDestroyed(const Entity &entity) = 0;
};

template <typename T> 
class ComponentArray : public IComponentArray {
public:
  void AddComponent(const Entity &entity, T component) {
    if (mComponentIndexDict.find(entity) != mComponentIndexDict.end()) {
      LOGE("entity already has same type of component");
      return;
    }
    if (mNumComponents + 1 >= MAX_ENTITIES) {
      LOGE("too much this type of component");
      return;
    }

    mComponentIndexDict[entity] = mNumComponents;
    mEntityDict[mNumComponents] = entity;
    mComponentArray[mNumComponents] = component;
    mNumComponents++;
  }

  void RemoveComponent(const Entity &entity) {
    if (mComponentIndexDict.find(entity) == mComponentIndexDict.end()) {
      LOGE("entity does not have this type of component");
      return;
    }
    if (mNumComponents <= 0) {
      LOGE("component array is empty");
      return;
    }

    // The removal is just overwrite.
    if (mNumComponents == 1) {
      mEntityDict.erase(0);
      mComponentIndexDict.erase(entity);
    } else {
      // Remove this component from array.
      // Take the last component to fill the slot.
      size_t idxToRemove = mComponentIndexDict[entity];
      size_t idxLast = mNumComponents - 1;
      Entity lastEntity = mEntityDict[idxLast];

      mComponentArray[idxToRemove] = mComponentArray[idxLast];

      mEntityDict[idxToRemove] = lastEntity;
      mComponentIndexDict[lastEntity] = idxToRemove;

      mEntityDict.erase(idxLast);
      mComponentIndexDict.erase(entity);
    }
    mNumComponents--;
  }

  T &GetComponent(const Entity &entity) {
    if (mComponentIndexDict.find(entity) == mComponentIndexDict.end()) {
      LOGE("entity does not has this type of component");
      assert(0);
    }
    return mComponentArray[mComponentIndexDict[entity]];
  }

  void EntityDestroyed(const Entity &entity) override {
    if (mComponentIndexDict.find(entity) == mComponentIndexDict.end()) {
      LOGE("entity does not has this type of component");
      return;
    }
    RemoveComponent(entity);
  }

private:
  std::array<T, MAX_ENTITIES> mComponentArray;
  size_t mNumComponents = 0;
  std::unordered_map<size_t, Entity> mEntityDict;
  std::unordered_map<Entity, size_t> mComponentIndexDict;
};

} // namespace vrtr::ECS