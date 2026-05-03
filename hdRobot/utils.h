#pragma once

#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/types.h>

PXR_NAMESPACE_OPEN_SCOPE

bool HdRobotIsPrimvarTypeSupported(const VtValue& value);

void HdRobotConvertVtBoolArrayToVtIntArray(VtValue& values);

enum class GiPrimvarType
{
  Float,
  Vec2,
  Vec3,
  Vec4,
  Int,
  Int2,
  Int3,
  Int4
};

enum class GiPrimvarInterpolation
{
  Constant,
  Instance,
  Uniform,
  Vertex,
  COUNT
};

struct GiPrimvarData
{
  std::string            name;
  GiPrimvarType          type;
  GiPrimvarInterpolation interpolation;
  std::vector<uint8_t>   data;
};

GiPrimvarType HdRobotGetGiPrimvarType(HdType type);

PXR_NAMESPACE_CLOSE_SCOPE
