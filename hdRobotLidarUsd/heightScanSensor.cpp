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
    TfType::Define<HdRobotLidarUsdHeightScanSensor,
        TfType::Bases< UsdGeomXformable > >();

    // Register the usd prim typename as an alias under UsdSchemaBase.
    TfType::AddAlias<UsdSchemaBase, HdRobotLidarUsdHeightScanSensor>("HeightScanSensor");
}

/* virtual */
HdRobotLidarUsdHeightScanSensor::~HdRobotLidarUsdHeightScanSensor()
{
}

/* static */
HdRobotLidarUsdHeightScanSensor
HdRobotLidarUsdHeightScanSensor::Get(const UsdStagePtr &stage, const SdfPath &path)
{
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return HdRobotLidarUsdHeightScanSensor();
    }
    return HdRobotLidarUsdHeightScanSensor(stage->GetPrimAtPath(path));
}

/* static */
HdRobotLidarUsdHeightScanSensor
HdRobotLidarUsdHeightScanSensor::Define(
    const UsdStagePtr &stage, const SdfPath &path)
{
    static TfToken usdPrimTypeName("HeightScanSensor");
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return HdRobotLidarUsdHeightScanSensor();
    }
    return HdRobotLidarUsdHeightScanSensor(
        stage->DefinePrim(path, usdPrimTypeName));
}

/* virtual */
UsdSchemaKind HdRobotLidarUsdHeightScanSensor::_GetSchemaKind() const
{
    return HdRobotLidarUsdHeightScanSensor::schemaKind;
}

/* static */
const TfType &
HdRobotLidarUsdHeightScanSensor::_GetStaticTfType()
{
    static TfType tfType = TfType::Find<HdRobotLidarUsdHeightScanSensor>();
    return tfType;
}

/* static */
bool
HdRobotLidarUsdHeightScanSensor::_IsTypedSchema()
{
    static bool isTyped = _GetStaticTfType().IsA<UsdTyped>();
    return isTyped;
}

/* virtual */
const TfType &
HdRobotLidarUsdHeightScanSensor::_GetTfType() const
{
    return _GetStaticTfType();
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::GetEnabledAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->enabled);
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::CreateEnabledAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->enabled,
                       SdfValueTypeNames->Bool,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::GetUStartAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->uStart);
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::CreateUStartAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->uStart,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::GetUEndAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->uEnd);
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::CreateUEndAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->uEnd,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::GetUStepAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->uStep);
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::CreateUStepAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->uStep,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::GetVStartAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->vStart);
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::CreateVStartAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->vStart,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::GetVEndAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->vEnd);
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::CreateVEndAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->vEnd,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::GetVStepAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->vStep);
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::CreateVStepAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->vStep,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::GetRayDirectionAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->rayDirection);
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::CreateRayDirectionAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->rayDirection,
                       SdfValueTypeNames->Float3,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::GetMaxRangeAttr() const
{
    return GetPrim().GetAttribute(HdRobotLidarUsdTokens->maxRange);
}

UsdAttribute
HdRobotLidarUsdHeightScanSensor::CreateMaxRangeAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(HdRobotLidarUsdTokens->maxRange,
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
HdRobotLidarUsdHeightScanSensor::GetSchemaAttributeNames(bool includeInherited)
{
    static TfTokenVector localNames = {
        HdRobotLidarUsdTokens->enabled,
        HdRobotLidarUsdTokens->uStart,
        HdRobotLidarUsdTokens->uEnd,
        HdRobotLidarUsdTokens->uStep,
        HdRobotLidarUsdTokens->vStart,
        HdRobotLidarUsdTokens->vEnd,
        HdRobotLidarUsdTokens->vStep,
        HdRobotLidarUsdTokens->rayDirection,
        HdRobotLidarUsdTokens->maxRange,
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
HdRobotLidarUsdHeightScanSensor::GetEnabled(UsdTimeCode time) const
{
    return _GetAttrValue(GetEnabledAttr(), time, true);
}

float
HdRobotLidarUsdHeightScanSensor::GetUStart(UsdTimeCode time) const
{
    return _GetAttrValue(GetUStartAttr(), time, -10.0f);
}

float
HdRobotLidarUsdHeightScanSensor::GetUEnd(UsdTimeCode time) const
{
    return _GetAttrValue(GetUEndAttr(), time, 10.0f);
}

float
HdRobotLidarUsdHeightScanSensor::GetUStep(UsdTimeCode time) const
{
    return _GetAttrValue(GetUStepAttr(), time, 0.1f);
}

float
HdRobotLidarUsdHeightScanSensor::GetVStart(UsdTimeCode time) const
{
    return _GetAttrValue(GetVStartAttr(), time, -10.0f);
}

float
HdRobotLidarUsdHeightScanSensor::GetVEnd(UsdTimeCode time) const
{
    return _GetAttrValue(GetVEndAttr(), time, 10.0f);
}

float
HdRobotLidarUsdHeightScanSensor::GetVStep(UsdTimeCode time) const
{
    return _GetAttrValue(GetVStepAttr(), time, 0.1f);
}

GfVec3f
HdRobotLidarUsdHeightScanSensor::GetRayDirection(UsdTimeCode time) const
{
    return _GetAttrValue(GetRayDirectionAttr(), time, GfVec3f(0.0f, 0.0f, -1.0f));
}

float
HdRobotLidarUsdHeightScanSensor::GetMaxRange(UsdTimeCode time) const
{
    return _GetAttrValue(GetMaxRangeAttr(), time, 200.0f);
}

HdRobotLidarUsdHeightScanSensor::Params
HdRobotLidarUsdHeightScanSensor::GetParams(UsdTimeCode time) const
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
