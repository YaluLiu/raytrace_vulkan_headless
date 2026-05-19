#include "renderPass.h"

#include "rasterBridge.h"

#include <memory>
#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

HdRobotRenderPass::HdRobotRenderPass(HdRenderIndex*             index,
                                     const HdRprimCollection&   collection,
                                     HdRobotRenderParam&        renderParam,
                                     std::string                resourcePath)
    : HdRenderPass(index, collection)
    , _isConverged(false)
    , _bridge(std::make_unique<HdRobotRasterBridge>(renderParam, std::move(resourcePath)))
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
  if(!_bridge->RenderRasterFrame(renderPassState, renderTags))
  {
    return;
  }
  _isConverged = true;
}

PXR_NAMESPACE_CLOSE_SCOPE
