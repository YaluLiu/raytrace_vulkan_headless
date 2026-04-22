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

2. 修改 nvpro_core 的 cmakelists，屏蔽掉下面的 code,启用 C++11

   ```
   add_definitions(-D_GLIBCXX_USE_CXX11_ABI=1)
   ```

## 运行方式

编译完成后，直接运行：

```bash
bash install.sh demo
```

可以运行 headless 的测试 demo

```bash
bash install.sh hydra
```

可以运行基于 headless 编译的 hydra 插件

运行 `hydra` 前，需要先从下面地址下载并准备 NVIDIA DLSS SDK：

- [NVIDIA/DLSS](https://github.com/NVIDIA/DLSS)

然后通过 `DLSS_SDK_ROOT` 指定给安装脚本：

```bash
DLSS_SDK_ROOT=/path/to/DLSS bash install.sh hydra
```

`DLSS_SDK_ROOT` 目录中需要包含：

- `include/nvsdk_ngx_vk.h`
- `lib/Linux_x86_64/libnvsdk_ngx.a`

可以用 `HYDRA_SCENE_PATH` 覆盖 `install.sh hydra` 默认打开的 Hydra 场景：

```bash
HYDRA_SCENE_PATH=hydra_lidar_verify/scene_a.usda bash install.sh hydra
```

仓库内提供了两个用于 lidar 相机分离验证的场景，位于 `hydra_lidar_verify/`：

- `scene_a.usda`：主渲染相机固定，lidar 相机位于 +X 侧
- `scene_b.usda`：主渲染相机不变，lidar 相机镜像到 -X 侧

## 视觉回归（AI 自验收）

把 demo 渲染结果作为功能回归门禁：

```bash
# 1) 采集基线（保存 result/gl_0.png 和 result/lidar_0.png）
bash install.sh baseline

# 2) 每次改动后重跑 demo，并与基线对比
bash install.sh selfcheck
```

产物目录：

- 基线图：`output/visual_baseline/`
- 当前运行图：`output/visual_current/`
- 对比报告（`json` + `diff_*.png`）：`output/visual_report/latest/`

常用参数：

- `VIS_DEMO_CMD`：自定义运行命令
- `VIS_MAX_MAE`、`VIS_MAX_RMSE`、`VIS_MAX_CHANGED_RATIO`：对比阈值
- `VIS_SKIP_RUN=1`：不重跑 demo，直接对当前 `result/*.png` 做对比

## 参考

- [nvpro-samples/vk_raytracing_tutorial_KHR](https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR)
- [nvpro-samples/nvpro_core](https://github.com/nvpro-samples/nvpro_core)
- [pablode/gatling](https://github.com/pablode/gatling.git)

## License

本项目遵循原仓库的 License 规则。
