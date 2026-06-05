#pragma once

#include <pxr/imaging/hd/renderPass.h>

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {
class EngineSession;
class PassBridge;
}

class HdRobotRenderPass final : public HdRenderPass
{
public:
  HdRobotRenderPass(HdRenderIndex*             index,
                    const HdRprimCollection&   collection,
                    hdrobot::EngineSession&      engineSession);

  ~HdRobotRenderPass() override;

public:
  bool IsConverged() const override;

protected:
  void        _Execute(const HdRenderPassStateSharedPtr& renderPassState, const TfTokenVector& renderTags) override;

private:
  bool                                _isConverged;
  std::unique_ptr<hdrobot::PassBridge> _bridge;
};

PXR_NAMESPACE_CLOSE_SCOPE
