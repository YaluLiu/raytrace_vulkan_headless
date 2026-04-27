//
// Copyright (C) 2023 Pablo Delgado Kramer
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

#include "shaders/host_device.h"

#include <ModelLoader.h>
#include <glm/glm.hpp>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec3i.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/types.h>
#include <pxr/imaging/hd/tokens.h>

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

  void set_default();
  MaterialObj toMaterialObj() const;
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

  Light toLight() const;
};

struct TextureRegistry
{
  int Register(const std::string& texturePath);
  const std::vector<std::string>& GetPaths() const;

private:
  std::vector<std::string>             _texturePaths;
  std::unordered_map<std::string, int> _textureIdByPath;
};

void ConvertVmeshToLoader(const HydraMesh& mesh, ModelLoader& loader);

PXR_NAMESPACE_CLOSE_SCOPE
