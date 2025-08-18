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

#pragma once

#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/imaging/hgi/texture.h>


PXR_NAMESPACE_OPEN_SCOPE

enum class GiRenderBufferFormat
{
  Int32,
  Float32,
  Float32Vec4
};
class HdGatlingRenderDelegate;
class Hgi;


class HdGatlingRenderBuffer final : public HdRenderBuffer
{
public:
  HdGatlingRenderBuffer(const SdfPath& id, HdGatlingRenderDelegate* renderDelegate);

  ~HdGatlingRenderBuffer() override;

public:
  bool Allocate(const GfVec3i& dimensions,
                HdFormat format,
                bool multiSamples) override;
  
  void clear(int num = 0);

public:
  unsigned int GetWidth() const override;
  unsigned int GetHeight() const override;
  unsigned int GetDepth() const override;

  HdFormat GetFormat() const override;

  bool IsMultiSampled() const override;
  bool IsConverged() const override;

  void SetConverged(bool converged);
  VtValue GetResource(bool multiSampled) const override;
  int GetTextureId();

public:
  void* Map() override;
  bool IsMapped() const override;

  void Unmap() override;

  void Resolve() override;
  void ConvertToHgiTexture();
  void MakeHgiTexture(GLuint textureId);

  void change_show_image();
  Hgi* _GetHgi();
  // The resolved output buffer.
  //std::vector<float> _buffer;
  float* _buffer;

  uint32_t _width;
  uint32_t _height;
  float* map_ptr = nullptr;

protected:
  void _Deallocate() override;

private:
  HdFormat _format;
  size_t   _buffer_size;

  // 测试而已
  int _frame_idx = 0;
  bool _isMaped = false;
  bool _isConverged = false;
  // Calculate the needed buffer size, given the allocation parameters.
  static size_t _GetBufferSize(GfVec2i const& dims, HdFormat format);

  HdGatlingRenderDelegate* _owner;
  Hgi* _hgi;
  HgiTextureHandle _texture;

  // 测试用，测试handlek的拷贝能否成功
  HgiTextureDesc _texDesc;
  void CopyTextureHandle();
  void createDesc();
};

PXR_NAMESPACE_CLOSE_SCOPE
