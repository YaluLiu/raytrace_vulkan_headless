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

#include "renderParam.h"

#include <pxr/base/tf/diagnostic.h>

#include <iomanip>
#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

int HdRobotRenderParam::RegisterTexturePath(const std::string& texturePath)
{
  std::lock_guard guard(mutex);
  return textureRegistry.Register(texturePath);
}

const std::vector<std::string>& HdRobotRenderParam::GetTexturePaths() const
{
  return textureRegistry.GetPaths();
}

void HdRobotRenderParam::UpdateLidarCamera(const SdfPath& cameraId, const HdRobotLidarData& lidarData)
{
  std::lock_guard<std::mutex> lock(_lidarMutex);

  if(_hasLidarCamera && _lidarCameraId != cameraId && !_warnedMultipleLidarCameras)
  {
    TF_WARN("Multiple lidar cameras are unsupported (%s -> %s). Latest synced camera wins.", _lidarCameraId.GetText(),
            cameraId.GetText());
    _warnedMultipleLidarCameras = true;
  }

  _lidarCameraId   = cameraId;
  _lidarCameraData = lidarData;
  _hasLidarCamera  = true;
}

void HdRobotRenderParam::ClearLidarCamera(const SdfPath& cameraId)
{
  std::lock_guard<std::mutex> lock(_lidarMutex);
  if(_hasLidarCamera && _lidarCameraId == cameraId)
  {
    _lidarCameraId              = SdfPath();
    _lidarCameraData            = HdRobotLidarData();
    _hasLidarCamera             = false;
    _warnedMultipleLidarCameras = false;
  }
}

bool HdRobotRenderParam::GetLidarCamera(HdRobotLidarData* lidarData) const
{
  std::lock_guard<std::mutex> lock(_lidarMutex);
  if(!_hasLidarCamera || lidarData == nullptr)
  {
    return false;
  }

  *lidarData = _lidarCameraData;
  return true;
}

void ConvertVmeshToLoader(const HydraMesh& mesh, ModelLoader& loader)
{
  size_t vertexOffset = 0;

  loader.m_vertices.clear();
  loader.m_indices.clear();

  const auto& points    = mesh.points;
  const auto& normals   = mesh.normals;
  const auto& texCoords = mesh.texCoords;
  const auto& faces     = mesh.faces;
  const auto& matIdx    = mesh.materialIds;

  VtVec2fArray newTexCoords = texCoords;
  if(newTexCoords.empty() || newTexCoords.size() != points.size())
  {
    newTexCoords.resize(points.size());
    for(size_t i = 0; i < points.size(); ++i)
    {
      newTexCoords[i].Set((points[i][0] + 1.0f) * 0.5f, (points[i][2] + 1.0f) * 0.5f);
    }
  }

  if(points.size() != normals.size() || points.size() != newTexCoords.size())
  {
    std::cerr << "points:" << points.size() << '\n';
    std::cerr << "normals:" << normals.size() << '\n';
    std::cerr << "texCoords:" << newTexCoords.size() << '\n';
    std::cerr << "Error: points, normals, texCoords size mismatch" << '\n';
    return;
  }

  for(size_t i = 0; i < points.size(); ++i)
  {
    VertexObj vertex;
    vertex.pos      = glm::vec3(points[i][0], points[i][1], points[i][2]);
    vertex.nrm      = glm::vec3(normals[i][0], normals[i][1], normals[i][2]);
    vertex.texCoord = glm::vec2(newTexCoords[i][0], 1 - newTexCoords[i][1]);
    vertex.color    = glm::vec3(1.0f, 1.0f, 1.0f);
    loader.m_vertices.push_back(vertex);
  }

  for(const auto& face : faces)
  {
    loader.m_indices.push_back(static_cast<uint32_t>(face[0]) + vertexOffset);
    loader.m_indices.push_back(static_cast<uint32_t>(face[1]) + vertexOffset);
    loader.m_indices.push_back(static_cast<uint32_t>(face[2]) + vertexOffset);
  }

  for(const auto& matId : matIdx)
  {
    loader.m_matIndx.push_back(static_cast<uint32_t>(matId));
  }

  vertexOffset += points.size();
}

void add_default_material(ModelLoader& loader)
{
  MaterialObj mat;
  mat.textureID = 0;
  loader.m_materials.emplace_back(mat);

  mat.textureID = -1;
  loader.m_materials.emplace_back(mat);

  if(loader.m_materials.empty())
  {
    loader.m_materials.emplace_back(mat);
  }
}

void PrintLoader(const ModelLoader& loader, int n)
{
  std::cout << std::fixed << std::setprecision(6);
  std::cout << "m_indices count: " << loader.m_indices.size() << '\n';
  std::cout << "m_matIndx count: " << loader.m_matIndx.size() << '\n';

  int start        = 0;
  int currentValue = loader.m_matIndx[0];
  for(int i = 1; i <= static_cast<int>(loader.m_matIndx.size()); ++i)
  {
    if(i == static_cast<int>(loader.m_matIndx.size()) || loader.m_matIndx[i] != currentValue)
    {
      std::cout << start << "-" << i - 1 << ": " << currentValue;
      if(i < static_cast<int>(loader.m_matIndx.size()))
      {
        std::cout << ", ";
        currentValue = loader.m_matIndx[i];
        start        = i;
      }
    }
  }
  std::cout << '\n';
  TF_UNUSED(n);
}

void compareLoaders(const ModelLoader& tempLoader, const ModelLoader& loader)
{
  if(tempLoader.m_vertices.size() != loader.m_vertices.size())
  {
    std::cout << "m_vertices mismatch: tempLoader=" << tempLoader.m_vertices.size() << ", loader="
              << loader.m_vertices.size() << '\n';
  }

  bool verticesMatch = true;
  for(size_t i = 0; i < 5; ++i)
  {
    const auto& v1 = tempLoader.m_vertices[i];
    const auto& v2 = loader.m_vertices[i];
    if(v1.pos != v2.pos || v1.nrm != v2.nrm || v1.texCoord != v2.texCoord || v1.color != v2.color)
    {
      std::cout << "m_vertices[" << i << "] mismatch\n";
      verticesMatch = false;
    }
  }

  if(tempLoader.m_indices.size() != loader.m_indices.size())
  {
    std::cout << "m_indices mismatch: tempLoader=" << tempLoader.m_indices.size() << ", loader="
              << loader.m_indices.size() << '\n';
  }

  bool indicesMatch = true;
  for(size_t i = 0; i < 5; ++i)
  {
    if(tempLoader.m_indices[i] != loader.m_indices[i])
    {
      std::cout << "m_indices[" << i << "] mismatch: tempLoader=" << tempLoader.m_indices[i]
                << ", loader=" << loader.m_indices[i] << '\n';
      indicesMatch = false;
    }
  }

  if(verticesMatch && indicesMatch)
  {
    std::cout << "data match\n";
  }
  else
  {
    std::cout << "data mismatch\n";
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
