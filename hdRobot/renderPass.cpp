#include "renderPass.h"

#include "passBridge.h"
#include "engineSession.h"

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

using hdrobot::EngineSession;
using hdrobot::PassBridge;

HdRobotRenderPass::HdRobotRenderPass(HdRenderIndex*             index,
                                     const HdRprimCollection&   collection,
                                     EngineSession&      engineSession)
    : HdRenderPass(index, collection)
    , _isConverged(false)
    , _bridge(std::make_unique<PassBridge>(engineSession))
{
}

HdRobotRenderPass::~HdRobotRenderPass() {}

bool HdRobotRenderPass::IsConverged() const
{
  return _isConverged;
}

void HdRobotRenderPass::_Execute(const HdRenderPassStateSharedPtr& renderPassState, const TfTokenVector& renderTags)
{
  _isConverged = false;
  _isConverged = _bridge->RenderFrameAndCopyAovs(renderPassState, renderTags).IsConverged();
}

PXR_NAMESPACE_CLOSE_SCOPE
