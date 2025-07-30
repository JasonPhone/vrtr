#pragma once
#include "Application.hpp"
#include "Window.hpp"
#include "utils/json.hpp"
#include "utils/log.hpp"
#include <thread>
#include <chrono>

namespace vrtr {
class Platform {
public:
  void init(const Json &config) {
    LOGI("platform init.");
    mWindow.init(fetchRequired<Json>(config, "window"));
    mApplication.Create(fetchRequired<Json>(config, "engine"), &mWindow);
  }
  void deinit() {
    LOGI("platform deinit.");
    mApplication.Destroy();
    mWindow.deinit();
  }
  void run() {
    LOGI("platform run.");
    /// Main loop here.
    while (!m_app_status.should_quit) {
      pollSDLEvent();
      if (m_app_status.stop_rendering) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      mApplication.TickLogic(1000.f / 60);
      mApplication.TickRender(1000.f / 60);
    }
    mApplication.WaitDevice();
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
  Window mWindow;
  Application mApplication;
};

} // namespace vrtr