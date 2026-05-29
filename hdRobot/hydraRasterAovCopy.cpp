#include "hydraRasterAovCopy.h"

#include <iostream>
#include <nvgl/extensions_gl.hpp>
#include <optional>

#include "glInteropCache.h"
#include <raster/raster_renderer.hpp>
#include "renderBuffer.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
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

  std::cerr << "[HydraRasterAovCopy] " << operation << " failed with GL error 0x" << std::hex << error << std::dec
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
} // namespace

bool CopyAovToRenderBuffer(const ::RasterRenderer &app,
                           const HdRobotAovCopyRequest &request,
                           ::HdRobotGlInteropCache &glInteropCache)
{
  const TfToken &name = request.aovName;
  HdRobotRenderBuffer *renderBuffer = request.renderBuffer;
  if(renderBuffer == nullptr)
  {
    std::cerr << "[HydraRasterAovCopy] Missing render buffer for AOV " << name.GetString() << std::endl;
    return false;
  }

  const std::optional<ExportedRasterAovTexture> src = app.GetAovTexture(request.rasterAov);
  if(!src)
  {
    std::cerr << "[HydraRasterAovCopy] Missing source AOV texture for " << name.GetString() << std::endl;
    return false;
  }

  const GLsizei width = static_cast<GLsizei>(renderBuffer->GetWidth());
  const GLsizei height = static_cast<GLsizei>(renderBuffer->GetHeight());
  if(width <= 0 || height <= 0)
  {
    std::cerr << "[HydraRasterAovCopy] Invalid render buffer size for AOV " << name.GetString() << ": " << width << "x"
              << height << std::endl;
    return false;
  }

  if(request.scaling == HdRobotAovCopyScaling::RequireExactSourceSize &&
     (src->extent.width != static_cast<uint32_t>(width) || src->extent.height != static_cast<uint32_t>(height)))
  {
    std::cerr << "[HydraRasterAovCopy] AOV " << name.GetString()
              << " render buffer size mismatch: src=" << src->extent.width << "x" << src->extent.height
              << ", dst=" << width << "x" << height << std::endl;
    return false;
  }

  const GLuint dstTextureId = renderBuffer->GetOpenGlTextureId();
  if(dstTextureId == 0)
  {
    std::cerr << "[HydraRasterAovCopy] Invalid destination GL texture id for AOV " << name.GetString() << std::endl;
    return false;
  }

  auto copyFromSourceTexture = [&](GLuint srcTextureId)
  {
    if(srcTextureId == 0 || srcTextureId == dstTextureId)
    {
      std::cerr << "[HydraRasterAovCopy] Invalid source GL texture id for AOV " << name.GetString() << " (src="
                << srcTextureId << ", dst=" << dstTextureId << ")" << std::endl;
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
      std::cerr << "[HydraRasterAovCopy] Invalid source AOV size for " << name.GetString() << ": " << srcWidth << "x"
                << srcHeight << std::endl;
      return false;
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
      std::cerr << "[HydraRasterAovCopy] Failed to create blit FBOs for AOV " << name.GetString() << std::endl;
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
      std::cerr << "[HydraRasterAovCopy] Incomplete blit FBO for AOV " << name.GetString() << " (read=0x" << std::hex
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
  };

  GLuint srcTextureId = glInteropCache.GetOrImportSourceGlTexture(*src);
  if(copyFromSourceTexture(srcTextureId))
  {
    return true;
  }

  glInteropCache.Evict(*src);
  srcTextureId = glInteropCache.GetOrImportSourceGlTexture(*src);
  return copyFromSourceTexture(srcTextureId);
}

PXR_NAMESPACE_CLOSE_SCOPE
