#pragma once
#include "utils/vk/common.hpp"
#include "utils/DeletionQueue.hpp"
#include "utils/vk/descriptors.hpp"
#include "utils/vk/allocation.hpp"

struct FrameData {
  VkCommandPool cmd_pool;
  VkCommandBuffer cmd_buffer_main;

  // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
  // Sync between GPU queues, one operation wait another to signal a semaphore.
  VkSemaphore swapchain_semaphore, render_semaphore; // Two one-way channels.
  // Sync between CPU and GPU, CPU waits for some GPU operations to finish.
  VkFence render_fence;

  DeletionQueue deletion_queue;
  DescriptorAllocator descriptor_allocator;

  AllocatedBuffer scene_data_buffer;
};