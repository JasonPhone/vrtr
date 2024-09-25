#define VMA_IMPLEMENTATION
#include "engine.h"

#include "vk_initializers.h"
#include "vk_types.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include <VkBootstrap.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include <fstream>

constexpr bool bUseValidationLayers = true;

static Engine *loaded_engine = nullptr;

Engine &Engine::get() { return *loaded_engine; }
void Engine::init() {
  // Only one engine initialization is allowed with the application.
  assert(loaded_engine == nullptr);
  loaded_engine = this;

  initVulkan();
  initBuffers();
  initCommands();
  initSyncStructures();
  initDescriptors();
  initPipelines();

  initDefaultData();
  is_initialized = true;
  m_tasks.clear();
  m_tasks.push_back({
      .input_0 = "./input_0.png",
      .input_1 = "./input_1.png",
      .output_0 = "./output_0.png",
  });
}

void Engine::cleanup() {
  if (is_initialized) {
    // Order matters, reversed of initialization.
    vkDeviceWaitIdle(m_device); // Wait for GPU to finish.
    m_main_deletion_queue.flush();
  }
  loaded_engine = nullptr;
}

void Engine::process() {
  fmt::println("process");
  stats.t_cpu_draw.begin();
  VK_CHECK(vkWaitForFences(m_device, 1, &m_compute_fence, true, VK_ONE_SEC));
  VK_CHECK(vkResetFences(m_device, 1, &m_compute_fence));

  VkCommandBuffer cmd = m_compute_cmd;
  VK_CHECK(vkResetCommandBuffer(cmd, 0));
  VkCommandBufferBeginInfo cmd_begin_info =
      vkinit::cmdBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));
  vkCmdResetQueryPool(cmd, m_query_pool_timestamp, 0,
                      static_cast<uint32_t>(m_timestamps.size()));
  { // Drawing commands.
    // Upload input.
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        m_query_pool_timestamp, 0);
    updateInput();
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        m_query_pool_timestamp, 1);
    // Process.
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        m_query_pool_timestamp, 2);
    doCompute(cmd);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        m_query_pool_timestamp, 3);
    // Read-back.
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        m_query_pool_timestamp, 4);
    getResult(cmd);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        m_query_pool_timestamp, 5);
  }
  VK_CHECK(vkEndCommandBuffer(cmd));
  VkCommandBufferSubmitInfo cmd_submit_info = vkinit::cmdBufferSubmitInfo(cmd);
  VkSubmitInfo2 submit_info =
      vkinit::submitInfo(&cmd_submit_info, nullptr, nullptr);
  VK_CHECK(vkQueueSubmit2(m_compute_queue, 1, &submit_info, m_compute_fence));

  stats.t_cpu_draw.end();
  vkGetQueryPoolResults(
      m_device, m_query_pool_timestamp, 0,
      static_cast<uint32_t>(m_timestamps.size()),
      m_timestamps.size() * sizeof(uint64_t), m_timestamps.data(),
      sizeof(uint64_t),
      // VK_QUERY_RESULT_64_BIT); // No wait for all timestamp being written.
      VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

  // Read from CPU.
  vkDeviceWaitIdle(m_device);
  for (auto &buffer : m_readback_buffers) {
    auto data = (const float *)buffer.alloc_info.pMappedData;
    for (size_t i = 0; i < 10; i++)
      fmt::print("{} ", data[i]);
    fmt::print("\n");
  }
}
void Engine::run() {
  for (const auto &task : m_tasks) {
    stats.t_frame.begin();
    process();
    stats.t_frame.end();
    float t_input = static_cast<float>(m_timestamps[1] - m_timestamps[0]) *
                    m_timestamp_period / 1000000.f;
    float t_compute = static_cast<float>(m_timestamps[3] - m_timestamps[2]) *
                      m_timestamp_period / 1000000.f;
    float t_readback = static_cast<float>(m_timestamps[5] - m_timestamps[4]) *
                       m_timestamp_period / 1000000.f;
    fmt::println("input   \t{}ms", t_input);
    fmt::println("compute \t{}ms", t_compute);
    fmt::println("readback\t{}ms", t_readback);
    task_number++;
  }
}

