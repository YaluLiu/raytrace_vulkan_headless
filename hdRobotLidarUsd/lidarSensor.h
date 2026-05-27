//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef HDROBOTLIDARUSD_GENERATED_LIDARSENSOR_H
#define HDROBOTLIDARUSD_GENERATED_LIDARSENSOR_H

/// \file hdRobotLidarUsd/lidarSensor.h

#include "pxr/pxr.h"
#include "./api.h"
#include "pxr/usd/usdGeom/xformable.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usd/timeCode.h"
#include "./tokens.h"

#include "pxr/base/vt/value.h"

#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/matrix4d.h"

#include "pxr/base/tf/token.h"
#include "pxr/base/tf/type.h"

PXR_NAMESPACE_OPEN_SCOPE

class SdfAssetPath;

// -------------------------------------------------------------------------- //
// LIDARSENSOR                                                                //
// -------------------------------------------------------------------------- //

/// \class HdRobotLidarUsdLidarSensor
///
/// Concrete transformable LiDAR sensor schema consumed by hdRobot.
///
class HdRobotLidarUsdLidarSensor : public UsdGeomXformable
{
public:
    /// Compile time constant representing what kind of schema this class is.
    ///
    /// \sa UsdSchemaKind
    static const UsdSchemaKind schemaKind = UsdSchemaKind::ConcreteTyped;

    /// Construct a HdRobotLidarUsdLidarSensor on UsdPrim \p prim .
    /// Equivalent to HdRobotLidarUsdLidarSensor::Get(prim.GetStage(), prim.GetPath())
    /// for a \em valid \p prim, but will not immediately throw an error for
    /// an invalid \p prim
    explicit HdRobotLidarUsdLidarSensor(const UsdPrim& prim=UsdPrim())
        : UsdGeomXformable(prim)
    {
    }

    /// Construct a HdRobotLidarUsdLidarSensor on the prim held by \p schemaObj .
    /// Should be preferred over HdRobotLidarUsdLidarSensor(schemaObj.GetPrim()),
    /// as it preserves SchemaBase state.
    explicit HdRobotLidarUsdLidarSensor(const UsdSchemaBase& schemaObj)
        : UsdGeomXformable(schemaObj)
    {
    }

    /// Destructor.
    HDROBOTLIDARUSD_API
    virtual ~HdRobotLidarUsdLidarSensor();

    /// Return a vector of names of all pre-declared attributes for this schema
    /// class and all its ancestor classes.  Does not include attributes that
    /// may be authored by custom/extended methods of the schemas involved.
    HDROBOTLIDARUSD_API
    static const TfTokenVector &
    GetSchemaAttributeNames(bool includeInherited=true);

    /// Return a HdRobotLidarUsdLidarSensor holding the prim adhering to this
    /// schema at \p path on \p stage.  If no prim exists at \p path on
    /// \p stage, or if the prim at that path does not adhere to this schema,
    /// return an invalid schema object.  This is shorthand for the following:
    ///
    /// \code
    /// HdRobotLidarUsdLidarSensor(stage->GetPrimAtPath(path));
    /// \endcode
    ///
    HDROBOTLIDARUSD_API
    static HdRobotLidarUsdLidarSensor
    Get(const UsdStagePtr &stage, const SdfPath &path);

    /// Attempt to ensure a \a UsdPrim adhering to this schema at \p path
    /// is defined (according to UsdPrim::IsDefined()) on this stage.
    ///
    /// If a prim adhering to this schema at \p path is already defined on this
    /// stage, return that prim.  Otherwise author an \a SdfPrimSpec with
    /// \a specifier == \a SdfSpecifierDef and this schema's prim type name for
    /// the prim at \p path at the current EditTarget.  Author \a SdfPrimSpec s
    /// with \p specifier == \a SdfSpecifierDef and empty typeName at the
    /// current EditTarget for any nonexistent, or existing but not \a Defined
    /// ancestors.
    ///
    /// The given \a path must be an absolute prim path that does not contain
    /// any variant selections.
    ///
    /// If it is impossible to author any of the necessary PrimSpecs, (for
    /// example, in case \a path cannot map to the current UsdEditTarget's
    /// namespace) issue an error and return an invalid \a UsdPrim.
    ///
    /// Note that this method may return a defined prim whose typeName does not
    /// specify this schema class, in case a stronger typeName opinion overrides
    /// the opinion at the current EditTarget.
    ///
    HDROBOTLIDARUSD_API
    static HdRobotLidarUsdLidarSensor
    Define(const UsdStagePtr &stage, const SdfPath &path);

protected:
    /// Returns the kind of schema this class belongs to.
    ///
    /// \sa UsdSchemaKind
    HDROBOTLIDARUSD_API
    UsdSchemaKind _GetSchemaKind() const override;

private:
    // needs to invoke _GetStaticTfType.
    friend class UsdSchemaRegistry;
    HDROBOTLIDARUSD_API
    static const TfType &_GetStaticTfType();

    static bool _IsTypedSchema();

    // override SchemaBase virtuals.
    HDROBOTLIDARUSD_API
    const TfType &_GetTfType() const override;

public:
    // --------------------------------------------------------------------- //
    // ENABLED 
    // --------------------------------------------------------------------- //
    /// 
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `bool enabled = 1` |
    /// | C++ Type | bool |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Bool |
    HDROBOTLIDARUSD_API
    UsdAttribute GetEnabledAttr() const;

