#include "renderTextureExport.h"

#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/resolvedPath.h>
#include <pxr/usd/ar/resolver.h>

#include <iostream>
#include <nvgl/extensions_gl.hpp>
#include <optional>

#include "glInteropCache.h"
#include "hello_vulkan.hpp"
#include "renderBuffer.h"
#include "tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace {
std::optional<HeadlessAov> GetHeadlessAov(const TfToken &name) {
  if (name == HdAovTokens->color) {
    return HeadlessAov::Color;
  }
  if (name == HdAovTokens->primId) {
    return HeadlessAov::PrimId;
  }
  if (name == HdAovTokens->instanceId) {
    return HeadlessAov::InstanceId;
  }
  if (name == HdRobotAovTokens->dlssRRDiffuseAlbedo) {
    return HeadlessAov::DlssRRDiffuseAlbedo;
  }
  if (name == HdRobotAovTokens->dlssRRSpecularAlbedo) {
    return HeadlessAov::DlssRRSpecularAlbedo;
  }
  if (name == HdRobotAovTokens->dlssRRNormalRoughness) {
    return HeadlessAov::DlssRRNormalRoughness;
  }
  if (name == HdRobotAovTokens->dlssRRMotionVector) {
    return HeadlessAov::DlssRRMotionVector;
  }
  if (name == HdAovTokens->depth || name == HdAovTokens->depthStencil) {
    return HeadlessAov::Depth;
  }
  if (name == HdRobotAovTokens->dlssRRLinearDepth) {
    return HeadlessAov::DlssRRLinearDepth;
  }
  if (name == HdRobotAovTokens->dlssRRSpecularHitDistance) {
    return HeadlessAov::DlssRRSpecularHitDistance;
  }
  if (name == HdRobotAovTokens->distanceToCamera) {
    return HeadlessAov::DistanceToCamera;
  }
  if (name == HdRobotAovTokens->lidarPointCloud) {
    return HeadlessAov::LidarPointCloud;
  }
  return std::nullopt;
}

HdRobotGlInteropCache &GetAovGlInteropCache() {
  static HdRobotGlInteropCache cache;
  return cache;
}

TextureAsset ExportResolvedTextureAsset(const TextureAsset &registeredTexture) {
  TextureAsset textureAsset;
  textureAsset.sourcePath = registeredTexture.sourcePath;
  textureAsset.usage = registeredTexture.usage;
  textureAsset.colorSpace = registeredTexture.colorSpace;

  ArResolver &resolver = ArGetResolver();
  ArResolvedPath resolvedPath = resolver.Resolve(textureAsset.sourcePath);
  if (!resolvedPath) {
    std::cout << "[RenderPass]:" << textureAsset.sourcePath << "is not valid"
              << std::endl;
    return textureAsset;
  }

  auto asset = resolver.OpenAsset(resolvedPath);
  if (!asset) {
    std::cout << "[RenderPass]:" << resolvedPath.GetPathString() << "failed"
              << std::endl;
    return textureAsset;
  }

  const auto buffer = asset->GetBuffer();
  const auto size = asset->GetSize();
  if (!buffer || size == 0) {
    std::cout << "[RenderPass]:" << resolvedPath.GetPathString()
              << "empty texture asset" << std::endl;
    return textureAsset;
  }

  const auto *bytes = reinterpret_cast<const uint8_t *>(buffer.get());
  textureAsset.encodedBytes.assign(bytes, bytes + size);
  return textureAsset;
}
} // namespace

std::vector<TextureAsset>
ExportRegisteredTextures(const std::vector<TextureAsset> &registeredTextures) {
  std::vector<TextureAsset> exportedTextures;
  exportedTextures.reserve(registeredTextures.size());
  for (const TextureAsset &registeredTexture : registeredTextures) {
    exportedTextures.push_back(ExportResolvedTextureAsset(registeredTexture));
  }
  return exportedTextures;
}

HdRobotRenderBuffer *
GetPrimaryRenderBuffer(const HdRenderPassAovBindingVector &bindings) {
  for (const HdRenderPassAovBinding &binding : bindings) {
    if (binding.renderBuffer != nullptr) {
      return static_cast<HdRobotRenderBuffer *>(binding.renderBuffer);
    }
  }
  return nullptr;
}

void CopyAovToRenderBuffer(const ::HelloVulkan &app, const TfToken &name,
                           HdRobotRenderBuffer *renderBuffer) {
  if (renderBuffer == nullptr) {
    return;
  }

  const std::optional<HeadlessAov> aov = GetHeadlessAov(name);
  if (!aov) {
    return;
  }

  const std::optional<HeadlessAovTexture> src = app.GetAovTexture(*aov);
  if (!src) {
    return;
  }

  const GLuint srcTextureId =
      GetAovGlInteropCache().GetOrImportSourceGlTexture(*src);
  const GLuint dstTextureId = renderBuffer->GetOpenGlTextureId();
  if (srcTextureId == 0 || dstTextureId == 0 || srcTextureId == dstTextureId) {
    return;
  }

  const GLsizei width = static_cast<GLsizei>(renderBuffer->GetWidth());
  const GLsizei height = static_cast<GLsizei>(renderBuffer->GetHeight());
  if (width <= 0 || height <= 0) {
    return;
  }

  GLint srcWidth = 0;
  GLint srcHeight = 0;
  glGetTextureLevelParameteriv(srcTextureId, 0, GL_TEXTURE_WIDTH, &srcWidth);
  glGetTextureLevelParameteriv(srcTextureId, 0, GL_TEXTURE_HEIGHT, &srcHeight);
  if (srcWidth <= 0 || srcHeight <= 0) {
    return;
  }

  if (srcWidth == width && srcHeight == height) {
    glCopyImageSubData(srcTextureId, GL_TEXTURE_2D, 0, 0, 0, 0, dstTextureId,
                       GL_TEXTURE_2D, 0, 0, 0, 0, width, height, 1);
    return;
  }

  GLuint readFbo = 0;
  GLuint drawFbo = 0;
  glCreateFramebuffers(1, &readFbo);
  glCreateFramebuffers(1, &drawFbo);
  glNamedFramebufferTexture(readFbo, GL_COLOR_ATTACHMENT0, srcTextureId, 0);
  glNamedFramebufferTexture(drawFbo, GL_COLOR_ATTACHMENT0, dstTextureId, 0);

  const GLenum readStatus =
      glCheckNamedFramebufferStatus(readFbo, GL_FRAMEBUFFER);
  const GLenum drawStatus =
      glCheckNamedFramebufferStatus(drawFbo, GL_FRAMEBUFFER);
  if (readStatus == GL_FRAMEBUFFER_COMPLETE &&
      drawStatus == GL_FRAMEBUFFER_COMPLETE) {
    glBlitNamedFramebuffer(readFbo, drawFbo, 0, 0, srcWidth, srcHeight, 0, 0,
                           width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  }

  glDeleteFramebuffers(1, &readFbo);
  glDeleteFramebuffers(1, &drawFbo);
}

void ClearAovGlInteropCache() { GetAovGlInteropCache().Clear(); }

PXR_NAMESPACE_CLOSE_SCOPE
