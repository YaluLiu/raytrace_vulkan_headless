#pragma once

#include <engine/height_scan_types.hpp>
#include <engine/lidar_types.hpp>
#include <engine/tile_config.hpp>

#include <vector>

struct TileOutputConfig
{
  TileAtlasConfig atlas;
  TileAovChannelMask requestedChannels{TileAovChannelMask::ColorDepth()};
};

struct LidarOutputConfig
{
  std::vector<LidarSensorSpec> sensors;
  LidarVisualizationConfig visualization;
};

struct HeightScanOutputConfig
{
  std::vector<HeightScanSensorSpec> sensors;
  HeightScanVisualizationConfig visualization;
};

struct RendererOutputConfig
{
  TileOutputConfig tile;
  LidarOutputConfig lidar;
  HeightScanOutputConfig heightScan;
};
