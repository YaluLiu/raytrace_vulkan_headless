#pragma once

#include <pxr/pxr.h>

#include <cstdint>

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {

struct MeshHandle
{
  uint32_t index = UINT32_MAX;
  uint32_t generation = 0;
};

struct MaterialHandle
{
  uint32_t index = UINT32_MAX;
  uint32_t generation = 0;
};

struct LightHandle
{
  uint32_t index = UINT32_MAX;
  uint32_t generation = 0;
};

struct CameraHandle
{
  uint32_t index = UINT32_MAX;
  uint32_t generation = 0;
};

struct LidarSensorHandle
{
  uint32_t index = UINT32_MAX;
  uint32_t generation = 0;
};

struct HeightScanSensorHandle
{
  uint32_t index = UINT32_MAX;
  uint32_t generation = 0;
};

inline bool IsValid(const MeshHandle& handle) { return handle.index != UINT32_MAX; }
inline bool IsValid(const MaterialHandle& handle) { return handle.index != UINT32_MAX; }
inline bool IsValid(const LightHandle& handle) { return handle.index != UINT32_MAX; }
inline bool IsValid(const CameraHandle& handle) { return handle.index != UINT32_MAX; }
inline bool IsValid(const LidarSensorHandle& handle) { return handle.index != UINT32_MAX; }
inline bool IsValid(const HeightScanSensorHandle& handle) { return handle.index != UINT32_MAX; }

} // namespace hdrobot

PXR_NAMESPACE_CLOSE_SCOPE
