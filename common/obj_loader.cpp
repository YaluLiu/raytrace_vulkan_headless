/*
 * Copyright (c) 2021-2023, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */

// 只在本文件里实现tiny_obj_loader库
#define TINYOBJLOADER_IMPLEMENTATION
// 包含头文件声明
#include "obj_loader.h"

// 实现ObjLoader::loadModel，加载OBJ模型到内存
void ObjLoader::loadModel(const std::string& filename) {
  // 创建tinyobj的Reader对象
  tinyobj::ObjReader reader;
  // 解析OBJ文件
  reader.ParseFromFile(filename);
  // 检查解析是否成功
  if(!reader.Valid()) {
    assert(reader.Valid());
  }

  // 将模型中的所有材质信息收集到本地
  for(const auto& material : reader.GetMaterials()) {
    // 构造自己的MaterialObj结构体
    MaterialObj m;
    m.ambient       = glm::vec3(material.ambient[0], material.ambient[1], material.ambient[2]);
    m.diffuse       = glm::vec3(material.diffuse[0], material.diffuse[1], material.diffuse[2]);
    m.specular      = glm::vec3(material.specular[0], material.specular[1], material.specular[2]);
    m.emission      = glm::vec3(material.emission[0], material.emission[1], material.emission[2]);
    m.transmittance = glm::vec3(material.transmittance[0], material.transmittance[1],
                                material.transmittance[2]);
    m.dissolve      = material.dissolve;
    m.ior           = material.ior;
    m.shininess     = material.shininess;
    m.illum         = material.illum;
    // 若存在漫反射贴图，存入m_textures并记录贴图ID
    if(!material.diffuse_texname.empty()) {
      m_textures.push_back(material.diffuse_texname);
      m.textureID = static_cast<int>(m_textures.size()) - 1;
    }

    // 加入材质数组
    m_materials.emplace_back(m);
  }

  // 如果没有任何材质，添加一个默认材质
  if(m_materials.empty())
    m_materials.emplace_back(MaterialObj());

  // 获取OBJ的全局属性（顶点、法线、纹理坐标等）
  const tinyobj::attrib_t& attrib = reader.GetAttrib();

  // 遍历所有形状（shape）
  for(const auto& shape : reader.GetShapes()) {
    // 预留顶点和索引空间
    m_vertices.reserve(shape.mesh.indices.size() + m_vertices.size());
    m_indices.reserve(shape.mesh.indices.size() + m_indices.size());
    // 添加该shape的材质索引到m_matIndx
    m_matIndx.insert(m_matIndx.end(), shape.mesh.material_ids.begin(),
                     shape.mesh.material_ids.end());

    // std::cout << "mesh_ids:";
    // for(auto id : shape.mesh.material_ids){
    //   std::cout << id << ",";
    // }
    // std::cout << std::endl;
    // std::cout << m_matIndx.size() << std::endl;

    // 遍历所有索引（三角形的顶点）
    for(const auto& index : shape.mesh.indices) {
      // 新建一个顶点
      VertexObj vertex = {};
      // 取得顶点坐标
      const float* vp = &attrib.vertices[3 * index.vertex_index];
      vertex.pos      = {*(vp + 0), *(vp + 1), *(vp + 2)};

      // 取得法线，如果有的话
      if(!attrib.normals.empty() && index.normal_index >= 0) {
        const float* np = &attrib.normals[3 * index.normal_index];
        vertex.nrm      = {*(np + 0), *(np + 1), *(np + 2)};
      }

      // 取得纹理坐标，如果有的话
      if(!attrib.texcoords.empty() && index.texcoord_index >= 0) {
        const float* tp = &attrib.texcoords[2 * index.texcoord_index + 0];
        // OpenGL的V轴需要反向
        vertex.texCoord = {*tp, 1.0f - *(tp + 1)};
      }

      // 取得颜色信息，如果有的话
      if(!attrib.colors.empty()) {
        const float* vc = &attrib.colors[3 * index.vertex_index];
        vertex.color    = {*(vc + 0), *(vc + 1), *(vc + 2)};
      }

      // 添加到顶点数组
      m_vertices.push_back(vertex);
      // 添加对应的索引
      m_indices.push_back(static_cast<int>(m_indices.size()));
    }
  }

  // 修正材质索引，确保都合法
  for(auto& mi : m_matIndx) {
    if(mi < 0 || mi > m_materials.size())
      mi = 0;
  }

  // 如果OBJ没有法线，则为每个三角形自动计算法线
  if(attrib.normals.empty()) {
    for(size_t i = 0; i < m_indices.size(); i += 3) {
      VertexObj& v0 = m_vertices[m_indices[i + 0]];
      VertexObj& v1 = m_vertices[m_indices[i + 1]];
      VertexObj& v2 = m_vertices[m_indices[i + 2]];

      // 用叉积计算三角形法线，再归一化
      glm::vec3 n = glm::normalize(glm::cross((v1.pos - v0.pos), (v2.pos - v0.pos)));
      v0.nrm      = n;
      v1.nrm      = n;
      v2.nrm      = n;
    }
  }
}