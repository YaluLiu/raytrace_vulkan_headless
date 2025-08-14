#pragma once

#include "hello_vulkan.h"
#include <vector>
#include <string>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "nvpsystem.hpp"
#include "nvvk/context_vk.hpp"

class HeadlessHelloVulkanApp
{
public:
    HeadlessHelloVulkanApp(int width = 1280, int height = 720);
    ~HeadlessHelloVulkanApp();

    // 初始化Vulkan和HelloVulkan
    void initialize();

    // 加载OBJ模型
    void loadScene();

    // 渲染循环，frameCount为渲染帧数
    void renderLoop(int frameCount = 10);

    // 清理资源
    void cleanup();

    // 获取渲染输出图片路径
    const std::string& getOutputImagePath() const { return m_outputImagePath; }

private:
    int m_width = 1280;
    int m_height = 720;

    HelloVulkan m_helloVk;
    std::string m_outputImagePath = "headless.png";
    std::chrono::system_clock::time_point m_startTime;

    // Vulkan相关
    nvvk::Context m_vkctx;

    // 内部辅助函数
    void setupCamera();
    void setupContext();
    void setupHelloVulkan();
};