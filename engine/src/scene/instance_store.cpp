#include "scene/instance_store.hpp"

void InstanceStore::clear()
{
  m_instances.clear();
  m_externalInstanceIds.clear();
}

uint32_t InstanceStore::addInstance(const glm::mat4& transform, uint32_t meshIndex, int externalInstanceId)
{
  SceneInstance instance;
  instance.transform = transform;
  instance.meshIndex = meshIndex;
  m_instances.push_back(instance);
  m_externalInstanceIds.push_back(externalInstanceId);
  return static_cast<uint32_t>(m_instances.size() - 1);
}

bool InstanceStore::updateInstance(uint32_t rendererInstanceIndex,
                                   glm::mat4 transform,
                                   bool visible,
                                   uint32_t traceMask)
{
  if(rendererInstanceIndex >= m_instances.size())
  {
    return false;
  }
  m_instances[rendererInstanceIndex].transform = transform;
  m_instances[rendererInstanceIndex].visible = visible;
  m_instances[rendererInstanceIndex].traceMask = traceMask;
  return true;
}
