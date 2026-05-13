#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "nvvk/memallocator_dma_vk.hpp"

struct RasterObjModel
{
  uint32_t     nbIndices{0};
  uint32_t     nbVertices{0};
  nvvk::Buffer vertexBuffer;
  nvvk::Buffer indexBuffer;
  nvvk::Buffer matColorBuffer;
  nvvk::Buffer matIndexBuffer;
};

struct RasterObjInstance
{
  glm::mat4 transform{1.0f};
  uint32_t  objIndex{0};
  bool      visible{true};
};
