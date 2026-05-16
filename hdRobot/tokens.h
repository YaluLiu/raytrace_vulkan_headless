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
  ((tileCameraWidth, "hdRobot:tile:cameraWidth")) \
  ((tileCameraHeight, "hdRobot:tile:cameraHeight")) \
  ((tileGridColumns, "hdRobot:tile:gridColumns")) \
  ((tileGridRows, "hdRobot:tile:gridRows"))

#define HD_ROBOT_AOV_TOKENS \
  (tileColor) \
  (tileDepth) \
  (tileColorDisplay) \
  (tileDepthDisplay) \
  ((tileDepthDisplayDepth, "tileDepthDisplay_depth"))

TF_DECLARE_PUBLIC_TOKENS(HdRobotNodeIdentifiers, HD_ROBOT_NODE_IDENTIFIER_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotSourceTypes, HD_ROBOT_SOURCE_TYPE_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotDiscoveryTypes, HD_ROBOT_DISCOVERY_TYPE_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotRenderContexts, HD_ROBOT_RENDER_CONTEXT_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotNodeContexts, HD_ROBOT_NODE_CONTEXT_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotNodeMetadata, HD_ROBOT_NODE_METADATA_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotCommandTokens, HD_ROBOT_COMMAND_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotRenderSettingTokens, HD_ROBOT_RENDER_SETTING_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdRobotAovTokens, HD_ROBOT_AOV_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE
