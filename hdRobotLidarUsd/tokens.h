//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef HDROBOTLIDARUSD_TOKENS_H
#define HDROBOTLIDARUSD_TOKENS_H

/// \file hdRobotLidarUsd/tokens.h

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


/// \class HdRobotLidarUsdTokensType
///
/// \link HdRobotLidarUsdTokens \endlink provides static, efficient
/// \link TfToken TfTokens\endlink for use in all public USD API.
///
/// These tokens are auto-generated from the module's schema, representing
/// property names, for when you need to fetch an attribute or relationship
/// directly by name, e.g. UsdPrim::GetAttribute(), in the most efficient
/// manner, and allow the compiler to verify that you spelled the name
/// correctly.
///
/// HdRobotLidarUsdTokens also contains all of the \em allowedTokens values
/// declared for schema builtin attributes of 'token' scene description type.
/// Use HdRobotLidarUsdTokens like so:
///
/// \code
///     gprim.GetMyTokenValuedAttr().Set(HdRobotLidarUsdTokens->azimuthEndDeg);
/// \endcode
struct HdRobotLidarUsdTokensType {
    HDROBOTLIDARUSD_API HdRobotLidarUsdTokensType();
    /// \brief "azimuthEndDeg"
    /// 
    /// HdRobotLidarUsdLidarSensor
    const TfToken azimuthEndDeg;
    /// \brief "azimuthStartDeg"
    /// 
    /// HdRobotLidarUsdLidarSensor
    const TfToken azimuthStartDeg;
    /// \brief "azimuthStepDeg"
    /// 
    /// HdRobotLidarUsdLidarSensor
    const TfToken azimuthStepDeg;
    /// \brief "enabled"
    /// 
    /// HdRobotLidarUsdLidarSensor
    const TfToken enabled;
    /// \brief "intensity"
    /// 
    /// HdRobotLidarUsdLidarSensor
    const TfToken intensity;
    /// \brief "lidarSensorParams"
    /// 
    /// Hydra data key for the complete LidarSensor parameter block.
    const TfToken lidarSensorParams;
    /// \brief "maxRange"
    /// 
    /// HdRobotLidarUsdLidarSensor
    const TfToken maxRange;
    /// \brief "verticalEndDeg"
    /// 
    /// HdRobotLidarUsdLidarSensor
    const TfToken verticalEndDeg;
    /// \brief "verticalStartDeg"
    /// 
    /// HdRobotLidarUsdLidarSensor
    const TfToken verticalStartDeg;
    /// \brief "verticalStepDeg"
    /// 
    /// HdRobotLidarUsdLidarSensor
    const TfToken verticalStepDeg;
    /// \brief "LidarSensor"
    /// 
    /// Schema identifer and family for HdRobotLidarUsdLidarSensor
    const TfToken LidarSensor;
    /// A vector of all of the tokens listed above.
    const std::vector<TfToken> allTokens;
};

/// \var HdRobotLidarUsdTokens
///
/// A global variable with static, efficient \link TfToken TfTokens\endlink
/// for use in all public USD API.  \sa HdRobotLidarUsdTokensType
extern HDROBOTLIDARUSD_API TfStaticData<HdRobotLidarUsdTokensType> HdRobotLidarUsdTokens;

PXR_NAMESPACE_CLOSE_SCOPE

#endif
