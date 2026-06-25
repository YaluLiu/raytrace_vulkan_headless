#include "usd_scene_loader.h"

#include "usd_light_reader.h"
#include "usd_mesh_reader.h"
#include "usd_scene_reader_utils.h"
#include "usd_sensor_reader.h"
#include "usd_texture_registry.h"

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdLux/cylinderLight.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/sphereLight.h>

#include <stdexcept>

namespace headless_training
{

TrainingSceneDescription LoadUsdTrainingScene(const std::filesystem::path& usdPath, const UsdSceneLoadOptions& options)
{
  PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(usdPath.string());
  if(!stage)
  {
    throw std::runtime_error("failed to open USD stage: " + usdPath.string());
  }

  const PXR_NS::UsdTimeCode time(stage->GetStartTimeCode());
  TrainingSceneDescription result;
  TextureRegistry textureRegistry(stage);
  const bool cameraPathRequested = !options.cameraPath.empty();
  const PXR_NS::SdfPath requestedCameraPath(options.cameraPath);
  bool requestedCameraFound = false;

  for(const PXR_NS::UsdPrim& prim : stage->Traverse())
  {
    if(!prim.IsActive())
    {
      continue;
    }

    PXR_NS::UsdGeomMesh mesh(prim);
    if(mesh)
    {
      result.meshes.push_back(ReadMesh(mesh, textureRegistry, time));
      continue;
    }

    PXR_NS::UsdGeomCamera camera(prim);
    if(camera)
    {
      result.cameras.push_back(ReadCamera(camera, time));
      if(cameraPathRequested && prim.GetPath() == requestedCameraPath)
      {
        requestedCameraFound = true;
      }
      continue;
    }

    if(IsLidarSensorPrim(prim))
    {
      PXR_NS::UsdGeomLidarSensor sensor(prim);
      if(sensor.GetEnabled(time))
      {
        result.lidarSensors.push_back(ReadLidarSensor(sensor, time));
      }
      continue;
    }

    if(IsHeightScanSensorPrim(prim))
    {
      PXR_NS::UsdGeomHeightScanSensor sensor(prim);
      if(sensor.GetEnabled(time))
      {
        result.heightScanSensors.push_back(ReadHeightScanSensor(sensor, time));
      }
      continue;
    }

    PXR_NS::UsdLuxDomeLight domeLight(prim);
    if(domeLight)
    {
      result.lights.push_back(ReadDomeLight(domeLight, textureRegistry, time));
      continue;
    }

    PXR_NS::UsdLuxSphereLight sphereLight(prim);
    if(sphereLight)
    {
      float radius = 0.5f;
      sphereLight.GetRadiusAttr().Get(&radius, time);
      result.lights.push_back(ReadSphereLikeLight(prim, radius, textureRegistry, time));
      continue;
    }

    PXR_NS::UsdLuxCylinderLight cylinderLight(prim);
    if(cylinderLight)
    {
      float radius = 0.5f;
      cylinderLight.GetRadiusAttr().Get(&radius, time);
      result.lights.push_back(ReadSphereLikeLight(prim, radius, textureRegistry, time));
      continue;
    }
  }

  if(cameraPathRequested && !requestedCameraFound)
  {
    throw std::runtime_error("requested camera path was not found or was not a UsdGeomCamera: " + options.cameraPath);
  }

  result.textureAssets = textureRegistry.TakeAssets();
  return result;
}

} // namespace headless_training
