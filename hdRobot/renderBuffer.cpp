//
// Copyright (C) 2019-2022 Pablo Delgado Krämer
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//


#include <pxr/base/gf/vec3i.h>

// for convert to hgiGL texture
#include <pxr/imaging/hdx/hgiConversions.h>
#include <pxr/imaging/hgi/blitCmds.h>
#include <pxr/imaging/hgi/blitCmdsOps.h>
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgiGL/texture.h>
#include <pxr/imaging/hgi/types.h>
#include <nvgl/extensions_gl.hpp>

#include "renderDelegate.h"
#include "renderBuffer.h"

#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <malloc.h>
#endif

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
constexpr size_t kCpuBufferAlignment = 64;

size_t AlignUp(size_t value, size_t alignment)
{
  return ((value + alignment - 1) / alignment) * alignment;
}

void* AllocateAlignedBuffer(size_t alignment, size_t size)
{
#ifdef _WIN32
  return _aligned_malloc(size, alignment);
#else
  return std::aligned_alloc(alignment, size);
#endif
}

void FreeAlignedBuffer(void* ptr)
{
#ifdef _WIN32
  _aligned_free(ptr);
#else
  std::free(ptr);
#endif
}

HgiTextureUsage GetTextureUsage(HdFormat format, const TfToken &nameToken)
{
  HgiTextureUsage usage = 0;

  const bool isIdAov = (nameToken == HdAovTokens->primId) || (nameToken == HdAovTokens->instanceId) ||
#if PXR_VERSION >= 2408
                       (nameToken == HdAovTokens->elementId) ||
#endif
                       false;

  switch(format)
  {
    case HdFormatFloat32Vec4:
    case HdFormatFloat32Vec3:
    case HdFormatFloat32Vec2:
    case HdFormatUNorm8Vec4:
    case HdFormatUNorm8Vec3:
      usage |= HgiTextureUsageBitsColorTarget | HgiTextureUsageBitsShaderRead;
      break;

    case HdFormatFloat32:
      if(nameToken == HdAovTokens->depth || nameToken == HdAovTokens->depthStencil)
      {
        usage |= HgiTextureUsageBitsDepthTarget;
      }
      else
      {
        usage |= HgiTextureUsageBitsColorTarget | HgiTextureUsageBitsShaderRead;
      }
      break;

    case HdFormatInt32:
      usage |= HgiTextureUsageBitsColorTarget | HgiTextureUsageBitsShaderRead;
      break;

    default:
      TF_WARN("Unsupported HdFormat in GetTextureUsage: %d", format);
      break;
  }

  if(nameToken == HdAovTokens->color || nameToken == HdAovTokens->normal || isIdAov)
  {
    usage |= HgiTextureUsageBitsShaderRead;
  }

  return usage;
}
}  // namespace

HdRobotRenderBuffer::HdRobotRenderBuffer(const SdfPath& id, HdRobotRenderDelegate* renderDelegate)
    : HdRenderBuffer(id)
    , _owner(renderDelegate)
{
}

HdRobotRenderBuffer::~HdRobotRenderBuffer()
{
  _Deallocate();
}

bool HdRobotRenderBuffer::Allocate(const GfVec3i& dimensions, HdFormat format, bool multiSampled)
{
  TF_UNUSED(multiSampled);
  _Deallocate();

  if(dimensions[2] != 1)
  {
    TF_RUNTIME_ERROR("3D render buffers not supported!");
    return false;
  }

  if(dimensions[0] == 0 || dimensions[1] == 0)
  {
    TF_RUNTIME_ERROR("Can't allocate empty render buffer!");
    return false;
  }

  _width       = dimensions[0];
  _height      = dimensions[1];
  _format      = format;
  _buffer_size = _GetBufferSize(GfVec2i(_width, _height), _format);
  const size_t alignedSize = AlignUp(_buffer_size, kCpuBufferAlignment);
  _buffer                 = AllocateAlignedBuffer(kCpuBufferAlignment, alignedSize);
  if(_buffer == nullptr)
  {
    TF_RUNTIME_ERROR("Failed to allocate render buffer (%zu bytes)", alignedSize);
    _Deallocate();
    return false;
  }

  createDesc();
  return true;
}

