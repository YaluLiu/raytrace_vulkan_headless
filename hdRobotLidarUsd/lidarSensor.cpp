//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "./lidarSensor.h"
#include "pxr/usd/usd/schemaRegistry.h"
#include "pxr/usd/usd/typed.h"

#include "pxr/usd/sdf/types.h"
#include "pxr/usd/sdf/assetPath.h"

PXR_NAMESPACE_OPEN_SCOPE

// Register the schema with the TfType system.
TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<HdRobotLidarUsdLidarSensor,
        TfType::Bases< UsdGeomXformable > >();
    
    // Register the usd prim typename as an alias under UsdSchemaBase. This
    // enables one to call
    // TfType::Find<UsdSchemaBase>().FindDerivedByName("LidarSensor")
    // to find TfType<HdRobotLidarUsdLidarSensor>, which is how IsA queries are
    // answered.
    TfType::AddAlias<UsdSchemaBase, HdRobotLidarUsdLidarSensor>("LidarSensor");
}

/* virtual */
HdRobotLidarUsdLidarSensor::~HdRobotLidarUsdLidarSensor()
{
}

/* static */
HdRobotLidarUsdLidarSensor
HdRobotLidarUsdLidarSensor::Get(const UsdStagePtr &stage, const SdfPath &path)
{
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return HdRobotLidarUsdLidarSensor();
    }
    return HdRobotLidarUsdLidarSensor(stage->GetPrimAtPath(path));
}

/* static */
HdRobotLidarUsdLidarSensor
HdRobotLidarUsdLidarSensor::Define(
    const UsdStagePtr &stage, const SdfPath &path)
{
    static TfToken usdPrimTypeName("LidarSensor");
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return HdRobotLidarUsdLidarSensor();
    }
    return HdRobotLidarUsdLidarSensor(
        stage->DefinePrim(path, usdPrimTypeName));
}

/* virtual */
UsdSchemaKind HdRobotLidarUsdLidarSensor::_GetSchemaKind() const
{
    return HdRobotLidarUsdLidarSensor::schemaKind;
}

/* static */
const TfType &
HdRobotLidarUsdLidarSensor::_GetStaticTfType()
{
    static TfType tfType = TfType::Find<HdRobotLidarUsdLidarSensor>();
    return tfType;
}

/* static */
bool 
HdRobotLidarUsdLidarSensor::_IsTypedSchema()
{
    static bool isTyped = _GetStaticTfType().IsA<UsdTyped>();
    return isTyped;
}

/* virtual */
const TfType &
HdRobotLidarUsdLidarSensor::_GetTfType() const
{
    return _GetStaticTfType();
}

UsdAttribute
HdRobotLidarUsdLidarSensor::GetEnabledAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->enabled);
}

UsdAttribute
HdRobotLidarUsdLidarSensor::CreateEnabledAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->enabled,
                       SdfValueTypeNames->Bool,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HdRobotLidarUsdLidarSensor::GetAzimuthStartDegAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->azimuthStartDeg);
}

UsdAttribute
HdRobotLidarUsdLidarSensor::CreateAzimuthStartDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->azimuthStartDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HdRobotLidarUsdLidarSensor::GetAzimuthEndDegAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->azimuthEndDeg);
}

UsdAttribute
HdRobotLidarUsdLidarSensor::CreateAzimuthEndDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->azimuthEndDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HdRobotLidarUsdLidarSensor::GetAzimuthStepDegAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->azimuthStepDeg);
}

UsdAttribute
HdRobotLidarUsdLidarSensor::CreateAzimuthStepDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->azimuthStepDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HdRobotLidarUsdLidarSensor::GetVerticalStartDegAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->verticalStartDeg);
}

UsdAttribute
HdRobotLidarUsdLidarSensor::CreateVerticalStartDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->verticalStartDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HdRobotLidarUsdLidarSensor::GetVerticalEndDegAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->verticalEndDeg);
}

UsdAttribute
HdRobotLidarUsdLidarSensor::CreateVerticalEndDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->verticalEndDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HdRobotLidarUsdLidarSensor::GetVerticalStepDegAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->verticalStepDeg);
}

UsdAttribute
HdRobotLidarUsdLidarSensor::CreateVerticalStepDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->verticalStepDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HdRobotLidarUsdLidarSensor::GetMaxRangeAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->maxRange);
}

UsdAttribute
HdRobotLidarUsdLidarSensor::CreateMaxRangeAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->maxRange,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HdRobotLidarUsdLidarSensor::GetIntensityAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->intensity);
}

