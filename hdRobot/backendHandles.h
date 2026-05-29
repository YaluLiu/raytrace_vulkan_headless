#pragma once

#include <pxr/pxr.h>

#include <cstdint>

PXR_NAMESPACE_OPEN_SCOPE

struct HdRobotMeshHandle
{
  uint32_t index = UINT32_MAX;
  uint32_t generation = 0;
};

struct HdRobotMaterialHandle
{
  uint32_t index = UINT32_MAX;
  uint32_t generation = 0;
};

struct HdRobotLightHandle
{
  uint32_t index = UINT32_MAX;
  uint32_t generation = 0;
};

struct HdRobotCameraHandle
{
  uint32_t index = UINT32_MAX;
  uint32_t generation = 0;
};

struct HdRobotLidarSensorHandle
{
  uint32_t index = UINT32_MAX;
  uint32_t generation = 0;
};

struct HdRobotHeightScanSensorHandle
{
  uint32_t index = UINT32_MAX;
  uint32_t generation = 0;
};

inline bool IsValid(const HdRobotMeshHandle& handle) { return handle.index != UINT32_MAX; }
inline bool IsValid(const HdRobotMaterialHandle& handle) { return handle.index != UINT32_MAX; }
inline bool IsValid(const HdRobotLightHandle& handle) { return handle.index != UINT32_MAX; }
inline bool IsValid(const HdRobotCameraHandle& handle) { return handle.index != UINT32_MAX; }
inline bool IsValid(const HdRobotLidarSensorHandle& handle) { return handle.index != UINT32_MAX; }
inline bool IsValid(const HdRobotHeightScanSensorHandle& handle) { return handle.index != UINT32_MAX; }

PXR_NAMESPACE_CLOSE_SCOPE
