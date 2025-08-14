#include "headless_hellovulkan_app.hpp"
#include "nvgl/contextwindow_gl.hpp"
int main()
{
#if ENABLE_GL_VK_CONVERSION
  glfwInit();
  // 设置 GLFW 窗口使用 OpenGL 4.5
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
  // 创建 GLFW 窗口
  GLFWwindow* gl_window = glfwCreateWindow(100, 100, PROJECT_NAME, NULL, NULL);
  // 设置当前 OpenGL 上下文
  glfwMakeContextCurrent(gl_window);

  // 加载OpenGL函数
  load_GL(nvgl::ContextWindow::sysGetProcAddress);
#endif

  int width = 1280;
  int height = 720;
  HeadlessHelloVulkanApp app(width, height);
  app.initialize();
  app.loadScene();
  app.createBVH();
  for(int i = 0; i < 10; i++)
  {
    // if(i % 3 == 0){
    //   width *= 1.1;
    //   height *= 1.1;
    // }
    app.resize(width, height);
    app.update();
    app.render();
    app.saveFrame();
  }
  return 0;
}