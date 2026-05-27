//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "./heightScanSensor.h"
#include "pxr/usd/usd/schemaRegistry.h"
#include "pxr/usd/usd/typed.h"

#include "pxr/usd/sdf/types.h"

PXR_NAMESPACE_OPEN_SCOPE

// Register the schema with the TfType system.
TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<UsdGeomHeightScanSensor,
        TfType::Bases< UsdGeomXformable > >();

    // Register the usd prim typename as an alias under UsdSchemaBase.
    TfType::AddAlias<UsdSchemaBase, UsdGeomHeightScanSensor>("HeightScanSensor");
}

/* virtual */
UsdGeomHeightScanSensor::~UsdGeomHeightScanSensor()
{
}

/* static */
UsdGeomHeightScanSensor
UsdGeomHeightScanSensor::Get(const UsdStagePtr &stage, const SdfPath &path)
{
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return UsdGeomHeightScanSensor();
    }
    return UsdGeomHeightScanSensor(stage->GetPrimAtPath(path));
}

/* static */
UsdGeomHeightScanSensor
UsdGeomHeightScanSensor::Define(
    const UsdStagePtr &stage, const SdfPath &path)
{
    static TfToken usdPrimTypeName("HeightScanSensor");
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return UsdGeomHeightScanSensor();
    }
    return UsdGeomHeightScanSensor(
        stage->DefinePrim(path, usdPrimTypeName));
}

/* virtual */
UsdSchemaKind UsdGeomHeightScanSensor::_GetSchemaKind() const
{
    return UsdGeomHeightScanSensor::schemaKind;
}

/* static */
const TfType &
UsdGeomHeightScanSensor::_GetStaticTfType()
{
    static TfType tfType = TfType::Find<UsdGeomHeightScanSensor>();
    return tfType;
}

/* static */
bool
UsdGeomHeightScanSensor::_IsTypedSchema()
{
    static bool isTyped = _GetStaticTfType().IsA<UsdTyped>();
    return isTyped;
}

/* virtual */
const TfType &
UsdGeomHeightScanSensor::_GetTfType() const
{
    return _GetStaticTfType();
}

UsdAttribute
UsdGeomHeightScanSensor::GetEnabledAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->enabled);
}

UsdAttribute
UsdGeomHeightScanSensor::CreateEnabledAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->enabled,
                       SdfValueTypeNames->Bool,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdGeomHeightScanSensor::GetUStartAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->uStart);
}

UsdAttribute
UsdGeomHeightScanSensor::CreateUStartAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->uStart,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdGeomHeightScanSensor::GetUEndAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->uEnd);
}

UsdAttribute
UsdGeomHeightScanSensor::CreateUEndAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->uEnd,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdGeomHeightScanSensor::GetUStepAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->uStep);
}

UsdAttribute
UsdGeomHeightScanSensor::CreateUStepAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->uStep,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdGeomHeightScanSensor::GetVStartAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->vStart);
}

UsdAttribute
UsdGeomHeightScanSensor::CreateVStartAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->vStart,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdGeomHeightScanSensor::GetVEndAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->vEnd);
}

UsdAttribute
UsdGeomHeightScanSensor::CreateVEndAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->vEnd,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdGeomHeightScanSensor::GetVStepAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->vStep);
}

UsdAttribute
UsdGeomHeightScanSensor::CreateVStepAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->vStep,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdGeomHeightScanSensor::GetRayDirectionAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->rayDirection);
}

UsdAttribute
UsdGeomHeightScanSensor::CreateRayDirectionAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->rayDirection,
                       SdfValueTypeNames->Float3,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdGeomHeightScanSensor::GetMaxRangeAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->maxRange);
}

UsdAttribute
UsdGeomHeightScanSensor::CreateMaxRangeAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->maxRange,
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
UsdGeomHeightScanSensor::GetSchemaAttributeNames(bool includeInherited)
{
    static TfTokenVector localNames = {
        UsdRaySensorTokens->enabled,
        UsdRaySensorTokens->uStart,
        UsdRaySensorTokens->uEnd,
        UsdRaySensorTokens->uStep,
        UsdRaySensorTokens->vStart,
        UsdRaySensorTokens->vEnd,
        UsdRaySensorTokens->vStep,
        UsdRaySensorTokens->rayDirection,
        UsdRaySensorTokens->maxRange,
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
UsdGeomHeightScanSensor::GetEnabled(UsdTimeCode time) const
{
    return _GetAttrValue(GetEnabledAttr(), time, true);
}

float
UsdGeomHeightScanSensor::GetUStart(UsdTimeCode time) const
{
    return _GetAttrValue(GetUStartAttr(), time, -10.0f);
}

float
UsdGeomHeightScanSensor::GetUEnd(UsdTimeCode time) const
{
    return _GetAttrValue(GetUEndAttr(), time, 10.0f);
}

float
UsdGeomHeightScanSensor::GetUStep(UsdTimeCode time) const
{
    return _GetAttrValue(GetUStepAttr(), time, 0.1f);
}

float
UsdGeomHeightScanSensor::GetVStart(UsdTimeCode time) const
{
    return _GetAttrValue(GetVStartAttr(), time, -10.0f);
}

float
UsdGeomHeightScanSensor::GetVEnd(UsdTimeCode time) const
{
    return _GetAttrValue(GetVEndAttr(), time, 10.0f);
}

float
UsdGeomHeightScanSensor::GetVStep(UsdTimeCode time) const
{
    return _GetAttrValue(GetVStepAttr(), time, 0.1f);
}

GfVec3f
UsdGeomHeightScanSensor::GetRayDirection(UsdTimeCode time) const
{
    return _GetAttrValue(GetRayDirectionAttr(), time, GfVec3f(0.0f, 0.0f, -1.0f));
}

float
UsdGeomHeightScanSensor::GetMaxRange(UsdTimeCode time) const
{
    return _GetAttrValue(GetMaxRangeAttr(), time, 200.0f);
}

UsdGeomHeightScanSensor::HeightScanSensorSpec
UsdGeomHeightScanSensor::GetHeightScanSensor(UsdTimeCode time) const
{
    HeightScanSensorSpec spec;
    spec.enabled = GetEnabled(time);
    spec.uStart = GetUStart(time);
    spec.uEnd = GetUEnd(time);
    spec.uStep = GetUStep(time);
    spec.vStart = GetVStart(time);
    spec.vEnd = GetVEnd(time);
    spec.vStep = GetVStep(time);
    spec.rayDirection = GetRayDirection(time);
    spec.maxRange = GetMaxRange(time);
    return spec;
}

UsdGeomHeightScanSensor::Params
UsdGeomHeightScanSensor::GetParams(UsdTimeCode time) const
{
    return GetHeightScanSensor(time);
}
PXR_NAMESPACE_CLOSE_SCOPE
