#include <stb_image.h>
#include <iostream>
#include "vk_loader.h"

#include "engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include "stb_image.h"
#include "stb_image_write.h"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>

VkFilter extractFilter(fastgltf::Filter filter) {
  switch (filter) {
  case fastgltf::Filter::Nearest:
  case fastgltf::Filter::NearestMipMapNearest:
  case fastgltf::Filter::NearestMipMapLinear:
    return VK_FILTER_NEAREST;
  case fastgltf::Filter::Linear:
  case fastgltf::Filter::LinearMipMapNearest:
  case fastgltf::Filter::LinearMipMapLinear:
  default:
    return VK_FILTER_LINEAR;
  }
}
VkSamplerMipmapMode extractMipmapMode(fastgltf::Filter filter) {
  switch (filter) {
  case fastgltf::Filter::NearestMipMapNearest:
  case fastgltf::Filter::LinearMipMapNearest:
    return VK_SAMPLER_MIPMAP_MODE_NEAREST;
  case fastgltf::Filter::NearestMipMapLinear:
  case fastgltf::Filter::LinearMipMapLinear:
  default:
    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
}
std::optional<AllocatedImage> loadImage(Engine *engine, fastgltf::Asset &asset,
                                        const fastgltf::Image &image) {
  bool mipmap = true;
  AllocatedImage newImage{};
  int w, h, n_channels;
  std::visit(
      fastgltf::visitor{
          [](auto &) {},
          [&](fastgltf::sources::URI &filePath) {
            // fmt::println("file path {}", filePath.uri.c_str());
            // We don't support offsets with stbi.
            assert(filePath.fileByteOffset == 0);
            assert(filePath.uri.isLocalPath());
            const std::string path(filePath.uri.path().begin(),
                                   filePath.uri.path().end());
            unsigned char *data =
                stbi_load(path.c_str(), &w, &h, &n_channels, 4);
            if (data) {
              VkExtent3D img_size;
              img_size.width = w;
              img_size.height = h;
              img_size.depth = 1;
              newImage =
                  engine->createImage(data, img_size, VK_FORMAT_R8G8B8A8_UNORM,
                                      VK_IMAGE_USAGE_SAMPLED_BIT, mipmap);
              stbi_image_free(data);
            }
          },
          [&](fastgltf::sources::Vector &vector) {
            // fmt::println("source vector");
            unsigned char *data = stbi_load_from_memory(
                (unsigned char *)vector.bytes.data(),
                static_cast<int>(vector.bytes.size()), &w, &h, &n_channels, 4);
            if (data) {
              VkExtent3D img_size;
              img_size.width = w;
              img_size.height = h;
              img_size.depth = 1;
              newImage =
                  engine->createImage(data, img_size, VK_FORMAT_R8G8B8A8_UNORM,
                                      VK_IMAGE_USAGE_SAMPLED_BIT, mipmap);
              stbi_image_free(data);
            }
          },
          [&](fastgltf::sources::BufferView &view) {
            auto &bufferView = asset.bufferViews[view.bufferViewIndex];
            auto &buffer = asset.buffers[bufferView.bufferIndex];
            std::visit(fastgltf::visitor{
                           [](auto &) { fmt::println("empty"); },
                           [&](fastgltf::sources::Array &arr) {
                             unsigned char *data = stbi_load_from_memory(
                                 (unsigned char *)arr.bytes.data() +
                                     bufferView.byteOffset,
                                 static_cast<int>(bufferView.byteLength), &w,
                                 &h, &n_channels, 4);
                             if (data) {
                               VkExtent3D imagesize;
                               imagesize.width = w;
                               imagesize.height = h;
                               imagesize.depth = 1;
                               newImage = engine->createImage(
                                   data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                   VK_IMAGE_USAGE_SAMPLED_BIT, mipmap);

                               stbi_image_free(data);
                             }
                           },
                       },
                       buffer.data);
          },
      },
      image.data);
  // if any of the attempts to load the data failed, we haven't written the
  // image so handle is null
  if (newImage.image == VK_NULL_HANDLE) {
    return {};
  } else {
    return newImage;
  }
}

std::optional<std::shared_ptr<LoadedGLTF>>
loadGltf(Engine *engine, std::filesystem::path file_path) {
  fmt::println("Loading GLTF: {}", file_path.string());
  std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
  scene->creator = engine;

  fastgltf::Parser parser{};
  constexpr auto kGltfOptions = fastgltf::Options::DontRequireValidAssetMember |
                                fastgltf::Options::AllowDouble |
                                fastgltf::Options::LoadExternalBuffers;

  auto data = fastgltf::GltfDataBuffer::FromPath(file_path);
  if (data.error() != fastgltf::Error::None) {
    std::cerr << "Error loading GLTF from file." << std::endl;
    return {};
  }
  fastgltf::Asset gltf;

  std::filesystem::path path = file_path;
  auto type = fastgltf::determineGltfFileType(data.get());
  if (type == fastgltf::GltfType::glTF) {
    auto load = parser.loadGltf(data.get(), path.parent_path(), kGltfOptions);
    if (load) {
      gltf = std::move(load.get());
    } else {
      std::cerr << "Failed to load glTF: "
                << fastgltf::to_underlying(load.error()) << std::endl;
      return {};
    }
  } else if (type == fastgltf::GltfType::GLB) {
    auto load =
        parser.loadGltfBinary(data.get(), path.parent_path(), kGltfOptions);
    if (load) {
      gltf = std::move(load.get());
    } else {
      std::cerr << "Failed to load glTF: "
                << fastgltf::to_underlying(load.error()) << std::endl;
      return {};
    }
  } else {
    std::cerr << "Failed to determine glTF container" << std::endl;
    return {};
  }

  /// For each material, in one single desc pool.
  std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};
  scene->descriptor_pool.initPool(engine->m_device, gltf.materials.size(),
                                  sizes);

  /// load samplers
  for (const fastgltf::Sampler &sampler : gltf.samplers) {
    VkSamplerCreateInfo ci_sampler = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
    ci_sampler.maxLod = VK_LOD_CLAMP_NONE;
    ci_sampler.minLod = 0;
    ci_sampler.magFilter =
        extractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
    ci_sampler.minFilter =
        extractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
    ci_sampler.mipmapMode = extractMipmapMode(
        sampler.minFilter.value_or(fastgltf::Filter::Nearest));
    /// TODO Remove device requirement.
    VkSampler new_sampler;
    VK_CHECK(
        vkCreateSampler(engine->m_device, &ci_sampler, nullptr, &new_sampler));
    scene->samplers.push_back(new_sampler);
  }

  std::vector<AllocatedImage> images;
  for (const fastgltf::Image &image : gltf.images) {
    std::optional<AllocatedImage> img = loadImage(engine, gltf, image);
    if (img.has_value()) {
      images.push_back(*img);
      scene->images[image.name.c_str()] = *img;
    } else {
      images.push_back(engine->m_error_image);
      std::cout << "gltf failed to load texture " << image.name << std::endl;
    }
  }

  std::vector<std::shared_ptr<GLTFMaterial>> materials;
  scene->material_data_buffer = engine->createBuffer(
      sizeof(GLTFMetallicRoughness::MaterialConstants) * gltf.materials.size(),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  GLTFMetallicRoughness::MaterialConstants *scene_material_constants =
      (GLTFMetallicRoughness::MaterialConstants *)
          scene->material_data_buffer.alloc_info.pMappedData;
  int data_index = 0;
  for (const fastgltf::Material &mat : gltf.materials) {
    std::shared_ptr<GLTFMaterial> new_mat = std::make_shared<GLTFMaterial>();
    materials.push_back(new_mat);
    scene->materials[mat.name.c_str()] = new_mat;

    /// MaterialConstants are parameters,
    /// MaterialResources are texture LUTs.
    GLTFMetallicRoughness::MaterialConstants constants;
    constants.color_factors.x = mat.pbrData.baseColorFactor[0];
    constants.color_factors.y = mat.pbrData.baseColorFactor[1];
    constants.color_factors.z = mat.pbrData.baseColorFactor[2];
    constants.color_factors.w = mat.pbrData.baseColorFactor[3];
    constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
    constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;
    /// Write material parameters to buffer.
    scene_material_constants[data_index] = constants;

    GLTFMetallicRoughness::MaterialResources material_res;
    /// Set the uniform buffer for the material constants.
    material_res.data_buffer = scene->material_data_buffer.buffer;
    material_res.data_buffer_offset =
        data_index * sizeof(GLTFMetallicRoughness::MaterialConstants);

    material_res.color_image = engine->m_white_image;
    material_res.color_sampler = engine->m_default_sampler_linear;
    material_res.metal_rough_image = engine->m_white_image;
    material_res.metal_rough_sampler = engine->m_default_sampler_linear;
    /// Grab textures from gltf file.
    if (mat.pbrData.baseColorTexture.has_value()) {
      size_t img_idx =
          gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex]
              .imageIndex.value();
      size_t sampler_idx =
          gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex]
              .samplerIndex.value();
      material_res.color_image = images[img_idx];
      material_res.color_sampler = scene->samplers[sampler_idx];
    }

    MaterialPass pass_type = MaterialPass::BasicMainColor;
    if (mat.alphaMode == fastgltf::AlphaMode::Blend)
      pass_type = MaterialPass::BasicTransparent;
    new_mat->data = engine->m_metal_rough_mat.writeMaterial(
        engine->m_device, pass_type, material_res, scene->descriptor_pool);
    data_index++;
  }

  /// Same vectors for all meshes to avoid too much reallocation.
  std::vector<std::shared_ptr<MeshAsset>> meshes;
  std::vector<uint32_t> indices;
  std::vector<Vertex> vertices;
  for (fastgltf::Mesh &mesh : gltf.meshes) {
    std::shared_ptr<MeshAsset> new_mesh = std::make_shared<MeshAsset>();
    meshes.push_back(new_mesh);
    scene->meshes[mesh.name.c_str()] = new_mesh;
    new_mesh->name = mesh.name;
    indices.clear();
    vertices.clear();
    for (auto &&p : mesh.primitives) {
      GeometrySurface new_surface;
      new_surface.start_index = (uint32_t)indices.size();
      new_surface.count =
          (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;
      size_t initial_vtx = vertices.size();
      { /// Indices.
        fastgltf::Accessor &accessor =
            gltf.accessors[p.indicesAccessor.value()];
        indices.reserve(indices.size() + accessor.count);
        fastgltf::iterateAccessor<std::uint32_t>(
            gltf, accessor,
            [&](std::uint32_t idx) { indices.push_back(idx + initial_vtx); });
      }
      { /// Vertex positions.
        fastgltf::Accessor &posAccessor =
            gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
        vertices.resize(vertices.size() + posAccessor.count);
        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            gltf, posAccessor, [&](glm::vec3 v, size_t index) {
              Vertex new_vert;
              new_vert.position = v;
              new_vert.normal = {1, 0, 0};
              new_vert.color = glm::vec4{1.f};
              new_vert.uv_x = 0;
              new_vert.uv_y = 0;
              vertices[initial_vtx + index] = new_vert;
            });
      }
      { /// Vertex data.
        auto normals = p.findAttribute("NORMAL");
        if (normals != p.attributes.end()) {
          fastgltf::iterateAccessorWithIndex<glm::vec3>(
              gltf, gltf.accessors[(*normals).accessorIndex],
              [&](glm::vec3 v, size_t index) {
                vertices[initial_vtx + index].normal = v;
              });
        }
        auto uv = p.findAttribute("TEXCOORD_0");
        if (uv != p.attributes.end()) {
          fastgltf::iterateAccessorWithIndex<glm::vec2>(
              gltf, gltf.accessors[(*uv).accessorIndex],
              [&](glm::vec2 v, size_t index) {
                vertices[initial_vtx + index].uv_x = v.x;
                vertices[initial_vtx + index].uv_y = v.y;
              });
        }
        auto colors = p.findAttribute("COLOR_0");
        if (colors != p.attributes.end()) {
          fastgltf::iterateAccessorWithIndex<glm::vec4>(
              gltf, gltf.accessors[(*colors).accessorIndex],
              [&](glm::vec4 v, size_t index) {
                vertices[initial_vtx + index].color = v;
              });
        }
      }
      if (p.materialIndex.has_value())
        new_surface.material = materials[p.materialIndex.value()];
      else
        new_surface.material = materials[0];

      /// Bounding sphere, AABB center as center, half AABB diag as radius.
      glm::vec3 min_pos = vertices[initial_vtx].position;
      glm::vec3 max_pos = vertices[initial_vtx].position;
      for (size_t i = initial_vtx; i < vertices.size(); i++) {
        min_pos = glm::min(min_pos, vertices[i].position);
        max_pos = glm::max(max_pos, vertices[i].position);
      }
      new_surface.bound.origin = 0.5f * (min_pos + max_pos);
      new_surface.bound.radius = glm::length(0.5f * (max_pos - min_pos));

      new_mesh->surfaces.push_back(new_surface);
    }
    new_mesh->mesh_buffers = engine->uploadMesh(indices, vertices);
  }

  /// Load all nodes and their meshes.
  std::vector<std::shared_ptr<Node>> nodes;
  for (const fastgltf::Node &node : gltf.nodes) {
    std::shared_ptr<Node> new_node;
    /// If the node has a mesh, hook it to the mesh pointer
    /// and allocate it with the MeshNode class, or just a Node.
    if (node.meshIndex.has_value()) {
      new_node = std::make_shared<MeshNode>();
      static_cast<MeshNode *>(new_node.get())->mesh = meshes[*node.meshIndex];
    } else {
      new_node = std::make_shared<Node>();
    }
    nodes.push_back(new_node);
    scene->nodes[node.name.c_str()];
    std::visit(
        fastgltf::visitor{
            [&](fastgltf::math::fmat4x4 matrix) {
              memcpy(&new_node->transform_local, matrix.data(), sizeof(matrix));
            },
            [&](fastgltf::TRS transform) {
              glm::vec3 tl(transform.translation[0], transform.translation[1],
                           transform.translation[2]);
              glm::quat rot(transform.rotation[3], transform.rotation[0],
                            transform.rotation[1], transform.rotation[2]);
              glm::vec3 sc(transform.scale[0], transform.scale[1],
                           transform.scale[2]);
              glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
              glm::mat4 rm = glm::toMat4(rot);
              glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);
              new_node->transform_local = tm * rm * sm;
            }},
        node.transform);
  }
  /// Enumerate again to setup node hierarchy.
  for (size_t i = 0; i < gltf.nodes.size(); i++) {
    fastgltf::Node &gltf_node = gltf.nodes[i];
    std::shared_ptr<Node> &scene_node = nodes[i];
    for (auto &c : gltf_node.children) {
      scene_node->children.push_back(nodes[c]);
      nodes[c]->parent = scene_node;
    }
  }
  /// Nodes with no parents are top nodes.
  for (auto &node : nodes) {
    if (node->parent.lock() == nullptr) {
      scene->top_nodes.push_back(node);
      node->updateTransform(glm::mat4{1.f});
    }
  }
  return scene;
}