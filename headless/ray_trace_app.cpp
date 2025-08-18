#include "nvh/cameramanipulator.hpp"
#include "nvh/fileoperations.hpp"
#include "nvvk/commands_vk.hpp"
#include <cassert>
#include <array>
#include "ray_trace_app.hpp"

// opengl上下文
#include "nvgl/contextwindow_gl.hpp"
#include <algorithm>

#include "obj_loader.h"

std::vector<std::string> defaultSearchPaths;

RayTraceApp::RayTraceApp() 
{
}

RayTraceApp::~RayTraceApp()
{
  cleanup();
}

void RayTraceApp::setup(int width, int height)
{
  m_width = width;
  m_height = height;
  setupCamera();
  setupContext();
  setupHelloVulkan();
}

void RayTraceApp::resize(int w, int h)
{
  m_helloVk.onResize(w,h);
}

void RayTraceApp::setupCamera()
{
  CameraManip.setWindowSize(m_width, m_height);
  CameraManip.setLookat(glm::vec3(5, 4, -4), glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));
}

void RayTraceApp::UpdateCamera()
{
}

#include <filesystem>
namespace fs = std::filesystem;
void RayTraceApp::setupContext()
{
  NVPSystem system("raytrace_vulkan_headless");
  std::string currentDir = fs::current_path().string();
  defaultSearchPaths = {
      NVPSystem::exePath() + PROJECT_RELDIRECTORY,
      NVPSystem::exePath() + PROJECT_RELDIRECTORY "/..",
      currentDir + "/headless",
  };

  nvvk::ContextCreateInfo contextInfo;
  contextInfo.setVersion(1, 2);

  VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
  contextInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &accelFeature);
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
  contextInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false, &rtPipelineFeature);
  contextInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

#if ENABLE_GL_VK_CONVERSION
  contextInfo.addInstanceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  contextInfo.addInstanceExtension(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
  contextInfo.addInstanceExtension(VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME);
  contextInfo.addInstanceExtension(VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
  
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME);
  
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME);

  contextInfo.addDeviceExtension(VK_NV_RAY_TRACING_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
#endif

  m_vkctx.initInstance(contextInfo);
  auto compatibleDevices = m_vkctx.getCompatibleDevices(contextInfo);
  assert(!compatibleDevices.empty());
  m_vkctx.initDevice(compatibleDevices[0], contextInfo);
}

void RayTraceApp::setupHelloVulkan()
{
  nvvkhl::AppBaseVkCreateInfo createInfo;
  createInfo.instance       = m_vkctx.m_instance;
  createInfo.device         = m_vkctx.m_device;
  createInfo.physicalDevice = m_vkctx.m_physicalDevice;
  createInfo.queueIndices   = {m_vkctx.m_queueGCT.familyIndex};
  createInfo.size           = {uint32_t(m_width), uint32_t(m_height)};
  m_helloVk.create(createInfo);
}

void RayTraceApp::createBVH()
{
  // 后续初始化
  m_helloVk.createOffscreenRender();
  m_helloVk.createDescriptorSetLayout();
  m_helloVk.createGraphicsPipeline();
  m_helloVk.createUniformBuffer();
  m_helloVk.createObjDescriptionBuffer();
  m_helloVk.updateDescriptorSet();

  // 光线追踪相关
  m_helloVk.initRayTracing();
  m_helloVk.createBottomLevelAS();
  m_helloVk.createTopLevelAS();
  m_helloVk.createRtDescriptorSet();
  m_helloVk.createRtPipeline();

  // 计算着色器相关
  m_helloVk.createCompDescriptors();
  m_helloVk.createCompPipelines();
}

void RayTraceApp::render()
{
  auto                   curFrame = m_helloVk.getCurFrame();
  const VkCommandBuffer& cmdBuf   = m_helloVk.getCommandBuffers()[curFrame];

  VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmdBuf, &beginInfo);

  m_helloVk.updateUniformBuffer(cmdBuf);

  std::array<VkClearValue, 2> clearValues{};
  glm::vec4                   clearColor = glm::vec4(1, 1, 1, 1.00f);
  clearValues[0].color                   = {{clearColor[0], clearColor[1], clearColor[2], clearColor[3]}};
  clearValues[1].depthStencil            = {1.0f, 0};

  m_helloVk.raytrace(cmdBuf, clearColor);

  vkEndCommandBuffer(cmdBuf);
  m_helloVk.submitFrame();
}

void RayTraceApp::saveFrame(std::string outputImagePath)
{
#if ENABLE_GL_VK_CONVERSION
  std::string gl_pngname = outputImagePath;
  gl_pngname.replace(gl_pngname.find("/"), 1, "/gl_");
  m_helloVk.dumpInteropTexture(gl_pngname.c_str());
#else
  std::string vk_pngname = outputImagePath;
  vk_pngname.replace(vk_pngname.find("/"), 1, "/vk_");
  m_helloVk.saveOffscreenColorToFile(vk_pngname.c_str());
#endif
}

void RayTraceApp::cleanup()
{
  vkDeviceWaitIdle(m_helloVk.getDevice());
  m_helloVk.destroyResources();
  m_helloVk.destroy();
  m_vkctx.deinit();
}

//-----------------------------------------------------------------------------------------------------
// for demo local test
// 
void RayTraceApp::loadScene()
{
  // 平面
  ObjLoader planeLoader;
  planeLoader.loadModel(nvh::findFile("media/scenes/plane.obj", defaultSearchPaths, true));
  m_helloVk.loadModel(planeLoader,
                      glm::scale(glm::mat4(1.f), glm::vec3(2.f, 1.f, 2.f)));

  // wuson
  ObjLoader wusonLoader;
  wusonLoader.loadModel(nvh::findFile("media/scenes/wuson.obj", defaultSearchPaths, true));
  m_helloVk.loadModel(wusonLoader);

  // 多个wuson实例
  uint32_t  wusonId = 1;
  glm::mat4 identity{1};
  for(int i = 0; i < 5; i++)
  {
    m_helloVk.m_instances.push_back({identity, wusonId});
  }

  // 球体
  ObjLoader sphereLoader;
  sphereLoader.loadModel(nvh::findFile("media/scenes/sphere.obj", defaultSearchPaths, true));
  m_helloVk.loadModel(sphereLoader);

  // 
  m_startTime = std::chrono::system_clock::now();
}

void RayTraceApp::animation()
{
  std::chrono::duration<float> diff = std::chrono::system_clock::now() - m_startTime;
  m_helloVk.animationObject(diff.count());
  m_helloVk.animationInstances(diff.count());
}