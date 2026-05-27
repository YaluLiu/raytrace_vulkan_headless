//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef HDROBOTLIDARUSD_GENERATED_HEIGHTSCANSENSOR_H
#define HDROBOTLIDARUSD_GENERATED_HEIGHTSCANSENSOR_H

/// \file hdRobotLidarUsd/heightScanSensor.h

#include "pxr/pxr.h"
#include "./api.h"
#include "pxr/usd/usdGeom/xformable.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usd/timeCode.h"
#include "./tokens.h"

#include "pxr/base/vt/value.h"

#include "pxr/base/gf/vec3f.h"

#include "pxr/base/tf/token.h"
#include "pxr/base/tf/type.h"

PXR_NAMESPACE_OPEN_SCOPE

// -------------------------------------------------------------------------- //
// HEIGHTSCANSENSOR                                                          //
// -------------------------------------------------------------------------- //

/// \class HdRobotLidarUsdHeightScanSensor
///
/// Concrete transformable height scan sensor schema consumed by hdRobot.
///
class HdRobotLidarUsdHeightScanSensor : public UsdGeomXformable
{
public:
    /// Compile time constant representing what kind of schema this class is.
    ///
    /// \sa UsdSchemaKind
    static const UsdSchemaKind schemaKind = UsdSchemaKind::ConcreteTyped;

    /// Construct a HdRobotLidarUsdHeightScanSensor on UsdPrim \p prim .
    explicit HdRobotLidarUsdHeightScanSensor(const UsdPrim& prim=UsdPrim())
        : UsdGeomXformable(prim)
    {
    }

    /// Construct a HdRobotLidarUsdHeightScanSensor on the prim held by
    /// \p schemaObj .
    explicit HdRobotLidarUsdHeightScanSensor(const UsdSchemaBase& schemaObj)
        : UsdGeomXformable(schemaObj)
    {
    }

    /// Destructor.
    HDROBOTLIDARUSD_API
    virtual ~HdRobotLidarUsdHeightScanSensor();

    /// Return a vector of names of all pre-declared attributes for this schema
    /// class and all its ancestor classes.
    HDROBOTLIDARUSD_API
    static const TfTokenVector &
    GetSchemaAttributeNames(bool includeInherited=true);

    /// Return a HdRobotLidarUsdHeightScanSensor holding the prim adhering to
    /// this schema at \p path on \p stage.
    HDROBOTLIDARUSD_API
    static HdRobotLidarUsdHeightScanSensor
    Get(const UsdStagePtr &stage, const SdfPath &path);

    /// Attempt to ensure a UsdPrim adhering to this schema at \p path is
    /// defined on this stage.
    HDROBOTLIDARUSD_API
    static HdRobotLidarUsdHeightScanSensor
    Define(const UsdStagePtr &stage, const SdfPath &path);

protected:
    /// Returns the kind of schema this class belongs to.
    ///
    /// \sa UsdSchemaKind
    HDROBOTLIDARUSD_API
    UsdSchemaKind _GetSchemaKind() const override;

private:
    friend class UsdSchemaRegistry;
    HDROBOTLIDARUSD_API
    static const TfType &_GetStaticTfType();

    static bool _IsTypedSchema();

    HDROBOTLIDARUSD_API
    const TfType &_GetTfType() const override;

public:
    // --------------------------------------------------------------------- //
    // ENABLED
    // --------------------------------------------------------------------- //
    HDROBOTLIDARUSD_API
    UsdAttribute GetEnabledAttr() const;

    HDROBOTLIDARUSD_API
    UsdAttribute CreateEnabledAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // USTART
    // --------------------------------------------------------------------- //
    HDROBOTLIDARUSD_API
    UsdAttribute GetUStartAttr() const;

    HDROBOTLIDARUSD_API
    UsdAttribute CreateUStartAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // UEND
    // --------------------------------------------------------------------- //
    HDROBOTLIDARUSD_API
    UsdAttribute GetUEndAttr() const;

    HDROBOTLIDARUSD_API
    UsdAttribute CreateUEndAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // USTEP
    // --------------------------------------------------------------------- //
    HDROBOTLIDARUSD_API
    UsdAttribute GetUStepAttr() const;

    HDROBOTLIDARUSD_API
    UsdAttribute CreateUStepAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // VSTART
    // --------------------------------------------------------------------- //
    HDROBOTLIDARUSD_API
    UsdAttribute GetVStartAttr() const;

    HDROBOTLIDARUSD_API
    UsdAttribute CreateVStartAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // VEND
    // --------------------------------------------------------------------- //
    HDROBOTLIDARUSD_API
    UsdAttribute GetVEndAttr() const;

    HDROBOTLIDARUSD_API
    UsdAttribute CreateVEndAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // VSTEP
    // --------------------------------------------------------------------- //
    HDROBOTLIDARUSD_API
    UsdAttribute GetVStepAttr() const;

    HDROBOTLIDARUSD_API
    UsdAttribute CreateVStepAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // RAYDIRECTION
    // --------------------------------------------------------------------- //
    HDROBOTLIDARUSD_API
    UsdAttribute GetRayDirectionAttr() const;

    HDROBOTLIDARUSD_API
    UsdAttribute CreateRayDirectionAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // MAXRANGE
    // --------------------------------------------------------------------- //
    HDROBOTLIDARUSD_API
    UsdAttribute GetMaxRangeAttr() const;

    HDROBOTLIDARUSD_API
    UsdAttribute CreateMaxRangeAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // ===================================================================== //
    // Feel free to add custom code below this line, it will be preserved by
    // the code generator.
    //
    // Just remember to:
    //  - Close the class declaration with };
    //  - Close the namespace with PXR_NAMESPACE_CLOSE_SCOPE
    //  - Close the include guard with #endif
    // ===================================================================== //
    // --(BEGIN CUSTOM CODE)--
    struct Params
    {
        bool enabled = true;
        float uStart = -10.0f;
        float uEnd = 10.0f;
        float uStep = 0.1f;
        float vStart = -10.0f;
        float vEnd = 10.0f;
        float vStep = 0.1f;
        GfVec3f rayDirection = GfVec3f(0.0f, 0.0f, -1.0f);
        float maxRange = 200.0f;

        bool GetEnabled() const { return enabled; }
        float GetUStart() const { return uStart; }
        float GetUEnd() const { return uEnd; }
        float GetUStep() const { return uStep; }
        float GetVStart() const { return vStart; }
        float GetVEnd() const { return vEnd; }
        float GetVStep() const { return vStep; }
        GfVec3f GetRayDirection() const { return rayDirection; }
        float GetMaxRange() const { return maxRange; }

        bool operator==(const Params& other) const
        {
            return enabled == other.enabled &&
                   uStart == other.uStart &&
                   uEnd == other.uEnd &&
                   uStep == other.uStep &&
                   vStart == other.vStart &&
                   vEnd == other.vEnd &&
                   vStep == other.vStep &&
                   rayDirection == other.rayDirection &&
                   maxRange == other.maxRange;
        }

        bool operator!=(const Params& other) const
        {
            return !(*this == other);
        }
    };

    HDROBOTLIDARUSD_API
    bool GetEnabled(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    float GetUStart(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    float GetUEnd(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    float GetUStep(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    float GetVStart(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    float GetVEnd(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    float GetVStep(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    GfVec3f GetRayDirection(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    float GetMaxRange(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    Params GetParams(UsdTimeCode time = UsdTimeCode::Default()) const;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
