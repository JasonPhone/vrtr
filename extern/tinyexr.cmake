cmake_minimum_required(VERSION 3.5)

project(tinyexr
  LANGUAGES CXX
  DESCRIPTION "tinyexr as library."
  HOMEPAGE_URL "https://github.com/syoyo/tinyexr"
)

add_library(${PROJECT_NAME}
  STATIC
  ./tinyexr.cpp
  ./tinyexr/tinyexr.cc
  ./tinyexr/deps/miniz/miniz.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
  tinyexr
  tinyexr/deps/miniz
)