UsdAttribute
HdRobotLidarUsdLidarSensor::CreateIntensityAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->intensity,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

namespace {
static inline TfTokenVector
_ConcatenateAttributeNames(const TfTokenVector& left,const TfTokenVector& right)
{
    TfTokenVector result;
    result.reserve(left.size() + right.size());
    result.insert(result.end(), left.begin(), left.end());
    result.insert(result.end(), right.begin(), right.end());
    return result;
}
}

/*static*/
const TfTokenVector&
HdRobotLidarUsdLidarSensor::GetSchemaAttributeNames(bool includeInherited)
{
    static TfTokenVector localNames = {
        HdRobotLidarUsdTokens->enabled,
        HdRobotLidarUsdTokens->azimuthStartDeg,
        HdRobotLidarUsdTokens->azimuthEndDeg,
        HdRobotLidarUsdTokens->azimuthStepDeg,
        HdRobotLidarUsdTokens->verticalStartDeg,
        HdRobotLidarUsdTokens->verticalEndDeg,
        HdRobotLidarUsdTokens->verticalStepDeg,
        HdRobotLidarUsdTokens->maxRange,
        HdRobotLidarUsdTokens->intensity,
    };
    static TfTokenVector allNames =
        _ConcatenateAttributeNames(
            UsdGeomXformable::GetSchemaAttributeNames(true),
            localNames);

    if (includeInherited)
        return allNames;
    else
        return localNames;
}

PXR_NAMESPACE_CLOSE_SCOPE

// ===================================================================== //
// Feel free to add custom code below this line. It will be preserved by
// the code generator.
//
// Just remember to wrap code in the appropriate delimiters:
// 'PXR_NAMESPACE_OPEN_SCOPE', 'PXR_NAMESPACE_CLOSE_SCOPE'.
// ===================================================================== //
// --(BEGIN CUSTOM CODE)--
PXR_NAMESPACE_OPEN_SCOPE
namespace {
template <typename T>
T _GetAttrValue(const UsdAttribute& attr, UsdTimeCode time, T fallback)
{
    T value = fallback;
    return attr && attr.Get(&value, time) ? value : fallback;
}
} // namespace

bool
HdRobotLidarUsdLidarSensor::GetEnabled(UsdTimeCode time) const
{
    return _GetAttrValue(GetEnabledAttr(), time, true);
}

float
HdRobotLidarUsdLidarSensor::GetAzimuthStartDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetAzimuthStartDegAttr(), time, -90.0f);
}

float
HdRobotLidarUsdLidarSensor::GetAzimuthEndDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetAzimuthEndDegAttr(), time, 90.0f);
}

float
HdRobotLidarUsdLidarSensor::GetAzimuthStepDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetAzimuthStepDegAttr(), time, 0.5f);
}

float
HdRobotLidarUsdLidarSensor::GetVerticalStartDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetVerticalStartDegAttr(), time, -2.0f);
}

float
HdRobotLidarUsdLidarSensor::GetVerticalEndDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetVerticalEndDegAttr(), time, -20.0f);
}

float
HdRobotLidarUsdLidarSensor::GetVerticalStepDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetVerticalStepDegAttr(), time, 1.0f);
}

float
HdRobotLidarUsdLidarSensor::GetMaxRange(UsdTimeCode time) const
{
    return _GetAttrValue(GetMaxRangeAttr(), time, 200.0f);
}

float
HdRobotLidarUsdLidarSensor::GetIntensity(UsdTimeCode time) const
{
    return _GetAttrValue(GetIntensityAttr(), time, 1.0f);
}

HdRobotLidarUsdLidarSensor::Params
HdRobotLidarUsdLidarSensor::GetParams(UsdTimeCode time) const
{
    Params params;
    params.enabled = GetEnabled(time);
    params.azimuthStartDeg = GetAzimuthStartDeg(time);
    params.azimuthEndDeg = GetAzimuthEndDeg(time);
    params.azimuthStepDeg = GetAzimuthStepDeg(time);
    params.verticalStartDeg = GetVerticalStartDeg(time);
    params.verticalEndDeg = GetVerticalEndDeg(time);
    params.verticalStepDeg = GetVerticalStepDeg(time);
    params.maxRange = GetMaxRange(time);
    params.intensity = GetIntensity(time);
    return params;
}
PXR_NAMESPACE_CLOSE_SCOPE
