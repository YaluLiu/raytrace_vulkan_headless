#include "../lidarPointCloudCsv.h"

#include <pxr/pxr.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

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

std::string ReadFile(const std::filesystem::path& path)
{
  std::ifstream input(path);
  std::ostringstream content;
  content << input.rdbuf();
  return content.str();
}
} // namespace

int main()
{
  ::LidarFramePointCloud frame;
  frame.frameId = 2;

  ::LidarSensorPointCloud sensor;
  sensor.name = "front,lidar";
  sensor.sensorIndex = 7;
  sensor.width = 3;
  sensor.height = 1;

  ::LidarPoint point;
  point.positionWs = glm::vec3(1.25f, 2.5f, 3.75f);
  point.rangeMeters = 4.5f;
  point.sensorIndex = sensor.sensorIndex;
  point.ringIndex = 0;
  point.beamIndex = 2;
  point.flags = LidarPointFlagValid | LidarPointFlagHit;
  point.intensity = 0.75f;
  sensor.points.push_back(point);
  frame.sensors.push_back(sensor);

  ::LidarSensorPointCloud otherSensor;
  otherSensor.name = "rear_lidar";
  otherSensor.sensorIndex = 8;
  otherSensor.width = 1;
  otherSensor.height = 1;
  otherSensor.points.push_back(point);
  frame.sensors.push_back(otherSensor);

  const ::LidarFramePointCloud selectedFrame =
      PXR_NS::hdrobot::SelectLidarPointCloudSensors(frame, false, 0);
  Require(selectedFrame.frameId == frame.frameId, "Selected frame did not preserve frame_id");
  Require(selectedFrame.sensors.size() == 1, "Selected frame should contain one sensor");
  Require(selectedFrame.sensors[0].name == sensor.name, "Selected frame picked the wrong sensor");

  const std::filesystem::path outputPath =
      std::filesystem::temp_directory_path() / "hdrobot_lidar_point_cloud_csv_test.csv";
  std::filesystem::remove(outputPath);

  std::string errorMessage;
  Require(PXR_NS::hdrobot::WriteLidarPointCloudCsv(selectedFrame,
                                                   selectedFrame.frameId,
                                                   outputPath.string(),
                                                   &errorMessage),
          errorMessage.c_str());

  const std::string content = ReadFile(outputPath);
  std::filesystem::remove(outputPath);

  const std::string expected =
      "frame_id,sensor_name,sensor_index,width,height,point_index,ring_index,beam_index,"
      "x,y,z,range_meters,intensity,flags,valid,hit,out_of_range\n"
      "2,\"front,lidar\",7,3,1,0,0,2,1.25,2.5,3.75,4.5,0.75,3,1,1,0\n";

  Require(content == expected, "CSV content did not match expected selected-sensor frame_id output");
  return 0;
}
