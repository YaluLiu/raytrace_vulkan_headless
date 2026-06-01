#version 460

layout(location = 0) in float inRangeMeters;
layout(location = 0) out float outRange;

void main()
{
  outRange = inRangeMeters;
}
