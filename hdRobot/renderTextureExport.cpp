#include "renderTextureExport.h"

#include "hello_vulkan.hpp"
#include "renderBuffer.h"
#include "tokens.h"

#include <nvgl/extensions_gl.hpp>

#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/resolvedPath.h>
#include <pxr/usd/ar/resolver.h>

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
GLuint GetAovSourceGlId(const ::HelloVulkan& app, const TfToken& name)
{
  if(name == HdAovTokens->color)
  {
    return app.m_rtOutputGL.oglId;
  }
  if(name == HdAovTokens->primId)
  {
    return app.m_rtObjectIdGL.oglId;
  }
  if(name == HdAovTokens->instanceId)
  {
    return app.m_rtInstanceIdGL.oglId;
  }
  if(name == HdRobotAovTokens->dlssRRDiffuseAlbedo)
  {
    return app.m_rtDiffuseAlbedoGL.oglId;
  }
  if(name == HdRobotAovTokens->dlssRRSpecularAlbedo)
  {
    return app.m_rtSpecularAlbedoGL.oglId;
  }
  if(name == HdRobotAovTokens->dlssRRNormalRoughness)
  {
    return app.m_rtNormalRoughnessGL.oglId;
  }
  if(name == HdRobotAovTokens->dlssRRMotionVector)
  {
    return app.m_rtMotionVectorGL.oglId;
  }
  if(name == HdAovTokens->depth || name == HdAovTokens->depthStencil)
  {
    return app.m_rtDepthAovGL.oglId;
  }
  if(name == HdRobotAovTokens->dlssRRLinearDepth)
  {
    return app.m_rtLinearDepthGL.oglId;
  }
  if(name == HdRobotAovTokens->dlssRRSpecularHitDistance)
  {
    return app.m_rtSpecularHitDistanceGL.oglId;
  }
  if(name == HdRobotAovTokens->distanceToCamera)
  {
    return app.m_rtDistanceToCameraGL.oglId;
  }
  if(name == HdRobotAovTokens->lidarPointCloud)
  {
    return app.m_rtLidarPointCloudGL.oglId;
  }
  return 0;
}

TextureAsset ExportResolvedTextureAsset(const std::string& path)
{
  TextureAsset textureAsset;
  textureAsset.sourcePath = path;

  ArResolver&    resolver     = ArGetResolver();
  ArResolvedPath resolvedPath = resolver.Resolve(path);
  if(!resolvedPath)
  {
    std::cout << "[RenderPass]:" << path << "is not valid" << std::endl;
    return textureAsset;
  }

  auto asset = resolver.OpenAsset(resolvedPath);
  if(!asset)
  {
    std::cout << "[RenderPass]:" << resolvedPath.GetPathString() << "failed" << std::endl;
    return textureAsset;
  }

  const auto buffer = asset->GetBuffer();
  const auto size   = asset->GetSize();
  if(!buffer || size == 0)
  {
    std::cout << "[RenderPass]:" << resolvedPath.GetPathString() << "empty texture asset" << std::endl;
    return textureAsset;
  }

  const auto* bytes = reinterpret_cast<const uint8_t*>(buffer.get());
  textureAsset.encodedBytes.assign(bytes, bytes + size);
  return textureAsset;
}
}  // namespace

std::vector<TextureAsset> ExportRegisteredTextures(const std::vector<std::string>& texturePaths)
{
  std::vector<TextureAsset> exportedTextures;
  exportedTextures.reserve(texturePaths.size());
  for(const std::string& texturePath : texturePaths)
  {
    exportedTextures.push_back(ExportResolvedTextureAsset(texturePath));
  }
  return exportedTextures;
}

HdRobotRenderBuffer* GetPrimaryRenderBuffer(const HdRenderPassAovBindingVector& bindings)
{
  for(const HdRenderPassAovBinding& binding : bindings)
  {
    if(binding.renderBuffer != nullptr)
    {
      return static_cast<HdRobotRenderBuffer*>(binding.renderBuffer);
    }
  }
  return nullptr;
}

void CopyAovToRenderBuffer(const ::HelloVulkan& app, const TfToken& name, HdRobotRenderBuffer* renderBuffer)
{
  if(renderBuffer == nullptr)
  {
    return;
  }

  const GLuint srcTextureId = GetAovSourceGlId(app, name);
  const GLuint dstTextureId = renderBuffer->GetOpenGlTextureId();
  if(srcTextureId == 0 || dstTextureId == 0 || srcTextureId == dstTextureId)
  {
    return;
  }

  const GLsizei width  = static_cast<GLsizei>(renderBuffer->GetWidth());
  const GLsizei height = static_cast<GLsizei>(renderBuffer->GetHeight());
  if(width <= 0 || height <= 0)
  {
    return;
  }

  GLint srcWidth  = 0;
  GLint srcHeight = 0;
  glGetTextureLevelParameteriv(srcTextureId, 0, GL_TEXTURE_WIDTH, &srcWidth);
  glGetTextureLevelParameteriv(srcTextureId, 0, GL_TEXTURE_HEIGHT, &srcHeight);
  if(srcWidth <= 0 || srcHeight <= 0)
  {
    return;
  }

  if(srcWidth == width && srcHeight == height)
  {
    glCopyImageSubData(srcTextureId, GL_TEXTURE_2D, 0, 0, 0, 0, dstTextureId, GL_TEXTURE_2D, 0, 0, 0, 0, width,
                       height, 1);
    return;
  }

  GLuint readFbo = 0;
  GLuint drawFbo = 0;
  glCreateFramebuffers(1, &readFbo);
  glCreateFramebuffers(1, &drawFbo);
  glNamedFramebufferTexture(readFbo, GL_COLOR_ATTACHMENT0, srcTextureId, 0);
  glNamedFramebufferTexture(drawFbo, GL_COLOR_ATTACHMENT0, dstTextureId, 0);

  const GLenum readStatus = glCheckNamedFramebufferStatus(readFbo, GL_FRAMEBUFFER);
  const GLenum drawStatus = glCheckNamedFramebufferStatus(drawFbo, GL_FRAMEBUFFER);
  if(readStatus == GL_FRAMEBUFFER_COMPLETE && drawStatus == GL_FRAMEBUFFER_COMPLETE)
  {
    glBlitNamedFramebuffer(readFbo, drawFbo, 0, 0, srcWidth, srcHeight, 0, 0, width, height, GL_COLOR_BUFFER_BIT,
                           GL_NEAREST);
  }

  glDeleteFramebuffers(1, &readFbo);
  glDeleteFramebuffers(1, &drawFbo);
}

PXR_NAMESPACE_CLOSE_SCOPE
