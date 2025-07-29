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

  SDL_Window *GetSDLHandle() const;

  VkSurfaceKHR CreateCurface(VkInstance instance) const;

  VkExtent2D GetWindowSize() const;

private:
  SDL_Window *mWindow;
};
} // namespace vrtr