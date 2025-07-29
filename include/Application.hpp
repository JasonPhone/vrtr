#pragma once

#include "GPU/GPU.hpp"
#include "Scene/Scene.hpp"
#include "utils/json.hpp"
#include "Window.hpp"
#include "rendering/RenderContext.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <set>
#include <optional>

constexpr std::array<const char *, 1> REQUIRED_LAYERS{
    "VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool ENABLE_VALIDATION_LAYER = false;
#else
constexpr bool ENABLE_VALIDATION_LAYER = true;
#endif

namespace vrtr {
struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  bool AllFound() {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};
class Application {
public:
  /**
   * @brief Create application.
   * create Vulkan
   * create render context
   * load the Scene
   * create RenderPipeline with ShaderModule(s)
   * create Camera
   * create Gui
   */
  void Create(const Json &config, const Window *window) {
    mWindow = window;

    // mGpu.init(mWindow->getSDLHandle());
    // mGpu.uploadScene(mScene);

    CreateVulkan();
  }

  void Destroy() {
    // ...
    // mGpu.deinit();
  }

  void TickLogic(float delta) {
    // Some CPU side update.
    // mScene.tick(delta);
  }

  void TickRender(float delta) {
    /**
     * render context
     *    begin frame, wait and acquire image in
     * request cmd buffer
     * update stats and gui
     * do render
     * present
     */
    // mGpu.updateScene(mScene);
    // mGpu.draw();
  }

private:
  void CreateVulkan();
  void CreateRenderContext();
  void LoadScene();
  void CreatePipeline();
  void CreateCamera();
  void CreateGui();

  const Window *mWindow;
  Scene mScene;

  GPU mGpu;

  vk::raii::Context mVkContext;
  // TODO See core/hpp_instance.h for more encapsule.
  vk::raii::Instance mVkInstance{nullptr};
  vk::raii::DebugUtilsMessengerEXT mDebugMessenger{nullptr};
  vk::raii::SurfaceKHR mSurface{nullptr};
  vk::raii::PhysicalDevice mPhysicalDevice{nullptr};
  vk::raii::Device mDevice{nullptr};
  vk::raii::Queue mGraphicsQueue{nullptr};
  vk::raii::Queue mPresentQueue{nullptr};

  std::unique_ptr<RenderContext> mRenderContext;

  DeletionQueue mDeletionQueue{};
};
} // namespace vrtr

static std::vector<const char *> GetRequiredExtensions(const vrtr::Window* window) {
  std::vector<const char *> extVec = window->GetRequiredExtensions();
  // For mac.
  // extVec.emplace_back(vk::KHRPortabilityEnumerationExtensionName);
  if constexpr (ENABLE_VALIDATION_LAYER) {
    extVec.emplace_back(vk::EXTDebugUtilsExtensionName);
  }

  return extVec;
}

static VKAPI_ATTR uint32_t VKAPI_CALL DebugMessageCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
    vk::DebugUtilsMessengerCallbackDataEXT const *pCallbackData,
    void *pUserData) {
  switch (messageSeverity) {
  case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
    LOGD("validation layer: {}", pCallbackData->pMessage);
    break;
  case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
    LOGI("validation layer: {}", pCallbackData->pMessage);
    break;
  case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
    LOGW("validation layer: {}", pCallbackData->pMessage);
    break;
  case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
    LOGE("validation layer: {}", pCallbackData->pMessage);
    break;
  }
  return false;
}

static vk::DebugUtilsMessengerCreateInfoEXT GetDebugMessengerCreateInfo() {
  vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
  vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
  vk::DebugUtilsMessengerCreateInfoEXT createInfo(
      {},                   // flag
      severityFlags,        // messageSeverity
      messageTypeFlags,     // messageType
      &DebugMessageCallback // user defined callback
  );
  return createInfo;
}

static vrtr::QueueFamilyIndices FindQueueFamilies(const vk::raii::PhysicalDevice& phyDevice, const vk::raii::SurfaceKHR& surface) {
  vrtr::QueueFamilyIndices indices{};
  auto queueFamilies = phyDevice.getQueueFamilyProperties();
  for (uint32_t i = 0; const auto& family : queueFamilies) {
    if (family.queueFlags & vk::QueueFlagBits::eGraphics)
      indices.graphicsFamily = i;

    if (phyDevice.getSurfaceSupportKHR(i, surface))
      indices.presentFamily = i;

    
    if (indices.AllFound()) break;
    i++;
  }
  return indices;
}

