#pragma once

#include <engine/aov_texture.hpp>
#include <nvgl/extensions_gl.hpp>
#include <pxr/pxr.h>

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {

class GlInteropCache {
public:
  GlInteropCache();
  GlInteropCache(const GlInteropCache &) = delete;
  GlInteropCache &operator=(const GlInteropCache &) = delete;
  ~GlInteropCache();

  GLuint GetOrImportSourceGlTexture(const ExportedAovTexture &texture);
  void Evict(const ExportedAovTexture &texture);
  void Clear();

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};

} // namespace hdrobot

PXR_NAMESPACE_CLOSE_SCOPE
