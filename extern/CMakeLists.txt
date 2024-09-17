add_subdirectory(tinyobjloader)
add_subdirectory(SDL EXCLUDE_FROM_ALL)
add_subdirectory(vk-bootstrap)
add_subdirectory(fmt)
add_subdirectory(fastgltf)

include(imgui.cmake)
# Impl in ./src/engine.cpp
# include(vma.cmake)
include(stb.cmake)