//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "./tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

UsdRaySensorTokensType::UsdRaySensorTokensType() :
    azimuthEndDeg("azimuthEndDeg", TfToken::Immortal),
    azimuthStartDeg("azimuthStartDeg", TfToken::Immortal),
    azimuthStepDeg("azimuthStepDeg", TfToken::Immortal),
    enabled("enabled", TfToken::Immortal),
    heightScanSensorParams("heightScanSensorParams", TfToken::Immortal),
    intensity("intensity", TfToken::Immortal),
    lidarSensorParams("lidarSensorParams", TfToken::Immortal),
    maxRange("maxRange", TfToken::Immortal),
    rayDirection("rayDirection", TfToken::Immortal),
    uEnd("uEnd", TfToken::Immortal),
    uStart("uStart", TfToken::Immortal),
    uStep("uStep", TfToken::Immortal),
    vEnd("vEnd", TfToken::Immortal),
    vStart("vStart", TfToken::Immortal),
    vStep("vStep", TfToken::Immortal),
    verticalEndDeg("verticalEndDeg", TfToken::Immortal),
    verticalStartDeg("verticalStartDeg", TfToken::Immortal),
    verticalStepDeg("verticalStepDeg", TfToken::Immortal),
    LidarSensor("LidarSensor", TfToken::Immortal),
    HeightScanSensor("HeightScanSensor", TfToken::Immortal),
    allTokens({
        azimuthEndDeg,
        azimuthStartDeg,
        azimuthStepDeg,
        enabled,
        heightScanSensorParams,
        intensity,
        lidarSensorParams,
        maxRange,
        rayDirection,
        uEnd,
        uStart,
        uStep,
        vEnd,
        vStart,
        vStep,
        verticalEndDeg,
        verticalStartDeg,
        verticalStepDeg,
        LidarSensor,
        HeightScanSensor
    })
{
}

TfStaticData<UsdRaySensorTokensType> UsdRaySensorTokens;

PXR_NAMESPACE_CLOSE_SCOPE
