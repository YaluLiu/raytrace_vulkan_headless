# raytrace_vulkan_headless

Headless Vulkan graphics renderer

## Overview

This project is a headless Vulkan renderer with an OpenUSD Hydra entry point.
The current renderer is a compact graphics baseline that exports color,
depth, primitive ID, and instance ID AOVs.

Major modifications include:

- **Removed window, surface, and swapchain code** to create a headless version
  suitable for environments without GUI, such as servers or automated
  rendering.
- Added Hydra render delegate integration for the headless engine renderer.

## Dependencies

- You need to clone [nvpro-samples/nvpro_core](https://github.com/nvpro-samples/nvpro_core) in addition to this repository.
- The `nvpro_core` directory can be placed either:

  1. Inside the root of this project, **or**
  2. At the same directory level as this project.

  Please refer to the `find_path` function in the project's `CMakeLists.txt` for more details on dependency search paths.

## Build Instructions

The build uses CMake and the nvpro_core helper libraries:

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

After building, you can use the following command to run the plugin workflow:

```bash
bash install.sh hydra
```

Runs the Hydra plugin based on headless mode.

You can override the default Hydra scene used by `install.sh hydra` with any
USD scene path supported by your local USD installation:

```bash
HYDRA_SCENE_PATH=/path/to/scene.usda bash install.sh hydra
```

## Visual Regression (AI Self-check)

The old `baseline` and `selfcheck` helpers have been removed from
`install.sh`. Use `bash install.sh hydra`, then compare the generated
color/depth/id AOV artifacts with your preferred image diff tooling.

```bash
bash install.sh baseline
bash install.sh selfcheck
```

Both commands currently print a removal message and return a non-zero status.

## References

- [nvpro-samples/nvpro_core](https://github.com/nvpro-samples/nvpro_core)
- [pablode/gatling](https://github.com/pablode/gatling.git)

## License

This project follows the license terms of the original repositories.
