#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "host_device.h"

layout(set = 0, binding = eFrameUniforms) uniform _FrameUniforms
{
  FrameUniforms frameUni;
};
layout(set = 1, binding = 0, std430) readonly buffer HeightScanSensors_
{
  HeightScanSensorGpu sensors[];
}
heightScanSensors;
layout(set = 1, binding = 1, std430) readonly buffer HeightScanSamples_
{
  HeightScanSampleGpu samples[];
}
heightScanSamples;

layout(push_constant) uniform _PushConstantPointOverlay
{
  PushConstantPointOverlay pc;
};

void main()
{
  HeightScanSensorGpu sensor = heightScanSensors.sensors[pc.sensorIndex];
  HeightScanSampleGpu heightSample = heightScanSamples.samples[sensor.sampleOffset + uint(gl_VertexIndex)];
  bool visible = (heightSample.flags & (HEIGHT_SCAN_SAMPLE_FLAG_VALID | HEIGHT_SCAN_SAMPLE_FLAG_HIT)) ==
                 (HEIGHT_SCAN_SAMPLE_FLAG_VALID | HEIGHT_SCAN_SAMPLE_FLAG_HIT);
  if(!visible)
  {
    gl_Position = vec4(2.0, 2.0, 1.0, 1.0);
    gl_PointSize = 1.0;
    return;
  }

  gl_Position = frameUni.camera.viewProj * vec4(heightSample.positionDistance.xyz, 1.0);
  gl_PointSize = max(pc.pointSizePixels, 1.0);
}