    /// See GetEnabledAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    HDROBOTLIDARUSD_API
    UsdAttribute CreateEnabledAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // AZIMUTHSTARTDEG 
    // --------------------------------------------------------------------- //
    /// 
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `float azimuthStartDeg = -90` |
    /// | C++ Type | float |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Float |
    HDROBOTLIDARUSD_API
    UsdAttribute GetAzimuthStartDegAttr() const;

    /// See GetAzimuthStartDegAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    HDROBOTLIDARUSD_API
    UsdAttribute CreateAzimuthStartDegAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // AZIMUTHENDDEG 
    // --------------------------------------------------------------------- //
    /// 
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `float azimuthEndDeg = 90` |
    /// | C++ Type | float |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Float |
    HDROBOTLIDARUSD_API
    UsdAttribute GetAzimuthEndDegAttr() const;

    /// See GetAzimuthEndDegAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    HDROBOTLIDARUSD_API
    UsdAttribute CreateAzimuthEndDegAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // AZIMUTHSTEPDEG 
    // --------------------------------------------------------------------- //
    /// 
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `float azimuthStepDeg = 0.5` |
    /// | C++ Type | float |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Float |
    HDROBOTLIDARUSD_API
    UsdAttribute GetAzimuthStepDegAttr() const;

    /// See GetAzimuthStepDegAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    HDROBOTLIDARUSD_API
    UsdAttribute CreateAzimuthStepDegAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // VERTICALSTARTDEG 
    // --------------------------------------------------------------------- //
    /// 
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `float verticalStartDeg = -2` |
    /// | C++ Type | float |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Float |
    HDROBOTLIDARUSD_API
    UsdAttribute GetVerticalStartDegAttr() const;

    /// See GetVerticalStartDegAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    HDROBOTLIDARUSD_API
    UsdAttribute CreateVerticalStartDegAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // VERTICALENDDEG 
    // --------------------------------------------------------------------- //
    /// 
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `float verticalEndDeg = -20` |
    /// | C++ Type | float |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Float |
    HDROBOTLIDARUSD_API
    UsdAttribute GetVerticalEndDegAttr() const;

    /// See GetVerticalEndDegAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    HDROBOTLIDARUSD_API
    UsdAttribute CreateVerticalEndDegAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // VERTICALSTEPDEG 
    // --------------------------------------------------------------------- //
    /// 
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `float verticalStepDeg = 1` |
    /// | C++ Type | float |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Float |
    HDROBOTLIDARUSD_API
    UsdAttribute GetVerticalStepDegAttr() const;

    /// See GetVerticalStepDegAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    HDROBOTLIDARUSD_API
    UsdAttribute CreateVerticalStepDegAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // MAXRANGE 
    // --------------------------------------------------------------------- //
    /// 
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `float maxRange = 200` |
    /// | C++ Type | float |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Float |
    HDROBOTLIDARUSD_API
    UsdAttribute GetMaxRangeAttr() const;

    /// See GetMaxRangeAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    HDROBOTLIDARUSD_API
    UsdAttribute CreateMaxRangeAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // INTENSITY 
    // --------------------------------------------------------------------- //
    /// 
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `float intensity = 1` |
    /// | C++ Type | float |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Float |
    HDROBOTLIDARUSD_API
    UsdAttribute GetIntensityAttr() const;

    /// See GetIntensityAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    HDROBOTLIDARUSD_API
    UsdAttribute CreateIntensityAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

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
        float azimuthStartDeg = -90.0f;
        float azimuthEndDeg = 90.0f;
        float azimuthStepDeg = 0.5f;
        float verticalStartDeg = -2.0f;
        float verticalEndDeg = -20.0f;
        float verticalStepDeg = 1.0f;
        float maxRange = 200.0f;
        float intensity = 1.0f;

        bool GetEnabled() const { return enabled; }
        float GetAzimuthStartDeg() const { return azimuthStartDeg; }
        float GetAzimuthEndDeg() const { return azimuthEndDeg; }
        float GetAzimuthStepDeg() const { return azimuthStepDeg; }
        float GetVerticalStartDeg() const { return verticalStartDeg; }
        float GetVerticalEndDeg() const { return verticalEndDeg; }
        float GetVerticalStepDeg() const { return verticalStepDeg; }
        float GetMaxRange() const { return maxRange; }
        float GetIntensity() const { return intensity; }

        bool operator==(const Params& other) const
        {
            return enabled == other.enabled &&
                   azimuthStartDeg == other.azimuthStartDeg &&
                   azimuthEndDeg == other.azimuthEndDeg &&
                   azimuthStepDeg == other.azimuthStepDeg &&
                   verticalStartDeg == other.verticalStartDeg &&
                   verticalEndDeg == other.verticalEndDeg &&
                   verticalStepDeg == other.verticalStepDeg &&
                   maxRange == other.maxRange &&
                   intensity == other.intensity;
        }

        bool operator!=(const Params& other) const
        {
            return !(*this == other);
        }
    };

    HDROBOTLIDARUSD_API
    bool GetEnabled(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    float GetAzimuthStartDeg(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    float GetAzimuthEndDeg(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    float GetAzimuthStepDeg(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    float GetVerticalStartDeg(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    float GetVerticalEndDeg(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    float GetVerticalStepDeg(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    float GetMaxRange(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    float GetIntensity(UsdTimeCode time = UsdTimeCode::Default()) const;

    HDROBOTLIDARUSD_API
    Params GetParams(UsdTimeCode time = UsdTimeCode::Default()) const;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
