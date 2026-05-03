# OpenChessSet MaterialX Support

This document records the currently supported MaterialX subset for
`/home/yalu/docker/assets/materialx/OpenChessSet/chess_set.usda`.

## Supported Path

- Primary path: OpenUSD Hydra render delegate through `hdRobot`.
- Scene entrypoint override: `HYDRA_SCENE_PATH=/home/yalu/docker/assets/materialx/OpenChessSet/chess_set.usda bash install.sh hydra`.
- The standalone `demo/usd_loader.cpp` path is intentionally not extended for
  this asset.

## Supported MaterialX Inputs

The Hydra material sync path handles the OpenChessSet `standard_surface` subset:

- `base_color`: factor or upstream image, registered as sRGB base color.
- `metalness`: factor or upstream image, registered as linear metallic data.
- `specular_roughness`: factor or upstream image, registered as linear roughness data.
- `normal`: upstream image through normalmap nodes, registered as linear normal data.
- `emission` / `emissiveColor`: factor or upstream image.
- `opacity`: factor or upstream image.
- `transmission`: factor, used as an opaque tinted glossy approximation.
- `transmission_color`: factor for the transmission tint.
- `subsurface`: factor or upstream scattering image, used as a diffuse wrap approximation.
- `subsurface_color`: factor when authored directly; connected OpenChessSet
  colors fall back to the resolved base-color contribution.
- `subsurface_scale`: factor used to modulate the wrap amount.

## Rendering Approximation

Transmission and subsurface are conservative viewport-style approximations:

- Transmission keeps the surface opaque, tints the base color toward
  `transmission_color`, reduces metallic response, tightens roughness, and uses
  a dielectric specular F0.
- Subsurface samples the scattering texture when present and adds a bounded
  wrap-diffuse contribution in direct lighting.
- The implementation does not do transparent sorting, alpha discard, recursive
  refraction, or physically accurate subsurface scattering.

## Regression Coverage

Contract tests cover these routes:

- `contracts.hydra_entrypoint`
- `contracts.pbr_material`
- `contracts.materialx_standard_surface`
- `contracts.texture_usage`
- `contracts.pbr_shader`
- `contracts.normal_map`
- `contracts.instancer_material_subset`
- `contracts.materialx_advanced_inputs`

Recommended verification before visual review:

```bash
ctest --test-dir build --output-on-failure
cmake --build build --target hdRobot -j2
```
