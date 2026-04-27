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

#pragma once

#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/renderPass.h>

#include <string>
#include <vector>

class HelloVulkan;

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderBuffer;

bool EnsureDirectoryExists(const std::string& path);
std::vector<std::string> ExportRegisteredTextures(const std::vector<std::string>& texturePaths);
HdRobotRenderBuffer* GetPrimaryRenderBuffer(const HdRenderPassAovBindingVector& bindings);
void CopyAovToRenderBuffer(const ::HelloVulkan& app, const TfToken& name, HdRobotRenderBuffer* renderBuffer);

PXR_NAMESPACE_CLOSE_SCOPE
