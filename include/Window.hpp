#pragma once
#include "utils/json.hpp"
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

namespace vrtr {
class Window {
public:
  Window() {}
  void init(const Json &config);
  void deinit();

  std::vector<const char *> GetRequiredExtensions() const;

  SDL_Window *getSDLHandle() const;

  VkSurfaceKHR CreateCurface(VkInstance instance) const;

private:
  SDL_Window *mWindow;
};
} // namespace vrtr