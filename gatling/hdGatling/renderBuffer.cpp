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
  _buffer      = static_cast<void*>(aligned_alloc(64, _buffer_size));

  createDesc();
  return true;
}

void HdGatlingRenderBuffer::clear(int num)
{
  memset(_buffer, num, _buffer_size);
}
size_t HdGatlingRenderBuffer::_GetBufferSize(GfVec2i const& dims, HdFormat format)
{
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
  if(_isIdAov)
  {
    read_object_texture(get_OpenGL_Texture_id());
  }
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
      // // 如果以后 GPU（RT 或 compute）直接写入，可再加：
      // usage |= HgiTextureUsageBitsStorage;
      // 若想用作颜色 attachment（少见，一般不需要）可以：
      usage |= HgiTextureUsageBitsColorTarget;
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
  TfToken nameToken = GetId().GetNameToken();
  _texDesc.usage    = _getTextureUsage(GetFormat(), nameToken);

  // 初始数据（CPU -> GPU 一次性上传）
  _texDesc.initialData    = _buffer;
  _texDesc.pixelsByteSize = _buffer_size;

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
    // _buffer will changed when resize
    _texDesc.initialData    = _buffer;
    _texDesc.pixelsByteSize = _buffer_size;
    _texture                = _GetHgi()->CreateTexture(_texDesc);
  }
}

VtValue HdGatlingRenderBuffer::GetResource(bool /*multiSampled*/) const
{
  return VtValue(_texture);
}

//----------------------------------------------------------------------------------------------------------
// test function,for test and learn how hgi works
//----------------------------------------------------------------------------------------------------------
#include <vector>
#include <cstdint>

HgiTextureHandle CreateHgiTextureHandle(GLuint textureId, int unique_id, const HgiTextureDesc& desc)
{
  HgiGLTexture* texture = HgiGLTexture::CreateTextureFromId(textureId, desc);
  if(!texture)
  {
    TF_CODING_ERROR("Failed to create HgiGLTexture");
    return HgiTextureHandle();
  }
  return HgiTextureHandle(texture, unique_id);
}

void HdGatlingRenderBuffer::MakeHgiTexture(GLuint textureId, int unique_id)
{
#if 0
  glBindTexture(GL_TEXTURE_2D, textureId);
  std::vector<float> pixels(_width * _height * 4);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
  _texture = _GetHgi()->CreateTexture(_texDesc);
#else
  _texture = CreateHgiTextureHandle(textureId, unique_id, _texDesc);
#endif
}

void HdGatlingRenderBuffer::read_color_texture(GLuint textureId)
{
  glBindTexture(GL_TEXTURE_2D, textureId);
  std::vector<float> pixels(_width * _height * 4);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
  memcpy(_buffer, pixels.data(), sizeof(float) * _width * _height * 4);
}

void HdGatlingRenderBuffer::read_object_texture(GLuint textureId)
{
  glBindTexture(GL_TEXTURE_2D, textureId);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_INT, _buffer);
  // std::vector<int> pixels(_width * _height);
  // glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_INT, pixels.data());
  // memcpy(_buffer, pixels.data(), sizeof(int) * _width * _height);
}


void HdGatlingRenderBuffer::make_test_color()
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

void HdGatlingRenderBuffer::make_test_object_id()
{
  std::vector<int> pixels(_width * _height, -1);

  int left, top, right, bottom;
  left   = 0;
  right  = _width;
  top    = 0;
  bottom = top + _height / 2;

  for(int h = 0; h < _height; ++h)
  {
    for(int w = 0; w < _width; ++w)
    {
      int index = h * _width + w;
      int value = -1;
      if(h * 2 > _height)
      {
        value += 2;
      }
      if(w * 2 > _width)
      {
        value += 1;
      }
      pixels[index] = value;
    }
  }

  memcpy(_buffer, pixels.data(), sizeof(int) * _width * _height);
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
  for(int h = 0; h < _height; h += 25)
  {
    for(int w = 0; w < _width; w += 25)
    {
      int step  = h * _width + w;
      int value = cur_buffer[step];
      if(value != -1)
      {
        out << "*";
      }
      else
      {
        out << ".";
      }
    }
    out << std::endl;
  }
  out.close();
  std::cout << "finish to write!" << std::endl;
}

PXR_NAMESPACE_CLOSE_SCOPE