void HdRobotRenderBuffer::clear(int num)
{
  if(_buffer != nullptr && _buffer_size > 0)
  {
    std::memset(_buffer, num, _buffer_size);
  }
}
size_t HdRobotRenderBuffer::_GetBufferSize(GfVec2i const& dims, HdFormat format)
{
  return dims[0] * dims[1] * HdDataSizeOfFormat(format);
}

unsigned int HdRobotRenderBuffer::GetWidth() const
{
  return _width;
}

unsigned int HdRobotRenderBuffer::GetHeight() const
{
  return _height;
}

unsigned int HdRobotRenderBuffer::GetDepth() const
{
  return 1u;
}

HdFormat HdRobotRenderBuffer::GetFormat() const
{
  return _format;
}

bool HdRobotRenderBuffer::IsMultiSampled() const
{
  return false;
}

bool HdRobotRenderBuffer::IsConverged() const
{
  return _isConverged;
}

void HdRobotRenderBuffer::SetConverged(bool converged)
{
  _isConverged = converged;
}

void* HdRobotRenderBuffer::Map()
{
  _isMapped = true;
  if(_texture)
  {
    read_texture(GetOpenGlTextureId());
  }
  return _buffer;
}

bool HdRobotRenderBuffer::IsMapped() const
{
  return _isMapped;
}

void HdRobotRenderBuffer::Unmap()
{
  _isMapped = false;
}

void HdRobotRenderBuffer::Resolve() {}

void HdRobotRenderBuffer::_Deallocate()
{
  if(_texture)
  {
    if(Hgi* hgi = _GetHgi())
    {
      hgi->DestroyTexture(&_texture);
    }
    _texture = HgiTextureHandle();
  }
  if(_buffer != nullptr)
  {
    FreeAlignedBuffer(_buffer);
    _buffer = nullptr;
  }

  _width  = 0;
  _height = 0;
  _format = HdFormatInvalid;
  _buffer_size = 0;
  _isMapped = false;
}


Hgi* HdRobotRenderBuffer::_GetHgi()
{
  return _owner != nullptr ? _owner->GetHgi() : nullptr;
}

void HdRobotRenderBuffer::createDesc()
{
  Hgi* hgi = _GetHgi();
  if(hgi == nullptr)
  {
    return;
  }

  const GfVec3i dim(GetWidth(), GetHeight(), 1);

  HdFormat hdFormat = GetFormat();
  if(hdFormat == HdFormatFloat32Vec3)
  {
    // Hgi 不支持直接 RGB32F，转成 RGBA32F
    hdFormat = HdFormatFloat32Vec4;
  }

  const HgiFormat hgiFormat = HdxHgiConversions::GetHgiFormat(hdFormat);

  // 构造描述
  _texDesc.debugName   = std::string("AovInput: ") + GetId().GetName();
  _texDesc.dimensions  = dim;
  _texDesc.format      = hgiFormat;
  _texDesc.layerCount  = 1;
  _texDesc.mipLevels   = 1;
  _texDesc.sampleCount = HgiSampleCount1;

  // 关键：根据格式 + AOV 名称计算 usage
  TfToken nameToken = GetId().GetNameToken();
  _texDesc.usage    = GetTextureUsage(GetFormat(), nameToken);

  // 初始数据（CPU -> GPU 一次性上传）
  _texDesc.initialData    = _buffer;
  _texDesc.pixelsByteSize = _buffer_size;

  if(_texture)
  {
    hgi->DestroyTexture(&_texture);
  }
  _texture = hgi->CreateTexture(_texDesc);
}

