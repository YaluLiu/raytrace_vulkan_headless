#pragma once

#include "scene/scene_types.hpp"

#include <span>
#include <vector>

#include <glm/glm.hpp>

class InstanceStore final
{
public:
  void clear();

  uint32_t addInstance(const glm::mat4& transform, uint32_t objIndex, int instanceId = 0);
  bool updateInstance(uint32_t instanceId, glm::mat4 transform, bool visible, uint32_t traceMask);

  size_t size() const { return m_instances.size(); }
  const SceneInstance& get(size_t index) const { return m_instances[index]; }
  std::span<const SceneInstance> instances() const { return m_instances; }
  std::span<const int> instanceIds() const { return m_instanceIds; }

private:
  std::vector<SceneInstance> m_instances;
  std::vector<int> m_instanceIds;
};
