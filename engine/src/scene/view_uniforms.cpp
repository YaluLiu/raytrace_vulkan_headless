#include "scene/view_uniforms.hpp"

#include "scene/camera_projection.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
struct ResolvedCamera
{
  glm::vec3 position{0.0f};
  glm::vec3 target{0.0f, 0.0f, -1.0f};
  glm::vec3 up{0.0f, 1.0f, 0.0f};
  float verticalFovDegrees{45.0f};
  float clipStart{0.1f};
  float clipEnd{1000.0f};
};

std::pair<float, float> sanitizeClipRange(float clipStart, float clipEnd)
{
  const float safeStart = std::max(clipStart, 0.001f);
  const float safeEnd = std::max(clipEnd, safeStart + 0.001f);
  return {safeStart, safeEnd};
}

VkDeviceSize alignTo(VkDeviceSize value, VkDeviceSize alignment)
{
  if(alignment == 0)
  {
    return value;
  }
  return ((value + alignment - 1) / alignment) * alignment;
}

ResolvedCamera resolveCamera(const CameraSpec& camera)
{
  ResolvedCamera resolved;
  resolved.position = camera.position;

  glm::vec3 forward = camera.forward;
  if(glm::length(forward) < 1e-6f)
  {
    forward = glm::vec3(0.0f, 0.0f, -1.0f);
  }

  resolved.up = camera.up;
  if(glm::length(glm::cross(forward, resolved.up)) < 1e-6f)
  {
    resolved.up = glm::vec3(0.0f, 1.0f, 0.0f);
  }

  resolved.target = resolved.position + forward;
  resolved.verticalFovDegrees = std::clamp(camera.verticalFovDegrees, 1.0f, 179.0f);
  const auto [clipStart, clipEnd] = sanitizeClipRange(camera.clipStart, camera.clipEnd);
  resolved.clipStart = clipStart;
  resolved.clipEnd = clipEnd;
  return resolved;
}

FrameUniforms makeFrameUniforms(const glm::mat4& view,
                                const CameraSpec& camera,
                                VkExtent2D renderSize,
                                size_t lightCount)
{
  FrameUniforms frameUBO = {};
  glm::mat4 proj = BuildCameraProjection(camera, renderSize);

  frameUBO.camera.viewProj = proj * view;
  frameUBO.camera.view = view;
  frameUBO.camera.viewInverse = glm::inverse(view);
  frameUBO.camera.projInverse = glm::inverse(proj);
  frameUBO.lightCount = static_cast<uint32_t>(std::min<size_t>(lightCount, MAX_SCENE_LIGHTS));
  return frameUBO;
}

void recordFrameUniformUpdate(const VkCommandBuffer& cmdBuf, VkBuffer deviceUBO, VkDeviceSize offset,
                              const FrameUniforms& frameUBO)
{
  auto uboUsageStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

  VkBufferMemoryBarrier beforeBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  beforeBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  beforeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  beforeBarrier.buffer = deviceUBO;
  beforeBarrier.offset = offset;
  beforeBarrier.size = sizeof(frameUBO);
  vkCmdPipelineBarrier(cmdBuf, uboUsageStages, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
                       nullptr, 1, &beforeBarrier, 0, nullptr);

  vkCmdUpdateBuffer(cmdBuf, deviceUBO, offset, sizeof(FrameUniforms), &frameUBO);

  VkBufferMemoryBarrier afterBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  afterBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  afterBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  afterBarrier.buffer = deviceUBO;
  afterBarrier.offset = offset;
  afterBarrier.size = sizeof(frameUBO);
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, uboUsageStages, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
                       nullptr, 1, &afterBarrier, 0, nullptr);
}