void _ConvertRGBtoRGBA(const float* rgbValues, size_t numRgbValues, std::vector<float>* rgbaValues)
{
  if(numRgbValues % 3 != 0)
  {
    TF_WARN("Value count should be divisible by 3.");
    return;
  }

  const size_t numRgbaValues = numRgbValues * 4 / 3;

  if(rgbValues != nullptr && rgbaValues != nullptr)
  {
    const float* rgbValuesIt = rgbValues;
    rgbaValues->resize(numRgbaValues);
    float*             rgbaValuesIt = rgbaValues->data();
    const float* const end          = rgbaValuesIt + numRgbaValues;

    while(rgbaValuesIt != end)
    {
      *rgbaValuesIt++ = *rgbValuesIt++;
      *rgbaValuesIt++ = *rgbValuesIt++;
      *rgbaValuesIt++ = *rgbValuesIt++;
      *rgbaValuesIt++ = 1.0f;
    }
  }
}

void HdRobotRenderBuffer::ConvertToHgiTexture()
{
  Hgi* hgi = _GetHgi();
  if(hgi == nullptr)
  {
    return;
  }

  const GfVec3i dim(GetWidth(), GetHeight(), GetDepth());

  const void* pixelData = _buffer;

  HdFormat hdFormat = GetFormat();
  // HgiFormatFloat32Vec3 not a supported texture format for Vulkan. Convert
  // data to vec4 format.
  if(hdFormat == HdFormatFloat32Vec3)
  {
    std::vector<float> float4Data;
    hdFormat               = HdFormatFloat32Vec4;
    const size_t numValues = 3 * dim[0] * dim[1] * dim[2];
    _ConvertRGBtoRGBA(reinterpret_cast<const float*>(pixelData), numValues, &float4Data);
    pixelData = reinterpret_cast<const void*>(float4Data.data());
  }

  const HgiFormat bufFormat     = HdxHgiConversions::GetHgiFormat(hdFormat);
  const size_t    pixelByteSize = HdDataSizeOfFormat(hdFormat);
  const size_t    dataByteSize  = dim[0] * dim[1] * dim[2] * pixelByteSize;

  // Update the existing texture if specs are compatible. This is more
  // efficient than re-creating, because the underlying framebuffer that
  // had the old texture attached would also need to be re-created.
  if(_texture && _texture->GetDescriptor().dimensions == dim && _texture->GetDescriptor().format == bufFormat)
  {
    HgiTextureCpuToGpuOp copyOp;
    copyOp.bufferByteSize         = dataByteSize;
    copyOp.cpuSourceBuffer        = pixelData;
    copyOp.gpuDestinationTexture  = _texture;
    HgiBlitCmdsUniquePtr blitCmds = hgi->CreateBlitCmds();
    blitCmds->PushDebugGroup("Upload CPU texels");
    blitCmds->CopyTextureCpuToGpu(copyOp);
    blitCmds->PopDebugGroup();
    hgi->SubmitCmds(blitCmds.get());
  }
  else
  {
    // Destroy old texture
    if(_texture)
    {
      hgi->DestroyTexture(&_texture);
    }
    // _buffer will changed when resize
    _texDesc.initialData    = _buffer;
    _texDesc.pixelsByteSize = _buffer_size;
    _texture                = hgi->CreateTexture(_texDesc);
  }
}

VtValue HdRobotRenderBuffer::GetResource(bool /*multiSampled*/) const
{
  return VtValue(_texture);
}

void HdRobotRenderBuffer::read_texture(uint32_t textureId)
{
  if(textureId == 0 || !_buffer || _width == 0 || _height == 0)
  {
    return;
  }

  glBindTexture(GL_TEXTURE_2D, textureId);
  if(_format == HdFormatInt32)
  {
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_INT, _buffer);
  }
  else if(_format == HdFormatFloat32)
  {
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, _buffer);
  }
  else if(_format == HdFormatFloat32Vec4)
  {
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, _buffer);
  }
  else if(_format == HdFormatFloat32Vec3)
  {
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, _buffer);
  }
  else if(_format == HdFormatFloat32Vec2)
  {
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RG, GL_FLOAT, _buffer);
  }
  else if(_format == HdFormatUNorm8Vec4)
  {
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, _buffer);
  }
  else if(_format == HdFormatUNorm8Vec3)
  {
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, _buffer);
  }
  else
  {
    TF_WARN("Unsupported texture readback format: %d", _format);
  }
}


PXR_NAMESPACE_CLOSE_SCOPE
