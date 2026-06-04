#include "renderPass.h"

#include "renderBridge.h"
#include "engineSession.h"

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

HdRobotRenderPass::HdRobotRenderPass(HdRenderIndex*             index,
                                     const HdRprimCollection&   collection,
                                     HdRobotEngineSession&      engineSession)
    : HdRenderPass(index, collection)
    , _isConverged(false)
    , _bridge(std::make_unique<HdRobotRenderBridge>(engineSession))
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
