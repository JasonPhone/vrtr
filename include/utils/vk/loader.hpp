#pragma once
#include "utils/vk/common.hpp"
#include "renderable.h"
#include <filesystem>

std::optional<std::shared_ptr<LoadedGLTF>>
loadGltf(Engine *engine, std::filesystem::path file_path);
