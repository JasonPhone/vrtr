#include "Application.hpp"
#include "utils/json.hpp"

int main(int, char *[]) {
  Json config = nlohmann::json::parse(
      R"({"name": "vrtr", "window": {"width": 1920, "height": 1080}, "engine": {}})");
  vrtr::Application app;
  app.init(config);
  app.run();
  app.deinit();
  return 0;
}
