/// An array to store the same type of components' data in a tight layout.
#pragma once

/// When a component is added to an entity, 
/// we append it to the end of array.

/// When a component is removed from an entity, 
/// we move last one in the array to replace its position, 
/// so it is still tight.

namespace vrtr::ECS {
class IComponentArray {
  public:
  virtual ~IComponentArray() = 0;
  virtual void EntityDestroyed() = 0;
};

}