#include "renderParam.h"

PXR_NAMESPACE_OPEN_SCOPE

int HdRobotRenderParam::RegisterTexturePath(const std::string& texturePath, TextureUsage usage)
{
  std::lock_guard guard(mutex);
  return textureRegistry.Register(texturePath, usage);
}

const std::vector<std::string>& HdRobotRenderParam::GetTexturePaths() const
{
  return textureRegistry.GetPaths();
}

const std::vector<TextureAsset>& HdRobotRenderParam::GetTextureAssets() const
{
  return textureRegistry.GetTextureAssets();
}

uint64_t HdRobotRenderParam::GetTextureRegistryVersion() const
{
  return textureRegistry.GetVersion();
}

HdRobotBackendScene& HdRobotRenderParam::GetBackendScene()
{
  return backendScene;
}

const HdRobotBackendScene& HdRobotRenderParam::GetBackendScene() const
{
  return backendScene;
}

void HdRobotRenderParam::ApplyPendingSceneUpdates()
{
  backendScene.ApplyPendingUpdates();
}

void HdRobotRenderParam::SetTileConfig(const HdRobotTileConfig& config)
{
  std::lock_guard guard(mutex);
  tileConfig = config;
}

HdRobotTileConfig HdRobotRenderParam::GetTileConfig() const
{
  std::lock_guard guard(mutex);
  return tileConfig;
}

void HdRobotRenderParam::SetLidarVisualizationConfig(const HdRobotLidarVisualizationConfig& config)
{
  std::lock_guard guard(mutex);
  lidarVisualizationConfig = config;
}

HdRobotLidarVisualizationConfig HdRobotRenderParam::GetLidarVisualizationConfig() const
{
  std::lock_guard guard(mutex);
  return lidarVisualizationConfig;
}

void HdRobotRenderParam::SetHeightScanVisualizationConfig(const HdRobotHeightScanVisualizationConfig& config)
{
  std::lock_guard guard(mutex);
  heightScanVisualizationConfig = config;
}

HdRobotHeightScanVisualizationConfig HdRobotRenderParam::GetHeightScanVisualizationConfig() const
{
  std::lock_guard guard(mutex);
  return heightScanVisualizationConfig;
}

PXR_NAMESPACE_CLOSE_SCOPE
