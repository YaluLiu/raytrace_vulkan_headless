# raytrace_vulkan_headless

Headless Vulkan rasterization renderer

## 项目介绍

本项目是一个支持 OpenUSD Hydra 插件入口的 headless Vulkan 渲染器。当前实现
是精简的光栅化基线，输出 color、depth、primitive ID 和 instance ID AOV。

主要修改包括：

- **去除窗口、surface 和 swapchain**，实现了 Headless 版本，适用于无界面环境。
- 增加 Hydra render delegate 集成，供 headless 光栅渲染器使用。

## 依赖

- 需要额外克隆 [nvpro-samples/nvpro_core](https://github.com/nvpro-samples/nvpro_core) 仓库。
- nvpro_core 支持两种放置方式：

  1. 放在当前项目根目录下；
  2. 或与本项目处于同一级目录下。

  具体依赖查找逻辑请参考本项目的 `CMakeLists.txt` 中的 `find_path` 函数。

## 编译方式

项目使用 CMake 和 nvpro_core helper 库进行构建：

1. 克隆本仓库和 nvpro_core：

   ```bash
   git clone https://github.com/YaluLiu/raytrace_vulkan_headless.git
   git clone https://github.com/nvpro-samples/nvpro_core.git
   ```

   > 可以将 nvpro_core 放在 raytrace_vulkan_headless 同级目录。

2. 修改 nvpro_core 的 cmakelists，屏蔽掉下面的 code,启用 C++11

   ```
   add_definitions(-D_GLIBCXX_USE_CXX11_ABI=1)
   ```

## 运行方式

编译完成后，可以运行 Hydra 插件流程：

```bash
bash install.sh hydra
```

可以运行基于 headless 编译的 hydra 插件

可以用 `HYDRA_SCENE_PATH` 覆盖 `install.sh hydra` 默认打开的 Hydra 场景：

```bash
HYDRA_SCENE_PATH=/path/to/scene.usda bash install.sh hydra
```

## 视觉回归（AI 自验收）

旧的 `baseline` 和 `selfcheck` helper 已从 `install.sh` 移除。现在可以使用
`bash install.sh hydra` 生成 color/depth/id AOV，再用外部图像对比工具做回归检查。

```bash
bash install.sh baseline
bash install.sh selfcheck
```

这两个命令目前只会输出移除提示并返回非零状态。

## 参考

- [nvpro-samples/nvpro_core](https://github.com/nvpro-samples/nvpro_core)
- [pablode/gatling](https://github.com/pablode/gatling.git)

## License

本项目遵循原仓库的 License 规则。
