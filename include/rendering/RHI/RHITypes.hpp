#pragma once

#include <cstdint>
#include <vulkan/vulkan.hpp>

namespace vrtr::rhi {

using RHITextureRef = std::shared_ptr<class RHITexture>;
using RHIFenceRef = std::shared_ptr<class RHIFence>;
using RHISemaphoreRef = std::shared_ptr<class RHISemaphore>;
using RHICommandContextRef = std::shared_ptr<class RHICommandContext>;
using RHICommandListRef = std::shared_ptr<class RHICommandList>;

using Extent2D = vk::Extent2D;

enum class RHIResourceType : uint32_t {
  None = 0,
  Queue,
  Surface,
  Swapchain,
  CommandPool,

  Count,
};
enum class QueueType : uint32_t {
  None = 0,
  Graphics,
  Compute,

  Count,
};
struct RHIQueueInfo {
  QueueType type;
};

struct RHISwapchainInfo {

};

struct RHICommandPoolInfo {

};

} // namespace vrtr::rhi