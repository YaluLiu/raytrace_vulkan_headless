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
    TfType::Define<HeightScanSensor,
        TfType::Bases< UsdGeomXformable > >();

    // Register the usd prim typename as an alias under UsdSchemaBase.
    TfType::AddAlias<UsdSchemaBase, HeightScanSensor>("HeightScanSensor");
}

/* virtual */
HeightScanSensor::~HeightScanSensor()
{
}

/* static */
HeightScanSensor
HeightScanSensor::Get(const UsdStagePtr &stage, const SdfPath &path)
{
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return HeightScanSensor();
    }
    return HeightScanSensor(stage->GetPrimAtPath(path));
}

/* static */
HeightScanSensor
HeightScanSensor::Define(
    const UsdStagePtr &stage, const SdfPath &path)
{
    static TfToken usdPrimTypeName("HeightScanSensor");
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return HeightScanSensor();
    }
    return HeightScanSensor(
        stage->DefinePrim(path, usdPrimTypeName));
}

/* virtual */
UsdSchemaKind HeightScanSensor::_GetSchemaKind() const
{
    return HeightScanSensor::schemaKind;
}

/* static */
const TfType &
HeightScanSensor::_GetStaticTfType()
{
    static TfType tfType = TfType::Find<HeightScanSensor>();
    return tfType;
}

/* static */
bool
HeightScanSensor::_IsTypedSchema()
{
    static bool isTyped = _GetStaticTfType().IsA<UsdTyped>();
    return isTyped;
}

/* virtual */
const TfType &
HeightScanSensor::_GetTfType() const
{
    return _GetStaticTfType();
}

UsdAttribute
HeightScanSensor::GetEnabledAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->enabled);
}

UsdAttribute
HeightScanSensor::CreateEnabledAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->enabled,
                       SdfValueTypeNames->Bool,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HeightScanSensor::GetUStartAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->uStart);
}

UsdAttribute
HeightScanSensor::CreateUStartAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->uStart,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HeightScanSensor::GetUEndAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->uEnd);
}

UsdAttribute
HeightScanSensor::CreateUEndAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->uEnd,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HeightScanSensor::GetUStepAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->uStep);
}

UsdAttribute
HeightScanSensor::CreateUStepAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->uStep,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HeightScanSensor::GetVStartAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->vStart);
}

UsdAttribute
HeightScanSensor::CreateVStartAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->vStart,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HeightScanSensor::GetVEndAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->vEnd);
}

UsdAttribute
HeightScanSensor::CreateVEndAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->vEnd,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HeightScanSensor::GetVStepAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->vStep);
}

UsdAttribute
HeightScanSensor::CreateVStepAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->vStep,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HeightScanSensor::GetRayDirectionAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->rayDirection);
}

UsdAttribute
HeightScanSensor::CreateRayDirectionAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdRaySensorTokens->rayDirection,
                       SdfValueTypeNames->Float3,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HeightScanSensor::GetMaxRangeAttr() const
{
    return GetPrim().GetAttribute(UsdRaySensorTokens->maxRange);
}

UsdAttribute
HeightScanSensor::CreateMaxRangeAttr(VtValue const &defaultValue, bool writeSparsely) const
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
HeightScanSensor::GetSchemaAttributeNames(bool includeInherited)
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
HeightScanSensor::GetEnabled(UsdTimeCode time) const
{
    return _GetAttrValue(GetEnabledAttr(), time, true);
}

float
HeightScanSensor::GetUStart(UsdTimeCode time) const
{
    return _GetAttrValue(GetUStartAttr(), time, -10.0f);
}

float
HeightScanSensor::GetUEnd(UsdTimeCode time) const
{
    return _GetAttrValue(GetUEndAttr(), time, 10.0f);
}

float
HeightScanSensor::GetUStep(UsdTimeCode time) const
{
    return _GetAttrValue(GetUStepAttr(), time, 0.1f);
}

float
HeightScanSensor::GetVStart(UsdTimeCode time) const
{
    return _GetAttrValue(GetVStartAttr(), time, -10.0f);
}

float
HeightScanSensor::GetVEnd(UsdTimeCode time) const
{
    return _GetAttrValue(GetVEndAttr(), time, 10.0f);
}

float
HeightScanSensor::GetVStep(UsdTimeCode time) const
{
    return _GetAttrValue(GetVStepAttr(), time, 0.1f);
}

GfVec3f
HeightScanSensor::GetRayDirection(UsdTimeCode time) const
{
    return _GetAttrValue(GetRayDirectionAttr(), time, GfVec3f(0.0f, 0.0f, -1.0f));
}

float
HeightScanSensor::GetMaxRange(UsdTimeCode time) const
{
    return _GetAttrValue(GetMaxRangeAttr(), time, 200.0f);
}

HeightScanSensor::Params
HeightScanSensor::GetParams(UsdTimeCode time) const
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
