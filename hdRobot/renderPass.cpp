#include "renderPass.h"

#include "renderBridge.h"
#include "renderResources.h"

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

HdRobotRenderPass::HdRobotRenderPass(HdRenderIndex*             index,
                                     const HdRprimCollection&   collection,
                                     HdRobotRenderResources&    resources)
    : HdRenderPass(index, collection)
    , _isConverged(false)
    , _bridge(std::make_unique<HdRobotRenderBridge>(resources))
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
  if(!_bridge->RenderFrame(renderPassState, renderTags))
  {
    return;
  }
  _isConverged = true;
}

PXR_NAMESPACE_CLOSE_SCOPE
