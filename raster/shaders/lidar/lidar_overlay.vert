#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "host_device.h"

layout(set = 0, binding = eFrameUniforms) uniform _FrameUniforms
{
  FrameUniforms frameUni;
};
layout(set = 1, binding = 0, std430) readonly buffer LidarSensors_
{
  LidarSensorGpu sensors[];
}
lidarSensors;
layout(set = 1, binding = 1, std430) readonly buffer LidarPoints_
{
  LidarPointGpu points[];
}
lidarPoints;

layout(push_constant) uniform _PushConstantPointOverlay
{
  PushConstantPointOverlay pc;
};

void main()
{
  LidarSensorGpu sensor = lidarSensors.sensors[pc.sensorIndex];
  LidarPointGpu point = lidarPoints.points[sensor.pointOffset + uint(gl_VertexIndex)];
  bool visible = (point.flags & (LIDAR_POINT_FLAG_VALID | LIDAR_POINT_FLAG_HIT)) ==
                 (LIDAR_POINT_FLAG_VALID | LIDAR_POINT_FLAG_HIT);
  if(!visible)
  {
    gl_Position = vec4(2.0, 2.0, 1.0, 1.0);
    gl_PointSize = 1.0;
    return;
  }

  gl_Position = frameUni.camera.viewProj * vec4(point.positionRange.xyz, 1.0);
  gl_PointSize = max(pc.pointSizePixels, 1.0);
}
