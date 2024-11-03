#pragma once
#include "Engine.hpp"
#include "Window.hpp"
#include "utils/json.hpp"
#include "utils/log.hpp"
#include <thread>
#include <chrono>

namespace vrtr {
class Application {
public:
  void init(const Json &config) {
    LOGI("Application init.");
    m_window.init(fetchRequired<Json>(config, "window"));
    m_engine.init(fetchRequired<Json>(config, "engine"), &m_window);
  }
  void deinit() {
    LOGI("Application deinit.");
    m_engine.deinit();
    m_window.deinit();
  }
  void run() {
    LOGI("Application run.");
    /// Main loop here.
    while (!m_app_status.should_quit) {
      pollSDLEvent();
      if (m_app_status.stop_rendering) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      m_engine.tick(1000.f / 60);
      m_engine.draw();
    }
  }

private:
  struct AppStatus {
    bool should_quit = false;
    bool stop_rendering = false;
  } m_app_status;

  void pollSDLEvent() {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_EVENT_QUIT)
        m_app_status.should_quit = true;
      if (event.type >= SDL_EVENT_WINDOW_FIRST &&
          event.type <= SDL_EVENT_WINDOW_LAST) {
        if (event.window.type == SDL_EVENT_WINDOW_MINIMIZED)
          m_app_status.stop_rendering = true;
        if (event.window.type == SDL_EVENT_WINDOW_RESTORED)
          m_app_status.stop_rendering = false;
      }
    }
  }
  Window m_window;
  Engine m_engine;
};

} // namespace vrtr