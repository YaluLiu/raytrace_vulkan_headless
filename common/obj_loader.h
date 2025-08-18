/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

// tiny_obj_loader是一个轻量级的OBJ模型文件加载库
#include "tiny_obj_loader.h"
// 引入array容器
#include <array>
// 引入iostream用于调试输出
#include <iostream>
// 引入stdint.h用于标准整数类型
#include <stdint.h>
// 引入unordered_map哈希表
#include <unordered_map>
// 引入vector动态数组
#include <vector>

#include "ModelLoader.h"

class ObjLoader : public ModelLoader
{
  public:
  void loadModel(const std::string& filename);
};