#pragma once

#include "tiny_obj_loader.h"
#include <array>
#include <iostream>
#include <stdint.h>
#include <unordered_map>
#include <vector>

#include "ModelLoader.h"

class ObjLoader : public ModelLoader
{
public:
  void loadModel(const std::string& filename);
};
