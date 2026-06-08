#pragma once

#include <cstdint>

#include <engine/renderer_types.hpp>

#include <glm/glm.hpp>

#include "nvvk/resourceallocator_vk.hpp"

struct MeshBuffers
{
  uint32_t     nbIndices{0};
  uint32_t     nbVertices{0};
  uint64_t     vertexBufferSize{0};
  uint64_t     indexBufferSize{0};
  nvvk::Buffer vertexBuffer;
  nvvk::Buffer indexBuffer;
  nvvk::Buffer matColorBuffer;
  nvvk::Buffer matIndexBuffer;
};

struct SceneInstance
{
  glm::mat4 transform{1.0f};
  uint32_t  meshIndex{0};
  uint32_t  traceMask{kTraceMaskDefaultGeometry};
  bool      visible{true};
};
