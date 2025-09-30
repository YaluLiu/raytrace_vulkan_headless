#include <sstream>
#include <glm/glm.hpp>
#include "hello_vulkan.hpp"
#include "nvh/alignment.hpp"
#include "nvh/cameramanipulator.hpp"
#include "nvh/fileoperations.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/images_vk.hpp"
#include "nvvk/pipeline_vk.hpp"
#include "nvvk/renderpasses_vk.hpp"
#include "nvvk/shaders_vk.hpp"
#include "nvvk/buffers_vk.hpp"
#include <random>

extern std::vector<std::string> defaultSearchPaths;

void HelloVulkan::initRayTracing()
{
  VkPhysicalDeviceProperties2 prop2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  prop2.pNext = &m_rtProperties;
  vkGetPhysicalDeviceProperties2(m_physicalDevice, &prop2);

  m_rtBuilder.setup(m_device, &m_alloc, m_graphicsQueueIndex);
  m_sbtWrapper.setup(m_device, m_graphicsQueueIndex, &m_alloc, m_rtProperties);
}

auto HelloVulkan::objectToVkGeometryKHR(const ObjModel& model)
{
  VkDeviceAddress vertexAddress = nvvk::getBufferDeviceAddress(m_device, model.vertexBuffer.buffer);
  VkDeviceAddress indexAddress  = nvvk::getBufferDeviceAddress(m_device, model.indexBuffer.buffer);

  uint32_t maxPrimitiveCount = model.nbIndices / 3;

  VkAccelerationStructureGeometryTrianglesDataKHR triangles{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
  triangles.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;  // 顶点为vec3
  triangles.vertexData.deviceAddress = vertexAddress;
  triangles.vertexStride             = sizeof(VertexObj);
  triangles.indexType                = VK_INDEX_TYPE_UINT32;
  triangles.indexData.deviceAddress  = indexAddress;
  triangles.maxVertex                = model.nbVertices - 1;

  VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
  asGeom.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  asGeom.flags              = VK_GEOMETRY_OPAQUE_BIT_KHR;
  asGeom.geometry.triangles = triangles;

  VkAccelerationStructureBuildRangeInfoKHR offset;
  offset.firstVertex     = 0;
  offset.primitiveCount  = maxPrimitiveCount;
  offset.primitiveOffset = 0;
  offset.transformOffset = 0;

  nvvk::RaytracingBuilderKHR::BlasInput input;
  input.asGeometry.emplace_back(asGeom);
  input.asBuildOffsetInfo.emplace_back(offset);

  return input;
}


auto HelloVulkan::sphereToVkGeometryKHR()
{
  VkDeviceAddress dataAddress = nvvk::getBufferDeviceAddress(m_device, m_spheresAabbBuffer.buffer);

  VkAccelerationStructureGeometryAabbsDataKHR aabbs{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR};
  aabbs.data.deviceAddress = dataAddress;
  aabbs.stride             = sizeof(Aabb);

  // Setting up the build info of the acceleration (C version, c++ gives wrong type)
  VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
  asGeom.geometryType   = VK_GEOMETRY_TYPE_AABBS_KHR;
  asGeom.flags          = VK_GEOMETRY_OPAQUE_BIT_KHR;
  asGeom.geometry.aabbs = aabbs;

  VkAccelerationStructureBuildRangeInfoKHR offset{};
  offset.firstVertex     = 0;
  offset.primitiveCount  = (uint32_t)m_spheres.size();  // Nb aabb
  offset.primitiveOffset = 0;
  offset.transformOffset = 0;

  nvvk::RaytracingBuilderKHR::BlasInput input;
  input.asGeometry.emplace_back(asGeom);
  input.asBuildOffsetInfo.emplace_back(offset);
  return input;
}

void HelloVulkan::createBottomLevelAS()
{
  m_blas.reserve(m_objModel.size());

  for(const auto& obj : m_objModel)
  {
    auto blas = objectToVkGeometryKHR(obj);
    m_blas.emplace_back(blas);
  }

  // Spheres
  {
    auto blas = sphereToVkGeometryKHR();
    m_blas.emplace_back(blas);
  }
  m_rtBuilder.buildBlas(m_blas, VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR
                                    | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR);
}

void HelloVulkan::createTopLevelAS()
{
  auto nbObj = static_cast<uint32_t>(m_instances.size()) - 1;

  for(const HelloVulkan::ObjInstance& inst : m_instances)
  {
    VkAccelerationStructureInstanceKHR rayInst{};
    rayInst.transform                              = nvvk::toTransformMatrixKHR(inst.transform);
    rayInst.instanceCustomIndex                    = inst.objIndex;
    rayInst.accelerationStructureReference         = m_rtBuilder.getBlasDeviceAddress(inst.objIndex);
    rayInst.flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    rayInst.mask                                   = 0xFF;
    rayInst.instanceShaderBindingTableRecordOffset = 0;
    m_tlas.emplace_back(rayInst);
  }

  // Add the blas containing all implicit objects
  {
    VkAccelerationStructureInstanceKHR rayInst{};
    rayInst.transform                      = nvvk::toTransformMatrixKHR(glm::mat4(1));  // (identity)
    rayInst.instanceCustomIndex            = nbObj;  // nbObj == last object == implicit
    rayInst.accelerationStructureReference = m_rtBuilder.getBlasDeviceAddress(static_cast<uint32_t>(m_objModel.size()));
    rayInst.flags                          = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    rayInst.mask                           = 0xFF;       //  Only be hit if rayMask & instance.mask != 0
    rayInst.instanceShaderBindingTableRecordOffset = 1;  // We will use the same hit group for all objects
    m_tlas.emplace_back(rayInst);
  }

  m_rtFlags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
  m_rtBuilder.buildTlas(m_tlas, m_rtFlags);
}

void HelloVulkan::updateTlas(uint32_t mesh_Id, glm::mat4 transform, bool visible)
{
  VkAccelerationStructureInstanceKHR& tinst = m_tlas[mesh_Id];
  tinst.mask                                = visible ? 0xFF : 0x00;
  tinst.transform                           = nvvk::toTransformMatrixKHR(transform);
}

void HelloVulkan::updateTlasEnd()
{
  m_rtBuilder.buildTlas(m_tlas, m_rtFlags, true);
}

void HelloVulkan::updateBlas(uint32_t mesh_Id)
{
  std::vector<VertexObj>& now_vertices = m_Loader[mesh_Id].m_vertices;
  ObjModel&               model        = m_objModel[mesh_Id];

  nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
  VkCommandBuffer   cmdBuf = genCmdBuf.createCommandBuffer();

  model.nbVertices = static_cast<uint32_t>(now_vertices.size());

  VkBufferUsageFlags flag = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  VkBufferUsageFlags rayTracingFlags =
      flag | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  m_alloc.destroy(model.vertexBuffer);
  model.vertexBuffer = m_alloc.createBuffer(cmdBuf, now_vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rayTracingFlags);
  genCmdBuf.submitAndWait(cmdBuf);
  m_rtBuilder.updateBlas(mesh_Id, m_blas[mesh_Id],
                         VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR);
}


//--------------------------------------------------------------------------------------------------
// Creating all spheres
//

void HelloVulkan::addSpheres(std::vector<Sphere> v_sphere)
{
  // All spheres
  uint32_t nbSpheres = v_sphere.size();
  m_spheres.resize(nbSpheres);

  for(uint32_t i = 0; i < nbSpheres; i++)
  {
    Sphere s;
    s.center     = v_sphere[i].center;
    s.radius     = v_sphere[i].radius;
    m_spheres[i] = std::move(s);
  }

  // std::random_device                    rd{};
  // std::mt19937                          gen{rd()};
  // std::normal_distribution<float>       xzd{0.f, 5.f};
  // std::normal_distribution<float>       yd{6.f, 3.f};
  // std::uniform_real_distribution<float> radd{.05f, .2f};

  // for(uint32_t i = 0; i < nbSpheres; i++)
  // {
  //   Sphere s;
  //   s.center     = glm::vec3(xzd(gen), yd(gen), xzd(gen));
  //   s.radius     = radd(gen);
  //   m_spheres[i] = std::move(s);
  // }

  // Axis aligned bounding box of each sphere
  std::vector<Aabb> aabbs;
  aabbs.reserve(nbSpheres);
  for(const auto& s : m_spheres)
  {
    Aabb aabb;
    aabb.minimum = s.center - glm::vec3(s.radius);
    aabb.maximum = s.center + glm::vec3(s.radius);
    aabbs.emplace_back(aabb);
  }

  // Creating two materials
  MaterialObj mat;
  mat.diffuse = glm::vec3(0, 1, 1);
  std::vector<MaterialObj> materials;
  std::vector<int>         matIdx(nbSpheres);
  materials.emplace_back(mat);
  mat.diffuse = glm::vec3(1, 1, 0);
  materials.emplace_back(mat);

  // Assign a material to each sphere
  for(size_t i = 0; i < m_spheres.size(); i++)
  {
    matIdx[i] = i % 2;
  }

  // Creating all buffers
  using vkBU = VkBufferUsageFlagBits;
  nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
  auto              cmdBuf = genCmdBuf.createCommandBuffer();
  m_spheresBuffer          = m_alloc.createBuffer(cmdBuf, m_spheres, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  m_spheresAabbBuffer      = m_alloc.createBuffer(cmdBuf, aabbs,
                                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                                                      | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
  m_spheresMatIndexBuffer =
      m_alloc.createBuffer(cmdBuf, matIdx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  m_spheresMatColorBuffer =
      m_alloc.createBuffer(cmdBuf, materials, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  genCmdBuf.submitAndWait(cmdBuf);

  // Debug information
  m_debug.setObjectName(m_spheresBuffer.buffer, "spheres");
  m_debug.setObjectName(m_spheresAabbBuffer.buffer, "spheresAabb");
  m_debug.setObjectName(m_spheresMatColorBuffer.buffer, "spheresMat");
  m_debug.setObjectName(m_spheresMatIndexBuffer.buffer, "spheresMatIdx");


  // Adding an extra instance to get access to the material buffers
  ObjDesc objDesc{};
  objDesc.materialAddress      = nvvk::getBufferDeviceAddress(m_device, m_spheresMatColorBuffer.buffer);
  objDesc.materialIndexAddress = nvvk::getBufferDeviceAddress(m_device, m_spheresMatIndexBuffer.buffer);
  m_objDesc.emplace_back(objDesc);

  ObjInstance instance{};
  instance.objIndex = static_cast<uint32_t>(m_objModel.size());
  m_instances.emplace_back(instance);
}

void HelloVulkan::createSpheres(uint32_t nbSpheres)
{
  std::random_device                    rd{};
  std::mt19937                          gen{rd()};
  std::normal_distribution<float>       xzd{0.f, 5.f};
  std::normal_distribution<float>       yd{6.f, 3.f};
  std::uniform_real_distribution<float> radd{.05f, .2f};

  // All spheres
  m_spheres.resize(nbSpheres);
  for(uint32_t i = 0; i < nbSpheres; i++)
  {
    Sphere s;
    s.center     = glm::vec3(xzd(gen), yd(gen), xzd(gen));
    s.radius     = radd(gen);
    m_spheres[i] = std::move(s);
  }

  // Axis aligned bounding box of each sphere
  std::vector<Aabb> aabbs;
  aabbs.reserve(nbSpheres);
  for(const auto& s : m_spheres)
  {
    Aabb aabb;
    aabb.minimum = s.center - glm::vec3(s.radius);
    aabb.maximum = s.center + glm::vec3(s.radius);
    aabbs.emplace_back(aabb);
  }

  // Creating two materials
  MaterialObj mat;
  mat.diffuse = glm::vec3(0, 1, 1);
  std::vector<MaterialObj> materials;
  std::vector<int>         matIdx(nbSpheres);
  materials.emplace_back(mat);
  mat.diffuse = glm::vec3(1, 1, 0);
  materials.emplace_back(mat);

  // Assign a material to each sphere
  for(size_t i = 0; i < m_spheres.size(); i++)
  {
    matIdx[i] = i % 2;
  }

  // Creating all buffers
  using vkBU = VkBufferUsageFlagBits;
  nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
  auto              cmdBuf = genCmdBuf.createCommandBuffer();
  m_spheresBuffer          = m_alloc.createBuffer(cmdBuf, m_spheres, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  m_spheresAabbBuffer      = m_alloc.createBuffer(cmdBuf, aabbs,
                                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                                                      | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
  m_spheresMatIndexBuffer =
      m_alloc.createBuffer(cmdBuf, matIdx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  m_spheresMatColorBuffer =
      m_alloc.createBuffer(cmdBuf, materials, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  genCmdBuf.submitAndWait(cmdBuf);

  // Debug information
  m_debug.setObjectName(m_spheresBuffer.buffer, "spheres");
  m_debug.setObjectName(m_spheresAabbBuffer.buffer, "spheresAabb");
  m_debug.setObjectName(m_spheresMatColorBuffer.buffer, "spheresMat");
  m_debug.setObjectName(m_spheresMatIndexBuffer.buffer, "spheresMatIdx");


  // Adding an extra instance to get access to the material buffers
  ObjDesc objDesc{};
  objDesc.materialAddress      = nvvk::getBufferDeviceAddress(m_device, m_spheresMatColorBuffer.buffer);
  objDesc.materialIndexAddress = nvvk::getBufferDeviceAddress(m_device, m_spheresMatIndexBuffer.buffer);
  m_objDesc.emplace_back(objDesc);

  ObjInstance instance{};
  instance.objIndex = static_cast<uint32_t>(m_objModel.size());
  m_instances.emplace_back(instance);
}