#include "viewer_app.h"

#include "preview_camera.h"
#include "training_frame_consumer.h"
#include "usd_scene_loader.h"
#include "viewer_export.h"

#include <engine/aov_texture.hpp>

#include "GLFW/glfw3.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "imgui/imgui_helper.h"
#include "nvh/fileoperations.hpp"
#include "nvp/nvpsystem.hpp"
#include "nvvk/shaders_vk.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>

extern std::vector<std::string> defaultSearchPaths;

namespace headless_training
{
namespace
{
constexpr float kMinControlsPanelWidth = 280.0f;
constexpr float kMaxControlsPanelWidth = 380.0f;
constexpr float kPreferredControlsPanelFraction = 0.26f;
constexpr const char* kAutoPreviewCameraLabel = "Auto preview";
constexpr float kUsdCameraResetDistance = 5.0f;
constexpr VkFormat kViewportDisplayFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
constexpr uint32_t kViewportConvertWorkgroupSize = 16;
constexpr auto kReplayFrameInterval = std::chrono::nanoseconds(1'000'000'000 / 60);
using ReplayClock = std::chrono::steady_clock;

void ThrowIfVkFailed(VkResult result, const char* message)
{
  if(result != VK_SUCCESS)
  {
    throw std::runtime_error(message);
  }
}

void AddExternalSharingExtensions(nvvk::ContextCreateInfo& contextInfo)
{
  contextInfo.addInstanceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  contextInfo.addInstanceExtension(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
  contextInfo.addInstanceExtension(VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME);
  contextInfo.addInstanceExtension(VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME);

  contextInfo.addDeviceExtension(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME);

#ifdef _WIN32
  contextInfo.addDeviceExtension("VK_KHR_external_memory_win32");
  contextInfo.addDeviceExtension("VK_KHR_external_semaphore_win32");
  contextInfo.addDeviceExtension("VK_KHR_external_fence_win32");
#else
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME);
#endif
}

std::vector<std::string> BuildCameraSourceLabels(const TrainingSceneDescription& scene)
{
  std::vector<std::string> result;
  result.push_back(kAutoPreviewCameraLabel);
  for(size_t i = 0; i < scene.cameras.size(); ++i)
  {
    result.push_back(scene.cameras[i].name.empty() ? "Camera " + std::to_string(i) : scene.cameras[i].name);
  }
  return result;
}

int SelectInitialCameraSource(const TrainingSceneDescription& scene, const std::string& cameraPath)
{
  if(cameraPath.empty())
  {
    return 0;
  }
  for(size_t i = 0; i < scene.cameras.size(); ++i)
  {
    if(scene.cameras[i].name == cameraPath)
    {
      return static_cast<int>(i + 1);
    }
  }
  return 0;
}

CameraSpec CameraForSource(const TrainingSceneDescription& scene, const CameraSpec& autoPreviewCamera, int sourceIndex)
{
  if(sourceIndex <= 0)
  {
    return autoPreviewCamera;
  }
  const size_t cameraIndex = static_cast<size_t>(sourceIndex - 1);
  if(cameraIndex < scene.cameras.size())
  {
    return scene.cameras[cameraIndex];
  }
  return autoPreviewCamera;
}

glm::vec3 ComputeUsdCameraResetTarget(const TrainingSceneDescription& scene, const CameraSpec& camera)
{
  const glm::vec3 focusTarget = ComputeViewerFocusTarget(scene, camera);
  const glm::vec3 forward = glm::length(camera.forward) > 0.0f ? glm::normalize(camera.forward)
                                                               : glm::vec3(0.0f, 0.0f, -1.0f);
  float distance = glm::dot(focusTarget - camera.position, forward);
  if(!std::isfinite(distance) || distance <= 0.0f)
  {
    distance = kUsdCameraResetDistance;
  }
  return camera.position + forward * distance;
}

std::vector<std::string> LidarSensorNames(const TrainingSceneDescription& scene)
{
  std::vector<std::string> result;
  for(size_t i = 0; i < scene.lidarSensors.size(); ++i)
  {
    result.push_back(scene.lidarSensors[i].name.empty() ? "LiDAR " + std::to_string(i) : scene.lidarSensors[i].name);
  }
  return result;
}

std::vector<std::string> HeightScanSensorNames(const TrainingSceneDescription& scene)
{
  std::vector<std::string> result;
  for(size_t i = 0; i < scene.heightScanSensors.size(); ++i)
  {
    result.push_back(scene.heightScanSensors[i].name.empty() ? "HeightScan " + std::to_string(i)
                                                             : scene.heightScanSensors[i].name);
  }
  return result;
}

} // namespace

ViewerApp::ViewerApp(ViewerCliOptions options) : m_options(std::move(options)) {}

ViewerApp::~ViewerApp()
{
  shutdown();
}

void ViewerApp::run()
{
  initialize();
  while(!glfwWindowShouldClose(m_window))
  {
    glfwPollEvents();
    if(isMinimized())
    {
      continue;
    }
    runFrame();
  }
}

void ViewerApp::onResize(int width, int height)
{
  if(width <= 0 || height <= 0 || !m_engineReady)
  {
    return;
  }
  releaseViewportTexture();
  destroyViewportDisplayImage();
  m_engine.resize(width, height);
}

void ViewerApp::initialize()
{
  UsdSceneLoadOptions loadOptions;
  loadOptions.cameraPath = m_options.cameraPath;
  TrainingSceneDescription scene = LoadUsdTrainingScene(m_options.usdPath, loadOptions);

  m_state.lidar.enableCompute = m_options.enableLidar;
  m_state.heightScan.enableCompute = m_options.enableHeightScan;
  m_state.exportSettings.outputDir = m_options.outputDir;
  m_autoPreviewCamera = BuildPreviewCamera(scene);
  m_cameraSourceLabels = BuildCameraSourceLabels(scene);
  m_selectedCameraSource = SelectInitialCameraSource(scene, m_options.cameraPath);

  m_runtime.emplace(std::move(scene));
  resetCameraControllerToSelectedCamera();
  m_physicsFrameSource = LoadUsdPhysicsReplaySource(m_options.usdPath, m_runtime->scene());

  createWindow();
  createVulkanContext();
  nvvkhl::AppBaseVkCreateInfo createInfo;
  createInfo.instance = m_vkctx.m_instance;
  createInfo.device = m_vkctx.m_device;
  createInfo.physicalDevice = m_vkctx.m_physicalDevice;
  createInfo.queueIndices = {m_vkctx.m_queueGCT.familyIndex};
  createInfo.surface = m_surface;
  createInfo.size = {static_cast<uint32_t>(m_options.width), static_cast<uint32_t>(m_options.height)};
  createInfo.window = m_window;
  createInfo.useVsync = true;
  create(createInfo);
  glfwSetFramebufferSizeCallback(m_window, &ViewerApp::framebufferSizeCallback);
  m_appReady = true;
  createViewportSampler();
  initializeEngine();
  createViewportConversionResources();
}

void ViewerApp::shutdown()
{
  if(m_viewportDescriptor != VK_NULL_HANDLE)
  {
    releaseViewportTexture();
  }
  destroyViewportConversionResources();

  if(m_engineReady)
  {
    m_engine.cleanup();
    m_engineReady = false;
  }

  if(m_viewportSampler != VK_NULL_HANDLE)
  {
    vkDestroySampler(getDevice(), m_viewportSampler, nullptr);
    m_viewportSampler = VK_NULL_HANDLE;
  }

  if(m_appReady)
  {
    destroy();
    m_appReady = false;
    m_surfaceCreated = false;
  }
  else if(m_surfaceCreated && m_surface != VK_NULL_HANDLE && m_vkctx.m_instance != VK_NULL_HANDLE)
  {
    vkDestroySurfaceKHR(m_vkctx.m_instance, m_surface, nullptr);
    m_surface = VK_NULL_HANDLE;
    m_surfaceCreated = false;
  }

  if(m_vkctx.m_device != VK_NULL_HANDLE || m_vkctx.m_instance != VK_NULL_HANDLE)
  {
    m_vkctx.deinit();
  }

  if(m_window != nullptr)
  {
    glfwDestroyWindow(m_window);
    m_window = nullptr;
  }
}

void ViewerApp::createWindow()
{
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  m_window = glfwCreateWindow(m_options.width, m_options.height, "robot_training_viewer", nullptr, nullptr);
  if(m_window == nullptr)
  {
    throw std::runtime_error("failed to create GLFW window");
  }
}

void ViewerApp::createVulkanContext()
{
  if(!glfwVulkanSupported())
  {
    throw std::runtime_error("GLFW reports Vulkan is not supported");
  }

  nvvk::ContextCreateInfo contextInfo;
  contextInfo.verboseUsed = false;
  contextInfo.verboseCompatibleDevices = false;
  contextInfo.verboseAvailable = false;
  contextInfo.setVersion(1, 2);

  uint32_t extensionCount = 0;
  const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);
  for(uint32_t i = 0; i < extensionCount; ++i)
  {
    contextInfo.addInstanceExtension(requiredExtensions[i]);
  }
  contextInfo.addInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true);

  VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
  contextInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &accelFeature);

  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
  contextInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false, &rtPipelineFeature);

  VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
  contextInfo.addDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME, false, &rayQueryFeature);

  contextInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  AddExternalSharingExtensions(contextInfo);

  if(!m_vkctx.initInstance(contextInfo))
  {
    throw std::runtime_error("failed to initialize Vulkan instance");
  }

  const VkSurfaceKHR surface = getVkSurface(m_vkctx.m_instance, m_window);
  m_surfaceCreated = true;

  auto compatibleDevices = m_vkctx.getCompatibleDevices(contextInfo);
  if(compatibleDevices.empty())
  {
    throw std::runtime_error("no compatible Vulkan device found for viewer and engine requirements");
  }
  if(!m_vkctx.initDevice(compatibleDevices[0], contextInfo))
  {
    throw std::runtime_error("failed to initialize Vulkan device");
  }
  if(!m_vkctx.setGCTQueueWithPresent(surface))
  {
    throw std::runtime_error("selected Vulkan graphics queue cannot present to the viewer surface");
  }
}

void ViewerApp::initializeEngine()
{
  if(!m_options.pluginSearchRoot.empty())
  {
    m_engine.setPluginSearchRoot(m_options.pluginSearchRoot);
  }
  m_engine.create(m_vkctx.m_instance, m_vkctx.m_device, m_vkctx.m_physicalDevice, m_vkctx.m_queueGCT.familyIndex,
                  {static_cast<uint32_t>(m_options.width), static_cast<uint32_t>(m_options.height)});
  m_engineReady = true;
  m_runtime->uploadToEngine(m_engine);
  m_engine.setCameras(m_runtime->scene().cameras);
  m_engine.setMainCamera(m_state.visualCamera);
  applyOutputConfigIfDirty();
  m_engine.createRenderResources();
}

