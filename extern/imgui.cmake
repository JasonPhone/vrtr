cmake_minimum_required(VERSION 3.5)

project(imgui
  VERSION 1.89.4
  LANGUAGES CXX
  DESCRIPTION "ImGui as library."
  HOMEPAGE_URL "https://github.com/ocornut/imgui"
)

set(IMGUI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ImGUI)
set(IMGUI_BACKEND_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ImGUI/backends)

# Headers
set(IMGUI_PUBLIC_HEADERS
  ${IMGUI_SRC_DIR}/imconfig.h
  ${IMGUI_SRC_DIR}/imgui_internal.h
  ${IMGUI_SRC_DIR}/imgui.h
  ${IMGUI_SRC_DIR}/imstb_rectpack.h
  ${IMGUI_SRC_DIR}/imstb_textedit.h
  ${IMGUI_SRC_DIR}/imstb_truetype.h

  # Backends.
  ${IMGUI_BACKEND_DIR}/imgui_impl_sdl3.h
  ${IMGUI_BACKEND_DIR}/imgui_impl_vulkan.h
)

# Source files
set(IMGUI_PUBLIC_SOURCES
  ${IMGUI_SRC_DIR}/imgui_demo.cpp
  ${IMGUI_SRC_DIR}/imgui_draw.cpp
  ${IMGUI_SRC_DIR}/imgui_tables.cpp
  ${IMGUI_SRC_DIR}/imgui_widgets.cpp
  ${IMGUI_SRC_DIR}/imgui.cpp

  # Backends.
  ${IMGUI_BACKEND_DIR}/imgui_impl_sdl3.cpp
  ${IMGUI_BACKEND_DIR}/imgui_impl_vulkan.cpp
)

# Fonts
# file(GLOB FONTS ${IMGUI_SRC_DIR}/fonts/*.ttf)
add_library(${PROJECT_NAME}
  STATIC
  ${IMGUI_PUBLIC_HEADERS}
  ${IMGUI_PUBLIC_SOURCES}
)

set(ENV{VULKAN_SDK} "C:/VulkanSDK/1.3.290.0")
find_package(Vulkan REQUIRED)

# add_subdirectory(SDL EXCLUDE_FROM_ALL)
target_include_directories(${PROJECT_NAME}
  PRIVATE
  ${IMGUI_SRC_DIR}
  ${IMGUI_BACKEND_DIR}
  ${Vulkan_INCLUDE_DIRS}
  ./SDL/include
)
