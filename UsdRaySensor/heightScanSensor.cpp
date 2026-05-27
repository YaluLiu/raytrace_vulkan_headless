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
    TfType::Define<UsdRaySensorHeightScanSensor,
        TfType::Bases< UsdGeomXformable > >();

    // Register the usd prim typename as an alias under UsdSchemaBase.
    TfType::AddAlias<UsdSchemaBase, UsdRaySensorHeightScanSensor>("HeightScanSensor");
}

/* virtual */
UsdRaySensorHeightScanSensor::~UsdRaySensorHeightScanSensor()
{
}

/* static */
UsdRaySensorHeightScanSensor
UsdRaySensorHeightScanSensor::Get(const UsdStagePtr &stage, const SdfPath &path)
{
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return UsdRaySensorHeightScanSensor();
    }
    return UsdRaySensorHeightScanSensor(stage->GetPrimAtPath(path));
}

/* static */
UsdRaySensorHeightScanSensor
UsdRaySensorHeightScanSensor::Define(
    const UsdStagePtr &stage, const SdfPath &path)
{
    static TfToken usdPrimTypeName("HeightScanSensor");
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return UsdRaySensorHeightScanSensor();
    }
    return UsdRaySensorHeightScanSensor(
        stage->DefinePrim(path, usdPrimTypeName));
}

/* virtual */
UsdSchemaKind UsdRaySensorHeightScanSensor::_GetSchemaKind() const
{
    return UsdRaySensorHeightScanSensor::schemaKind;
}

/* static */
const TfType &
UsdRaySensorHeightScanSensor::_GetStaticTfType()
{
    static TfType tfType = TfType::Find<UsdRaySensorHeightScanSensor>();
    return tfType;
}

/* static */
bool
UsdRaySensorHeightScanSensor::_IsTypedSchema()
{
    static bool isTyped = _GetStaticTfType().IsA<UsdTyped>();
    return isTyped;
}

/* virtual */
const TfType &
UsdRaySensorHeightScanSensor::_GetTfType() const
{
    return _GetStaticTfType();
}

UsdAttribute
UsdRaySensorHeightScanSensor::GetEnabledAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->enabled);
}

UsdAttribute
UsdRaySensorHeightScanSensor::CreateEnabledAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->enabled,
                       SdfValueTypeNames->Bool,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdRaySensorHeightScanSensor::GetUStartAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->uStart);
}

UsdAttribute
UsdRaySensorHeightScanSensor::CreateUStartAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->uStart,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdRaySensorHeightScanSensor::GetUEndAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->uEnd);
}

UsdAttribute
UsdRaySensorHeightScanSensor::CreateUEndAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->uEnd,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdRaySensorHeightScanSensor::GetUStepAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->uStep);
}

UsdAttribute
UsdRaySensorHeightScanSensor::CreateUStepAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->uStep,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdRaySensorHeightScanSensor::GetVStartAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->vStart);
}

UsdAttribute
UsdRaySensorHeightScanSensor::CreateVStartAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->vStart,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdRaySensorHeightScanSensor::GetVEndAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->vEnd);
}

UsdAttribute
UsdRaySensorHeightScanSensor::CreateVEndAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->vEnd,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdRaySensorHeightScanSensor::GetVStepAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->vStep);
}

UsdAttribute
UsdRaySensorHeightScanSensor::CreateVStepAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->vStep,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdRaySensorHeightScanSensor::GetRayDirectionAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->rayDirection);
}

UsdAttribute
UsdRaySensorHeightScanSensor::CreateRayDirectionAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->rayDirection,
                       SdfValueTypeNames->Float3,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdRaySensorHeightScanSensor::GetMaxRangeAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->maxRange);
}

UsdAttribute
UsdRaySensorHeightScanSensor::CreateMaxRangeAttr(VtValue const &defaultValue, bool writeSparsely) const
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
UsdRaySensorHeightScanSensor::GetSchemaAttributeNames(bool includeInherited)
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
UsdRaySensorHeightScanSensor::GetEnabled(UsdTimeCode time) const
{
    return _GetAttrValue(GetEnabledAttr(), time, true);
}

float
UsdRaySensorHeightScanSensor::GetUStart(UsdTimeCode time) const
{
    return _GetAttrValue(GetUStartAttr(), time, -10.0f);
}

float
UsdRaySensorHeightScanSensor::GetUEnd(UsdTimeCode time) const
{
    return _GetAttrValue(GetUEndAttr(), time, 10.0f);
}

float
UsdRaySensorHeightScanSensor::GetUStep(UsdTimeCode time) const
{
    return _GetAttrValue(GetUStepAttr(), time, 0.1f);
}

float
UsdRaySensorHeightScanSensor::GetVStart(UsdTimeCode time) const
{
    return _GetAttrValue(GetVStartAttr(), time, -10.0f);
}

float
UsdRaySensorHeightScanSensor::GetVEnd(UsdTimeCode time) const
{
    return _GetAttrValue(GetVEndAttr(), time, 10.0f);
}

float
UsdRaySensorHeightScanSensor::GetVStep(UsdTimeCode time) const
{
    return _GetAttrValue(GetVStepAttr(), time, 0.1f);
}

GfVec3f
UsdRaySensorHeightScanSensor::GetRayDirection(UsdTimeCode time) const
{
    return _GetAttrValue(GetRayDirectionAttr(), time, GfVec3f(0.0f, 0.0f, -1.0f));
}

float
UsdRaySensorHeightScanSensor::GetMaxRange(UsdTimeCode time) const
{
    return _GetAttrValue(GetMaxRangeAttr(), time, 200.0f);
}

UsdRaySensorHeightScanSensor::Params
UsdRaySensorHeightScanSensor::GetParams(UsdTimeCode time) const
{
    Params params;
    params.enabled = GetEnabled(time);
    params.uStart = GetUStart(time);
    params.uEnd = GetUEnd(time);
    params.uStep = GetUStep(time);
    params.vStart = GetVStart(time);
    params.vEnd = GetVEnd(time);
    params.vStep = GetVStep(time);
    params.rayDirection = GetRayDirection(time);
    params.maxRange = GetMaxRange(time);
    return params;
}
PXR_NAMESPACE_CLOSE_SCOPE
