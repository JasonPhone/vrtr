#pragma once
#include "utils/log.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) {                                                                 \
      LOGE("Detected Vulkan error: {}, {}:{}", string_VkResult(err), __FILE__, \
           __LINE__);                                                          \
      abort();                                                                 \
    }                                                                          \
  } while (0)

#define VK_ONE_SEC 1000000000