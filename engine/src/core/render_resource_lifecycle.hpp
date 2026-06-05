#pragma once

#include <engine/texture_asset.hpp>

#include <vector>

class Engine;

namespace engine
{
class RenderResourceLifecycle final
{
public:
  static void createResourcesInDependencyOrder(Engine& renderer);
  static void destroyResourcesInShutdownOrder(Engine& renderer);

  static void rebuildTexturesAndSceneBindings(Engine& renderer, const std::vector<TextureAsset>& textureAssets);
};
} // namespace engine
