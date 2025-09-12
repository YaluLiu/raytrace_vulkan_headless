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

#include "renderDelegate.h"
#include "renderBuffer.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace {
static std::map<HdFormat, GiRenderBufferFormat> s_supportedRenderBufferFormats = {
    {HdFormatInt32, GiRenderBufferFormat::Int32},
    {HdFormatFloat32, GiRenderBufferFormat::Float32},
    {HdFormatFloat32Vec4, GiRenderBufferFormat::Float32Vec4}};
}

HdGatlingRenderBuffer::HdGatlingRenderBuffer(const SdfPath& id, HdGatlingRenderDelegate* renderDelegate)
    : HdRenderBuffer(id)
    , _owner(renderDelegate)
    , _isConverged(false)
{
}

HdGatlingRenderBuffer::~HdGatlingRenderBuffer() {}

bool HdGatlingRenderBuffer::Allocate(const GfVec3i& dimensions, HdFormat format, bool multiSampled)
{
  _Deallocate();

  auto it = s_supportedRenderBufferFormats.find(format);
  if(it == s_supportedRenderBufferFormats.end())
  {
    TF_RUNTIME_ERROR("Unsupported render buffer format!");
    return false;
  }

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
  // _buffer.resize(now_size);
  _buffer = static_cast<float*>(aligned_alloc(64, _buffer_size));

  createDesc();
  return true;
}

void HdGatlingRenderBuffer::clear(int num)
{
  memset(_buffer, num, _buffer_size);
}
size_t HdGatlingRenderBuffer::_GetBufferSize(GfVec2i const& dims, HdFormat format)
{
  // std::cout << "format:" << format << ",size:" << HdDataSizeOfFormat(format) << std::endl;
  return dims[0] * dims[1] * HdDataSizeOfFormat(format);
}

unsigned int HdGatlingRenderBuffer::GetWidth() const
{
  return _width;
}

unsigned int HdGatlingRenderBuffer::GetHeight() const
{
  return _height;
}

unsigned int HdGatlingRenderBuffer::GetDepth() const
{
  return 1u;
}

HdFormat HdGatlingRenderBuffer::GetFormat() const
{
  return _format;
}

bool HdGatlingRenderBuffer::IsMultiSampled() const
{
  return false;
}

bool HdGatlingRenderBuffer::IsConverged() const
{
  return _isConverged;
}

void HdGatlingRenderBuffer::SetConverged(bool converged)
{
  _isConverged = converged;
}

void* HdGatlingRenderBuffer::Map()
{
  // return _renderBuffer ? giGetRenderBufferMem(_renderBuffer) : nullptr;
  _isMaped = true;
  return _buffer;
}

bool HdGatlingRenderBuffer::IsMapped() const
{
  return _isMaped;
}

void HdGatlingRenderBuffer::Unmap() {}

void HdGatlingRenderBuffer::Resolve() {}

void HdGatlingRenderBuffer::_Deallocate()
{
  _width  = 0;
  _height = 0;
  _format = HdFormatInvalid;
}

void HdGatlingRenderBuffer::change_show_image()
{
  _frame_idx     = (_frame_idx + 1) % 400;
  float* rgbaImg = (float*)_buffer;

  // 清空缓冲区（填充透明黑色）
  const size_t pixelCount = _width * _height * 4;
  memset(rgbaImg, 0, pixelCount * sizeof(float));

  int width_rect  = 100;
  int height_rect = 100;
  int left, top, right, bottom;
  left   = _frame_idx * 2;
  right  = left + width_rect;
  top    = _frame_idx * 2;
  bottom = top + height_rect;

  for(int w = left; w < right; ++w)
  {
    for(int h = top; h < bottom; ++h)
    {
      int i          = (h * _width + w) * 4;
      rgbaImg[i + 0] = (float)255;
      rgbaImg[i + 1] = (float)0;
      rgbaImg[i + 2] = (float)0;
      rgbaImg[i + 3] = 255;
    }
  }
}

Hgi* HdGatlingRenderBuffer::_GetHgi()
{
  return _owner->GetHgi();
}

