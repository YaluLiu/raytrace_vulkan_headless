#pragma once

#include <engine/mesh_types.hpp>
#include <engine/shaders/host_device.h>

#include <cstdint>
#include <string>

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

// Light is a GPU shader ABI type from host_device.h and must keep its stable layout.
constexpr uint32_t kTraceMaskInvisible = 0x00u;
constexpr uint32_t kTraceMaskDefaultGeometry = 0x01u;
constexpr uint32_t kTraceMaskGround = 0x02u;

enum class CameraConformPolicy
{
  MatchVertically,
  MatchHorizontally,
  Fit,
  Crop,
  DontConform,
};

struct CameraSpec
{
  std::string name;
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 forward{0.0f, 0.0f, -1.0f};
  glm::vec3 up{0.0f, 1.0f, 0.0f};
  float vfov_deg{45.0f};
  float hfov_deg{0.0f};
  float clipStart{0.1f};
  float clipEnd{1000.0f};
  CameraConformPolicy conformPolicy{CameraConformPolicy::MatchVertically};
};

struct MaterialUpdate
{
  int modelIndex;
  int materialIndex;
  Material newMaterial;
};

struct InstanceInfo
{
  glm::mat4 transform{1.0f};
  uint32_t objIndex{0};
  int instanceId{0};
  bool visible{true};
};

struct TlasDescriptorInfo
{
  VkAccelerationStructureKHR accelerationStructure{VK_NULL_HANDLE};
};
