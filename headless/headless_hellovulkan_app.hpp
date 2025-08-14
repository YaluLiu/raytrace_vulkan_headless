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
    HeadlessHelloVulkanApp();
    HeadlessHelloVulkanApp(int width = 1280, int height = 720);
    ~HeadlessHelloVulkanApp();

    // 初始化Vulkan和HelloVulkan
    void initialize();

    // 加载OBJ模型
    void loadScene();

    // 更新模型的mesh和translation
    void update();

    // 渲染
    void render();

    // 清理资源
    void cleanup();

    // save local png file
    void saveFrame(std::string outputImagePath="headless.png");

private:
    int m_width = 1280;
    int m_height = 720;

    HelloVulkan m_helloVk;

    // Vulkan相关
    nvvk::Context m_vkctx;

    // 内部辅助函数
    void setupCamera();
    void setupContext();
    void setupHelloVulkan();

    // 计时，计算动画
    std::chrono::system_clock::time_point m_startTime;
};