// 原来的 _getTextureUsage 替换为下面这个更完整的版本
HgiTextureUsage _getTextureUsage(HdFormat format, TfToken const& nameToken)
{
  HgiTextureUsage usage = 0;

  const bool isIdAov = (nameToken == HdAovTokens->primId) || (nameToken == HdAovTokens->instanceId) ||
#if PXR_VERSION >= 2408
                       (nameToken == HdAovTokens->elementId) ||  // 新版本里可能存在
#endif
                       false;

  switch(format)
  {
    case HdFormatFloat32Vec4:
    case HdFormatFloat32Vec3:
    case HdFormatUNorm8Vec4:
    case HdFormatUNorm8Vec3:
      // 典型颜色 AOV：可以作为渲染目标（如果以后想直接写进去），也允许被读取
      usage |= HgiTextureUsageBitsColorTarget | HgiTextureUsageBitsShaderRead;
      break;

    case HdFormatFloat32:
      // depth / depthStencil 场景；这里只在名称匹配时加 DepthTarget
      if(nameToken == HdAovTokens->depth || nameToken == HdAovTokens->depthStencil)
      {
        usage |= HgiTextureUsageBitsDepthTarget;
      }
      // 如果你需要在着色器里采样（少见），可再加 ShaderRead
      break;

    case HdFormatInt32:
      // object/prim/instance id 之类整数 AOV
      // 当前是 CPU 写 -> 上传，只需要 ShaderRead 让上层采样/拷贝
      usage |= HgiTextureUsageBitsShaderRead;
      // 如果以后 GPU（RT 或 compute）直接写入，可再加：
      // usage |= HgiTextureUsageBitsStorage;
      // 若想用作颜色 attachment（少见，一般不需要）可以：
      // usage |= HgiTextureUsageBitsColorTarget;
      break;

    default:
      TF_WARN("Unsupported HdFormat in _getTextureUsage: %d", format);
      break;
  }

  // 补充：某些自定义 AOV（normal、color）我们已经在上面加了 ShaderRead，
  // 这里如果需要对特定 Token 进行强制保障，可再次加：
  if(nameToken == HdAovTokens->color || nameToken == HdAovTokens->normal)
  {
    usage |= HgiTextureUsageBitsShaderRead;
  }
  if(isIdAov)
  {
    // primId/instanceId 等如果你希望 CPU 上传后还能被别的 pass 采样，确保有 ShaderRead
    usage |= HgiTextureUsageBitsShaderRead;
  }

  return usage;
}

// 在 createDesc 中（保留你原来的函数开头），替换/补充 usage 相关段落
void HdGatlingRenderBuffer::createDesc()
{
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
  _texDesc.usage = _getTextureUsage(GetFormat(), GetId().GetNameToken());

  // 初始数据（CPU -> GPU 一次性上传）
  _texDesc.initialData    = _buffer;
  _texDesc.pixelsByteSize = _buffer_size;

  // 如果之前已有纹理，先销毁
  if(_texture)
  {
    _GetHgi()->DestroyTexture(&_texture);
  }
  _texture = _GetHgi()->CreateTexture(_texDesc);
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

void HdGatlingRenderBuffer::ConvertToHgiTexture()
{
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
    HgiBlitCmdsUniquePtr blitCmds = _GetHgi()->CreateBlitCmds();
    blitCmds->PushDebugGroup("Upload CPU texels");
    blitCmds->CopyTextureCpuToGpu(copyOp);
    blitCmds->PopDebugGroup();
    _GetHgi()->SubmitCmds(blitCmds.get());
  }
  else
  {
    // Destroy old texture
    if(_texture)
    {
      _GetHgi()->DestroyTexture(&_texture);
    }
    _texDesc.initialData    = pixelData;
    _texDesc.pixelsByteSize = dataByteSize;
    _texture                = _GetHgi()->CreateTexture(_texDesc);
  }
}

HgiTextureHandle CreateHgiTextureHandle(GLuint textureId, const HgiTextureDesc& desc)
{
  HgiGLTexture* texture = HgiGLTexture::CreateTextureFromId(textureId, desc);
  if(!texture)
  {
    TF_CODING_ERROR("Failed to create HgiGLTexture");
    return HgiTextureHandle();
  }
  return HgiTextureHandle(texture, 122222);
}

#include <vector>
#include <cstdint>

