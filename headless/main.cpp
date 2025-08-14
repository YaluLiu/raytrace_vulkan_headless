#include "headless_hellovulkan_app.hpp"

int main()
{
    HeadlessHelloVulkanApp app(1280, 720);
    app.initialize();
    app.loadScene();
    for(int i = 0; i < 10; i ++){
        app.update();
        app.render();
        app.saveFrame();
    }
    return 0;
}