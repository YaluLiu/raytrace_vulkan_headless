#include <engine/tile_depth_types.hpp>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
void Require(bool condition, const char* message)
{
  if(condition)
  {
    return;
  }
  std::cerr << message << '\n';
  std::exit(1);
}

bool Near(float lhs, float rhs, float epsilon = 1.0e-5f)
{
  return std::fabs(lhs - rhs) <= epsilon;
}
} // namespace

int main()
{
  constexpr float nearPlane = 0.1f;
  constexpr float farPlane = 100.0f;
  Require(Near(LinearizePerspectiveDepth(0.0f, nearPlane, farPlane), nearPlane),
          "zero depth should map to the near clip");
  Require(Near(LinearizePerspectiveDepth(1.0f, nearPlane, farPlane), farPlane, 0.01f),
          "one depth should map to the far clip");

  const float expectedMidpoint = nearPlane * farPlane / (farPlane - 0.5f * (farPlane - nearPlane));
  Require(Near(LinearizePerspectiveDepth(0.5f, nearPlane, farPlane), expectedMidpoint),
          "perspective depth midpoint linearization mismatch");
  Require(Near(LinearizePerspectiveDepth(-1.0f, nearPlane, farPlane), nearPlane),
          "depth below zero should clamp to the near clip");
  Require(Near(LinearizePerspectiveDepth(2.0f, nearPlane, farPlane), farPlane, 0.01f),
          "depth above one should clamp to the far clip");
  Require(Near(LinearizePerspectiveDepth(std::numeric_limits<float>::quiet_NaN(), nearPlane, farPlane), farPlane),
          "non-finite depth should map to the far clip");
  return 0;
}
