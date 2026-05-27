//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef USDRAYSENSOR_GENERATED_HEIGHTSCANSENSOR_H
#define USDRAYSENSOR_GENERATED_HEIGHTSCANSENSOR_H

/// \file UsdRaySensor/heightScanSensor.h

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

/// \class UsdGeomHeightScanSensor
///
/// Concrete transformable height scan sensor schema for ray-sensor consumers.
///
class UsdGeomHeightScanSensor : public UsdGeomXformable
{
public:
    /// Compile time constant representing what kind of schema this class is.
    ///
    /// \sa UsdSchemaKind
    static const UsdSchemaKind schemaKind = UsdSchemaKind::ConcreteTyped;

    /// Construct a UsdGeomHeightScanSensor on UsdPrim \p prim .
    explicit UsdGeomHeightScanSensor(const UsdPrim& prim=UsdPrim())
        : UsdGeomXformable(prim)
    {
    }

    /// Construct a UsdGeomHeightScanSensor on the prim held by
    /// \p schemaObj .
    explicit UsdGeomHeightScanSensor(const UsdSchemaBase& schemaObj)
        : UsdGeomXformable(schemaObj)
    {
    }

    /// Destructor.
    USDRAYSENSOR_API
    virtual ~UsdGeomHeightScanSensor();

    /// Return a vector of names of all pre-declared attributes for this schema
    /// class and all its ancestor classes.
    USDRAYSENSOR_API
    static const TfTokenVector &
    GetSchemaAttributeNames(bool includeInherited=true);

    /// Return a UsdGeomHeightScanSensor holding the prim adhering to
    /// this schema at \p path on \p stage.
    USDRAYSENSOR_API
    static UsdGeomHeightScanSensor
    Get(const UsdStagePtr &stage, const SdfPath &path);

    /// Attempt to ensure a UsdPrim adhering to this schema at \p path is
    /// defined on this stage.
    USDRAYSENSOR_API
    static UsdGeomHeightScanSensor
    Define(const UsdStagePtr &stage, const SdfPath &path);

protected:
    /// Returns the kind of schema this class belongs to.
    ///
    /// \sa UsdSchemaKind
    USDRAYSENSOR_API
    UsdSchemaKind _GetSchemaKind() const override;

private:
    friend class UsdSchemaRegistry;
    USDRAYSENSOR_API
    static const TfType &_GetStaticTfType();

    static bool _IsTypedSchema();

    USDRAYSENSOR_API
    const TfType &_GetTfType() const override;

public:
    // --------------------------------------------------------------------- //
    // ENABLED
    // --------------------------------------------------------------------- //
    USDRAYSENSOR_API
    UsdAttribute GetEnabledAttr() const;

    USDRAYSENSOR_API
    UsdAttribute CreateEnabledAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // USTART
    // --------------------------------------------------------------------- //
    USDRAYSENSOR_API
    UsdAttribute GetUStartAttr() const;

    USDRAYSENSOR_API
    UsdAttribute CreateUStartAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // UEND
    // --------------------------------------------------------------------- //
    USDRAYSENSOR_API
    UsdAttribute GetUEndAttr() const;

    USDRAYSENSOR_API
    UsdAttribute CreateUEndAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // USTEP
    // --------------------------------------------------------------------- //
    USDRAYSENSOR_API
    UsdAttribute GetUStepAttr() const;

    USDRAYSENSOR_API
    UsdAttribute CreateUStepAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // VSTART
    // --------------------------------------------------------------------- //
    USDRAYSENSOR_API
    UsdAttribute GetVStartAttr() const;

    USDRAYSENSOR_API
    UsdAttribute CreateVStartAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // VEND
    // --------------------------------------------------------------------- //
    USDRAYSENSOR_API
    UsdAttribute GetVEndAttr() const;

    USDRAYSENSOR_API
    UsdAttribute CreateVEndAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // VSTEP
    // --------------------------------------------------------------------- //
    USDRAYSENSOR_API
    UsdAttribute GetVStepAttr() const;

    USDRAYSENSOR_API
    UsdAttribute CreateVStepAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // RAYDIRECTION
    // --------------------------------------------------------------------- //
    USDRAYSENSOR_API
    UsdAttribute GetRayDirectionAttr() const;

    USDRAYSENSOR_API
    UsdAttribute CreateRayDirectionAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // MAXRANGE
    // --------------------------------------------------------------------- //
    USDRAYSENSOR_API
    UsdAttribute GetMaxRangeAttr() const;

    USDRAYSENSOR_API
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
    struct HeightScanSensorSpec
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

        bool operator==(const HeightScanSensorSpec& other) const
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

        bool operator!=(const HeightScanSensorSpec& other) const
        {
            return !(*this == other);
        }
    };

    using Params = HeightScanSensorSpec;

    USDRAYSENSOR_API
    bool GetEnabled(UsdTimeCode time = UsdTimeCode::Default()) const;

    USDRAYSENSOR_API
    float GetUStart(UsdTimeCode time = UsdTimeCode::Default()) const;

    USDRAYSENSOR_API
    float GetUEnd(UsdTimeCode time = UsdTimeCode::Default()) const;

    USDRAYSENSOR_API
    float GetUStep(UsdTimeCode time = UsdTimeCode::Default()) const;

    USDRAYSENSOR_API
    float GetVStart(UsdTimeCode time = UsdTimeCode::Default()) const;

    USDRAYSENSOR_API
    float GetVEnd(UsdTimeCode time = UsdTimeCode::Default()) const;

    USDRAYSENSOR_API
    float GetVStep(UsdTimeCode time = UsdTimeCode::Default()) const;

    USDRAYSENSOR_API
    GfVec3f GetRayDirection(UsdTimeCode time = UsdTimeCode::Default()) const;

    USDRAYSENSOR_API
    float GetMaxRange(UsdTimeCode time = UsdTimeCode::Default()) const;

    USDRAYSENSOR_API
    HeightScanSensorSpec GetHeightScanSensor(UsdTimeCode time = UsdTimeCode::Default()) const;

    USDRAYSENSOR_API
    Params GetParams(UsdTimeCode time = UsdTimeCode::Default()) const;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
