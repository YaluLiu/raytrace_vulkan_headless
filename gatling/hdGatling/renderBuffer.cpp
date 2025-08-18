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

namespace
{
  static std::map<HdFormat, GiRenderBufferFormat> s_supportedRenderBufferFormats = {
    { HdFormatInt32, GiRenderBufferFormat::Int32 },
    { HdFormatFloat32, GiRenderBufferFormat::Float32 },
    { HdFormatFloat32Vec4, GiRenderBufferFormat::Float32Vec4 }
  };
}

HdGatlingRenderBuffer::HdGatlingRenderBuffer(const SdfPath& id, HdGatlingRenderDelegate* renderDelegate):
  HdRenderBuffer(id),
  _owner(renderDelegate),
  _isConverged(false)
{
}

HdGatlingRenderBuffer::~HdGatlingRenderBuffer()
{
}

bool HdGatlingRenderBuffer::Allocate(const GfVec3i& dimensions,
                                     HdFormat format,
                                     bool multiSampled)
{
  _Deallocate();

  auto it = s_supportedRenderBufferFormats.find(format);
  if (it == s_supportedRenderBufferFormats.end())
  {
    TF_RUNTIME_ERROR("Unsupported render buffer format!");
    return false;
  }

  if (dimensions[2] != 1)
  {
    TF_RUNTIME_ERROR("3D render buffers not supported!");
    return false;
  }

  if (dimensions[0] == 0 || dimensions[1] == 0)
  {
    TF_RUNTIME_ERROR("Can't allocate empty render buffer!");
    return false;
  }

  _width = dimensions[0];
  _height = dimensions[1];
  _format = format;
  _buffer_size = _GetBufferSize(GfVec2i(_width, _height), _format);
  // _buffer.resize(now_size);
  _buffer = static_cast<float*>(aligned_alloc(64, _buffer_size));

  createDesc();
  return true;
}

void HdGatlingRenderBuffer::clear(int num)
{
  memset(_buffer,num,_buffer_size);
}
size_t
HdGatlingRenderBuffer::_GetBufferSize(GfVec2i const& dims, HdFormat format)
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

void HdGatlingRenderBuffer::Unmap()
{
}

void HdGatlingRenderBuffer::Resolve()
{
}

void HdGatlingRenderBuffer::_Deallocate()
{
  _width = 0;
  _height = 0;
  _format = HdFormatInvalid;
}

void HdGatlingRenderBuffer::change_show_image() {
    _frame_idx = (_frame_idx+1)%400;
    float* rgbaImg = (float*)_buffer;
    
    // 清空缓冲区（填充透明黑色）
    const size_t pixelCount = _width * _height * 4;
    memset(rgbaImg, 0, pixelCount * sizeof(float));
    
    int width_rect = 100;
    int height_rect = 100;
    int left,top,right,bottom;
    left =  _frame_idx*2;
    right = left + width_rect;
    top =   _frame_idx*2;
    bottom = top + height_rect;

    for (int w = left; w < right; ++w) {
      for (int h = top; h < bottom; ++h){
        int i = (h * _width + w) * 4;
        rgbaImg[i + 0] = (float)255;
        rgbaImg[i + 1] = (float)0;
        rgbaImg[i + 2] = (float)0;
        rgbaImg[i + 3] = 255;
      }
    }
}

Hgi*
HdGatlingRenderBuffer::_GetHgi()
{
  return _owner->GetHgi();
}

HgiTextureUsage _getTextureUsage(HdFormat format, TfToken const& nameToken) {
    HgiTextureUsage usage = 0;

    // 根据格式确定用途
    switch (format) {
        case HdFormatFloat32Vec4:
        case HdFormatFloat32Vec3:
        case HdFormatUNorm8Vec4:
        case HdFormatUNorm8Vec3:
            // 颜色格式，通常用作颜色目标或着色器读取
            usage |= HgiTextureUsageBitsColorTarget;
            break;
        case HdFormatFloat32:
            // 深度格式，通常用作深度目标
            if (nameToken == HdAovTokens->depth || nameToken == HdAovTokens->depthStencil) {
                usage |= HgiTextureUsageBitsDepthTarget;
            }
            break;
        default:
            TF_WARN("Unsupported HdFormat: %d", format);
            break;
    }

    // 根据名称标识添加额外用途（可选）
    if (nameToken == HdAovTokens->color || nameToken == HdAovTokens->normal) {
        usage |= HgiTextureUsageBitsShaderRead;
    }

    return usage;
}

