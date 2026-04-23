//
// Copyright (C) 2023 Pablo Delgado Krämer
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

#include "camera.h"
#include "shaders/host_device.h"

#include <ModelLoader.h>
#include <glm/glm.hpp>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/types.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/tokens.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

struct HydraMesh
{
  VtVec3iArray faces;
  VtVec3fArray points;
  VtVec3fArray normals;
  VtVec2fArray texCoords;
  VtIntArray   materialIds;
  bool         visible   = true;
  bool         valid     = true;
  TfToken      renderTag = HdRenderTagTokens->geometry;

  bool blas_changed = false;
  bool tlas_changed = false;

  std::vector<int>       scene_mat_ids;
  std::vector<glm::mat4> instanceTransforms = {glm::mat4{1.0f}};
  bool                   hasInstances       = true;
  glm::mat4              transform          = glm::mat4{1.0f};
  std::vector<int>       tlasIds;
};

struct HydraMaterial
{
  glm::vec3 ambient       = glm::vec3(0.1f, 0.1f, 0.1f);
  glm::vec3 diffuse       = glm::vec3(0.18f, 0.18f, 0.18f);
  glm::vec3 specular      = glm::vec3(1.0f, 1.0f, 1.0f);
  glm::vec3 transmittance = glm::vec3(0.0f, 0.0f, 0.0f);
  glm::vec3 emission      = glm::vec3(0.0f, 0.0f, 0.0f);
  float     shininess     = 0.0f;
  float     ior           = 1.0f;
  float     dissolve      = 1.0f;
  int       illum         = 0;
  int       textureID     = -1;
  std::string texturePath;
  bool        material_changed = false;

  void set_default()
  {
    diffuse          = glm::vec3(0.18f, 0.18f, 0.18f);
    material_changed = true;
  }

  MaterialObj toMaterialObj() const
  {
    MaterialObj mat;
    mat.ambient       = ambient;
    mat.diffuse       = diffuse;
    mat.specular      = specular;
    mat.transmittance = transmittance;
    mat.emission      = emission;
    mat.shininess     = shininess;
    mat.ior           = ior;
    mat.dissolve      = dissolve;
    mat.illum         = illum;
    mat.textureID     = textureID;
    return mat;
  }
};

struct HydraLight
{
  int   type;
  int   valid = 0;
  vec3  baseEmission;
  float diffuse;
  float specular;

  vec3  direction;
  float angle;

  vec3  position;
  float radius;

  vec4        rotateQuat;
  int         textureID = -1;
  std::string texturePath;

  Light toLight() const
  {
    Light light;
    light.type         = type;
    light.textureID    = textureID;
    light.baseEmission = baseEmission;
    light.diffuse      = diffuse;
    light.specular     = specular;
    light.direction    = direction;
    light.angle        = angle;
    light.position     = position;
    light.radius       = radius;
    light.rotateQuat   = rotateQuat;
    return light;
  }
};

struct TextureRegistry
{
  int Register(const std::string& texturePath)
  {
    const auto it = _textureIdByPath.find(texturePath);
    if(it != _textureIdByPath.end())
    {
      return it->second;
    }

    const int textureId = static_cast<int>(_texturePaths.size());
    _texturePaths.emplace_back(texturePath);
    _textureIdByPath.emplace(texturePath, textureId);
    return textureId;
  }

  const std::vector<std::string>& GetPaths() const
  {
    return _texturePaths;
  }

private:
  std::vector<std::string>          _texturePaths;
  std::unordered_map<std::string, int> _textureIdByPath;
};

class HdRobotRenderParam final : public HdRenderParam
{
public:
  int RegisterTexturePath(const std::string& texturePath);
  const std::vector<std::string>& GetTexturePaths() const;

  void UpdateLidarCamera(const SdfPath& cameraId, const HdRobotLidarData& lidarData);
  void ClearLidarCamera(const SdfPath& cameraId);
  bool GetLidarCamera(HdRobotLidarData* lidarData) const;

public:
  // Shared render state synchronized by Hydra prims and consumed by render pass.
  std::mutex                mutex;
  std::vector<HydraMesh>    v_mesh;
  std::vector<HydraMaterial> v_mat;
  std::vector<HydraLight>   v_light;
  std::vector<Sphere>       v_sphere;
  TextureRegistry           textureRegistry;

private:
  mutable std::mutex _lidarMutex;
  SdfPath            _lidarCameraId;
  HdRobotLidarData   _lidarCameraData;
  bool               _hasLidarCamera{ false };
  bool               _warnedMultipleLidarCameras{ false };
};

void ConvertVmeshToLoader(const HydraMesh& mesh, ModelLoader& loader);

PXR_NAMESPACE_CLOSE_SCOPE