void recordTileFrameUniformUpdate(const VkCommandBuffer& cmdBuf, VkBuffer deviceUBO, const TileFrameUniforms& tileUBO)
{
  auto uboUsageStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

  VkBufferMemoryBarrier beforeBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  beforeBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  beforeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  beforeBarrier.buffer = deviceUBO;
  beforeBarrier.offset = 0;
  beforeBarrier.size = sizeof(tileUBO);
  vkCmdPipelineBarrier(cmdBuf, uboUsageStages, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
                       nullptr, 1, &beforeBarrier, 0, nullptr);

  vkCmdUpdateBuffer(cmdBuf, deviceUBO, 0, sizeof(TileFrameUniforms), &tileUBO);

  VkBufferMemoryBarrier afterBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  afterBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  afterBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  afterBarrier.buffer = deviceUBO;
  afterBarrier.offset = 0;
  afterBarrier.size = sizeof(tileUBO);
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, uboUsageStages, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
                       nullptr, 1, &afterBarrier, 0, nullptr);
}

CameraSpec makeProjectionCamera(const MainViewState& state)
{
  CameraSpec camera;
  camera.verticalFovDegrees = state.verticalFovDegrees;
  camera.clipStart = state.clipStart;
  camera.clipEnd = state.clipEnd;
  return camera;
}
} // namespace

void ViewUniforms::setup(VkDevice device,
                               VkPhysicalDevice physicalDevice,
                               nvvk::ResourceAllocatorDma& allocator,
                               nvvk::DebugUtil& debug)
{
  m_device = device;
  m_alloc = &allocator;
  m_debug = &debug;

  VkPhysicalDeviceProperties deviceProperties{};
  vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
  m_frameUniformStride = alignTo(sizeof(FrameUniforms), deviceProperties.limits.minUniformBufferOffsetAlignment);
}

void ViewUniforms::destroy()
{
  if(m_alloc != nullptr)
  {
    m_alloc->destroy(m_bFrameUniforms);
    m_alloc->destroy(m_bTileFrameUniforms);
  }
  m_bFrameUniforms = {};
  m_bTileFrameUniforms = {};
  m_frameUniformSlotCount = 0;
}