void HdGatlingRenderBuffer::MakeHgiTexture(GLuint textureId)
{
  GLint realFormat;
  glGetTextureLevelParameteriv(textureId, 0, GL_TEXTURE_INTERNAL_FORMAT, &realFormat);
  assert(realFormat == GL_RGBA32F);

  GLint memoryBound;
  glGetTextureParameteriv(textureId, GL_TEXTURE_TILING_EXT, &memoryBound);
  assert(memoryBound == GL_TRUE);
#if 0
  glBindTexture(GL_TEXTURE_2D, textureId);
  std::vector<float> pixels(_width * _height * 4);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());

  _texDesc.initialData = pixels.data();
  _texture = _GetHgi()->CreateTexture(_texDesc);
#else
  _texture = CreateHgiTextureHandle(textureId, _texDesc);
#endif
}

void HdGatlingRenderBuffer::read_color_texture(GLuint textureId)
{
  GLint realFormat;
  glGetTextureLevelParameteriv(textureId, 0, GL_TEXTURE_INTERNAL_FORMAT, &realFormat);
  assert(realFormat == GL_RGBA32F);

  GLint memoryBound;
  glGetTextureParameteriv(textureId, GL_TEXTURE_TILING_EXT, &memoryBound);
  assert(memoryBound == GL_TRUE);
  glBindTexture(GL_TEXTURE_2D, textureId);
  std::vector<float> pixels(_width * _height * 4);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
  memcpy(_buffer, pixels.data(), sizeof(float) * _width * _height * 4);
}

VtValue HdGatlingRenderBuffer::GetResource(bool /*multiSampled*/) const
{
  return VtValue(_texture);
}

void HdGatlingRenderBuffer::check_format()
{
  switch(_format)
  {
    case HdFormatInt32:
      std::cout << "[renderBuffer] HdFormatInt32" << std::endl;
      break;
    case HdFormatFloat32:
      std::cout << "[renderBuffer] HdFormatFloat32" << std::endl;
      break;
    case HdFormatFloat32Vec4:
      std::cout << "[renderBuffer] HdFormatFloat32Vec4" << std::endl;
      break;
    default:
      TF_WARN("WriteIntData: unsupported format %d", int(_format));
      break;
  }
}

void HdGatlingRenderBuffer::WriteIntData(unsigned int* data, size_t count)
{
  if(_buffer == nullptr || count == 0)
    return;

  switch(_format)
  {
    case HdFormatInt32:
      assert(sizeof(int) * count == _buffer_size);
      memcpy(_buffer, data, _buffer_size);
      break;
    case HdFormatFloat32: {
      float* buf = _buffer;
      for(size_t i = 0; i < count; ++i)
        buf[i] = static_cast<float>(data[i]);
    }
    break;
    case HdFormatFloat32Vec4: {
      float* buf = _buffer;
      for(size_t i = 0; i < count; ++i)
      {
        buf[i * 4 + 0] = static_cast<float>(data[i]);
        buf[i * 4 + 1] = 0.0f;
        buf[i * 4 + 2] = 0.0f;
        buf[i * 4 + 3] = 1.0f;
      }
    }
    break;
    default:
      TF_WARN("WriteIntData: unsupported format %d", int(_format));
      break;
  }
}

void HdGatlingRenderBuffer::print()
{
  std::ofstream out("aa.txt");
  if(!out)
  {
    std::cerr << "Failed to open file for writing: " << "aa.txt" << std::endl;
    return;
  }
  auto cur_buffer = (int*)_buffer;
  if(!cur_buffer)
  {
    std::cerr << "RenderBuffer mem is nullptr!" << std::endl;
    out.close();
    return;
  }
  for(int h = 0; h < _height; ++h)
  {
    int first_w = -1;
    int last_w  = -1;
    for(int w = 0; w < _width; ++w)
    {
      int step  = h * _width + w;
      int value = cur_buffer[step];
      if(value != -1)
      {
        if(first_w == -1)
          first_w = w;
        last_w = w;
      }
    }
    if(first_w != -1 && last_w != -1)
    {
      out << "row " << h << ": first=(" << h << "," << first_w << ") last=(" << h << "," << last_w << ")" << std::endl;
    }
    else
    {
      out << "row " << h << ": no valid value" << std::endl;
    }
  }
  out.close();
  std::cout << "finish to write!" << std::endl;
}
PXR_NAMESPACE_CLOSE_SCOPE
