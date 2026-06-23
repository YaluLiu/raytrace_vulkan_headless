#include "viewer_app.h"

#include "output_writers.h"
#include "preview_camera.h"
#include "usd_scene_loader.h"

#include <engine/aov_texture.hpp>

#include "GLFW/glfw3.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "imgui/imgui_helper.h"
#include "nvp/nvpsystem.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <utility>

namespace headless_training
{
namespace
{
constexpr float kMinControlsPanelWidth = 280.0f;
constexpr float kMaxControlsPanelWidth = 380.0f;
constexpr float kPreferredControlsPanelFraction = 0.26f;

bool IsOption(const char* arg, const char* name)
{
  return std::strcmp(arg, name) == 0;
}

bool ReadValue(int& index, int argc, char** argv, std::string& value, std::string& error)
{
  if(index + 1 >= argc)
  {
    error = std::string("missing value for ") + argv[index];
    return false;
  }
  value = argv[++index];
  return true;
}

bool ParsePositiveInt(const std::string& text, int& value)
{
  char* end = nullptr;
  const long parsed = std::strtol(text.c_str(), &end, 10);
  if(end == text.c_str() || *end != '\0' || parsed <= 0 || parsed > std::numeric_limits<int>::max())
  {
    return false;
  }
  value = static_cast<int>(parsed);
  return true;
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

CameraSpec SelectInitialCamera(const TrainingSceneDescription& scene, const ViewerCliOptions& options)
{
  UsdSceneLoadOptions loadOptions;
  loadOptions.cameraPath = options.cameraPath;
  if(!options.cameraPath.empty() && !scene.cameras.empty())
  {
    return scene.cameras.front();
  }
  return BuildPreviewCamera(scene);
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

void ThrowIfWriteFailed(bool ok, const std::string& errorMessage, const std::filesystem::path& path)
{
  if(!ok)
  {
    throw std::runtime_error(errorMessage.empty() ? "failed to write " + path.string() : errorMessage);
  }
}
} // namespace

ViewerCliParseResult ParseViewerCommandLine(int argc, char** argv)
{
  ViewerCliParseResult result;
  result.ok = true;

  for(int i = 1; i < argc; ++i)
  {
    const char* arg = argv[i];
    std::string value;
    if(IsOption(arg, "--help") || IsOption(arg, "-h"))
    {
      result.options.showHelp = true;
    }
    else if(IsOption(arg, "--usd"))
    {
      if(!ReadValue(i, argc, argv, value, result.error))
      {
        result.ok = false;
        return result;
      }
      result.options.usdPath = value;
    }
    else if(IsOption(arg, "--width") || IsOption(arg, "--height"))
    {
      if(!ReadValue(i, argc, argv, value, result.error))
      {
        result.ok = false;
        return result;
      }
      int parsed = 0;
      if(!ParsePositiveInt(value, parsed))
      {
        result.ok = false;
        result.error = std::string("invalid positive integer for ") + arg + ": " + value;
        return result;
      }
      if(IsOption(arg, "--width"))
      {
        result.options.width = parsed;
      }
      else
      {
        result.options.height = parsed;
      }
    }
    else if(IsOption(arg, "--physics-replay"))
    {
      if(!ReadValue(i, argc, argv, value, result.error))
      {
        result.ok = false;
        return result;
      }
      result.options.physicsReplayPath = value;
    }
    else if(IsOption(arg, "--camera"))
    {
      if(!ReadValue(i, argc, argv, value, result.error))
      {
        result.ok = false;
        return result;
      }
      result.options.cameraPath = value;
    }
    else if(IsOption(arg, "--plugin-search-root"))
    {
      if(!ReadValue(i, argc, argv, value, result.error))
      {
        result.ok = false;
        return result;
      }
      result.options.pluginSearchRoot = value;
    }
    else if(IsOption(arg, "--output-dir"))
    {
      if(!ReadValue(i, argc, argv, value, result.error))
      {
        result.ok = false;
        return result;
      }
      result.options.outputDir = value;
    }
    else if(IsOption(arg, "--enable-lidar"))
    {
      result.options.enableLidar = true;
    }
    else if(IsOption(arg, "--disable-lidar"))
    {
      result.options.enableLidar = false;
    }
    else if(IsOption(arg, "--enable-height-scan"))
    {
      result.options.enableHeightScan = true;
    }
    else if(IsOption(arg, "--disable-height-scan"))
    {
      result.options.enableHeightScan = false;
    }
    else
    {
      result.ok = false;
      result.error = std::string("unknown option: ") + arg;
      return result;
    }
  }

  if(!result.options.showHelp && result.options.usdPath.empty())
  {
    result.ok = false;
    result.error = "missing required --usd <path>";
  }
  return result;
}

std::string BuildViewerHelpText()
{
  return "Usage: robot_training_viewer --usd <path> [options]\n\n"
         "Options:\n"
         "  --width <pixels>             Window and render width (default: 1280)\n"
         "  --height <pixels>            Window and render height (default: 720)\n"
         "  --physics-replay <path>      Optional replay JSON\n"
         "  --camera <usd-path>          Optional USD camera path\n"
         "  --plugin-search-root <path>  Optional shader/plugin search root\n"
         "  --output-dir <path>          Optional directory for explicit debug exports\n"
         "  --enable-lidar               Enable LiDAR compute at startup\n"
         "  --disable-lidar              Disable LiDAR compute at startup\n"
         "  --enable-height-scan         Enable height scan compute at startup\n"
         "  --disable-height-scan        Disable height scan compute at startup\n"
         "  --help                       Show this help\n";
}

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
  m_engine.resize(width, height);
  m_resizePending = true;
  m_viewportDescriptor = VK_NULL_HANDLE;
  m_registeredImageView = VK_NULL_HANDLE;
}

void ViewerApp::initialize()
{
  UsdSceneLoadOptions loadOptions;
  loadOptions.cameraPath = m_options.cameraPath;
  TrainingSceneDescription scene = LoadUsdTrainingScene(m_options.usdPath, loadOptions);

  m_state.lidar.enableCompute = m_options.enableLidar;
  m_state.heightScan.enableCompute = m_options.enableHeightScan;
  m_state.exportSettings.outputDir = m_options.outputDir;
  const CameraSpec initialCamera = SelectInitialCamera(scene, m_options);
  m_state.visualCamera = initialCamera;
  m_cameraController.reset(initialCamera, ComputeViewerFocusTarget(scene, initialCamera));

  m_runtime.emplace(std::move(scene));
  if(!m_options.physicsReplayPath.empty())
  {
    m_physicsReplay = LoadPhysicsReplay(m_options.physicsReplayPath);
  }

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
  create(createInfo);
  m_appReady = true;
  createViewportSampler();
  initializeEngine();
}

void ViewerApp::shutdown()
{
  if(m_engineReady)
  {
    m_engine.cleanup();
    m_engineReady = false;
  }

  if(m_viewportDescriptor != VK_NULL_HANDLE && ImGui::GetCurrentContext() != nullptr)
  {
    ImGui_ImplVulkan_RemoveTexture(m_viewportDescriptor);
    m_viewportDescriptor = VK_NULL_HANDLE;
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

void ViewerApp::refreshViewportTexture()
{
  const std::optional<ExportedAovTexture> colorAov = m_engine.GetAovTexture(Aov::Color);
  if(!colorAov || colorAov->imageView == VK_NULL_HANDLE)
  {
    return;
  }
  if(m_viewportDescriptor != VK_NULL_HANDLE && m_registeredImageView == colorAov->imageView)
  {
    return;
  }
  if(m_viewportDescriptor != VK_NULL_HANDLE)
  {
    ImGui_ImplVulkan_RemoveTexture(m_viewportDescriptor);
  }
  m_viewportDescriptor =
      ImGui_ImplVulkan_AddTexture(m_viewportSampler, colorAov->imageView, colorAov->layout);
  m_registeredImageView = colorAov->imageView;
}

void ViewerApp::runFrame()
{
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  drawUi();

  if(!m_state.replayPaused && !m_state.replayEnded)
  {
    stepReplay();
  }

  m_state.visualCamera = m_cameraController.camera();
  m_engine.setMainCamera(m_state.visualCamera);
  applyOutputConfigIfDirty();
  m_engine.render();
  refreshViewportTexture();
  processPendingExports();

  ImGui::Render();
  drawFrameToSwapchain();
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
    const CameraSpec resetCamera = SelectInitialCamera(m_runtime->scene(), m_options);
    m_cameraController.reset(resetCamera, ComputeViewerFocusTarget(m_runtime->scene(), resetCamera));
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
    if(m_physicsReplay == nullptr)
    {
      m_statusLine = "load a replay with --physics-replay to play frame updates";
    }
    else if(m_state.replayEnded)
    {
      m_statusLine = "replay ended";
    }
    else
    {
      m_state.replayPaused = !m_state.replayPaused;
    }
  }
  ImGui::SameLine();
  if(ImGui::Button("Step"))
  {
    if(m_physicsReplay == nullptr)
    {
      m_statusLine = "load a replay with --physics-replay to step frames";
    }
    else
    {
      stepReplay();
    }
  }
  ImGui::Text("Applied frames: %llu", static_cast<unsigned long long>(m_state.frameIndex));
  if(m_physicsReplay == nullptr)
  {
    ImGui::TextUnformatted("No replay loaded");
  }
  else if(m_state.replayEnded)
  {
    ImGui::TextUnformatted("Replay ended");
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
    exportScreenshot();
    m_pendingScreenshotExport = false;
  }
  if(m_pendingLidarCsvExport)
  {
    exportLidarCsv();
    m_pendingLidarCsvExport = false;
  }
  if(m_pendingHeightScanCsvExport)
  {
    exportHeightScanCsv();
    m_pendingHeightScanCsvExport = false;
  }
}

void ViewerApp::stepReplay()
{
  if(!m_physicsReplay || m_state.replayEnded)
  {
    return;
  }
  std::vector<InstancePoseUpdate> updates;
  if(!m_physicsReplay->nextFrame(updates))
  {
    m_state.replayEnded = true;
    m_state.replayPaused = true;
    m_statusLine = "replay ended";
    return;
  }
  m_runtime->applyPoseUpdates(m_engine, updates);
  m_state.frameIndex = m_nextReplayFrameIndex;
  ++m_nextReplayFrameIndex;
}

void ViewerApp::exportScreenshot()
{
  const std::filesystem::path path = viewerDebugPath("frame", ".png");
  std::filesystem::create_directories(path.parent_path());
  m_engine.saveFrame(path.string());
  m_statusLine = "saved " + path.string();
}

void ViewerApp::exportLidarCsv()
{
  const std::filesystem::path path = viewerDebugPath("lidar_frame", ".csv");
  std::filesystem::create_directories(path.parent_path());
  std::string error;
  ThrowIfWriteFailed(WriteLidarCsv(m_engine.readLidarPointCloudFrame(), m_state.frameIndex, path, &error), error, path);
  m_statusLine = "saved " + path.string();
}

void ViewerApp::exportHeightScanCsv()
{
  const std::filesystem::path path = viewerDebugPath("height_scan_frame", ".csv");
  std::filesystem::create_directories(path.parent_path());
  std::string error;
  ThrowIfWriteFailed(WriteHeightScanCsv(m_engine.readHeightScanFrame(), m_state.frameIndex, path, &error), error, path);
  m_statusLine = "saved " + path.string();
}

void ViewerApp::exportDebugBundle()
{
  exportScreenshot();
  if(m_state.lidar.enableCompute)
  {
    exportLidarCsv();
  }
  if(m_state.heightScan.enableCompute)
  {
    exportHeightScanCsv();
  }
  m_statusLine = "saved debug bundle to " + (m_state.exportSettings.outputDir / "viewer_debug").string();
}

std::filesystem::path ViewerApp::viewerDebugPath(const std::string& prefix, const std::string& extension) const
{
  return makeUniquePath(m_state.exportSettings.outputDir / "viewer_debug" /
                        FormatFrameFileName(prefix, m_state.frameIndex, extension));
}

std::filesystem::path ViewerApp::makeUniquePath(std::filesystem::path path) const
{
  if(!std::filesystem::exists(path))
  {
    return path;
  }

  const std::filesystem::path parent = path.parent_path();
  const std::string stem = path.stem().string();
  const std::string extension = path.extension().string();
  for(int suffix = 1; suffix < 10000; ++suffix)
  {
    std::ostringstream name;
    name << stem << "_" << suffix;
    std::filesystem::path candidate = parent / (name.str() + extension);
    if(!std::filesystem::exists(candidate))
    {
      return candidate;
    }
  }
  return path;
}

} // namespace headless_training
