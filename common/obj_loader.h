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