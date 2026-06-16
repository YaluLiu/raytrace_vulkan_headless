#include "lidarPointCloudCsv.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <exception>

PXR_NAMESPACE_OPEN_SCOPE
namespace hdrobot {
namespace
{
bool HasLidarPointFlag(uint32_t flags, uint32_t flag)
{
  return (flags & flag) != 0;
}

void WriteCsvString(std::ostream& output, const std::string& value)
{
  if(value.find_first_of(",\"\n\r") == std::string::npos)
  {
    output << value;
    return;
  }

  output << '"';
  for(const char c : value)
  {
    if(c == '"')
    {
      output << "\"\"";
    }
    else
    {
      output << c;
    }
  }
  output << '"';
}
} // namespace

size_t CountLidarPoints(const ::LidarFramePointCloud& frame)
{
  size_t result = 0;
  for(const ::LidarSensorPointCloud& sensor : frame.sensors)
  {
    result += sensor.points.size();
  }
  return result;
}

::LidarFramePointCloud SelectLidarPointCloudSensors(const ::LidarFramePointCloud& frame,
                                                    bool allSensors,
                                                    uint32_t sensorIndex)
{
  if(allSensors)
  {
    return frame;
  }

  ::LidarFramePointCloud result;
  result.frameId = frame.frameId;
  if(sensorIndex < frame.sensors.size())
  {
    result.sensors.push_back(frame.sensors[sensorIndex]);
  }
  return result;
}

bool WriteLidarPointCloudCsv(const ::LidarFramePointCloud& frame,
                             uint64_t csvFrameId,
                             const std::string& filePath,
                             std::string* errorMessage)
{
  try
  {
    const std::filesystem::path outputPath(filePath);
    if(outputPath.has_parent_path())
    {
      std::filesystem::create_directories(outputPath.parent_path());
    }

    std::ofstream output(outputPath, std::ios::out | std::ios::trunc);
    if(!output)
    {
      if(errorMessage != nullptr)
      {
        *errorMessage = "failed to open output file";
      }
      return false;
    }

    output << std::setprecision(9);
    output << "frame_id,sensor_name,sensor_index,width,height,point_index,ring_index,beam_index,"
              "x,y,z,range_meters,intensity,flags,valid,hit,out_of_range\n";

    for(const ::LidarSensorPointCloud& sensor : frame.sensors)
    {
      for(size_t pointIndex = 0; pointIndex < sensor.points.size(); ++pointIndex)
      {
        const ::LidarPoint& point = sensor.points[pointIndex];
        output << csvFrameId << ',';
        WriteCsvString(output, sensor.name);
        output << ',' << sensor.sensorIndex << ',' << sensor.width << ',' << sensor.height << ',' << pointIndex
               << ',' << point.ringIndex << ',' << point.beamIndex << ',' << point.positionWs.x << ','
               << point.positionWs.y << ',' << point.positionWs.z << ',' << point.rangeMeters << ','
               << point.intensity << ',' << point.flags << ','
               << (HasLidarPointFlag(point.flags, LidarPointFlagValid) ? 1 : 0) << ','
               << (HasLidarPointFlag(point.flags, LidarPointFlagHit) ? 1 : 0) << ','
               << (HasLidarPointFlag(point.flags, LidarPointFlagOutOfRange) ? 1 : 0) << '\n';
      }
    }

    if(!output)
    {
      if(errorMessage != nullptr)
      {
        *errorMessage = "failed while writing output file";
      }
      return false;
    }
  }
  catch(const std::exception& e)
  {
    if(errorMessage != nullptr)
    {
      *errorMessage = e.what();
    }
    return false;
  }

  return true;
}

} // namespace hdrobot
PXR_NAMESPACE_CLOSE_SCOPE
