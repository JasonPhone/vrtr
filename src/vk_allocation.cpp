#include "vk_allocation.h"
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

struct AllocatedImage::AllocationImpl {
  VmaAllocation allocation;
};
AllocatedImage::AllocatedImage()
    : alloc_impl(std::make_unique<AllocationImpl>()) {}

struct Allocator::AllocatorImpl {
  VmaAllocator allocator;
};
Allocator::Allocator() : alloc_impl(std::make_unique<AllocatorImpl>()) {}

void Allocator::create(VkPhysicalDevice chosen_GPU, VkDevice device,
                       VkInstance instance) {
  VmaAllocatorCreateInfo alloc_info = {};
  alloc_info.physicalDevice = chosen_GPU;
  alloc_info.device = device;
  alloc_info.instance = instance;
  alloc_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vmaCreateAllocator(&alloc_info, &(alloc_impl->allocator));
}
void Allocator::destroy() { vmaDestroyAllocator(alloc_impl->allocator); }