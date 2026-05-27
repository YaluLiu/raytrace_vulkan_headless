//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef USDRAYSENSOR_TOKENS_H
#define USDRAYSENSOR_TOKENS_H

/// \file UsdRaySensor/tokens.h

// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
// 
// This is an automatically generated file (by usdGenSchema.py).
// Do not hand-edit!
// 
// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

#include "pxr/pxr.h"
#include "./api.h"
#include "pxr/base/tf/staticData.h"
#include "pxr/base/tf/token.h"
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE


/// \class UsdRaySensorTokensType
///
/// \link UsdRaySensorTokens \endlink provides static, efficient
/// \link TfToken TfTokens\endlink for use in all public USD API.
///
/// These tokens are auto-generated from the module's schema, representing
/// property names, for when you need to fetch an attribute or relationship
/// directly by name, e.g. UsdPrim::GetAttribute(), in the most efficient
/// manner, and allow the compiler to verify that you spelled the name
/// correctly.
///
/// UsdRaySensorTokens also contains all of the \em allowedTokens values
/// declared for schema builtin attributes of 'token' scene description type.
/// Use UsdRaySensorTokens like so:
///
/// \code
///     gprim.GetMyTokenValuedAttr().Set(UsdRaySensorTokens->azimuthEndDeg);
/// \endcode
struct UsdRaySensorTokensType {
    USDRAYSENSOR_API UsdRaySensorTokensType();
    /// \brief "azimuthEndDeg"
    ///
    /// LidarSensor
    const TfToken azimuthEndDeg;
    /// \brief "azimuthStartDeg"
    ///
    /// LidarSensor
    const TfToken azimuthStartDeg;
    /// \brief "azimuthStepDeg"
    ///
    /// LidarSensor
    const TfToken azimuthStepDeg;
    /// \brief "enabled"
    ///
    /// HeightScanSensor, LidarSensor
    const TfToken enabled;
    /// \brief "heightScanSensorParams"
    ///
    /// Hydra data key for the complete HeightScanSensor parameter block.
    const TfToken heightScanSensorParams;
    /// \brief "intensity"
    ///
    /// LidarSensor
    const TfToken intensity;
    /// \brief "lidarSensorParams"
    ///
    /// Hydra data key for the complete LidarSensor parameter block.
    const TfToken lidarSensorParams;
    /// \brief "maxRange"
    ///
    /// HeightScanSensor, LidarSensor
    const TfToken maxRange;
    /// \brief "rayDirection"
    ///
    /// HeightScanSensor
    const TfToken rayDirection;
    /// \brief "uEnd"
    ///
    /// HeightScanSensor
    const TfToken uEnd;
    /// \brief "uStart"
    ///
    /// HeightScanSensor
    const TfToken uStart;
    /// \brief "uStep"
    ///
    /// HeightScanSensor
    const TfToken uStep;
    /// \brief "vEnd"
    ///
    /// HeightScanSensor
    const TfToken vEnd;
    /// \brief "vStart"
    ///
    /// HeightScanSensor
    const TfToken vStart;
    /// \brief "vStep"
    ///
    /// HeightScanSensor
    const TfToken vStep;
    /// \brief "verticalEndDeg"
    ///
    /// LidarSensor
    const TfToken verticalEndDeg;
    /// \brief "verticalStartDeg"
    ///
    /// LidarSensor
    const TfToken verticalStartDeg;
    /// \brief "verticalStepDeg"
    ///
    /// LidarSensor
    const TfToken verticalStepDeg;
    /// \brief "LidarSensor"
    ///
    /// Schema identifer and family for LidarSensor
    const TfToken LidarSensor;
    /// \brief "HeightScanSensor"
    ///
    /// Schema identifer and family for HeightScanSensor
    const TfToken HeightScanSensor;
    /// A vector of all of the tokens listed above.
    const std::vector<TfToken> allTokens;
};

/// \var UsdRaySensorTokens
///
/// A global variable with static, efficient \link TfToken TfTokens\endlink
/// for use in all public USD API.  \sa UsdRaySensorTokensType
extern USDRAYSENSOR_API TfStaticData<UsdRaySensorTokensType> UsdRaySensorTokens;

PXR_NAMESPACE_CLOSE_SCOPE

#endif
