#extension GL_EXT_scalar_block_layout : enable

layout(scalar) struct hitPayload
{
  vec3 hitValue;    // 12字节
  int  objId;       // 4字节
  int  instanceId;  // 4字节
};
