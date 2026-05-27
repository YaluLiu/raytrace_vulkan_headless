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
    TfType::Define<UsdGeomLidarSensor,
        TfType::Bases< UsdGeomXformable > >();
    
    // Register the usd prim typename as an alias under UsdSchemaBase. This
    // enables one to call
    // TfType::Find<UsdSchemaBase>().FindDerivedByName("LidarSensor")
    // to find TfType<UsdGeomLidarSensor>, which is how IsA queries are
    // answered.
    TfType::AddAlias<UsdSchemaBase, UsdGeomLidarSensor>("LidarSensor");
}

/* virtual */
UsdGeomLidarSensor::~UsdGeomLidarSensor()
{
}

/* static */
UsdGeomLidarSensor
UsdGeomLidarSensor::Get(const UsdStagePtr &stage, const SdfPath &path)
{
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return UsdGeomLidarSensor();
    }
    return UsdGeomLidarSensor(stage->GetPrimAtPath(path));
}

/* static */
UsdGeomLidarSensor
UsdGeomLidarSensor::Define(
    const UsdStagePtr &stage, const SdfPath &path)
{
    static TfToken usdPrimTypeName("LidarSensor");
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return UsdGeomLidarSensor();
    }
    return UsdGeomLidarSensor(
        stage->DefinePrim(path, usdPrimTypeName));
}

/* virtual */
UsdSchemaKind UsdGeomLidarSensor::_GetSchemaKind() const
{
    return UsdGeomLidarSensor::schemaKind;
}

/* static */
const TfType &
UsdGeomLidarSensor::_GetStaticTfType()
{
    static TfType tfType = TfType::Find<UsdGeomLidarSensor>();
    return tfType;
}

/* static */
bool 
UsdGeomLidarSensor::_IsTypedSchema()
{
    static bool isTyped = _GetStaticTfType().IsA<UsdTyped>();
    return isTyped;
}

/* virtual */
const TfType &
UsdGeomLidarSensor::_GetTfType() const
{
    return _GetStaticTfType();
}

UsdAttribute
UsdGeomLidarSensor::GetEnabledAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->enabled);
}

UsdAttribute
UsdGeomLidarSensor::CreateEnabledAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->enabled,
                       SdfValueTypeNames->Bool,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdGeomLidarSensor::GetAzimuthStartDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->azimuthStartDeg);
}

UsdAttribute
UsdGeomLidarSensor::CreateAzimuthStartDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->azimuthStartDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdGeomLidarSensor::GetAzimuthEndDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->azimuthEndDeg);
}

UsdAttribute
UsdGeomLidarSensor::CreateAzimuthEndDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->azimuthEndDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdGeomLidarSensor::GetAzimuthStepDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->azimuthStepDeg);
}

UsdAttribute
UsdGeomLidarSensor::CreateAzimuthStepDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->azimuthStepDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdGeomLidarSensor::GetVerticalStartDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->verticalStartDeg);
}

UsdAttribute
UsdGeomLidarSensor::CreateVerticalStartDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->verticalStartDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdGeomLidarSensor::GetVerticalEndDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->verticalEndDeg);
}

UsdAttribute
UsdGeomLidarSensor::CreateVerticalEndDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->verticalEndDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdGeomLidarSensor::GetVerticalStepDegAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->verticalStepDeg);
}

UsdAttribute
UsdGeomLidarSensor::CreateVerticalStepDegAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->verticalStepDeg,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdGeomLidarSensor::GetMaxRangeAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->maxRange);
}

UsdAttribute
UsdGeomLidarSensor::CreateMaxRangeAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->maxRange,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdGeomLidarSensor::GetIntensityAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->intensity);
}

UsdAttribute
UsdGeomLidarSensor::CreateIntensityAttr(VtValue const &defaultValue, bool writeSparsely) const
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
UsdGeomLidarSensor::GetSchemaAttributeNames(bool includeInherited)
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
UsdGeomLidarSensor::GetEnabled(UsdTimeCode time) const
{
    return _GetAttrValue(GetEnabledAttr(), time, true);
}

float
UsdGeomLidarSensor::GetAzimuthStartDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetAzimuthStartDegAttr(), time, -90.0f);
}

float
UsdGeomLidarSensor::GetAzimuthEndDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetAzimuthEndDegAttr(), time, 90.0f);
}

float
UsdGeomLidarSensor::GetAzimuthStepDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetAzimuthStepDegAttr(), time, 0.5f);
}

float
UsdGeomLidarSensor::GetVerticalStartDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetVerticalStartDegAttr(), time, -2.0f);
}

float
UsdGeomLidarSensor::GetVerticalEndDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetVerticalEndDegAttr(), time, -20.0f);
}

float
UsdGeomLidarSensor::GetVerticalStepDeg(UsdTimeCode time) const
{
    return _GetAttrValue(GetVerticalStepDegAttr(), time, 1.0f);
}

float
UsdGeomLidarSensor::GetMaxRange(UsdTimeCode time) const
{
    return _GetAttrValue(GetMaxRangeAttr(), time, 200.0f);
}

float
UsdGeomLidarSensor::GetIntensity(UsdTimeCode time) const
{
    return _GetAttrValue(GetIntensityAttr(), time, 1.0f);
}
PXR_NAMESPACE_CLOSE_SCOPE
