#pragma once
#include "ECS/Systems.hpp"
#include "ECS/Components.hpp"

#include <unordered_map>
#include <typeinfo>
#include <memory>

namespace vrtr::ECS {
class SystemManager {
public:
  template <typename T> 
  std::shared_ptr<T> RegisterSystem() {
    const char *typeName = typeid(T).name();
    if (mSystems.find(typeName) == mSystems.end()) {
      LOGE("System {} already registered", typeName);
      return nullptr;
    }
    auto system = std::make_shared<T>();
    mSystems.insert({typeName, system});
    return system;
  }

  template <typename T>
  void SetSignature(Signature signature) {
    const char *typeName = typeid(T).name();
    if (mSystems.find(typeName) == mSystems.end()) {
      LOGE("System {} already registered", typeName);
      return;
    }
    mSignatures.insert({typeName, signature});
  }

  void EntityDestroyed(Entity entity) {
    for (auto const &pair : mSystems) {
      auto const &system = pair.second;
      system->mEntities.erase(entity);
    }
  }

  void EntitySignatureChanged(Entity entity, Signature entitySignature) {
    for (auto const &pair : mSystems) {
      auto const &type = pair.first;
      auto const &system = pair.second;
      auto const &systemSignature = mSignatures[type];

      // 如果Entity包含了System所需的所有Component
      if ((entitySignature & systemSignature) == systemSignature) {
        system->mEntities.insert(entity);
      }
      // 否则删除该Entity（说明此时System需要的某些Component没有被该Entity包含）
      // 此时System无法处理该Entity
      else {
        system->mEntities.erase(entity);
      }
    }
  }

private:
  // The components each system focuses on.
  std::unordered_map<const char *, Signature> mSignatures{};
  std::unordered_map<const char *, std::shared_ptr<System>> mSystems{};
};
} // namespace vrtr::ECS