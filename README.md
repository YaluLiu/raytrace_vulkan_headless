# raytrace_vulkan_headless

Headless Vulkan ray tracing demo

## Overview

This project is adapted from the `ray_tracing_animation` demo in [nvpro-samples/vk_raytracing_tutorial_KHR](https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR).  
Major modifications include:

- **Removed window, surface, and swapchain code** to create a headless version suitable for environments without GUI (such as servers or automated rendering).
- The overall code structure and build process remain consistent with the original nvpro-samples repository.

## Dependencies

- You need to clone [nvpro-samples/nvpro_core](https://github.com/nvpro-samples/nvpro_core) in addition to this repository.
- The `nvpro_core` directory can be placed either:

  1. Inside the root of this project, **or**
  2. At the same directory level as this project.

  Please refer to the `find_path` function in the project's `CMakeLists.txt` for more details on dependency search paths.

## Build Instructions

The build process is identical to [nvpro-samples/vk_raytracing_tutorial_KHR](https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR):

1. Clone this repository and `nvpro_core`:

   ```bash
   git clone https://github.com/YaluLiu/raytrace_vulkan_headless.git
   git clone https://github.com/nvpro-samples/nvpro_core.git
   ```

   > You can place `nvpro_core` inside `raytrace_vulkan_headless` or at the same level.

2. delete/comment the code in cmakelists of nvpro_core

   ```
   add_definitions(-D_GLIBCXX_USE_CXX11_ABI=1)
   ```

## Run

After building, you can use the following commands to run demos or plugins:

```bash
bash install.sh demo
```

Runs the headless test demo.

```bash
bash install.sh hydra
```

Runs the Hydra plugin based on headless mode.

Before running `hydra`, download and prepare the NVIDIA DLSS SDK from:

- [NVIDIA/DLSS](https://github.com/NVIDIA/DLSS)

Then pass the SDK root to `install.sh` with `DLSS_SDK_ROOT`:

```bash
DLSS_SDK_ROOT=/path/to/DLSS bash install.sh hydra
```

`DLSS_SDK_ROOT` should contain:

- `include/nvsdk_ngx_vk.h`
- `lib/Linux_x86_64/libnvsdk_ngx.a`

You can override the default Hydra scene used by `install.sh hydra`:

```bash
HYDRA_SCENE_PATH=hydra_lidar_verify/scene_a.usda bash install.sh hydra
```

Tracked lidar-camera verification scenes live in `hydra_lidar_verify/`:

- `scene_a.usda`: main render camera fixed, lidar camera on the +X side
- `scene_b.usda`: same main camera, lidar camera mirrored to the -X side

## Visual Regression (AI Self-check)

Use rendered images as a functional regression gate:

```bash
# 1) Capture baseline (stores result/gl_0.png and result/lidar_0.png)
bash install.sh baseline

# 2) After code changes, rerun demo and compare against baseline
bash install.sh selfcheck
```

Artifacts:

- Baseline images: `output/visual_baseline/`
- Current captures: `output/visual_current/`
- Diff reports (`json` + `diff_*.png`): `output/visual_report/latest/`

Common overrides:

- `VIS_DEMO_CMD`: custom demo command
- `VIS_MAX_MAE`, `VIS_MAX_RMSE`, `VIS_MAX_CHANGED_RATIO`: pass/fail thresholds
- `VIS_SKIP_RUN=1`: compare existing `result/*.png` without rerunning demo

## References

- [nvpro-samples/vk_raytracing_tutorial_KHR](https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR)
- [nvpro-samples/nvpro_core](https://github.com/nvpro-samples/nvpro_core)
- [pablode/gatling](https://github.com/pablode/gatling.git)

## License

This project follows the license terms of the original repositories.
