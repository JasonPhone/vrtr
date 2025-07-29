#pragma once

#include "Window.hpp"

#include <vulkan/vulkan.hpp>

namespace vrtr {
  /**
   * @brief Frame manager of the whole application.
   * 
   */
class RenderContext {
public:
  RenderContext(vk::Device& device, vk::SurfaceKHR& surface, const Window& window) {

  }
  RenderContext(const RenderContext &) = delete;
  RenderContext(RenderContext &&) = delete;
  RenderContext &operator=(const RenderContext &) = delete;
  RenderContext &operator=(RenderContext &&) = delete;
};
} // namespace vrtr