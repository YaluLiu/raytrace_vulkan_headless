#pragma once

#include <pxr/base/tf/staticTokens.h>

PXR_NAMESPACE_OPEN_SCOPE

// mtlx node identifier is given by UsdMtlx.
#define HD_ROBOT_NODE_IDENTIFIER_TOKENS (mdl)(mtlx)

#define HD_ROBOT_SOURCE_TYPE_TOKENS (mdl)(mtlx)

#define HD_ROBOT_DISCOVERY_TYPE_TOKENS (mdl)(mtlx)

#define HD_ROBOT_RENDER_CONTEXT_TOKENS (mdl)(mtlx)

#define HD_ROBOT_NODE_CONTEXT_TOKENS (mdl)(mtlx)

#define HD_ROBOT_NODE_METADATA_TOKENS (subIdentifier)

#define HD_ROBOT_COMMAND_TOKENS (printLicenses)

#define HD_ROBOT_RENDER_SETTING_TOKENS \
  ((tileEnabled, "hdRobot:tile:enabled")) \
  ((tileColorEnabled, "hdRobot:tile:colorEnabled")) \
  ((tileDepthEnabled, "hdRobot:tile:depthEnabled")) \
  ((tileCameraWidth, "hdRobot:tile:cameraWidth")) \
  ((tileCameraHeight, "hdRobot:tile:cameraHeight")) \
  ((tileGridColumns, "hdRobot:tile:gridColumns")) \
  ((tileGridRows, "hdRobot:tile:gridRows")) \
  ((lidarVisualizeEnabled, "hdRobot:lidar:visualizeEnabled")) \
  ((lidarVisualizeSensorIndex, "hdRobot:lidar:visualizeSensorIndex")) \
  ((lidarVisualizePointSize, "hdRobot:lidar:visualizePointSize"))

#define HD_ROBOT_CAMERA_PARAM_TOKENS \
  ((lidarIsLidar, "lidar:isLidar")) \
  ((lidarAzimuthMinDeg, "lidar:azimuthMinDeg")) \
  ((lidarAzimuthMaxDeg, "lidar:azimuthMaxDeg")) \
  ((lidarAzimuthStepDeg, "lidar:azimuthStepDeg")) \
  ((lidarVerticalMinDeg, "lidar:verticalMinDeg")) \
  ((lidarVerticalMaxDeg, "lidar:verticalMaxDeg")) \
  ((lidarVerticalStepDeg, "lidar:verticalStepDeg")) \
  ((lidarMaxDistance, "lidar:maxDistance"))

#define HD_ROBOT_AOV_TOKENS \
  (tileColor) \
  (tileDepth) \
  (tileDisplayColor) \
  (tileDisplayDepth)

TF_DECLARE_PUBLIC_TOKENS(HdRobotNodeIdentifiers, HD_ROBOT_NODE_IDENTIFIER_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotSourceTypes, HD_ROBOT_SOURCE_TYPE_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotDiscoveryTypes, HD_ROBOT_DISCOVERY_TYPE_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotRenderContexts, HD_ROBOT_RENDER_CONTEXT_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotNodeContexts, HD_ROBOT_NODE_CONTEXT_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotNodeMetadata, HD_ROBOT_NODE_METADATA_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotCommandTokens, HD_ROBOT_COMMAND_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotRenderSettingTokens, HD_ROBOT_RENDER_SETTING_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotCameraParamTokens, HD_ROBOT_CAMERA_PARAM_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotAovTokens, HD_ROBOT_AOV_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE
