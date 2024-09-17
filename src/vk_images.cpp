#include "vk_images.h"
#include "vk_initializers.h"

void vkutil::transitionImage(VkCommandBuffer cmd, VkImage image,
                             VkImageLayout cur_layout,
                             VkImageLayout new_layout) {
  // Pipeline barrier.
  VkImageMemoryBarrier2 img_barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  img_barrier.pNext = nullptr;

  img_barrier.srcStageMask =
      VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; // Can be more specific.
  img_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
  img_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  img_barrier.dstAccessMask =
      VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

  img_barrier.oldLayout = cur_layout;
  img_barrier.newLayout = new_layout;

  VkImageAspectFlags aspectMask =
      (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
          ? VK_IMAGE_ASPECT_DEPTH_BIT
          : VK_IMAGE_ASPECT_COLOR_BIT;
  img_barrier.subresourceRange = vkinit::imageSubresourceRange(aspectMask);
  img_barrier.image = image;

  VkDependencyInfo dep_info{};
  dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep_info.pNext = nullptr;

  dep_info.imageMemoryBarrierCount =
      1; // Transit multiple images at once will be faster.
  dep_info.pImageMemoryBarriers = &img_barrier;

  vkCmdPipelineBarrier2(cmd, &dep_info);
}

void vkutil::copyImage(VkCommandBuffer cmd, VkImage src, VkImage dst,
                       VkExtent2D src_size, VkExtent2D dst_size) {
  VkImageBlit2 blit_region{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
                           .pNext = nullptr};

  blit_region.srcOffsets[1].x = src_size.width;
  blit_region.srcOffsets[1].y = src_size.height;
  blit_region.srcOffsets[1].z = 1;

  blit_region.dstOffsets[1].x = dst_size.width;
  blit_region.dstOffsets[1].y = dst_size.height;
  blit_region.dstOffsets[1].z = 1;

  blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit_region.srcSubresource.baseArrayLayer = 0;
  blit_region.srcSubresource.layerCount = 1;
  blit_region.srcSubresource.mipLevel = 0;

  blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit_region.dstSubresource.baseArrayLayer = 0;
  blit_region.dstSubresource.layerCount = 1;
  blit_region.dstSubresource.mipLevel = 0;

  VkBlitImageInfo2 blitInfo{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                            .pNext = nullptr};
  blitInfo.dstImage = dst;
  blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  blitInfo.srcImage = src;
  blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  blitInfo.filter = VK_FILTER_LINEAR;
  blitInfo.regionCount = 1;
  blitInfo.pRegions = &blit_region;

  // Slower than vkCmdCopyImage but less limitation.
  vkCmdBlitImage2(cmd, &blitInfo);
}

void vkutil::generateMipmap(VkCommandBuffer cmd, VkImage image,
                            VkExtent2D image_size) {
  // Compute shader is faster and generates all levels at once.
  int mip_level = int(std::floor(std::log2(
                      std::max(image_size.width, image_size.height)))) +
                  1;
  for (int mip = 0; mip < mip_level; mip++) {
    VkExtent2D half_size = image_size;
    half_size.width /= 2;
    half_size.height /= 2;

    VkImageMemoryBarrier2 img_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .pNext = nullptr};

    img_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    img_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    img_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    img_barrier.dstAccessMask =
        VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
    img_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    img_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    img_barrier.subresourceRange = vkinit::imageSubresourceRange(aspectMask);
    img_barrier.subresourceRange.levelCount = 1;
    img_barrier.subresourceRange.baseMipLevel = mip;
    img_barrier.image = image;

    VkDependencyInfo depInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                             .pNext = nullptr};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &img_barrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);

    if (mip < mip_level - 1) {
      VkImageBlit2 blitRegion{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
                              .pNext = nullptr};
      blitRegion.srcOffsets[1].x = image_size.width;
      blitRegion.srcOffsets[1].y = image_size.height;
      blitRegion.srcOffsets[1].z = 1;
      blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blitRegion.srcSubresource.baseArrayLayer = 0;
      blitRegion.srcSubresource.layerCount = 1;
      blitRegion.srcSubresource.mipLevel = mip;

      blitRegion.dstOffsets[1].x = half_size.width;
      blitRegion.dstOffsets[1].y = half_size.height;
      blitRegion.dstOffsets[1].z = 1;
      blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blitRegion.dstSubresource.baseArrayLayer = 0;
      blitRegion.dstSubresource.layerCount = 1;
      blitRegion.dstSubresource.mipLevel = mip + 1;

      VkBlitImageInfo2 blitInfo{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                                .pNext = nullptr};
      blitInfo.dstImage = image;
      blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      blitInfo.srcImage = image;
      blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      blitInfo.filter = VK_FILTER_LINEAR;
      blitInfo.regionCount = 1;
      blitInfo.pRegions = &blitRegion;
      vkCmdBlitImage2(cmd, &blitInfo);

      image_size = half_size;
    }
  }
  transitionImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}