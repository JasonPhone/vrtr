/// Manage different components (data arrays)
#pragma once
#include "ECS/Components.hpp"
#include "ECS/ComponentArray.hpp"
#include "utils/log.hpp"

#include <unordered_map>
#include <typeinfo>

namespace vrtr::ECS {
class ComponentManager {
public:
  template <typename T>
  void RegisterComponent() {
    auto typeStr = typeid(T).name();
    if (mRegisteredTypes.find(typeStr) != mRegisteredTypes.end()) {
      LOGE("component type {} already registered", typeStr);
      return;
    }
    mRegisteredTypes.insert({typeStr, mNextComponentType});
    mComponentArrays.insert({typeStr, std::make_shared<ComponentArray<T>>()});
    mNextComponentType++;
  }

  template <typename T>
  void AddComponent(const Entity &entity, const T &component) {
    GetComponentArray<T>()->AddComponent(entity, component);
  }

  template <typename T>
  void RemoveComponent(const Entity &entity) {
    GetComponentArray<T>()->RemoveComponent(entity);
  }

  template <typename T>
  T &GetComponent(const Entity &entity) {
    return GetComponentArray<T>()->GetComponent(entity);
  }

  void EntityDestroyed(const Entity &entity) {
    for (const auto &kv : mComponentArrays) {
      auto arr = kv.second;
      arr->EntityDestroyed(entity);
    }
  }

  template <typename T>
  ComponentType GetComponentType() {
    auto typeStr = typeid(T).name();
    if (mRegisteredTypes.find(typeStr) == mRegisteredTypes.end())
      LOGE("component type {} not registered", typeStr);
    return mNextComponentType[typeStr];
  }

private:
  std::unordered_map<const char *, ComponentType> mRegisteredTypes{};
  std::unordered_map<const char *, std::shared_ptr<IComponentArray>>
      mComponentArrays{};
  ComponentType mNextComponentType{};

  template <typename T>
  std::shared_ptr<ComponentArray<T>> GetComponentArray() {
    auto typeString = typeid(T).name();
    if (mRegisteredTypes.find(typeString) == mRegisteredTypes.end())
      LOGE("component type {} not registered", typeString);
    return std::static_pointer_cast<ComponentArray<T>>(
        mComponentArrays[typeString]);
  }
};
} // namespace vrtr::ECS
