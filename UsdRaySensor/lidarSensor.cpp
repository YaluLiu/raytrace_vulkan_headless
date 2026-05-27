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
    TfType::Define<UsdRaySensorLidarSensor,
        TfType::Bases< UsdGeomXformable > >();
    
    // Register the usd prim typename as an alias under UsdSchemaBase. This
    // enables one to call
    // TfType::Find<UsdSchemaBase>().FindDerivedByName("LidarSensor")
    // to find TfType<UsdRaySensorLidarSensor>, which is how IsA queries are
    // answered.
    TfType::AddAlias<UsdSchemaBase, UsdRaySensorLidarSensor>("LidarSensor");
}

/* virtual */
UsdRaySensorLidarSensor::~UsdRaySensorLidarSensor()
{
}

/* static */
UsdRaySensorLidarSensor
UsdRaySensorLidarSensor::Get(const UsdStagePtr &stage, const SdfPath &path)
{
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return UsdRaySensorLidarSensor();
    }
    return UsdRaySensorLidarSensor(stage->GetPrimAtPath(path));
}

/* static */
UsdRaySensorLidarSensor
UsdRaySensorLidarSensor::Define(
    const UsdStagePtr &stage, const SdfPath &path)
{
    static TfToken usdPrimTypeName("LidarSensor");
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return UsdRaySensorLidarSensor();
    }
    return UsdRaySensorLidarSensor(
        stage->DefinePrim(path, usdPrimTypeName));
}

/* virtual */
UsdSchemaKind UsdRaySensorLidarSensor::_GetSchemaKind() const
{
    return UsdRaySensorLidarSensor::schemaKind;
}

/* static */
const TfType &
UsdRaySensorLidarSensor::_GetStaticTfType()
{
    static TfType tfType = TfType::Find<UsdRaySensorLidarSensor>();
    return tfType;
}

/* static */
bool 
UsdRaySensorLidarSensor::_IsTypedSchema()
{
    static bool isTyped = _GetStaticTfType().IsA<UsdTyped>();
    return isTyped;
}

/* virtual */
const TfType &
UsdRaySensorLidarSensor::_GetTfType() const
{
    return _GetStaticTfType();
}

UsdAttribute
UsdRaySensorLidarSensor::GetEnabledAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->enabled);
}

UsdAttribute
UsdRaySensorLidarSensor::CreateEnabledAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->enabled,
                       SdfValueTypeNames->Bool,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdRaySensorLidarSensor::GetAzimuthStartDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->azimuthStartDeg);
}

UsdAttribute
UsdRaySensorLidarSensor::CreateAzimuthStartDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->azimuthStartDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdRaySensorLidarSensor::GetAzimuthEndDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->azimuthEndDeg);
}

UsdAttribute
UsdRaySensorLidarSensor::CreateAzimuthEndDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->azimuthEndDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdRaySensorLidarSensor::GetAzimuthStepDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->azimuthStepDeg);
}

UsdAttribute
UsdRaySensorLidarSensor::CreateAzimuthStepDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->azimuthStepDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdRaySensorLidarSensor::GetVerticalStartDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->verticalStartDeg);
}

UsdAttribute
UsdRaySensorLidarSensor::CreateVerticalStartDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->verticalStartDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdRaySensorLidarSensor::GetVerticalEndDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->verticalEndDeg);
}

UsdAttribute
UsdRaySensorLidarSensor::CreateVerticalEndDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->verticalEndDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdRaySensorLidarSensor::GetVerticalStepDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->verticalStepDeg);
}

UsdAttribute
UsdRaySensorLidarSensor::CreateVerticalStepDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->verticalStepDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdRaySensorLidarSensor::GetMaxRangeAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->maxRange);
}

UsdAttribute
UsdRaySensorLidarSensor::CreateMaxRangeAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->maxRange,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdRaySensorLidarSensor::GetIntensityAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->intensity);
}

UsdAttribute
UsdRaySensorLidarSensor::CreateIntensityAttr(VtValue const &defaultValue, bool writeSparsely) const
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
UsdRaySensorLidarSensor::GetSchemaAttributeNames(bool includeInherited)
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
UsdRaySensorLidarSensor::GetEnabled(UsdTimeCode time) const
{
    return _GetAttrValue(GetEnabledAttr(), time, true);
}

float
UsdRaySensorLidarSensor::GetAzimuthStartDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetAzimuthStartDegAttr(), time, -90.0f);
}

float
UsdRaySensorLidarSensor::GetAzimuthEndDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetAzimuthEndDegAttr(), time, 90.0f);
}

float
UsdRaySensorLidarSensor::GetAzimuthStepDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetAzimuthStepDegAttr(), time, 0.5f);
}

float
UsdRaySensorLidarSensor::GetVerticalStartDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetVerticalStartDegAttr(), time, -2.0f);
}

float
UsdRaySensorLidarSensor::GetVerticalEndDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetVerticalEndDegAttr(), time, -20.0f);
}

float
UsdRaySensorLidarSensor::GetVerticalStepDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetVerticalStepDegAttr(), time, 1.0f);
}

float
UsdRaySensorLidarSensor::GetMaxRange(UsdTimeCode time) const
{
    return _GetAttrValue(GetMaxRangeAttr(), time, 200.0f);
}

float
UsdRaySensorLidarSensor::GetIntensity(UsdTimeCode time) const
{
    return _GetAttrValue(GetIntensityAttr(), time, 1.0f);
}

UsdRaySensorLidarSensor::Params
UsdRaySensorLidarSensor::GetParams(UsdTimeCode time) const
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
