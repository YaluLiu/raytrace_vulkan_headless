#pragma once

#include <raster/aov_texture.hpp>
#include <nvgl/extensions_gl.hpp>

#include <memory>

class HdRobotGlInteropCache {
public:
  HdRobotGlInteropCache();
  HdRobotGlInteropCache(const HdRobotGlInteropCache &) = delete;
  HdRobotGlInteropCache &operator=(const HdRobotGlInteropCache &) = delete;
  ~HdRobotGlInteropCache();

  GLuint GetOrImportSourceGlTexture(const ExportedRasterAovTexture &texture);
  void Evict(const ExportedRasterAovTexture &texture);
  void Clear();

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};
