#pragma once

#include <engine/engine.hpp>
#include <engine/height_scan_types.hpp>
#include <engine/lidar_types.hpp>
#include <engine/mesh_types.hpp>
#include <engine/output_config.hpp>
#include <engine/renderer_types.hpp>
#include <engine/texture_asset.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace headless_training
{

struct TrainingMeshInstance
{
  std::string name;
  MeshGeometry geometry;
  std::vector<Material> materials;
  glm::mat4 worldTransform{1.0f};
  bool visible{true};
  uint32_t traceMask{kTraceMaskDefaultGeometry};
  uint32_t engineMeshIndex{std::numeric_limits<uint32_t>::max()};
  uint32_t engineInstanceIndex{std::numeric_limits<uint32_t>::max()};
};

struct TrainingSceneDescription
{
  std::vector<TrainingMeshInstance> meshes;
  std::vector<TextureAsset> textureAssets;
  std::vector<Light> lights;
  std::vector<CameraSpec> cameras;
  std::vector<LidarSensorSpec> lidarSensors;
  std::vector<HeightScanSensorSpec> heightScanSensors;
};

struct InstancePoseUpdate
{
  std::string name;
  glm::mat4 worldTransform{1.0f};
  bool visible{true};
};

class TrainingSceneRuntime
{
public:
  explicit TrainingSceneRuntime(TrainingSceneDescription scene);

  void uploadToEngine(Engine& engine);
  void configureEngineOutputs(Engine& engine, bool exportLidar, bool exportHeightScan) const;
  void configureEngineOutputs(Engine& engine, bool exportLidar, bool exportHeightScan, const CameraSpec& mainCamera,
                              bool previewLidarPoints = false, bool previewHeightScanPoints = false) const;
  void applyPoseUpdates(Engine& engine, const std::vector<InstancePoseUpdate>& updates) const;

  const TrainingSceneDescription& scene() const { return m_scene; }
  const TrainingMeshInstance* findMesh(std::string_view name) const;

private:
  TrainingSceneDescription m_scene;
  std::unordered_map<std::string, size_t> m_meshByName;

  void rebuildIndex();
};

CameraSpec MakeDefaultCamera();
RendererOutputConfig BuildRendererOutputConfig(const TrainingSceneDescription& scene, bool exportLidar,
                                               bool exportHeightScan, bool previewLidarPoints = false,
                                               bool previewHeightScanPoints = false);
glm::mat4 MakeEngineTransformFromUsdRows(const std::array<double, 16>& rowMajorMatrix);

} // namespace headless_training
