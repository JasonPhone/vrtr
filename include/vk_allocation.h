#pragma once
#include "vk_types.h"

struct AllocatedImage {
  AllocatedImage();
  VkImage image;
  VkImageView image_view;
  VkExtent3D image_extent;
  VkFormat image_format;


  struct AllocationImpl;
  std::unique_ptr<AllocationImpl> alloc_impl;

};

struct Allocator {
  Allocator();
  void create(VkPhysicalDevice chosen_gpu, VkDevice device, VkInstance instance);
  void destroy();
  struct AllocatorImpl;
  std::unique_ptr<AllocatorImpl> alloc_impl;
};