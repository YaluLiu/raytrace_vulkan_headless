#include "training_scene.h"

#include <iostream>
#include <utility>

namespace headless_training
{

TrainingSceneRuntime::TrainingSceneRuntime(TrainingSceneDescription scene)
    : m_scene(std::move(scene))
{
  rebuildIndex();
}

void TrainingSceneRuntime::rebuildIndex()
{
  m_meshByName.clear();
  for(size_t i = 0; i < m_scene.meshes.size(); ++i)
  {
    m_meshByName.emplace(m_scene.meshes[i].name, i);
  }
}

void TrainingSceneRuntime::uploadToEngine(Engine& engine)
{
  engine.loadTextureAssets(m_scene.textureAssets);

  for(TrainingMeshInstance& mesh : m_scene.meshes)
  {
    const uint32_t meshIndex = static_cast<uint32_t>(engine.getMeshSourceCount());
    const uint32_t instanceIndex = static_cast<uint32_t>(engine.getInstanceCount());
    engine.uploadMesh(mesh.geometry, mesh.materials, mesh.worldTransform);
    mesh.engineMeshIndex = meshIndex;
    mesh.engineInstanceIndex = instanceIndex;
    engine.updateInstance(mesh.engineInstanceIndex, mesh.worldTransform, mesh.visible, mesh.traceMask);
  }

  engine.clearLights();
  for(const Light& light : m_scene.lights)
  {
    engine.addLight(light);
  }
}

void TrainingSceneRuntime::configureEngineOutputs(Engine& engine, bool exportLidar, bool exportHeightScan) const
{
  std::vector<CameraSpec> cameras = m_scene.cameras;
  if(cameras.empty())
  {
    cameras.push_back(MakeDefaultCamera());
  }
  configureEngineOutputs(engine, exportLidar, exportHeightScan, cameras.front());
}

void TrainingSceneRuntime::configureEngineOutputs(Engine& engine, bool exportLidar, bool exportHeightScan,
                                                  const CameraSpec& mainCamera, bool previewLidarPoints,
                                                  bool previewHeightScanPoints) const
{
  std::vector<CameraSpec> cameras = m_scene.cameras;
  if(cameras.empty())
  {
    cameras.push_back(mainCamera);
  }
  engine.setCameras(cameras);
  engine.setMainCamera(mainCamera);
  engine.configureOutputs(
      BuildRendererOutputConfig(m_scene, exportLidar, exportHeightScan, previewLidarPoints, previewHeightScanPoints));
}

void TrainingSceneRuntime::applyPoseUpdates(Engine& engine, const std::vector<InstancePoseUpdate>& updates) const
{
  for(const InstancePoseUpdate& update : updates)
  {
    const TrainingMeshInstance* mesh = findMesh(update.name);
    if(mesh == nullptr)
    {
      std::cerr << "[robot_training_headless] Warning: physics replay references unknown instance " << update.name
                << '\n';
      continue;
    }

    engine.updateInstance(mesh->engineInstanceIndex, update.worldTransform, mesh->visible && update.visible,
                          mesh->traceMask);
  }
}

const TrainingMeshInstance* TrainingSceneRuntime::findMesh(std::string_view name) const
{
  const auto it = m_meshByName.find(std::string(name));
  if(it == m_meshByName.end())
  {
    return nullptr;
  }
  return &m_scene.meshes[it->second];
}

CameraSpec MakeDefaultCamera()
{
  CameraSpec camera;
  camera.name = "default";
  camera.position = glm::vec3(5.0f, 4.0f, -4.0f);
  camera.forward = glm::normalize(glm::vec3(-5.0f, -3.0f, 4.0f));
  camera.up = glm::vec3(0.0f, 1.0f, 0.0f);
  camera.verticalFovDegrees = 45.0f;
  camera.clipStart = 0.1f;
  camera.clipEnd = 1000.0f;
  return camera;
}

RendererOutputConfig BuildRendererOutputConfig(const TrainingSceneDescription& scene, bool exportLidar,
                                               bool exportHeightScan, bool previewLidarPoints,
                                               bool previewHeightScanPoints)
{
  RendererOutputConfig config;
  config.tile.atlas.enabled = false;
  config.tile.requestedChannels = TileAovChannelMask::None();
  if(exportLidar || previewLidarPoints)
  {
    config.lidar.sensors = scene.lidarSensors;
  }
  if(previewLidarPoints && !scene.lidarSensors.empty())
  {
    config.lidar.visualization.enabled = true;
    config.lidar.visualization.visualizeAllSensors = true;
  }
  if(exportHeightScan || previewHeightScanPoints)
  {
    config.heightScan.sensors = scene.heightScanSensors;
  }
  if(previewHeightScanPoints && !scene.heightScanSensors.empty())
  {
    config.heightScan.visualization.enabled = true;
    config.heightScan.visualization.visualizeAllSensors = true;
  }
  return config;
}

glm::mat4 MakeEngineTransformFromUsdRows(const std::array<double, 16>& rowMajorMatrix)
{
  glm::mat4 result(1.0f);
  for(int column = 0; column < 4; ++column)
  {
    for(int row = 0; row < 4; ++row)
    {
      result[column][row] = static_cast<float>(rowMajorMatrix[static_cast<size_t>(column * 4 + row)]);
    }
  }
  return result;
}

} // namespace headless_training
