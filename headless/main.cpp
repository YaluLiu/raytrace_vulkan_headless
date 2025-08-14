#include "headless_hellovulkan_app.hpp"

int main()
{
    HeadlessHelloVulkanApp app(1280, 720);
    app.initialize();
    app.loadScene();
    app.renderLoop(10);     // 渲染10帧
    // 渲染结束后自动清理
    return 0;
}