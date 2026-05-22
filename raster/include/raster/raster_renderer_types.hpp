#pragma once

#include <raster/shaders/host_device.h>

#include <cstdint>
#include <string>

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

constexpr uint32_t kRasterTraceMaskInvisible = 0x00u;
constexpr uint32_t kRasterTraceMaskDefaultGeometry = 0x01u;
constexpr uint32_t kRasterTraceMaskGround = 0x02u;

struct RasterCameraSpec
{
  std::string name;
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 forward{0.0f, 0.0f, -1.0f};
  glm::vec3 up{0.0f, 1.0f, 0.0f};
  float vfov_deg{45.0f};
  float clipStart{0.1f};
  float clipEnd{1000.0f};
};

struct RasterMaterialUpdate
{
  int modelIndex;
  int materialIndex;
  WaveFrontMaterial newMaterial;
};

struct RasterInstanceInfo
{
  glm::mat4 transform{1.0f};
  uint32_t objIndex{0};
  int instanceId{0};
  bool visible{true};
};

struct RasterTlasDescriptorInfo
{
  VkAccelerationStructureKHR accelerationStructure{VK_NULL_HANDLE};
};
