cmake_minimum_required(VERSION 3.5)

project(json
  LANGUAGES CXX
  DESCRIPTION "Nlohmann_json as library."
  HOMEPAGE_URL "https://github.com/nlohmann/json"
)


set(JSON_BuildTests OFF CACHE INTERNAL "")

add_subdirectory(json)
add_library(${PROJECT_NAME}
  STATIC
  json.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)