project("main")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/")

# message(STATUS "PROJECT_SOURCE_DIR: ${PROJECT_SOURCE_DIR}")
# message(STATUS "CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
# message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
# find_package(glfw3 CONFIG REQUIRED)
message(STATUS "Project ${PROJECT_NAME}")

if(WIN32)
  message(STATUS "Running on Windows.")
  set(HEADER_DIR "${CMAKE_SOURCE_DIR}/include")
  set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")

  set(ENV{VULKAN_SDK} "C:/VulkanSDK/1.3.290.0")
  find_package(Vulkan REQUIRED)

  add_library(${PROJECT_NAME}_runtime STATIC
    ${SOURCE_DIR}/GPU/GPU.cpp

    ${SOURCE_DIR}/utils/vk/descriptors.cpp
    ${SOURCE_DIR}/utils/vk/images.cpp
    ${SOURCE_DIR}/utils/vk/initializers.cpp
    # ${SOURCE_DIR}/utils/vk/loader.cpp
    ${SOURCE_DIR}/utils/vk/pipelines.cpp
  )
  target_link_libraries(${PROJECT_NAME}_runtime PRIVATE
    fastgltf
    spdlog
  )
  target_include_directories(${PROJECT_NAME}_runtime PRIVATE
    ${Vulkan_INCLUDE_DIRS}

    ${CMAKE_SOURCE_DIR}/extern/GLM
    ${CMAKE_SOURCE_DIR}/extern/SDL/include
    ${CMAKE_SOURCE_DIR}/extern/ImGUI/
    ${CMAKE_SOURCE_DIR}/extern/stb
    ${CMAKE_SOURCE_DIR}/extern/tinyobjloader
    ${CMAKE_SOURCE_DIR}/extern/vk-bootstrap/src
    ${CMAKE_SOURCE_DIR}/extern/VMA/include
    ${CMAKE_SOURCE_DIR}/extern/fmt/include
    ${CMAKE_SOURCE_DIR}/extern/fastgltf/include

    # ${CMAKE_SOURCE_DIR}/extern/spdlog/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
  )

  add_executable(${PROJECT_NAME} main.cpp)

  target_include_directories(${PROJECT_NAME} PRIVATE
    ${Vulkan_INCLUDE_DIRS}

    ${CMAKE_SOURCE_DIR}/extern/GLM
    ${CMAKE_SOURCE_DIR}/extern/SDL/include
    ${CMAKE_SOURCE_DIR}/extern/ImGUI/
    ${CMAKE_SOURCE_DIR}/extern/stb
    ${CMAKE_SOURCE_DIR}/extern/tinyobjloader
    ${CMAKE_SOURCE_DIR}/extern/vk-bootstrap/src
    ${CMAKE_SOURCE_DIR}/extern/VMA/include
    ${CMAKE_SOURCE_DIR}/extern/fmt/include
    ${CMAKE_SOURCE_DIR}/extern/json/include

    # ${CMAKE_SOURCE_DIR}/extern/spdlog/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
  )

  target_link_libraries(${PROJECT_NAME} PRIVATE

    # External binaries.
    ${Vulkan_LIBRARIES}
    SDL3::SDL3
    tinyobjloader
    imgui
    json
    vk-bootstrap::vk-bootstrap
    stb
    spdlog

    # vma # Impl in ./src/vk_engine.cpp
    fmt::fmt

    # Project binaries.
    ${PROJECT_NAME}_runtime
  )

  add_dependencies(${PROJECT_NAME} Shaders)
  target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)

  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/bin/"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
  )
elseif(UNIX)
  message(FATAL_ERROR "Running on Unix. No impl.")

# find_package(glfw3 REQUIRED)
# find_package(Vulkan REQUIRED)
# add_executable(${PROJECT_NAME} main.cpp)
# target_link_libraries(${PROJECT_NAME} glfw vulkan)
else()
  message(FATAL_ERROR "Unknown host system.")
endif()