void Engine::initVulkan() {
  fmt::print("init vulkan\n");
  vkb::InstanceBuilder builder;
  auto inst_ret = builder.set_app_name("Example Vulkan Application")
                      .request_validation_layers(bUseValidationLayers)
                      .use_default_debug_messenger()
                      .require_api_version(1, 3, 0)
                      .build();
  vkb::Instance vkb_inst = inst_ret.value();
  m_instance = vkb_inst.instance;
  m_debug_msngr = vkb_inst.debug_messenger;

  // Vulkan 1.3 features.
  VkPhysicalDeviceVulkan13Features features13{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  features13.dynamicRendering = true;
  features13.synchronization2 = true;
  // Vulkan 1.2 features.
  VkPhysicalDeviceVulkan12Features features12{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  features12.bufferDeviceAddress = true;
  features12.descriptorIndexing = true;

  // Select a gpu.
  // We want a gpu that can write to the SDL surface and supports vulkan 1.3
  // with the correct features
  vkb::PhysicalDeviceSelector selector{vkb_inst};
  vkb::PhysicalDevice physical_device = {};
  auto phy_device_list =
      selector.set_minimum_version(1, 3)
          .set_required_features_13(features13)
          .set_required_features_12(features12)
          .require_present(false)
          .add_required_extension(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME)
          .select_devices();
  // Naive select by physical device property.
  if (phy_device_list.has_value()) {
    for (const auto &d : phy_device_list.value()) {
      if (d.properties.limits.timestampPeriod > 0 &&
          d.properties.limits.timestampComputeAndGraphics) {
        physical_device = d;
        m_timestamp_period = d.properties.limits.timestampPeriod;
        break;
      }
    }
  } else {
    throw std::runtime_error{
        "No suitable physical devices by features and extensions."};
  }
  if (physical_device.physical_device == VK_NULL_HANDLE) {
    throw std::runtime_error{
        "No suitable physical devices by device properties."};
  }

  // Final vulkan device.
  vkb::DeviceBuilder device_builder{physical_device};
  vkb::Device vkb_device = device_builder.build().value();
  // Get the VkDevice handle for the rest of a vulkan application.
  m_device = vkb_device.device;
  m_chosen_GPU = physical_device.physical_device;
  m_compute_queue = vkb_device.get_queue(vkb::QueueType::compute).value();
  m_compute_queue_family =
      vkb_device.get_queue_index(vkb::QueueType::compute).value();

  // Mem allocator.
  VmaAllocatorCreateInfo ci_alloc = {};
  ci_alloc.physicalDevice = m_chosen_GPU;
  ci_alloc.device = m_device;
  ci_alloc.instance = m_instance;
  ci_alloc.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vmaCreateAllocator(&ci_alloc, &m_allocator);

  // Profiler.
  m_timestamps.resize(6);
  VkQueryPoolCreateInfo ci_query_pool = {};
  ci_query_pool.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  ci_query_pool.queryType = VK_QUERY_TYPE_TIMESTAMP;
  ci_query_pool.queryCount = static_cast<uint32_t>(m_timestamps.size());
  VK_CHECK(vkCreateQueryPool(m_device, &ci_query_pool, nullptr,
                             &m_query_pool_timestamp));

  m_main_deletion_queue.push([&]() {
    vmaDestroyAllocator(m_allocator);
    vkDestroyQueryPool(m_device, m_query_pool_timestamp, nullptr);
    vkDestroyDevice(m_device, nullptr);
    vkb::destroy_debug_utils_messenger(m_instance, m_debug_msngr);
    vkDestroyInstance(m_instance, nullptr);
  });
}
void Engine::initBuffers() {
  m_input_buffers.clear();
  m_output_buffers.clear();
  m_readback_buffers.clear();
  for (size_t i = 0; i < kNInputBuffers; i++) {
    auto buffer = createBuffer(1024 * sizeof(float),
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                               VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    m_input_buffers.push_back(buffer);
  }
  for (size_t i = 0; i < kNOutputBuffers; i++) {
    auto buffer = createBuffer(1024 * sizeof(float),
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                               VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    m_output_buffers.push_back(buffer);
    buffer =
        createBuffer(1024 * sizeof(float), VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VMA_MEMORY_USAGE_AUTO,
                     VmaAllocationCreateFlagBits(
                         VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                         VMA_ALLOCATION_CREATE_MAPPED_BIT));
    m_readback_buffers.push_back(buffer);
  }
  fmt::println("#input buffers {}", m_input_buffers.size());
  fmt::println("#output buffers {}", m_output_buffers.size());
  fmt::println("#readback buffers {}", m_readback_buffers.size());

  m_main_deletion_queue.push([&]() {
    for (auto &buffer : m_input_buffers)
      destroyBuffer(buffer);
    for (auto &buffer : m_output_buffers)
      destroyBuffer(buffer);
    for (auto &buffer : m_readback_buffers)
      destroyBuffer(buffer);
  });
}
void Engine::initCommands() {
  fmt::print("init commands\n");
  VkCommandPoolCreateInfo ci_cmd_pool = vkinit::cmdPoolCreateInfo(
      m_compute_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
  VK_CHECK(vkCreateCommandPool(m_device, &ci_cmd_pool, nullptr,
                               &m_compute_cmd_pool));
  VkCommandBufferAllocateInfo cmd_alloc_info =
      vkinit::cmdBufferAllocInfo(m_compute_cmd_pool, 1);
  VK_CHECK(vkAllocateCommandBuffers(m_device, &cmd_alloc_info, &m_compute_cmd));

  VK_CHECK(
      vkCreateCommandPool(m_device, &ci_cmd_pool, nullptr, &m_imm_cmd_pool));
  cmd_alloc_info = vkinit::cmdBufferAllocInfo(m_imm_cmd_pool, 1);
  VK_CHECK(vkAllocateCommandBuffers(m_device, &cmd_alloc_info, &m_imm_cmd));

  m_main_deletion_queue.push([&]() {
    vkDestroyCommandPool(m_device, m_compute_cmd_pool, nullptr);
    vkDestroyCommandPool(m_device, m_imm_cmd_pool, nullptr);
  });
}
void Engine::initSyncStructures() {
  // One fence to control when the gpu has finished rendering the frame.
  // 2 semaphores to synchronize rendering with swapchain.
  fmt::print("init sync structures\n");

  // The fence starts signalled so we can wait on it on the first frame.
  VkFenceCreateInfo ci_fence =
      vkinit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);

  VK_CHECK(vkCreateFence(m_device, &ci_fence, nullptr, &m_compute_fence));
  VK_CHECK(vkCreateFence(m_device, &ci_fence, nullptr, &m_imm_fence));
  m_main_deletion_queue.push([&]() {
    vkDestroyFence(m_device, m_compute_fence, nullptr);
    vkDestroyFence(m_device, m_imm_fence, nullptr);
  });
}
void Engine::initDescriptors() {
  std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
  };
  m_global_ds_allocator.initPool(m_device, 10, sizes);

  {
    DescriptorLayoutBuilder builder;
    DescriptorWriter writer;
    for (size_t i = 0; i < m_input_buffers.size(); i++) {
      builder.addBinding(i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      writer.writeBuffer(i, m_input_buffers[i].buffer, 1024 * sizeof(float), 0,
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }
    m_input_ds_layout = builder.build(m_device, VK_SHADER_STAGE_COMPUTE_BIT);
    m_input_ds = m_global_ds_allocator.allocate(m_device, m_input_ds_layout);
    writer.updateDescriptorSet(m_device, m_input_ds);
  }
  {
    DescriptorLayoutBuilder builder;
    DescriptorWriter writer;
    for (size_t i = 0; i < m_output_buffers.size(); i++) {
      builder.addBinding(i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      writer.writeBuffer(i, m_output_buffers[i].buffer, 1024 * sizeof(float), 0,
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }
    m_output_ds_layout = builder.build(m_device, VK_SHADER_STAGE_COMPUTE_BIT);
    m_output_ds = m_global_ds_allocator.allocate(m_device, m_output_ds_layout);
    writer.updateDescriptorSet(m_device, m_output_ds);
  }

  m_main_deletion_queue.push([&]() {
    m_global_ds_allocator.destroyPools(m_device);
    vkDestroyDescriptorSetLayout(m_device, m_input_ds_layout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_output_ds_layout, nullptr);
  });
}
void Engine::initPipelines() {
  // Compute pipelines.
  initComputePipelines();
}
void Engine::initComputePipelines() {
  VkDescriptorSetLayout layouts[2] = {m_input_ds_layout, m_output_ds_layout};
  VkPushConstantRange push_range = {};
  push_range.offset = 0;
  push_range.size = sizeof(ComputePushConstants);
  push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  // Create pipeline layout.
  VkPipelineLayoutCreateInfo ci_comp_layout = {};
  ci_comp_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  ci_comp_layout.pNext = nullptr;
  ci_comp_layout.pSetLayouts = layouts;
  ci_comp_layout.setLayoutCount = 2;
  ci_comp_layout.pPushConstantRanges = &push_range;
  ci_comp_layout.pushConstantRangeCount = 1;
  VK_CHECK(vkCreatePipelineLayout(m_device, &ci_comp_layout, nullptr,
                                  &m_compute_layout));
  VkPipelineShaderStageCreateInfo ci_stage = {};
  ci_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  ci_stage.pNext = nullptr;
  ci_stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  ci_stage.pName = "main";
  VkComputePipelineCreateInfo ci_comp_pipeline = {};
  ci_comp_pipeline.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  ci_comp_pipeline.pNext = nullptr;
  ci_comp_pipeline.layout = m_compute_layout;
  ci_comp_pipeline.stage = ci_stage; // Copy by values.

  VkShaderModule add_shader;
  if (!vkutil::loadShaderModule("../../assets/shaders/add.comp.spv", m_device,
                                &add_shader)) {
    fmt::println("Error building compute shader.");
  }
  m_compute_pipeline.layout = m_compute_layout;
  m_compute_pipeline.name = "add";
  m_compute_pipeline.data = {};
  ci_comp_pipeline.stage.module = add_shader;
  VK_CHECK(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1,
                                    &ci_comp_pipeline, nullptr,
                                    &m_compute_pipeline.pipeline));
  vkDestroyShaderModule(m_device, add_shader, nullptr);

  m_main_deletion_queue.push([&]() {
    vkDestroyPipelineLayout(m_device, m_compute_layout, nullptr);
    vkDestroyPipeline(m_device, m_compute_pipeline.pipeline, nullptr);
  });
}

AllocatedBuffer Engine::createBuffer(size_t alloc_size,
                                     VkBufferUsageFlags usage,
                                     VmaMemoryUsage mem_usage,
                                     VmaAllocationCreateFlagBits mem_alloc) {
  VkBufferCreateInfo ci_buffer = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
  };
  ci_buffer.size = alloc_size;
  ci_buffer.usage = usage;
  VmaAllocationCreateInfo ci_alloc = {};
  ci_alloc.usage = mem_usage;
  ci_alloc.flags = mem_alloc;
  AllocatedBuffer buffer;
  VK_CHECK(vmaCreateBuffer(m_allocator, &ci_buffer, &ci_alloc, &buffer.buffer,
                           &buffer.allocation, &buffer.alloc_info));
  return buffer;
}
void Engine::destroyBuffer(const AllocatedBuffer &buffer) {
  vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);
}

void Engine::initDefaultData() {}

void Engine::updateInput() {
  VkBufferCopy copy;
  copy.srcOffset = 0;
  copy.dstOffset = 0;
  copy.size = 1024 * sizeof(float);
  AllocatedBuffer staging =
      createBuffer(1024 * sizeof(float), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VMA_MEMORY_USAGE_AUTO,
                   VmaAllocationCreateFlagBits(
                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                       VMA_ALLOCATION_CREATE_MAPPED_BIT));
  float *data = new float[1024];

  std::fill(data, data + 1024, 3.14f);
  memcpy(staging.alloc_info.pMappedData, data, 1024 * sizeof(float));
  immediateSubmit([&](VkCommandBuffer _cmd) {
    vkCmdCopyBuffer(_cmd, staging.buffer, m_input_buffers[0].buffer, 1, &copy);
  });

  std::fill(data, data + 1024, 2.72f);
  memcpy(staging.alloc_info.pMappedData, data, 1024 * sizeof(float));
  immediateSubmit([&](VkCommandBuffer _cmd) {
    vkCmdCopyBuffer(_cmd, staging.buffer, m_input_buffers[1].buffer, 1, &copy);
  });

  delete[] data;
  destroyBuffer(staging);
}
void Engine::doCompute(VkCommandBuffer cmd) {
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                    m_compute_pipeline.pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute_layout,
                          0, 1, &m_input_ds, 0, nullptr);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute_layout,
                          1, 1, &m_output_ds, 0, nullptr);
  vkCmdPushConstants(cmd, m_compute_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                     sizeof(ComputePushConstants), &m_compute_pipeline.data);
  vkCmdDispatch(cmd, 4, 1, 1);
}
void Engine::getResult(VkCommandBuffer cmd) {
  VkBufferCopy copy;
  copy.srcOffset = 0;
  copy.dstOffset = 0;
  copy.size = 1024 * sizeof(float);
  vkCmdCopyBuffer(cmd, m_output_buffers[0].buffer, m_readback_buffers[0].buffer,
                  1, &copy);
}
void Engine::immediateSubmit(std::function<void(VkCommandBuffer cmd)> &&func) {
  VK_CHECK(vkResetFences(m_device, 1, &m_imm_fence));
  VK_CHECK(vkResetCommandBuffer(m_imm_cmd, 0));

  VkCommandBuffer cmd = m_imm_cmd;
  VkCommandBufferBeginInfo cmdBeginInfo =
      vkinit::cmdBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  func(cmd);

  VK_CHECK(vkEndCommandBuffer(cmd));
  VkCommandBufferSubmitInfo cmd_info = vkinit::cmdBufferSubmitInfo(cmd);
  VkSubmitInfo2 submit = vkinit::submitInfo(&cmd_info, nullptr, nullptr);

  VK_CHECK(vkQueueSubmit2(m_compute_queue, 1, &submit, m_imm_fence));
  VK_CHECK(vkWaitForFences(m_device, 1, &m_imm_fence, true, VK_ONE_SEC));
}
