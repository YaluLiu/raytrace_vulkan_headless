//
// Copyright (C) 2019-2022 Pablo Delgado Krämer
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

#include <pxr/base/tf/staticTokens.h>

PXR_NAMESPACE_OPEN_SCOPE

#define HD_ROBOT_SETTINGS_TOKENS                                                                                     \
  ((spp, "spp"))((maxBounces, "max-bounces"))((rrBounceOffset, "rr-bounce-offset"))(                                   \
      (rrInvMinTermProb, "rr-inv-min-term-prob"))((nextEventEstimation, "next-event-estimation"))(                     \
      (progressiveAccumulation, "progressive-accumulation"))((dlssRRDenoise, "dlss-rr-denoise"))(                       \
      (dlssSREnable, "dlss-sr-enable"))((dlssSRScale, "dlss-sr-scale"))((lidarEnable, "lidar-enable"))(                \
      (filterImportanceSampling, "filter-importance-sampling"))((depthOfField, "depth-of-field"))(                     \
      (lightIntensityMultiplier, "light-intensity-multiplier"))((mediumStackSize, "medium-stack-size"))(               \
      (maxVolumeWalkLength, "max-volume-walk-length"))((jitteredSampling, "jittered-sampling"))(                       \
      (clippingPlanes, "clipping-planes"))((metersPerSceneUnit, "meters-per-scene-unit"))

// mtlx node identifier is given by UsdMtlx.
#define HD_ROBOT_NODE_IDENTIFIER_TOKENS (mdl)(mtlx)

#define HD_ROBOT_SOURCE_TYPE_TOKENS (mdl)(mtlx)

#define HD_ROBOT_DISCOVERY_TYPE_TOKENS (mdl)(mtlx)

#define HD_ROBOT_RENDER_CONTEXT_TOKENS (mdl)(mtlx)

#define HD_ROBOT_NODE_CONTEXT_TOKENS (mdl)(mtlx)

#define HD_ROBOT_NODE_METADATA_TOKENS (subIdentifier)

#define HD_ROBOT_AOV_TOKENS                                                                                          \
  ((debugNee, "debug:nee"))((debugBarycentrics, "debug:barycentrics"))((debugTexcoords, "debug:texcoords"))(           \
      (debugBounces, "debug:bounces"))((debugClockCycles, "debug:clock_cycles"))((debugOpacity, "debug:opacity"))(     \
      (debugTangents, "debug:tangents"))((debugBitangents, "debug:bitangents"))(                                       \
      (debugThinWalled, "debug:thinWalled"))((debugDoubleSided, "debug:doubleSided"))(                                \
      (distanceToCamera, "distance_to_camera"))(                                                                        \
      (lidarPointCloud, "lidar:pointCloud"))(                                                                           \
      (dlssRRDiffuseAlbedo, "dlssRR:diffuseAlbedo"))((dlssRRSpecularAlbedo, "dlssRR:specularAlbedo"))(                \
      (dlssRRNormalRoughness, "dlssRR:normalRoughness"))((dlssRRMotionVector, "dlssRR:motionVector"))(                \
      (dlssRRLinearDepth, "dlssRR:linearDepth"))((dlssRRSpecularHitDistance, "dlssRR:specularHitDistance"))

#define HD_ROBOT_CAMERA_PARAM_TOKENS                                                                                \
  ((lidarIsLidar, "lidar:isLidar"))((lidarAzimuthMinDeg, "lidar:azimuthMinDeg"))((lidarAzimuthMaxDeg,                \
                                                                                     "lidar:azimuthMaxDeg"))(         \
      (lidarAzimuthStepDeg, "lidar:azimuthStepDeg"))((lidarVerticalMinDeg, "lidar:verticalMinDeg"))(                 \
      (lidarVerticalMaxDeg, "lidar:verticalMaxDeg"))((lidarVerticalStepDeg, "lidar:verticalStepDeg"))(               \
      (lidarPointRadiusPixels, "lidar:pointRadiusPixels"))((lidarMaxDistance, "lidar:maxDistance"))

#define HD_ROBOT_COMMAND_TOKENS (printLicenses)

TF_DECLARE_PUBLIC_TOKENS(HdRobotSettingsTokens, HD_ROBOT_SETTINGS_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotNodeIdentifiers, HD_ROBOT_NODE_IDENTIFIER_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotSourceTypes, HD_ROBOT_SOURCE_TYPE_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotDiscoveryTypes, HD_ROBOT_DISCOVERY_TYPE_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotRenderContexts, HD_ROBOT_RENDER_CONTEXT_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotNodeContexts, HD_ROBOT_NODE_CONTEXT_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotNodeMetadata, HD_ROBOT_NODE_METADATA_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotAovTokens, HD_ROBOT_AOV_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotCameraParamTokens, HD_ROBOT_CAMERA_PARAM_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotCommandTokens, HD_ROBOT_COMMAND_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE
