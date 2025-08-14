#include "headless_hellovulkan_app.hpp"

int main()
{
  int width = 1280;
  int height = 720;
  HeadlessHelloVulkanApp app(width, height);
  app.initialize();
  app.loadScene();
  app.createBVH();
  for(int i = 0; i < 10; i++)
  {
    if(i % 3 == 0){
      width *= 1.1;
      height *= 1.1;
    }
    app.resize(width, height);
    app.update();
    app.render();
    app.saveFrame();
  }
  return 0;
}