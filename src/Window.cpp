#include "Window.hpp"

void vrtr::Window::init(const Json &config) {
  SDL_Init(SDL_INIT_VIDEO);

  int w = fetchRequired<int>(config, "width");
  int h = fetchRequired<int>(config, "height");
  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);
  // (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  mWindow = SDL_CreateWindow("Vulkan Engine", w, h, window_flags);
}
void vrtr::Window::deinit() { SDL_DestroyWindow(mWindow); }

std::vector<const char *> vrtr::Window::GetRequiredExtensions() const {
  uint32_t sdlExtCount = 0;
  auto sdlExtPtr = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
  if (sdlExtPtr == NULL) {
    LOGE("cannot get sdl vulkan instance extensions");
    return {};
  }
  std::vector<const char *> extVec{sdlExtPtr, sdlExtPtr + sdlExtCount};
  return extVec;
}
SDL_Window *vrtr::Window::getSDLHandle() const { return mWindow; }

VkSurfaceKHR vrtr::Window::CreateCurface(VkInstance instance) const {
  // TODO Error check.
  VkSurfaceKHR surface{nullptr};
  SDL_Vulkan_CreateSurface(mWindow, instance, nullptr, &surface);
  return surface;
}
