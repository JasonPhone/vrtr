cmake_minimum_required(VERSION 3.5)

project(stb
  LANGUAGES CXX
  DESCRIPTION "stb as library."
  HOMEPAGE_URL "https://github.com/nothings/stb"
)

add_library(${PROJECT_NAME}
  STATIC
  ./stb.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
  stb
)
