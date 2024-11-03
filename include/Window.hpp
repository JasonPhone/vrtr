#pragma once
#include "utils/json.hpp"
#include <SDL3/SDL.h>

namespace vrtr {
class Window {
public:
  Window() {}
  void init(const Json &config) {
    SDL_Init(SDL_INIT_VIDEO);

    int w = fetchRequired<int>(config, "width");
    int h = fetchRequired<int>(config, "height");
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);
    // (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    m_window = SDL_CreateWindow("Vulkan Engine", w, h, window_flags);
  }
  void deinit() {
    SDL_DestroyWindow(m_window);
  }
  SDL_Window *getSDLHandle() const { return m_window; }

private:
  SDL_Window *m_window;
};
} // namespace vrtr