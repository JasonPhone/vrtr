#pragma once

#include "Window.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vrtr {
/**
 * Frame manager of the whole application.
 * Forward vulkan resource requests to active frame.
 *
 *
 */
class RenderContext {
public:
  RenderContext(vk::Device &device, vk::SurfaceKHR &surface,
                const Window &window) {}
  RenderContext(const RenderContext &) = delete;
  RenderContext(RenderContext &&) = delete;
  RenderContext &operator=(const RenderContext &) = delete;
  RenderContext &operator=(RenderContext &&) = delete;
};
} // namespace vrtr