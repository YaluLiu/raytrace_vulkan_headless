//
// Copyright (C) 2019-2022 Pablo Delgado Kramer
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "renderPass.h"

#include "headlessRenderBridge.h"

#include <memory>
#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

HdRobotRenderPass::HdRobotRenderPass(HdRenderIndex*             index,
                                     const HdRprimCollection&   collection,
                                     const HdRenderSettingsMap& settings,
                                     HdRobotRenderParam&        renderParam,
                                     std::string                resourcePath)
    : HdRenderPass(index, collection)
    , _isConverged(false)
    , _bridge(std::make_unique<HeadlessRenderBridge>(settings, renderParam, std::move(resourcePath)))
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
