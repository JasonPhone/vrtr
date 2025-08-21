#include "Platform.hpp"
#include "utils/json.hpp"

void runEngine() {
  Json config = nlohmann::json::parse(
      R"({"name": "vrtr", "window": {"width": 1280, "height": 720}, "engine": {}})");
#ifdef NDEBUG
  spdlog::set_level(spdlog::level::info);
#else
  spdlog::set_level(spdlog::level::debug);
#endif
  try {
    vrtr::Platform platform;
    platform.init(config);
    platform.run();
    platform.deinit();
  } catch (const vk::SystemError &err) {
    LOGE("vk::SystemError - code: {} ", err.code().message());
    LOGE("vk::SystemError - what: {}", err.what());
  } catch (const std::exception &err) {
    LOGE("vk::SystemError - what: {}", err.what());
  }
}

int main(int, char *[]) {

  runEngine();

  return 0;
}
