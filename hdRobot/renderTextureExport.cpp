#include "renderTextureExport.h"

#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/resolvedPath.h>
#include <pxr/usd/ar/resolver.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <nvgl/extensions_gl.hpp>
#include <optional>
#include <vector>

#include "glInteropCache.h"
#include "hello_vulkan.hpp"
#include "renderBuffer.h"
#include "tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

void ClearGlErrors();
bool CheckGlError(const char *operation);

namespace
{
std::optional<HeadlessAov> GetHeadlessAov(const TfToken &name)
{
  if(name == HdAovTokens->color)
  {
    return HeadlessAov::Color;
  }
  if(name == HdAovTokens->primId)
  {
    return HeadlessAov::PrimId;
  }
  if(name == HdAovTokens->instanceId)
  {
    return HeadlessAov::InstanceId;
  }
  if(name == HdAovTokens->depth || name == HdAovTokens->depthStencil)
  {
    return HeadlessAov::Depth;
  }
  if(name == HdRobotAovTokens->tileColor)
  {
    return HeadlessAov::TileColor;
  }
  if(name == HdRobotAovTokens->tileDepth)
  {
    return HeadlessAov::TileDepth;
  }
  if(name == HdRobotAovTokens->tileColorDisplay)
  {
    return HeadlessAov::TileColorDisplay;
  }
  if(name == HdRobotAovTokens->tileDepthDisplay)
  {
    return HeadlessAov::TileDepthDisplay;
  }
  return std::nullopt;
}

bool IsFixedTileAov(const TfToken &name)
{
  return name == HdRobotAovTokens->tileColor || name == HdRobotAovTokens->tileDepth;
}

bool CopyTileDepthDisplayToRenderBuffer(GLuint srcTextureId, GLsizei srcWidth, GLsizei srcHeight,
                                        GLuint dstTextureId, GLsizei dstWidth, GLsizei dstHeight)
{
  if(srcTextureId == 0 || dstTextureId == 0 || srcWidth <= 0 || srcHeight <= 0 || dstWidth <= 0 || dstHeight <= 0)
  {
    return false;
  }

  std::vector<float> srcPixels(static_cast<size_t>(srcWidth) * static_cast<size_t>(srcHeight), 0.0f);
  const size_t srcBytes = srcPixels.size() * sizeof(float);
  if(srcBytes > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
  {
    return false;
  }

  ClearGlErrors();
  glGetTextureSubImage(srcTextureId, 0, 0, 0, 0, srcWidth, srcHeight, 1, GL_RED, GL_FLOAT,
                       static_cast<GLsizei>(srcBytes), srcPixels.data());
  if(!CheckGlError("glGetTextureSubImage(tileDepthDisplay)"))
  {
    return false;
  }

  constexpr float kClearDepthThreshold = 0.999999f;
  float visualMin = std::numeric_limits<float>::infinity();
  float visualMax = -std::numeric_limits<float>::infinity();
  size_t foregroundCount = 0;

  for(float depth : srcPixels)
  {
    if(std::isfinite(depth) && depth < kClearDepthThreshold)
    {
      visualMin = std::min(visualMin, depth);
      visualMax = std::max(visualMax, depth);
      ++foregroundCount;
    }
  }

  const float visualRange = visualMax - visualMin;
  const bool canNormalize = foregroundCount > 0 && std::isfinite(visualMin) && std::isfinite(visualMax) &&
                            std::fabs(visualRange) > 1.0e-8f;

  std::vector<float> dstPixels(static_cast<size_t>(dstWidth) * static_cast<size_t>(dstHeight) * 4, 1.0f);
  for(GLsizei y = 0; y < dstHeight; ++y)
  {
    const GLsizei srcY =
        std::min(srcHeight - 1, static_cast<GLsizei>((static_cast<int64_t>(y) * srcHeight) / dstHeight));
    for(GLsizei x = 0; x < dstWidth; ++x)
    {
      const GLsizei srcX =
          std::min(srcWidth - 1, static_cast<GLsizei>((static_cast<int64_t>(x) * srcWidth) / dstWidth));
      const float depth = srcPixels[static_cast<size_t>(srcY) * static_cast<size_t>(srcWidth) + srcX];

      float value = 1.0f;
      if(!std::isfinite(depth))
      {
        value = 0.0f;
      }
      else if(depth >= kClearDepthThreshold)
      {
        value = 1.0f;
      }
      else if(canNormalize)
      {
        value = (depth - visualMin) / visualRange;
      }
      else
      {
        value = std::clamp(depth, 0.0f, 1.0f);
      }

      const size_t dstIndex = (static_cast<size_t>(y) * static_cast<size_t>(dstWidth) + x) * 4;
      dstPixels[dstIndex + 0] = value;
      dstPixels[dstIndex + 1] = value;
      dstPixels[dstIndex + 2] = value;
      dstPixels[dstIndex + 3] = 1.0f;
    }
  }

  const size_t dstBytes = dstPixels.size() * sizeof(float);
  if(dstBytes > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
  {
    return false;
  }

  ClearGlErrors();
  glTextureSubImage2D(dstTextureId, 0, 0, 0, dstWidth, dstHeight, GL_RGBA, GL_FLOAT, dstPixels.data());
  return CheckGlError("glTextureSubImage2D(tileDepthDisplay)");
}

TextureAsset ExportResolvedTextureAsset(const TextureAsset &registeredTexture)
{
  TextureAsset textureAsset;
  textureAsset.sourcePath = registeredTexture.sourcePath;
  textureAsset.usage = registeredTexture.usage;
  textureAsset.colorSpace = registeredTexture.colorSpace;

  ArResolver &resolver = ArGetResolver();
  ArResolvedPath resolvedPath = resolver.Resolve(textureAsset.sourcePath);
  if(!resolvedPath)
  {
    std::cout << "[RenderPass]:" << textureAsset.sourcePath << "is not valid" << std::endl;
    return textureAsset;
  }

  auto asset = resolver.OpenAsset(resolvedPath);
  if(!asset)
  {
    std::cout << "[RenderPass]:" << resolvedPath.GetPathString() << "failed" << std::endl;
    return textureAsset;
  }

  const auto buffer = asset->GetBuffer();
  const auto size = asset->GetSize();
  if(!buffer || size == 0)
  {
    std::cout << "[RenderPass]:" << resolvedPath.GetPathString() << "empty texture asset" << std::endl;
    return textureAsset;
  }

  const auto *bytes = reinterpret_cast<const uint8_t *>(buffer.get());
  textureAsset.encodedBytes.assign(bytes, bytes + size);
  return textureAsset;
}
} // namespace

std::vector<TextureAsset> ExportRegisteredTextures(const std::vector<TextureAsset> &registeredTextures)
{
  std::vector<TextureAsset> exportedTextures;
  exportedTextures.reserve(registeredTextures.size());
  for(const TextureAsset &registeredTexture : registeredTextures)
  {
    exportedTextures.push_back(ExportResolvedTextureAsset(registeredTexture));
  }
  return exportedTextures;
}

void ClearGlErrors()
{
  while(glGetError() != GL_NO_ERROR)
  {
  }
}

bool CheckGlError(const char *operation)
{
  const GLenum error = glGetError();
  if(error == GL_NO_ERROR)
  {
    return true;
  }

  std::cerr << "[RenderTextureExport] " << operation << " failed with GL error 0x" << std::hex << error << std::dec
            << std::endl;
  return false;
}

void DeleteFramebuffers(GLuint readFbo, GLuint drawFbo)
{
  if(readFbo != 0)
  {
    glDeleteFramebuffers(1, &readFbo);
  }
  if(drawFbo != 0)
  {
    glDeleteFramebuffers(1, &drawFbo);
  }
}

bool CopyAovToRenderBuffer(const ::HelloVulkan &app, const TfToken &name, HdRobotRenderBuffer *renderBuffer,
                           ::HdRobotGlInteropCache &glInteropCache)
{
  if(renderBuffer == nullptr)
  {
    std::cerr << "[RenderTextureExport] Missing render buffer for AOV " << name.GetString() << std::endl;
    return false;
  }

  const std::optional<HeadlessAov> aov = GetHeadlessAov(name);
  if(!aov)
  {
#if PXR_VERSION >= 2408
    if(name == HdAovTokens->elementId)
    {
      return true;
    }
#endif
    std::cerr << "[RenderTextureExport] Unsupported AOV token " << name.GetString() << std::endl;
    return false;
  }

  const std::optional<HeadlessAovTexture> src = app.GetAovTexture(*aov);
  if(!src)
  {
    std::cerr << "[RenderTextureExport] Missing source AOV texture for " << name.GetString() << std::endl;
    return false;
  }

  const GLsizei width = static_cast<GLsizei>(renderBuffer->GetWidth());
  const GLsizei height = static_cast<GLsizei>(renderBuffer->GetHeight());
  if(width <= 0 || height <= 0)
  {
    std::cerr << "[RenderTextureExport] Invalid render buffer size for AOV " << name.GetString() << ": " << width << "x"
              << height << std::endl;
    return false;
  }

  if(IsFixedTileAov(name) &&
     (src->extent.width != static_cast<uint32_t>(width) || src->extent.height != static_cast<uint32_t>(height)))
  {
    std::cerr << "[RenderTextureExport] Fixed tile AOV " << name.GetString()
              << " render buffer size mismatch: src=" << src->extent.width << "x" << src->extent.height
              << ", dst=" << width << "x" << height << std::endl;
    return false;
  }

  const GLuint srcTextureId = glInteropCache.GetOrImportSourceGlTexture(*src);
  const GLuint dstTextureId = renderBuffer->GetOpenGlTextureId();
  if(srcTextureId == 0 || dstTextureId == 0 || srcTextureId == dstTextureId)
  {
    std::cerr << "[RenderTextureExport] Invalid GL texture ids for AOV " << name.GetString() << " (src=" << srcTextureId
              << ", dst=" << dstTextureId << ")" << std::endl;
    return false;
  }

  GLint srcWidth = 0;
  GLint srcHeight = 0;
  ClearGlErrors();
  glGetTextureLevelParameteriv(srcTextureId, 0, GL_TEXTURE_WIDTH, &srcWidth);
  glGetTextureLevelParameteriv(srcTextureId, 0, GL_TEXTURE_HEIGHT, &srcHeight);
  if(!CheckGlError("glGetTextureLevelParameteriv"))
  {
    return false;
  }
  if(srcWidth <= 0 || srcHeight <= 0)
  {
    std::cerr << "[RenderTextureExport] Invalid source AOV size for " << name.GetString() << ": " << srcWidth << "x"
              << srcHeight << std::endl;
    return false;
  }

  if(name == HdRobotAovTokens->tileDepthDisplay)
  {
    return CopyTileDepthDisplayToRenderBuffer(srcTextureId, static_cast<GLsizei>(srcWidth),
                                              static_cast<GLsizei>(srcHeight), dstTextureId, width, height);
  }

  if(srcWidth == width && srcHeight == height)
  {
    ClearGlErrors();
    glCopyImageSubData(srcTextureId, GL_TEXTURE_2D, 0, 0, 0, 0, dstTextureId, GL_TEXTURE_2D, 0, 0, 0, 0, width, height,
                       1);
    return CheckGlError("glCopyImageSubData");
  }

  GLuint readFbo = 0;
  GLuint drawFbo = 0;
  ClearGlErrors();
  glCreateFramebuffers(1, &readFbo);
  glCreateFramebuffers(1, &drawFbo);
  if(readFbo == 0 || drawFbo == 0 || !CheckGlError("glCreateFramebuffers"))
  {
    std::cerr << "[RenderTextureExport] Failed to create blit FBOs for AOV " << name.GetString() << std::endl;
    DeleteFramebuffers(readFbo, drawFbo);
    return false;
  }

  ClearGlErrors();
  glNamedFramebufferTexture(readFbo, GL_COLOR_ATTACHMENT0, srcTextureId, 0);
  glNamedFramebufferTexture(drawFbo, GL_COLOR_ATTACHMENT0, dstTextureId, 0);
  if(!CheckGlError("glNamedFramebufferTexture"))
  {
    DeleteFramebuffers(readFbo, drawFbo);
    return false;
  }

  const GLenum readStatus = glCheckNamedFramebufferStatus(readFbo, GL_FRAMEBUFFER);
  const GLenum drawStatus = glCheckNamedFramebufferStatus(drawFbo, GL_FRAMEBUFFER);
  if(!CheckGlError("glCheckNamedFramebufferStatus"))
  {
    DeleteFramebuffers(readFbo, drawFbo);
    return false;
  }

  if(readStatus != GL_FRAMEBUFFER_COMPLETE || drawStatus != GL_FRAMEBUFFER_COMPLETE)
  {
    std::cerr << "[RenderTextureExport] Incomplete blit FBO for AOV " << name.GetString() << " (read=0x" << std::hex
              << readStatus << ", draw=0x" << drawStatus << std::dec << ")" << std::endl;
    DeleteFramebuffers(readFbo, drawFbo);
    return false;
  }

  ClearGlErrors();
  glBlitNamedFramebuffer(readFbo, drawFbo, 0, 0, srcWidth, srcHeight, 0, 0, width, height, GL_COLOR_BUFFER_BIT,
                         GL_NEAREST);
  const bool copied = CheckGlError("glBlitNamedFramebuffer");

  DeleteFramebuffers(readFbo, drawFbo);
  return copied;
}

PXR_NAMESPACE_CLOSE_SCOPE
