#pragma once
#include "vk_types.h"
#include "renderable.h"
#include <unordered_map>
#include <filesystem>

std::optional<std::vector<std::shared_ptr<MeshAsset>>>
loadGltfMeshes(Engine *engine, std::filesystem::path file_path);

std::optional<std::shared_ptr<LoadedGLTF>>
loadGltf(Engine *engine, std::filesystem::path file_path);
