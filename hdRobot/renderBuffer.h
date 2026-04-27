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
#include "pxr/imaging/hgiGL/texture.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderDelegate;
class Hgi;


class HdRobotRenderBuffer final : public HdRenderBuffer
{
public:
  HdRobotRenderBuffer(const SdfPath& id, HdRobotRenderDelegate* renderDelegate);

  ~HdRobotRenderBuffer() override;

public:
  bool Allocate(const GfVec3i& dimensions, HdFormat format, bool multiSamples) override;

  void clear(int num = 0);

public:
  unsigned int GetWidth() const override;
  unsigned int GetHeight() const override;
  unsigned int GetDepth() const override;

  HdFormat GetFormat() const override;

  bool IsMultiSampled() const override;
  bool IsConverged() const override;

  void SetConverged(bool converged);

public:
  void* Map() override;
  bool  IsMapped() const override;

  void Unmap() override;

  void Resolve() override;

  VtValue GetResource(bool multiSampled) const override;


  //just for test
  int  _frame_idx = 0;
  void read_texture(uint32_t textureId);
  void ConvertToHgiTexture();


  Hgi*  _GetHgi();
  void* _buffer = nullptr;

  uint32_t GetOpenGlTextureId() const
  {
    if(!_texture)
    {
      return 0;
    }
    const HgiGLTexture* srcTexture = static_cast<HgiGLTexture*>(_texture.Get());
    return srcTexture ? srcTexture->GetTextureId() : 0;
  }

protected:
  void _Deallocate() override;

private:
  HdFormat _format = HdFormatInvalid;

  size_t _buffer_size = 0;

  uint32_t         _width = 0;
  uint32_t         _height = 0;
  HgiTextureHandle _texture;

  bool _isMapped    = false;
  bool _isConverged = false;
  // Calculate the needed buffer size, given the allocation parameters.
  static size_t _GetBufferSize(GfVec2i const& dims, HdFormat format);

  HdRobotRenderDelegate* _owner = nullptr;
  Hgi*                   _hgi = nullptr;


  // 测试用，测试handlek的拷贝能否成功
  HgiTextureDesc _texDesc;
  void           createDesc();
};

PXR_NAMESPACE_CLOSE_SCOPE
