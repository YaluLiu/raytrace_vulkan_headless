#pragma once

#include <engine/renderer_types.hpp>

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

float ComputeVerticalFovForRenderTarget(const CameraSpec& camera, VkExtent2D renderSize);
glm::mat4 BuildCameraProjection(const CameraSpec& camera, VkExtent2D renderSize);
