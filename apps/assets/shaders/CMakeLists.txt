find_program(GLSL_VALIDATOR glslangValidator HINTS $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

file(GLOB_RECURSE GLSL_SOURCE_FILES
  "*.frag"
  "*.vert"
  "*.comp"
)

message(STATUS "BUILDING SHADER")

foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/apps/assets/shaders/${FILE_NAME}.spv")
  set(COMPILE_CMD ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV})
  message(STATUS ${GLSL})
  # message(STATUS "COMMAND ${COMPILE_CMD}")

  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${COMPILE_CMD}
    DEPENDS ${GLSL}
  )

  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

message(STATUS "targets ${SPIRV_BINARY_FILES}")

add_custom_target(
  Shaders
  DEPENDS ${SPIRV_BINARY_FILES}
)