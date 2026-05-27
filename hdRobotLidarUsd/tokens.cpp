//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "./tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

HdRobotLidarUsdTokensType::HdRobotLidarUsdTokensType() :
    azimuthEndDeg("azimuthEndDeg", TfToken::Immortal),
    azimuthStartDeg("azimuthStartDeg", TfToken::Immortal),
    azimuthStepDeg("azimuthStepDeg", TfToken::Immortal),
    enabled("enabled", TfToken::Immortal),
    intensity("intensity", TfToken::Immortal),
    lidarSensorParams("lidarSensorParams", TfToken::Immortal),
    maxRange("maxRange", TfToken::Immortal),
    verticalEndDeg("verticalEndDeg", TfToken::Immortal),
    verticalStartDeg("verticalStartDeg", TfToken::Immortal),
    verticalStepDeg("verticalStepDeg", TfToken::Immortal),
    LidarSensor("LidarSensor", TfToken::Immortal),
    allTokens({
        azimuthEndDeg,
        azimuthStartDeg,
        azimuthStepDeg,
        enabled,
        intensity,
        lidarSensorParams,
        maxRange,
        verticalEndDeg,
        verticalStartDeg,
        verticalStepDeg,
        LidarSensor
    })
{
}

TfStaticData<HdRobotLidarUsdTokensType> HdRobotLidarUsdTokens;

PXR_NAMESPACE_CLOSE_SCOPE