void ViewerApp::createViewportSampler()
{
  VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  samplerInfo.magFilter = VK_FILTER_NEAREST;
  samplerInfo.minFilter = VK_FILTER_NEAREST;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.maxLod = 1.0f;
  if(vkCreateSampler(getDevice(), &samplerInfo, nullptr, &m_viewportSampler) != VK_SUCCESS)
  {
    throw std::runtime_error("failed to create viewer viewport sampler");
  }
}

void ViewerApp::createViewportConversionResources()
{
  if(m_viewportConversionPipeline != VK_NULL_HANDLE)
  {
    return;
  }

  std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  bindings[1].binding = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();
  ThrowIfVkFailed(vkCreateDescriptorSetLayout(getDevice(), &layoutInfo, nullptr, &m_viewportConversionDescriptorSetLayout),
                  "failed to create viewer display conversion descriptor set layout");

  std::array<VkDescriptorPoolSize, 2> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[0].descriptorCount = 1;
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  poolSizes[1].descriptorCount = 1;

  VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  poolInfo.maxSets = 1;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  ThrowIfVkFailed(vkCreateDescriptorPool(getDevice(), &poolInfo, nullptr, &m_viewportConversionDescriptorPool),
                  "failed to create viewer display conversion descriptor pool");

  VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  allocateInfo.descriptorPool = m_viewportConversionDescriptorPool;
  allocateInfo.descriptorSetCount = 1;
  allocateInfo.pSetLayouts = &m_viewportConversionDescriptorSetLayout;
  ThrowIfVkFailed(vkAllocateDescriptorSets(getDevice(), &allocateInfo, &m_viewportConversionDescriptorSet),
                  "failed to allocate viewer display conversion descriptor set");

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &m_viewportConversionDescriptorSetLayout;
  ThrowIfVkFailed(vkCreatePipelineLayout(getDevice(), &pipelineLayoutInfo, nullptr, &m_viewportConversionPipelineLayout),
                  "failed to create viewer display conversion pipeline layout");

  const std::string shaderCode =
      nvh::loadFile("spv/viewer_display_color.comp.spv", true, defaultSearchPaths, true);
  if(shaderCode.empty())
  {
    throw std::runtime_error("failed to load viewer display conversion shader");
  }
  VkShaderModule shaderModule = nvvk::createShaderModule(getDevice(), shaderCode);
  if(shaderModule == VK_NULL_HANDLE)
  {
    throw std::runtime_error("failed to create viewer display conversion shader module");
  }

  VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  pipelineInfo.stage.module = shaderModule;
  pipelineInfo.stage.pName = "main";
  pipelineInfo.layout = m_viewportConversionPipelineLayout;
  const VkResult pipelineResult =
      vkCreateComputePipelines(getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_viewportConversionPipeline);
  vkDestroyShaderModule(getDevice(), shaderModule, nullptr);
  ThrowIfVkFailed(pipelineResult, "failed to create viewer display conversion pipeline");
}

void ViewerApp::destroyViewportConversionResources()
{
  VkDevice device = getDevice();
  if(device == VK_NULL_HANDLE)
  {
    return;
  }

  vkDeviceWaitIdle(device);
  destroyViewportDisplayImage();

  if(m_viewportConversionPipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(device, m_viewportConversionPipeline, nullptr);
    m_viewportConversionPipeline = VK_NULL_HANDLE;
  }
  if(m_viewportConversionPipelineLayout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(device, m_viewportConversionPipelineLayout, nullptr);
    m_viewportConversionPipelineLayout = VK_NULL_HANDLE;
  }
  if(m_viewportConversionDescriptorPool != VK_NULL_HANDLE)
  {
    vkDestroyDescriptorPool(device, m_viewportConversionDescriptorPool, nullptr);
    m_viewportConversionDescriptorPool = VK_NULL_HANDLE;
    m_viewportConversionDescriptorSet = VK_NULL_HANDLE;
  }
  if(m_viewportConversionDescriptorSetLayout != VK_NULL_HANDLE)
  {
    vkDestroyDescriptorSetLayout(device, m_viewportConversionDescriptorSetLayout, nullptr);
    m_viewportConversionDescriptorSetLayout = VK_NULL_HANDLE;
  }
  m_viewportConversionSourceView = VK_NULL_HANDLE;
  m_viewportConversionOutputView = VK_NULL_HANDLE;
}

