#pragma once

#include <glm/glm.hpp>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/renderPass.h>

#include <memory>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotMesh;
class MaterialNetworkCompiler;
class HdRobotCamera;
class HdRobotRasterBridge;
class HdRobotRenderParam;
struct HdRobotCameraData;

struct GiCameraDesc
{
  float     position[3];
  float     forward[3];
  float     up[3];
  float     vfov;
  float     fStop;
  float     focusDistance;
  float     focalLength;
  float     clipStart;
  float     clipEnd;
  float     exposure;
  glm::mat4 projMatrix;
  glm::mat4 viewMatrix;
};

glm::mat4 computeProjectionMatrix(const GiCameraDesc& cameraDesc, float aspectRatio);

class HdRobotRenderPass final : public HdRenderPass
{
public:
  HdRobotRenderPass(HdRenderIndex*             index,
                    const HdRprimCollection&   collection,
                    HdRobotRenderParam&        renderParam,
                    std::string                resourcePath);

  ~HdRobotRenderPass() override;

public:
  bool IsConverged() const override;

protected:
  void        _Execute(const HdRenderPassStateSharedPtr& renderPassState, const TfTokenVector& renderTags) override;

private:
  bool                                _isConverged;
  std::unique_ptr<HdRobotRasterBridge> _bridge;
};

PXR_NAMESPACE_CLOSE_SCOPE
