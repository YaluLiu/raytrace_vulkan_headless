# raytrace_vulkan_headless

Headless Vulkan ray tracing demo

## 项目介绍

本项目基于 [nvpro-samples/vk_raytracing_tutorial_KHR](https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR) 仓库下 `ray_tracing_animation` 文件夹中的 demo，进行了如下修改：

- **去除窗口、surface 和 swapchain**，实现了 Headless 版本，适用于无界面环境。
- 其余代码结构、编译方式保持与原仓库一致。

## 依赖

- 需要额外克隆 [nvpro-samples/nvpro_core](https://github.com/nvpro-samples/nvpro_core) 仓库。
- nvpro_core 支持两种放置方式：

  1. 放在当前项目根目录下；
  2. 或与本项目处于同一级目录下。

  具体依赖查找逻辑请参考本项目的 `CMakeLists.txt` 中的 `find_path` 函数。

## 编译方式

编译流程与 [nvpro-samples/vk_raytracing_tutorial_KHR](https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR) 完全一致：

1. 克隆本仓库和 nvpro_core：

   ```bash
   git clone https://github.com/YaluLiu/raytrace_vulkan_headless.git
   git clone https://github.com/nvpro-samples/nvpro_core.git
   ```

   > 可以将 nvpro_core 放在 raytrace_vulkan_headless 同级目录。

## 运行方式

编译完成后，直接运行：

```bash
bash install.sh demo
```

可以运行 headless 的测试 demo

```bash
bash install.sh gatling
```

可以运行基于 headless 编译的 hydra 插件

## 参考

- [nvpro-samples/vk_raytracing_tutorial_KHR](https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR)
- [nvpro-samples/nvpro_core](https://github.com/nvpro-samples/nvpro_core)
- [pablode/gatling](https://github.com/pablode/gatling.git)

## License

本项目遵循原仓库的 License 规则。
