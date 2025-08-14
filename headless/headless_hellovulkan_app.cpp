#include "nvh/cameramanipulator.hpp"
#include "nvh/fileoperations.hpp"
#include "nvvk/commands_vk.hpp"
#include <cassert>
#include <array>
#include "headless_hellovulkan_app.hpp"

std::vector<std::string> defaultSearchPaths;

HeadlessHelloVulkanApp::HeadlessHelloVulkanApp()
{
}

HeadlessHelloVulkanApp::HeadlessHelloVulkanApp(int width, int height)
    : m_width(width), m_height(height)
{
}

HeadlessHelloVulkanApp::~HeadlessHelloVulkanApp()
{
    cleanup();
}

void HeadlessHelloVulkanApp::initialize()
{
    setupCamera();
    setupContext();
    setupHelloVulkan();
}

void HeadlessHelloVulkanApp::setupCamera()
{
    CameraManip.setWindowSize(m_width, m_height);
    CameraManip.setLookat(glm::vec3(5, 4, -4), glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));
}

void HeadlessHelloVulkanApp::setupContext()
{
    NVPSystem system("raytrace_vulkan_headless");
    defaultSearchPaths = {
        NVPSystem::exePath() + PROJECT_RELDIRECTORY,
        NVPSystem::exePath() + PROJECT_RELDIRECTORY "..",
        std::string("raytrace_vulkan_headless"),
    };

    nvvk::ContextCreateInfo contextInfo;
    contextInfo.setVersion(1, 2);

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    contextInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &accelFeature);
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    contextInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false, &rtPipelineFeature);
    contextInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

    m_vkctx.initInstance(contextInfo);
    auto compatibleDevices = m_vkctx.getCompatibleDevices(contextInfo);
    assert(!compatibleDevices.empty());
    m_vkctx.initDevice(compatibleDevices[0], contextInfo);
}

void HeadlessHelloVulkanApp::setupHelloVulkan()
{
    nvvkhl::AppBaseVkCreateInfo createInfo;
    createInfo.instance = m_vkctx.m_instance;
    createInfo.device = m_vkctx.m_device;
    createInfo.physicalDevice = m_vkctx.m_physicalDevice;
    createInfo.queueIndices = {m_vkctx.m_queueGCT.familyIndex};
    createInfo.size = {uint32_t(m_width), uint32_t(m_height)};
    m_helloVk.create(createInfo);
}

void HeadlessHelloVulkanApp::loadScene()
{
    // 地面
    m_helloVk.loadModel(nvh::findFile("media/scenes/plane.obj", defaultSearchPaths, true),
                        glm::scale(glm::mat4(1.f), glm::vec3(2.f, 1.f, 2.f)));
    // wuson
    m_helloVk.loadModel(nvh::findFile("media/scenes/wuson.obj", defaultSearchPaths, true));
    // 多个wuson实例
    uint32_t  wusonId = 1;
    glm::mat4 identity{1};
    for(int i = 0; i < 5; i++)
    {
        m_helloVk.m_instances.push_back({identity, wusonId});
    }
    // 球体
    m_helloVk.loadModel(nvh::findFile("media/scenes/sphere.obj", defaultSearchPaths, true));

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

    m_startTime = std::chrono::system_clock::now();
}

void HeadlessHelloVulkanApp::update()
{
    std::chrono::duration<float> diff = std::chrono::system_clock::now() - m_startTime;
    m_helloVk.animationObject(diff.count());
    m_helloVk.animationInstances(diff.count());
}

void HeadlessHelloVulkanApp::render()
{
    auto curFrame = m_helloVk.getCurFrame();
    const VkCommandBuffer& cmdBuf = m_helloVk.getCommandBuffers()[curFrame];

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuf, &beginInfo);

    m_helloVk.updateUniformBuffer(cmdBuf);

    std::array<VkClearValue, 2> clearValues{};
    glm::vec4 clearColor   = glm::vec4(1, 1, 1, 1.00f);
    clearValues[0].color        = {{clearColor[0], clearColor[1], clearColor[2], clearColor[3]}};
    clearValues[1].depthStencil = {1.0f, 0};

    m_helloVk.raytrace(cmdBuf, clearColor);

    vkEndCommandBuffer(cmdBuf);
    m_helloVk.submitFrame();
}

void HeadlessHelloVulkanApp::saveFrame(std::string outputImagePath)
{
    m_helloVk.saveOffscreenColorToFile(outputImagePath.c_str());
}

void HeadlessHelloVulkanApp::cleanup()
{
    vkDeviceWaitIdle(m_helloVk.getDevice());
    m_helloVk.destroyResources();
    m_helloVk.destroy();
    m_vkctx.deinit();
}