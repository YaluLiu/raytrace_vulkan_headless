#include "utils.h"

#include <pxr/base/vt/array.h>
#include <pxr/base/vt/types.h>

PXR_NAMESPACE_OPEN_SCOPE

bool HdRobotIsPrimvarTypeSupported(const VtValue& value)
{
  return value.IsHolding<VtVec4fArray>() || value.IsHolding<VtVec3fArray>() || value.IsHolding<VtVec2fArray>()
         || value.IsHolding<VtFloatArray>() || value.IsHolding<VtVec4iArray>() || value.IsHolding<VtVec3iArray>()
         || value.IsHolding<VtVec2iArray>() || value.IsHolding<VtBoolArray>() || value.IsHolding<VtIntArray>();
}

void HdRobotConvertVtBoolArrayToVtIntArray(VtValue& values)
{
  auto       boolArray = values.Get<VtBoolArray>();
  VtIntArray intArray(boolArray.size());

  for(int i = 0; i < boolArray.size(); i++)
  {
    intArray[i] = boolArray[i] ? 1 : 0;
  }

  values = std::move(intArray);
}


GiPrimvarType HdRobotGetGiPrimvarType(HdType type)
{
  switch(type)
  {
    case HdTypeFloat:
      return GiPrimvarType::Float;
    case HdTypeFloatVec2:
      return GiPrimvarType::Vec2;
    case HdTypeFloatVec3:
      return GiPrimvarType::Vec3;
    case HdTypeFloatVec4:
      return GiPrimvarType::Vec4;
    case HdTypeInt32:
      return GiPrimvarType::Int;
    case HdTypeInt32Vec2:
      return GiPrimvarType::Int2;
    case HdTypeInt32Vec3:
      return GiPrimvarType::Int3;
    case HdTypeInt32Vec4:
      return GiPrimvarType::Int4;
    default:
      TF_CODING_ERROR("primvar type %i unsupported", int(type));
      return GiPrimvarType::Float;
  }
}
PXR_NAMESPACE_CLOSE_SCOPE
