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
    TfType::Define<LidarSensor,
        TfType::Bases< UsdGeomXformable > >();
    
    // Register the usd prim typename as an alias under UsdSchemaBase. This
    // enables one to call
    // TfType::Find<UsdSchemaBase>().FindDerivedByName("LidarSensor")
    // to find TfType<LidarSensor>, which is how IsA queries are
    // answered.
    TfType::AddAlias<UsdSchemaBase, LidarSensor>("LidarSensor");
}

/* virtual */
LidarSensor::~LidarSensor()
{
}

/* static */
LidarSensor
LidarSensor::Get(const UsdStagePtr &stage, const SdfPath &path)
{
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return LidarSensor();
    }
    return LidarSensor(stage->GetPrimAtPath(path));
}

/* static */
LidarSensor
LidarSensor::Define(
    const UsdStagePtr &stage, const SdfPath &path)
{
    static TfToken usdPrimTypeName("LidarSensor");
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return LidarSensor();
    }
    return LidarSensor(
        stage->DefinePrim(path, usdPrimTypeName));
}

/* virtual */
UsdSchemaKind LidarSensor::_GetSchemaKind() const
{
    return LidarSensor::schemaKind;
}

/* static */
const TfType &
LidarSensor::_GetStaticTfType()
{
    static TfType tfType = TfType::Find<LidarSensor>();
    return tfType;
}

/* static */
bool 
LidarSensor::_IsTypedSchema()
{
    static bool isTyped = _GetStaticTfType().IsA<UsdTyped>();
    return isTyped;
}

/* virtual */
const TfType &
LidarSensor::_GetTfType() const
{
    return _GetStaticTfType();
}

UsdAttribute
LidarSensor::GetEnabledAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->enabled);
}

UsdAttribute
LidarSensor::CreateEnabledAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->enabled,
                       SdfValueTypeNames->Bool,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
LidarSensor::GetAzimuthStartDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->azimuthStartDeg);
}

UsdAttribute
LidarSensor::CreateAzimuthStartDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->azimuthStartDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
LidarSensor::GetAzimuthEndDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->azimuthEndDeg);
}

UsdAttribute
LidarSensor::CreateAzimuthEndDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->azimuthEndDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
LidarSensor::GetAzimuthStepDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->azimuthStepDeg);
}

UsdAttribute
LidarSensor::CreateAzimuthStepDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->azimuthStepDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
LidarSensor::GetVerticalStartDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->verticalStartDeg);
}

UsdAttribute
LidarSensor::CreateVerticalStartDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->verticalStartDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
LidarSensor::GetVerticalEndDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->verticalEndDeg);
}

UsdAttribute
LidarSensor::CreateVerticalEndDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->verticalEndDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
LidarSensor::GetVerticalStepDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->verticalStepDeg);
}

UsdAttribute
LidarSensor::CreateVerticalStepDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->verticalStepDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
LidarSensor::GetMaxRangeAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->maxRange);
}

UsdAttribute
LidarSensor::CreateMaxRangeAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->maxRange,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
LidarSensor::GetIntensityAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->intensity);
}

UsdAttribute
LidarSensor::CreateIntensityAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->intensity,
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
LidarSensor::GetSchemaAttributeNames(bool includeInherited)
{
    static TfTokenVector localNames = {
        UsdRaySensorTokens->enabled,
        UsdRaySensorTokens->azimuthStartDeg,
        UsdRaySensorTokens->azimuthEndDeg,
        UsdRaySensorTokens->azimuthStepDeg,
        UsdRaySensorTokens->verticalStartDeg,
        UsdRaySensorTokens->verticalEndDeg,
        UsdRaySensorTokens->verticalStepDeg,
        UsdRaySensorTokens->maxRange,
        UsdRaySensorTokens->intensity,
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
LidarSensor::GetEnabled(UsdTimeCode time) const
{
    return _GetAttrValue(GetEnabledAttr(), time, true);
}

float
LidarSensor::GetAzimuthStartDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetAzimuthStartDegAttr(), time, -90.0f);
}

float
LidarSensor::GetAzimuthEndDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetAzimuthEndDegAttr(), time, 90.0f);
}

float
LidarSensor::GetAzimuthStepDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetAzimuthStepDegAttr(), time, 0.5f);
}

float
LidarSensor::GetVerticalStartDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetVerticalStartDegAttr(), time, -2.0f);
}

float
LidarSensor::GetVerticalEndDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetVerticalEndDegAttr(), time, -20.0f);
}

float
LidarSensor::GetVerticalStepDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetVerticalStepDegAttr(), time, 1.0f);
}

float
LidarSensor::GetMaxRange(UsdTimeCode time) const
{
    return _GetAttrValue(GetMaxRangeAttr(), time, 200.0f);
}

float
LidarSensor::GetIntensity(UsdTimeCode time) const
{
    return _GetAttrValue(GetIntensityAttr(), time, 1.0f);
}

LidarSensor::Params
LidarSensor::GetParams(UsdTimeCode time) const
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
