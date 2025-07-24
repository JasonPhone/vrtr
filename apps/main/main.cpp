#include "Application.hpp"
#include "utils/json.hpp"

#include "ECS/EntityManager.hpp"

int main(int, char *[]) {

  auto mngr = new vrtr::ECS::EntityManager{};
//   Json config = nlohmann::json::parse(
//       R"({"name": "vrtr", "window": {"width": 1280, "height": 720}, "engine": {}})");
// #ifdef NDEBUG
//     spdlog::set_level(spdlog::level::info);
// #else
//     spdlog::set_level(spdlog::level::debug);
// #endif
//   vrtr::Application app;
//   app.init(config);
//   app.run();
//   app.deinit();

  return 0;
}