bool ViewUniforms::ensureFrameUniformCapacity(uint32_t slotCount)
{
  slotCount = std::max<uint32_t>(slotCount, 1);
  if(m_device == VK_NULL_HANDLE || m_alloc == nullptr ||
     (m_bFrameUniforms.buffer != VK_NULL_HANDLE && slotCount <= m_frameUniformSlotCount))
  {
    return false;
  }

  if(m_frameUniformStride == 0)
  {
    m_frameUniformStride = sizeof(FrameUniforms);
  }

  m_alloc->destroy(m_bFrameUniforms);
  m_bFrameUniforms = m_alloc->createBuffer(m_frameUniformStride * slotCount,
                                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  m_frameUniformSlotCount = slotCount;
  if(m_debug != nullptr)
  {
    m_debug->setObjectName(m_bFrameUniforms.buffer, "FrameUniforms");
  }
  return true;
}

bool ViewUniforms::ensureTileFrameUniformBuffer()
{
  if(m_device == VK_NULL_HANDLE || m_alloc == nullptr || m_bTileFrameUniforms.buffer != VK_NULL_HANDLE)
  {
    return false;
  }

  static_assert(sizeof(TileFrameUniforms) <= 65536, "TileFrameUniforms must fit vkCmdUpdateBuffer limits");
  m_bTileFrameUniforms = m_alloc->createBuffer(sizeof(TileFrameUniforms),
                                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if(m_debug != nullptr)
  {
    m_debug->setObjectName(m_bTileFrameUniforms.buffer, "TileFrameUniforms");
  }
  return true;
}

void ViewUniforms::updateFrameUniformBuffer(const VkCommandBuffer& cmdBuf, VkExtent2D renderSize, size_t lightCount)
{
  if(renderSize.width == 0 || renderSize.height == 0 || m_bFrameUniforms.buffer == VK_NULL_HANDLE)
  {
    return;
  }

  const FrameUniforms frameUBO =
      makeFrameUniforms(m_mainViewState.view, makeProjectionCamera(m_mainViewState), renderSize, lightCount);
  recordFrameUniformUpdate(cmdBuf, m_bFrameUniforms.buffer, 0, frameUBO);
}

void ViewUniforms::updateTileFrameUniformBufferForBatch(const VkCommandBuffer& cmdBuf,
                                                              uint32_t firstCameraIndex,
                                                              uint32_t cameraCount,
                                                              uint32_t viewCount,
                                                              VkExtent2D tileExtent,
                                                              size_t lightCount)
{
  if(m_bTileFrameUniforms.buffer == VK_NULL_HANDLE || tileExtent.width == 0 || tileExtent.height == 0 ||
     firstCameraIndex >= m_cameras.size() || cameraCount == 0 || viewCount == 0)
  {
    return;
  }

  const uint32_t maxViews = std::min<uint32_t>(MAX_TILE_MULTIVIEW_VIEWS, viewCount);
  const uint32_t availableCameras =
      static_cast<uint32_t>(std::min<size_t>(m_cameras.size() - firstCameraIndex, cameraCount));
  const uint32_t validCameraCount = std::min(maxViews, availableCameras);
  if(validCameraCount == 0)
  {
    return;
  }

  TileFrameUniforms tileUBO{};
  tileUBO.lightCount = static_cast<uint32_t>(std::min<size_t>(lightCount, MAX_SCENE_LIGHTS));
  tileUBO.viewCount = validCameraCount;
  tileUBO.batchBaseCameraIndex = firstCameraIndex;
  const uint32_t lastCameraSlot = validCameraCount - 1;

  for(uint32_t viewIndex = 0; viewIndex < MAX_TILE_MULTIVIEW_VIEWS; ++viewIndex)
  {
    const uint32_t cameraSlot = std::min(viewIndex, lastCameraSlot);
    const auto& camera = m_cameras[firstCameraIndex + cameraSlot];
    const auto resolved = resolveCamera(camera);
    const glm::mat4 view = glm::lookAtRH(resolved.position, resolved.target, resolved.up);
    CameraSpec projectionCamera = camera;
    projectionCamera.verticalFovDegrees = resolved.verticalFovDegrees;
    projectionCamera.clipStart = resolved.clipStart;
    projectionCamera.clipEnd = resolved.clipEnd;
    const auto frameUBO = makeFrameUniforms(view, projectionCamera, tileExtent, lightCount);
    tileUBO.cameras[viewIndex] = frameUBO.camera;
  }

  recordTileFrameUniformUpdate(cmdBuf, m_bTileFrameUniforms.buffer, tileUBO);
}

void ViewUniforms::setCameras(std::vector<CameraSpec> cameras)
{
  m_cameras = std::move(cameras);
}

void ViewUniforms::setMainViewState(MainViewState state)
{
  state.verticalFovDegrees = std::clamp(state.verticalFovDegrees, 1.0f, 179.0f);
  const auto [clipStart, clipEnd] = sanitizeClipRange(state.clipStart, state.clipEnd);
  state.clipStart = clipStart;
  state.clipEnd = clipEnd;
  m_mainViewState = state;
}

void ViewUniforms::setMainCamera(const CameraSpec& camera)
{
  const ResolvedCamera resolved = resolveCamera(camera);
  setMainViewState(MainViewState{
      glm::lookAtRH(resolved.position, resolved.target, resolved.up),
      resolved.verticalFovDegrees,
      resolved.clipStart,
      resolved.clipEnd,
  });
}

void ViewUniforms::setMainCameraClipRange(float clipStart, float clipEnd)
{
  const auto [safeStart, safeEnd] = sanitizeClipRange(clipStart, clipEnd);
  if(std::fabs(m_mainViewState.clipStart - safeStart) > 1e-6f ||
     std::fabs(m_mainViewState.clipEnd - safeEnd) > 1e-6f)
  {
    m_mainViewState.clipStart = safeStart;
    m_mainViewState.clipEnd = safeEnd;
  }
}

VkDescriptorBufferInfo ViewUniforms::getFrameUniformDescriptorInfo() const
{
  return {m_bFrameUniforms.buffer, 0, sizeof(FrameUniforms)};
}

VkDescriptorBufferInfo ViewUniforms::getTileFrameUniformDescriptorInfo() const
{
  return {m_bTileFrameUniforms.buffer, 0, sizeof(TileFrameUniforms)};
}
