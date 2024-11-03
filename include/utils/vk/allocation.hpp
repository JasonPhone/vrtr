#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-extension"
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#include <vk_mem_alloc.h>
#pragma clang diagnostic pop

/**
 * @brief Separate image.
 *        Images from swapchain are not guaranteed in formats
 *        (may be low precision) and have fixed resolution only.
 */
struct AllocatedImage {
  VkImage image;
  VkImageView view;
  VkExtent3D extent;
  VkFormat format;

  VkDevice device;
  VmaAllocation allocation;
  VmaAllocator allocator;
  void destroy() {
    vkDestroyImageView(device, view, nullptr);
    vmaDestroyImage(allocator, image, allocation);
  }
};
/**
 * @brief Push data to shader using 'Buffer Device Address'.
 */
struct AllocatedBuffer {
  VkBuffer buffer;

  VmaAllocation allocation;
  VmaAllocationInfo alloc_info;
  VmaAllocator allocator;
  void destroy() {
    vmaDestroyBuffer(allocator, buffer, allocation);
  }
};