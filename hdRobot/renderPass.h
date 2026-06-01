#pragma once

#include <pxr/imaging/hd/renderPass.h>

#include <memory>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderBridge;
class HdRobotRenderParam;

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
  std::unique_ptr<HdRobotRenderBridge> _bridge;
};

PXR_NAMESPACE_CLOSE_SCOPE
