#pragma once
#include "utils/vk/common.hpp"
#include "utils/vk/allocation.hpp"
#include "utils/vk/buffers.hpp"
#include "utils/vk/initializers.hpp"
#include <vulkan/vulkan.h>
#include <algorithm>

namespace vkimage {

class ImageBuilder {
public:
  ImageBuilder &setUsage(VkImageUsageFlags usage) {
    m_usage = usage;
    return *this;
  }
  ImageBuilder &addUsage(VkImageUsageFlagBits usage) {
    m_usage |= usage;
    return *this;
  }
  ImageBuilder &setExtent(uint32_t width, uint32_t height, uint32_t depth) {
    m_extent.width = width;
    m_extent.height = height;
    m_extent.depth = depth;
    return *this;
  }
  ImageBuilder &setFormat(VkFormat format) {
    m_format = format;
    return *this;
  }
  AllocatedImage build(VkDevice device, VmaAllocator &allocator,
                       bool mipmap = false) {
    AllocatedImage image;
    image.format = m_format;
    image.extent = m_extent;
    image.device = device;
    image.allocator = allocator;

    VkImageCreateInfo ci_image =
        vkinit::imageCreateInfo(m_format, m_usage, m_extent);
    if (mipmap)
      ci_image.mipLevels = static_cast<uint32_t>(std::floor(std::log2(
                               std::max(m_extent.width, m_extent.height)))) +
                           1;

    VmaAllocationCreateInfo ci_alloc = {};
    ci_alloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    ci_alloc.requiredFlags =
        // Double check the allocation is in VRAM.
        VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vmaCreateImage(allocator, &ci_image, &ci_alloc, &image.image,
                            &image.allocation, nullptr));
    VkImageAspectFlags aspect_flags = m_format == VK_FORMAT_D32_SFLOAT
                                          ? VK_IMAGE_ASPECT_DEPTH_BIT
                                          : VK_IMAGE_ASPECT_COLOR_BIT;
    VkImageViewCreateInfo ci_view =
        vkinit::imageViewCreateInfo(m_format, image.image, aspect_flags);
    ci_view.subresourceRange.levelCount = ci_image.mipLevels;
    VK_CHECK(vkCreateImageView(device, &ci_view, nullptr, &image.view));
    return image;
  }

private:
  VkImageUsageFlags m_usage = 0;
  VkExtent3D m_extent = {};
  VkFormat m_format = {};
};

void transitionImage(VkCommandBuffer cmd, VkImage image,
                     VkImageLayout cur_layout, VkImageLayout new_layout);
void copyImage(VkCommandBuffer cmd, VkImage src, VkImage dst,
               VkExtent2D src_size, VkExtent2D dst_size);
void generateMipmap(VkCommandBuffer cmd, VkImage image, VkExtent2D image_size);
} // namespace vkimage