void ViewerApp::destroyViewportDisplayImage()
{
  VkDevice device = getDevice();
  if(device == VK_NULL_HANDLE)
  {
    return;
  }

  if(m_viewportDisplayImageView != VK_NULL_HANDLE || m_viewportDisplayImage != VK_NULL_HANDLE
     || m_viewportDisplayMemory != VK_NULL_HANDLE)
  {
    vkDeviceWaitIdle(device);
  }

  if(m_viewportDisplayImageView != VK_NULL_HANDLE)
  {
    vkDestroyImageView(device, m_viewportDisplayImageView, nullptr);
    m_viewportDisplayImageView = VK_NULL_HANDLE;
  }
  if(m_viewportDisplayImage != VK_NULL_HANDLE)
  {
    vkDestroyImage(device, m_viewportDisplayImage, nullptr);
    m_viewportDisplayImage = VK_NULL_HANDLE;
  }
  if(m_viewportDisplayMemory != VK_NULL_HANDLE)
  {
    vkFreeMemory(device, m_viewportDisplayMemory, nullptr);
    m_viewportDisplayMemory = VK_NULL_HANDLE;
  }
  m_viewportDisplayExtent = {0, 0};
  m_viewportDisplayInitialized = false;
  m_viewportConversionOutputView = VK_NULL_HANDLE;
}

void ViewerApp::ensureViewportDisplayImage(VkExtent2D extent)
{
  if(extent.width == 0 || extent.height == 0)
  {
    return;
  }
  if(m_viewportDisplayImage != VK_NULL_HANDLE && m_viewportDisplayExtent.width == extent.width
     && m_viewportDisplayExtent.height == extent.height)
  {
    return;
  }

  releaseViewportTexture();
  destroyViewportDisplayImage();

  VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.format = kViewportDisplayFormat;
  imageInfo.extent = {extent.width, extent.height, 1};
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  ThrowIfVkFailed(vkCreateImage(getDevice(), &imageInfo, nullptr, &m_viewportDisplayImage),
                  "failed to create viewer display image");

  VkMemoryRequirements memReq{};
  vkGetImageMemoryRequirements(getDevice(), m_viewportDisplayImage, &memReq);
  VkMemoryAllocateInfo memoryInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  memoryInfo.allocationSize = memReq.size;
  memoryInfo.memoryTypeIndex = getMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  ThrowIfVkFailed(vkAllocateMemory(getDevice(), &memoryInfo, nullptr, &m_viewportDisplayMemory),
                  "failed to allocate viewer display image memory");
  ThrowIfVkFailed(vkBindImageMemory(getDevice(), m_viewportDisplayImage, m_viewportDisplayMemory, 0),
                  "failed to bind viewer display image memory");

  VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  viewInfo.image = m_viewportDisplayImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = kViewportDisplayFormat;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.layerCount = 1;
  ThrowIfVkFailed(vkCreateImageView(getDevice(), &viewInfo, nullptr, &m_viewportDisplayImageView),
                  "failed to create viewer display image view");

  m_viewportDisplayExtent = extent;
  m_viewportDisplayInitialized = false;
  m_viewportConversionOutputView = VK_NULL_HANDLE;
}

void ViewerApp::updateViewportConversionDescriptorSet(const ExportedAovTexture& colorAov)
{
  if(m_viewportConversionDescriptorSet == VK_NULL_HANDLE || m_viewportSampler == VK_NULL_HANDLE
     || m_viewportDisplayImageView == VK_NULL_HANDLE)
  {
    return;
  }
  if(m_viewportConversionSourceView == colorAov.imageView
     && m_viewportConversionOutputView == m_viewportDisplayImageView)
  {
    return;
  }

  VkDescriptorImageInfo sourceInfo{};
  sourceInfo.sampler = m_viewportSampler;
  sourceInfo.imageView = colorAov.imageView;
  sourceInfo.imageLayout = colorAov.layout;

  VkDescriptorImageInfo outputInfo{};
  outputInfo.imageView = m_viewportDisplayImageView;
  outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  std::array<VkWriteDescriptorSet, 2> writes{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = m_viewportConversionDescriptorSet;
  writes[0].dstBinding = 0;
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[0].pImageInfo = &sourceInfo;
  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = m_viewportConversionDescriptorSet;
  writes[1].dstBinding = 1;
  writes[1].descriptorCount = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  writes[1].pImageInfo = &outputInfo;
  vkUpdateDescriptorSets(getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

  m_viewportConversionSourceView = colorAov.imageView;
  m_viewportConversionOutputView = m_viewportDisplayImageView;
}

void ViewerApp::convertViewportTexture(const ExportedAovTexture& colorAov)
{
  if(m_viewportConversionPipeline == VK_NULL_HANDLE || m_viewportConversionDescriptorSet == VK_NULL_HANDLE
     || m_viewportDisplayImage == VK_NULL_HANDLE || colorAov.image == VK_NULL_HANDLE)
  {
    return;
  }

  VkCommandBuffer cmdBuf = createTempCmdBuffer();

  VkImageMemoryBarrier sourceBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  sourceBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
  sourceBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  sourceBarrier.oldLayout = colorAov.layout;
  sourceBarrier.newLayout = colorAov.layout;
  sourceBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  sourceBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  sourceBarrier.image = colorAov.image;
  sourceBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  sourceBarrier.subresourceRange.levelCount = 1;
  sourceBarrier.subresourceRange.layerCount = 1;
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &sourceBarrier);

  VkImageMemoryBarrier displayWriteBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  displayWriteBarrier.srcAccessMask = m_viewportDisplayInitialized ? VK_ACCESS_SHADER_READ_BIT : 0;
  displayWriteBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  displayWriteBarrier.oldLayout =
      m_viewportDisplayInitialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
  displayWriteBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  displayWriteBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  displayWriteBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  displayWriteBarrier.image = m_viewportDisplayImage;
  displayWriteBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  displayWriteBarrier.subresourceRange.levelCount = 1;
  displayWriteBarrier.subresourceRange.layerCount = 1;
  vkCmdPipelineBarrier(cmdBuf,
                       m_viewportDisplayInitialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                                    : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &displayWriteBarrier);

  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_viewportConversionPipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_viewportConversionPipelineLayout, 0, 1,
                          &m_viewportConversionDescriptorSet, 0, nullptr);
  const uint32_t groupCountX =
      (m_viewportDisplayExtent.width + kViewportConvertWorkgroupSize - 1) / kViewportConvertWorkgroupSize;
  const uint32_t groupCountY =
      (m_viewportDisplayExtent.height + kViewportConvertWorkgroupSize - 1) / kViewportConvertWorkgroupSize;
  vkCmdDispatch(cmdBuf, groupCountX, groupCountY, 1);

  VkImageMemoryBarrier displayReadBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  displayReadBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  displayReadBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  displayReadBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  displayReadBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  displayReadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  displayReadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  displayReadBarrier.image = m_viewportDisplayImage;
  displayReadBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  displayReadBarrier.subresourceRange.levelCount = 1;
  displayReadBarrier.subresourceRange.layerCount = 1;
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &displayReadBarrier);

  submitTempCmdBuffer(cmdBuf);
  m_viewportDisplayInitialized = true;
}

