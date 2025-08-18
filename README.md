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

## Run

After building, you can use the following commands to run demos or plugins:

```bash
bash install.sh demo
```

Runs the headless test demo.

```bash
bash install.sh gatling
```

Runs the Hydra plugin based on headless mode.

## References

- [nvpro-samples/vk_raytracing_tutorial_KHR](https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR)
- [nvpro-samples/nvpro_core](https://github.com/nvpro-samples/nvpro_core)
- [pablode/gatling](https://github.com/pablode/gatling.git)

## License

This project follows the license terms of the original repositories.