static bool IsDeviceSuitable(const vk::raii::PhysicalDevice& phyDevice, const vk::raii::SurfaceKHR& surface) {
  // Device name, vulkan version, etc.
  // vk::PhysicalDeviceProperties properties = phyDevice.getProperties();
  // Supported features.
  // vk::PhysicalDeviceFeatures features = phyDevice.getFeatures();

  auto indices = FindQueueFamilies(phyDevice, surface);
  
  return indices.AllFound();
}

inline void vrtr::Application::CreateVulkan() {
  /**
   * instance, surface
   * physical device, logical device
   * creating the Swapchain
   */
  LOGI("create vk");

  {
    LOGI("create vk instance");
    vk::ApplicationInfo appInfo;
    appInfo.pApplicationName = "vrtr";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "vrtrEngine";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char *> extVec = GetRequiredExtensions(mWindow);

    vk::InstanceCreateInfo instanceCi;
    instanceCi.setPApplicationInfo(&appInfo)
        .setFlags({})
        .setPEnabledExtensionNames(extVec);

    // Outside the constexpr if to ensure pointer life span.
    auto debugMessengerCi = GetDebugMessengerCreateInfo();
    if constexpr (ENABLE_VALIDATION_LAYER) {
      LOGI("use validation layer");
      const auto layers = mVkContext.enumerateInstanceLayerProperties();
      std::set<std::string> requiredLayers(REQUIRED_LAYERS.begin(),
                                           REQUIRED_LAYERS.end());
      for (const auto &layer : layers)
        requiredLayers.erase(layer.layerName);

      if (requiredLayers.empty()) {

      } else {
        throw std::runtime_error{"required validation layer not supported"};
      }
      // Device-wise layer is deprecated.
      instanceCi.setPEnabledLayerNames(REQUIRED_LAYERS);
      instanceCi.setPNext(&debugMessengerCi);
    }
    mVkInstance = mVkContext.createInstance(instanceCi);
  }
  {
    LOGI("setup debug messenger");
    if (ENABLE_VALIDATION_LAYER) {
      mDebugMessenger = mVkInstance.createDebugUtilsMessengerEXT(
          GetDebugMessengerCreateInfo());
    }
  }
  {
    LOGI("create surface");
    // *mVkInstance to get a vk::Instance, then implicitly convert to VKInstance.
    auto cSurface = mWindow->CreateCurface(*mVkInstance);
    mSurface = vk::raii::SurfaceKHR{mVkInstance, cSurface};
  }
  {
    LOGI("select physical device");
    auto physicalDevices = mVkInstance.enumeratePhysicalDevices();
    if (physicalDevices.empty())
      throw std::runtime_error{"no physical devices for vk"};
    for (const auto &d : physicalDevices) {
      if (IsDeviceSuitable(d, mSurface)) {
        mPhysicalDevice = d;
        break;
      }
    }
  }
  {
    LOGI("create queue and logical device");
    std::vector<vk::DeviceQueueCreateInfo> queueCis{};

    auto indices = FindQueueFamilies(mPhysicalDevice, mSurface);
    std::set<uint32_t> uniqueFamilyIndices = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    constexpr float priority = 1.0;
    for (auto idx : uniqueFamilyIndices) {
      vk::DeviceQueueCreateInfo ci{};
      queueCis.emplace_back(ci.setQueueFamilyIndex(idx).setQueuePriorities(priority));
    }

    vk::PhysicalDeviceFeatures phyDeviceFeatures{};
    vk::DeviceCreateInfo deviceCi{};
    deviceCi.setQueueCreateInfos(queueCis);
    deviceCi.setPEnabledFeatures(&phyDeviceFeatures);

    mDevice = mPhysicalDevice.createDevice(deviceCi);
    mGraphicsQueue = mDevice.getQueue(indices.graphicsFamily.value(), 0);
    mPresentQueue = mDevice.getQueue(indices.presentFamily.value(), 0);
  }

  mDeletionQueue.push([&]() {});
}
inline void vrtr::Application::CreateRenderContext() {}
inline void vrtr::Application::LoadScene() {
  // TODO ECS scene.
}
inline void vrtr::Application::CreatePipeline() {}
inline void vrtr::Application::CreateCamera() {}
inline void vrtr::Application::CreateGui() {}