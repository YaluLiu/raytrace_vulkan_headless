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

#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/types.h>

PXR_NAMESPACE_OPEN_SCOPE

bool HdGatlingIsPrimvarTypeSupported(const VtValue& value);

void HdGatlingConvertVtBoolArrayToVtIntArray(VtValue& values);

enum class GiPrimvarType
{
  Float, Vec2, Vec3, Vec4, Int, Int2, Int3, Int4
};

enum class GiPrimvarInterpolation
{
  Constant, Instance, Uniform, Vertex, COUNT
};

struct GiPrimvarData
{
  std::string name;
  GiPrimvarType type;
  GiPrimvarInterpolation interpolation;
  std::vector<uint8_t> data;
};

GiPrimvarType HdGatlingGetGiPrimvarType(HdType type);

PXR_NAMESPACE_CLOSE_SCOPE