void ViewerApp::refreshViewportTexture()
{
  const std::optional<ExportedAovTexture> colorAov = m_engine.GetAovTexture(Aov::Color);
  if(!colorAov || colorAov->imageView == VK_NULL_HANDLE)
  {
    return;
  }
  ensureViewportDisplayImage(colorAov->extent);
  if(m_viewportDisplayImageView == VK_NULL_HANDLE)
  {
    return;
  }

  updateViewportConversionDescriptorSet(*colorAov);
  convertViewportTexture(*colorAov);

  if(m_viewportDescriptor != VK_NULL_HANDLE && m_registeredImageView == m_viewportDisplayImageView)
  {
    return;
  }
  if(m_viewportDescriptor != VK_NULL_HANDLE)
  {
    ImGui_ImplVulkan_RemoveTexture(m_viewportDescriptor);
  }
  m_viewportDescriptor = ImGui_ImplVulkan_AddTexture(m_viewportSampler, m_viewportDisplayImageView,
                                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_registeredImageView = m_viewportDisplayImageView;
}

void ViewerApp::releaseViewportTexture()
{
  if(m_viewportDescriptor != VK_NULL_HANDLE && ImGui::GetCurrentContext() != nullptr)
  {
    ImGui_ImplVulkan_RemoveTexture(m_viewportDescriptor);
  }
  m_viewportDescriptor = VK_NULL_HANDLE;
  m_registeredImageView = VK_NULL_HANDLE;
}

void ViewerApp::runFrame()
{
  syncFramebufferResizeBeforeRender();

  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  drawUi();

  advanceReplayForPlayback();

  m_state.visualCamera = m_cameraController.camera();
  m_engine.setMainCamera(m_state.visualCamera);
  applyOutputConfigIfDirty();
  m_engine.render();
  refreshViewportTexture();
  processPendingExports();

  ImGui::Render();
  drawFrameToSwapchain();
}

void ViewerApp::resetCameraControllerToSelectedCamera()
{
  if(!m_runtime)
  {
    return;
  }

  const TrainingSceneDescription& scene = m_runtime->scene();
  const int maxSourceIndex = m_cameraSourceLabels.empty() ? 0 : static_cast<int>(m_cameraSourceLabels.size() - 1);
  m_selectedCameraSource = std::clamp(m_selectedCameraSource, 0, maxSourceIndex);

  const CameraSpec camera = CameraForSource(scene, m_autoPreviewCamera, m_selectedCameraSource);
  const glm::vec3 target = m_selectedCameraSource == 0 ? ComputeViewerFocusTarget(scene, camera)
                                                       : ComputeUsdCameraResetTarget(scene, camera);
  m_state.visualCamera = camera;
  m_cameraController.reset(camera, target);
}

void ViewerApp::drawUi()
{
  const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
  const ImVec2 workPos = mainViewport->WorkPos;
  const ImVec2 workSize = mainViewport->WorkSize;
  float controlsWidth =
      std::clamp(workSize.x * kPreferredControlsPanelFraction, kMinControlsPanelWidth, kMaxControlsPanelWidth);
  if(workSize.x < 760.0f)
  {
    controlsWidth = std::min(controlsWidth, workSize.x * 0.42f);
  }
  const float viewportWidth = std::max(workSize.x - controlsWidth, 1.0f);

  ImGui::SetNextWindowPos(workPos, ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(viewportWidth, workSize.y), ImGuiCond_Always);
  drawViewportPanel();

  ImGui::SetNextWindowPos(ImVec2(workPos.x + viewportWidth, workPos.y), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(controlsWidth, workSize.y), ImGuiCond_Always);
  const ImGuiWindowFlags controlsFlags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
  ImGui::Begin("Controls", nullptr, controlsFlags);
  drawCameraPanel();
  drawSensorsPanel();
  drawReplayPanel();
  drawExportPanel();
  if(!m_statusLine.empty())
  {
    ImGui::Separator();
    ImGui::TextWrapped("%s", m_statusLine.c_str());
  }
  ImGui::End();
}

void ViewerApp::drawViewportPanel()
{
  const ImGuiWindowFlags viewportFlags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
  ImGui::Begin("Viewport", nullptr, viewportFlags);
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const ImVec2 viewportSize(std::max(available.x, 1.0f), std::max(available.y, 1.0f));
  if(m_viewportDescriptor != VK_NULL_HANDLE)
  {
    ImGui::Image(reinterpret_cast<ImTextureID>(m_viewportDescriptor), viewportSize);
  }
  else
  {
    ImGui::Dummy(viewportSize);
  }
  const bool viewportHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
  applyCameraInput(viewportHovered, viewportSize);
  ImGui::End();
}

void ViewerApp::drawCameraPanel()
{
  if(!ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
  {
    return;
  }
  if(!m_cameraSourceLabels.empty())
  {
    m_selectedCameraSource =
        std::clamp(m_selectedCameraSource, 0, static_cast<int>(m_cameraSourceLabels.size() - 1));
    ImGui::SetNextItemWidth(-FLT_MIN);
    if(ImGui::BeginCombo("Source", m_cameraSourceLabels[static_cast<size_t>(m_selectedCameraSource)].c_str()))
    {
      for(size_t i = 0; i < m_cameraSourceLabels.size(); ++i)
      {
        const bool selected = static_cast<int>(i) == m_selectedCameraSource;
        if(ImGui::Selectable(m_cameraSourceLabels[i].c_str(), selected))
        {
          m_selectedCameraSource = static_cast<int>(i);
          resetCameraControllerToSelectedCamera();
        }
        if(selected)
        {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
  }
  float fov = m_state.visualCamera.verticalFovDegrees;
  float clipStart = m_state.visualCamera.clipStart;
  float clipEnd = m_state.visualCamera.clipEnd;
  ImGui::SetNextItemWidth(-FLT_MIN);
  if(ImGui::SliderFloat("FOV", &fov, 1.0f, 179.0f))
  {
    m_cameraController.setVerticalFovDegrees(fov);
  }
  ImGui::SetNextItemWidth(-FLT_MIN);
  if(ImGui::InputFloat("Clip start", &clipStart) | ImGui::InputFloat("Clip end", &clipEnd))
  {
    m_cameraController.setClipRange(clipStart, clipEnd);
  }
  if(ImGui::Button("Reset camera"))
  {
    resetCameraControllerToSelectedCamera();
  }
  ImGui::SameLine();
  if(ImGui::Button("Print camera args"))
  {
    const CameraSpec camera = m_cameraController.camera();
    std::cout << "--preview-camera-position " << camera.position.x << "," << camera.position.y << ","
              << camera.position.z << " --preview-camera-target " << m_cameraController.target().x << ","
              << m_cameraController.target().y << "," << m_cameraController.target().z
              << " --preview-camera-fov " << camera.verticalFovDegrees << '\n';
    m_statusLine = "camera args printed to stdout";
  }
}

void ViewerApp::drawSensorsPanel()
{
  if(!ImGui::CollapsingHeader("Sensors", ImGuiTreeNodeFlags_DefaultOpen))
  {
    return;
  }
  const TrainingSceneDescription& scene = m_runtime->scene();
  drawSensorControls("LiDAR", m_state.lidar, scene.lidarSensors.size(), LidarSensorNames(scene));
  drawSensorControls("HeightScan", m_state.heightScan, scene.heightScanSensors.size(), HeightScanSensorNames(scene));
}

void ViewerApp::drawReplayPanel()
{
  if(!ImGui::CollapsingHeader("Replay", ImGuiTreeNodeFlags_DefaultOpen))
  {
    return;
  }
  if(ImGui::Button(m_state.replayPaused ? "Play" : "Pause"))
  {
    if(m_physicsFrameSource == nullptr)
    {
      m_statusLine = "USD file has no sampled animation";
    }
    else if(m_state.replayEnded)
    {
      m_statusLine = "USD animation ended";
    }
    else
    {
      const bool wasPaused = m_state.replayPaused;
      m_state.replayPaused = !m_state.replayPaused;
      if(wasPaused && !m_state.replayPaused)
      {
        resetReplayPlaybackClock();
      }
    }
  }
  ImGui::SameLine();
  if(ImGui::Button("Step"))
  {
    if(m_physicsFrameSource == nullptr)
    {
      m_statusLine = "USD file has no sampled animation";
    }
    else
    {
      stepReplay();
      m_nextReplayTick = ReplayClock::now() + kReplayFrameInterval;
    }
  }
  ImGui::SameLine();
  if(ImGui::Button("Reset"))
  {
    resetReplay();
  }
  ImGui::Text("Applied frames: %llu", static_cast<unsigned long long>(m_state.frameIndex));
  if(m_physicsFrameSource == nullptr)
  {
    ImGui::TextUnformatted("No USD animation samples");
  }
  else if(m_state.replayEnded)
  {
    ImGui::TextUnformatted("USD animation ended");
  }
}

void ViewerApp::drawExportPanel()
{
  if(!ImGui::CollapsingHeader("Export", ImGuiTreeNodeFlags_DefaultOpen))
  {
    return;
  }
  const bool hasOutputDir = !m_state.exportSettings.outputDir.empty();
  ImGui::TextWrapped("Output: %s", hasOutputDir ? m_state.exportSettings.outputDir.string().c_str() : "(disabled)");
  ImGui::BeginDisabled(!hasOutputDir);
  if(ImGui::Button("Save screenshot"))
  {
    m_pendingScreenshotExport = true;
  }
  if(ImGui::Button("Export LiDAR CSV"))
  {
    m_pendingLidarCsvExport = true;
  }
  if(ImGui::Button("Export HeightScan CSV"))
  {
    m_pendingHeightScanCsvExport = true;
  }
  if(ImGui::Button("Export debug bundle"))
  {
    m_pendingScreenshotExport = true;
    m_pendingLidarCsvExport = m_state.lidar.enableCompute;
    m_pendingHeightScanCsvExport = m_state.heightScan.enableCompute;
  }
  ImGui::EndDisabled();
}

void ViewerApp::drawSensorControls(const char* label, SensorViewerSettings& settings, size_t sensorCount,
                                   const std::vector<std::string>& sensorNames)
{
  if(!ImGui::TreeNode(label))
  {
    return;
  }
  bool changed = false;
  changed |= ImGui::Checkbox("Enable compute", &settings.enableCompute);
  changed |= ImGui::Checkbox("Show overlay", &settings.showOverlay);
  int mode = settings.displayMode == SensorDisplayMode::All ? 0 : 1;
  if(ImGui::RadioButton("All", mode == 0))
  {
    mode = 0;
    changed = true;
  }
  ImGui::SameLine();
  if(ImGui::RadioButton("Single", mode == 1))
  {
    mode = 1;
    changed = true;
  }
  settings.displayMode = mode == 0 ? SensorDisplayMode::All : SensorDisplayMode::Single;

  int selectedIndex = static_cast<int>(ClampSensorIndex(settings.sensorIndex, sensorCount));
  if(sensorNames.empty())
  {
    ImGui::BeginDisabled();
    ImGui::Combo("Sensor", &selectedIndex, "None\0");
    ImGui::EndDisabled();
  }
  else if(ImGui::BeginCombo("Sensor", sensorNames[static_cast<size_t>(selectedIndex)].c_str()))
  {
    for(size_t i = 0; i < sensorNames.size(); ++i)
    {
      const bool selected = selectedIndex == static_cast<int>(i);
      if(ImGui::Selectable(sensorNames[i].c_str(), selected))
      {
        selectedIndex = static_cast<int>(i);
        changed = true;
      }
      if(selected)
      {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  settings.sensorIndex = static_cast<uint32_t>(std::max(selectedIndex, 0));
  ImGui::SetNextItemWidth(-FLT_MIN);
  changed |= ImGui::SliderFloat("Point size", &settings.pointSizePixels, 0.0f, 12.0f);
  m_outputConfigDirty = m_outputConfigDirty || changed;
  ImGui::TreePop();
}

void ViewerApp::drawFrameToSwapchain()
{
  if(syncFramebufferResizeBeforeRender())
  {
    return;
  }
  prepareFrame();
  const uint32_t curFrame = getCurFrame();
  const VkCommandBuffer cmdBuf = getCommandBuffers()[curFrame];

  VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmdBuf, &beginInfo);

  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {{0.02f, 0.02f, 0.02f, 1.0f}};
  clearValues[1].depthStencil = {1.0f, 0};

  VkRenderPassBeginInfo renderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  renderPassBeginInfo.renderPass = m_renderPass;
  renderPassBeginInfo.framebuffer = m_framebuffers[curFrame];
  renderPassBeginInfo.renderArea = {{0, 0}, m_size};
  renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassBeginInfo.pClearValues = clearValues.data();
  vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);
  vkCmdEndRenderPass(cmdBuf);

  vkEndCommandBuffer(cmdBuf);
  submitFrame();
}

void ViewerApp::framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
  auto* app = static_cast<ViewerApp*>(glfwGetWindowUserPointer(window));
  if(app != nullptr)
  {
    app->deferFramebufferResize(width, height);
  }
}

void ViewerApp::deferFramebufferResize(int width, int height)
{
  if(width <= 0 || height <= 0)
  {
    return;
  }
  m_pendingResizeWidth = width;
  m_pendingResizeHeight = height;
  m_resizePending = true;
  if(ImGui::GetCurrentContext() != nullptr)
  {
    ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
  }
}

void ViewerApp::applyCameraInput(bool viewportHovered, ImVec2 viewportSize)
{
  const ImGuiIO& io = ImGui::GetIO();
  const bool     rightDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
  const bool     middleDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);

  if(!rightDown)
  {
    m_orbitDragActive = false;
  }
  else if(viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
  {
    m_orbitDragActive = true;
  }

  if(!middleDown)
  {
    m_panDragActive = false;
  }
  else if(viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
  {
    m_panDragActive = true;
  }

  ViewerCameraInput input;
  input.viewportWidth = static_cast<int>(viewportSize.x);
  input.viewportHeight = static_cast<int>(viewportSize.y);
  input.deltaX = io.MouseDelta.x;
  input.deltaY = io.MouseDelta.y;
  input.orbit = m_orbitDragActive && rightDown;
  input.pan = m_panDragActive && middleDown;
  if(viewportHovered)
  {
    input.wheelDelta = io.MouseWheel;
  }
  m_cameraController.update(input);
}

bool ViewerApp::syncFramebufferResizeBeforeRender()
{
  if(m_window == nullptr || !m_appReady)
  {
    return false;
  }

  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(m_window, &width, &height);
  if(width > 0 && height > 0 &&
     (static_cast<uint32_t>(width) != m_size.width || static_cast<uint32_t>(height) != m_size.height))
  {
    deferFramebufferResize(width, height);
  }
  if(!m_resizePending)
  {
    return false;
  }

  const int pendingWidth = m_pendingResizeWidth;
  const int pendingHeight = m_pendingResizeHeight;
  m_resizePending = false;
  m_pendingResizeWidth = 0;
  m_pendingResizeHeight = 0;
  if(pendingWidth > 0 && pendingHeight > 0 &&
     (static_cast<uint32_t>(pendingWidth) != m_size.width || static_cast<uint32_t>(pendingHeight) != m_size.height))
  {
    nvvkhl::AppBaseVk::onFramebufferSize(pendingWidth, pendingHeight);
    return true;
  }
  return false;
}

void ViewerApp::applyOutputConfigIfDirty()
{
  if(!m_outputConfigDirty)
  {
    return;
  }
  m_lastOutputConfig = BuildViewerOutputConfig(m_runtime->scene(), m_state);
  m_engine.configureOutputs(m_lastOutputConfig);
  m_outputConfigDirty = false;
}

void ViewerApp::processPendingExports()
{
  if(m_pendingScreenshotExport)
  {
    m_statusLine = ExportViewerScreenshot(m_engine, m_state.exportSettings, m_state.frameIndex);
    m_pendingScreenshotExport = false;
  }
  if(m_pendingLidarCsvExport)
  {
    m_statusLine = ExportViewerLidarCsv(m_engine, m_state.exportSettings, m_state.frameIndex);
    m_pendingLidarCsvExport = false;
  }
  if(m_pendingHeightScanCsvExport)
  {
    m_statusLine = ExportViewerHeightScanCsv(m_engine, m_state.exportSettings, m_state.frameIndex);
    m_pendingHeightScanCsvExport = false;
  }
}

void ViewerApp::resetReplayPlaybackClock()
{
  m_nextReplayTick = ReplayClock::now();
}

void ViewerApp::advanceReplayForPlayback()
{
  if(m_state.replayPaused || m_state.replayEnded || !m_physicsFrameSource)
  {
    return;
  }

  const auto now = ReplayClock::now();
  if(now < m_nextReplayTick)
  {
    return;
  }

  stepReplay();
  m_nextReplayTick += kReplayFrameInterval;
  if(m_nextReplayTick < now)
  {
    m_nextReplayTick = now + kReplayFrameInterval;
  }
}

void ViewerApp::stepReplay()
{
  if(!m_physicsFrameSource || m_state.replayEnded)
  {
    return;
  }
  PhysicsFrameSnapshot snapshot;
  const PhysicsFrameStatus status = m_physicsFrameSource->nextFrame(snapshot);
  if(status == PhysicsFrameStatus::EndOfStream)
  {
    m_state.replayEnded = true;
    m_state.replayPaused = true;
    m_statusLine = "USD animation ended";
    return;
  }
  if(status == PhysicsFrameStatus::NoSource)
  {
    m_statusLine = "USD file has no sampled animation";
    return;
  }
  m_physicsFabric.publish(std::move(snapshot));
  const TrainingFrameApplyResult applyResult = ApplyLatestFabricFrame(m_physicsFabric, *m_runtime, m_engine);
  if(applyResult.sensorOutputsDirty)
  {
    m_outputConfigDirty = true;
  }
  m_state.frameIndex = m_nextAnimationFrameIndex;
  ++m_nextAnimationFrameIndex;
}

void ViewerApp::resetReplay()
{
  if(m_physicsFrameSource == nullptr)
  {
    m_statusLine = "USD file has no sampled animation";
    return;
  }

  m_physicsFrameSource->reset();
  m_physicsFabric.clear();
  m_state.replayEnded = false;
  m_state.replayPaused = true;
  m_nextReplayTick = {};
  m_nextAnimationFrameIndex = 0;
  stepReplay();
}

} // namespace headless_training
