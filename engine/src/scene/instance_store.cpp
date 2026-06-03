#include "scene/instance_store.hpp"

void InstanceStore::clear()
{
  m_instances.clear();
  m_instanceIds.clear();
}

uint32_t InstanceStore::addInstance(const glm::mat4& transform, uint32_t objIndex, int instanceId)
{
  SceneInstance instance;
  instance.transform = transform;
  instance.objIndex = objIndex;
  m_instances.push_back(instance);
  m_instanceIds.push_back(instanceId);
  return static_cast<uint32_t>(m_instances.size() - 1);
}

bool InstanceStore::updateInstance(uint32_t instanceId, glm::mat4 transform, bool visible, uint32_t traceMask)
{
  if(instanceId >= m_instances.size())
  {
    return false;
  }
  m_instances[instanceId].transform = transform;
  m_instances[instanceId].visible = visible;
  m_instances[instanceId].traceMask = traceMask;
  return true;
}
