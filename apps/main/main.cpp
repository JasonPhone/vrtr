#include "engine.h"
#include <iostream>

int main(int, char *[]) {
  Engine engine{};
  std::cout << "init" << std::endl;
  engine.init();
  fmt::print("run\n");
  engine.run();
  fmt::print("cleanup\n");
  engine.cleanup();

  return 0;
}