void _ConvertRGBtoRGBA(const float* rgbValues,
                  size_t numRgbValues,
                  std::vector<float>* rgbaValues)
{
    if (numRgbValues % 3 != 0) {
        TF_WARN("Value count should be divisible by 3.");
        return;
    }

    const size_t numRgbaValues = numRgbValues * 4 / 3;

    if (rgbValues != nullptr && rgbaValues != nullptr) {
        const float *rgbValuesIt = rgbValues;
        rgbaValues->resize(numRgbaValues);
        float *rgbaValuesIt = rgbaValues->data();
        const float * const end = rgbaValuesIt + numRgbaValues;

        while (rgbaValuesIt != end) {
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
    std::vector<float> float4Data;
    if (hdFormat == HdFormatFloat32Vec3) {
        hdFormat = HdFormatFloat32Vec4;
        const size_t numValues = 3 * dim[0] * dim[1] * dim[2];
        _ConvertRGBtoRGBA(
            reinterpret_cast<const float*>(pixelData), numValues, &float4Data);
        pixelData = reinterpret_cast<const void*>(float4Data.data());
    }

    const HgiFormat bufFormat = HdxHgiConversions::GetHgiFormat(hdFormat);
    const size_t pixelByteSize = HdDataSizeOfFormat(hdFormat);
    const size_t dataByteSize = dim[0] * dim[1] * dim[2] * pixelByteSize;

    // Update the existing texture if specs are compatible. This is more
    // efficient than re-creating, because the underlying framebuffer that
    // had the old texture attached would also need to be re-created.
    if (_texture && _texture->GetDescriptor().dimensions == dim &&
            _texture->GetDescriptor().format == bufFormat) {
        HgiTextureCpuToGpuOp copyOp;
        copyOp.bufferByteSize = dataByteSize;
        copyOp.cpuSourceBuffer = pixelData;
        copyOp.gpuDestinationTexture = _texture;
        HgiBlitCmdsUniquePtr blitCmds = _GetHgi()->CreateBlitCmds();
        blitCmds->PushDebugGroup("Upload CPU texels");
        blitCmds->CopyTextureCpuToGpu(copyOp);
        blitCmds->PopDebugGroup();
        _GetHgi()->SubmitCmds(blitCmds.get());
    } else {
        // Destroy old texture
        if(_texture) {
            _GetHgi()->DestroyTexture(&_texture);
        }
        _texDesc.initialData = pixelData;
        _texDesc.pixelsByteSize = dataByteSize;
        _texture = _GetHgi()->CreateTexture(_texDesc);
    }
}

//---------------------------------------------------------------------
// 渲染结果 color专属，vulkan->opengl
//---------------------------------------------------------------------
void HdGatlingRenderBuffer::createDesc()
{
    const GfVec3i dim(GetWidth(), GetHeight(), 1);
    // std::cout << "[RenderBuff:size]" << _width << "x" << _height << std::endl;

    HdFormat hdFormat = GetFormat();
    if (hdFormat == HdFormatFloat32Vec3) {
        hdFormat = HdFormatFloat32Vec4;
    }
    const HgiFormat bufFormat = HdxHgiConversions::GetHgiFormat(hdFormat);
    _texDesc.debugName = "AovInput Texture";
    _texDesc.dimensions = dim;
    _texDesc.format = bufFormat;
    _texDesc.layerCount = 1;
    _texDesc.mipLevels = 1;
    _texDesc.sampleCount = HgiSampleCount1;
    _texDesc.usage = _getTextureUsage(GetFormat(), GetId().GetNameToken()) | HgiTextureUsageBitsShaderRead; // 纹理用途组合();

    _texDesc.initialData = _buffer;
    _texDesc.pixelsByteSize = _buffer_size;
    _texture = _GetHgi()->CreateTexture(_texDesc);
}

HgiTextureHandle CreateHgiTextureHandle(GLuint textureId, const HgiTextureDesc& desc)
{
    HgiGLTexture* texture = HgiGLTexture::CreateTextureFromId(textureId, desc);
    if (!texture) {
        TF_CODING_ERROR("Failed to create HgiGLTexture");
        return HgiTextureHandle();
    }
    return HgiTextureHandle(texture, 122222);
}

#include <vector>
#include <cstdint> // 用于uint8_t

GLuint CreateTestTexture(int width, int height) {
    // 创建纹理对象
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // 计算中间分界线
    const int midX = width / 2;
    
    // 创建像素数据缓冲区 (RGBA格式)
#if 0
    std::vector<uint8_t> pixels(width * height * 4);
    
    // 填充像素数据
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int index = (y * width + x) * 4;
            
            // 左半部分：红色 (255, 0, 0)
            // 右半部分：蓝色 (0, 0, 255)
            if (x < midX) {
                pixels[index + 0] = 255;  // R
                pixels[index + 1] = 0;    // G
                pixels[index + 2] = 0;    // B
            } else {
                pixels[index + 0] = 0;    // R
                pixels[index + 1] = 0;    // G
                pixels[index + 2] = 255;  // B
            }
            pixels[index + 3] = 255;  // A (完全不透明)
        }
    }


    
    //对应uint8_t
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, 
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
#else
    std::vector<float> pixels(width * height * 4);
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
          const int index = (y * width + x) * 4;
          if (x < midX) {
              pixels[index + 0] = 1.0f;  // R (0.0-1.0范围)
              pixels[index + 1] = 0.0f;  // G
              pixels[index + 2] = 0.0f;  // B
          } else {
              pixels[index + 0] = 0.0f;  // R
              pixels[index + 1] = 0.0f;  // G
              pixels[index + 2] = 1.0f;  // B
          }
          pixels[index + 3] = 1.0f;  // A
      }
    }  
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, 
                    GL_RGBA, GL_FLOAT, pixels.data()); 
#endif
    // 设置纹理参数和数据
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 可选：生成mipmap（如果需要）
    // glGenerateMipmap(GL_TEXTURE_2D);
    
    return textureID;
}

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
#elif 0
  GLuint id = CreateTestTexture(_width,_height);
  _texture = CreateHgiTextureHandle(id,_texDesc);
#else
  _texture = CreateHgiTextureHandle(textureId,_texDesc);
#endif
}

VtValue
HdGatlingRenderBuffer::GetResource(bool /*multiSampled*/) const
{
    return VtValue(_texture);
}

//测试用函数，没用了
void HdGatlingRenderBuffer::CopyTextureHandle()
{
  HgiGLTexture* opengl_texture = (HgiGLTexture*)_texture.Get();
  _texture = CreateHgiTextureHandle(opengl_texture->GetTextureId(),opengl_texture->GetDescriptor());
}

int HdGatlingRenderBuffer::GetTextureId()
{
  HgiGLTexture* gl_texture = (HgiGLTexture*)_texture.Get();
  return gl_texture->GetTextureId();
}
PXR_NAMESPACE_CLOSE_SCOPE
