#pragma once

#include "aov_texture.hpp"
#include <nvgl/extensions_gl.hpp>

class HdRobotGlInteropCache {
public:
  HdRobotGlInteropCache() = default;
  HdRobotGlInteropCache(const HdRobotGlInteropCache &) = delete;
  HdRobotGlInteropCache &operator=(const HdRobotGlInteropCache &) = delete;
  ~HdRobotGlInteropCache();

  GLuint GetOrImportSourceGlTexture(const HeadlessAovTexture &texture);
  void Clear();
};
