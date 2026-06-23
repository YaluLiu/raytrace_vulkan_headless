#include "output_writers.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace headless_training
{
namespace
{
bool HasFlag(uint32_t flags, uint32_t flag)
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

void WriteJsonString(std::ostream& output, const std::string& value)
{
  output << '"';
  for(const char c : value)
  {
    switch(c)
    {
      case '"':
        output << "\\\"";
        break;
      case '\\':
        output << "\\\\";
        break;
      case '\n':
        output << "\\n";
        break;
      case '\r':
        output << "\\r";
        break;
      case '\t':
        output << "\\t";
        break;
      default:
        if(static_cast<unsigned char>(c) < 0x20)
        {
          output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                 << static_cast<int>(static_cast<unsigned char>(c)) << std::dec << std::setfill(' ');
        }
        else
        {
          output << c;
        }
        break;
    }
  }
  output << '"';
}

bool OpenOutputFile(const std::filesystem::path& filePath, std::ofstream& output, std::string* errorMessage)
{
  if(filePath.has_parent_path())
  {
    std::filesystem::create_directories(filePath.parent_path());
  }

  output.open(filePath, std::ios::out | std::ios::trunc);
  if(!output)
  {
    if(errorMessage != nullptr)
    {
      *errorMessage = "failed to open output file: " + filePath.string();
    }
    return false;
  }
  return true;
}
} // namespace

size_t CountLidarPoints(const LidarFramePointCloud& frame)
{
  size_t result = 0;
  for(const LidarSensorPointCloud& sensor : frame.sensors)
  {
    result += sensor.points.size();
  }
  return result;
}

size_t CountHeightScanSamples(const HeightScanFrame& frame)
{
  size_t result = 0;
  for(const HeightScanSensorGrid& sensor : frame.sensors)
  {
    result += sensor.samples.size();
  }
  return result;
}

bool WriteLidarCsv(const LidarFramePointCloud& frame,
                   uint64_t csvFrameId,
                   const std::filesystem::path& filePath,
                   std::string* errorMessage)
{
  try
  {
    std::ofstream output;
    if(!OpenOutputFile(filePath, output, errorMessage))
    {
      return false;
    }

    output << std::setprecision(9);
    output << "frame_id,sensor_name,sensor_index,width,height,point_index,ring_index,beam_index,"
              "x,y,z,range_meters,intensity,flags,valid,hit,out_of_range\n";
    for(const LidarSensorPointCloud& sensor : frame.sensors)
    {
      for(size_t pointIndex = 0; pointIndex < sensor.points.size(); ++pointIndex)
      {
        const LidarPoint& point = sensor.points[pointIndex];
        output << csvFrameId << ',';
        WriteCsvString(output, sensor.name);
        output << ',' << sensor.sensorIndex << ',' << sensor.width << ',' << sensor.height << ',' << pointIndex
               << ',' << point.ringIndex << ',' << point.beamIndex << ',' << point.positionWs.x << ','
               << point.positionWs.y << ',' << point.positionWs.z << ',' << point.rangeMeters << ','
               << point.intensity << ',' << point.flags << ','
               << (HasFlag(point.flags, LidarPointFlagValid) ? 1 : 0) << ','
               << (HasFlag(point.flags, LidarPointFlagHit) ? 1 : 0) << ','
               << (HasFlag(point.flags, LidarPointFlagOutOfRange) ? 1 : 0) << '\n';
      }
    }
    return static_cast<bool>(output);
  }
  catch(const std::exception& e)
  {
    if(errorMessage != nullptr)
    {
      *errorMessage = e.what();
    }
    return false;
  }
}

bool WriteHeightScanCsv(const HeightScanFrame& frame,
                        uint64_t csvFrameId,
                        const std::filesystem::path& filePath,
                        std::string* errorMessage)
{
  try
  {
    std::ofstream output;
    if(!OpenOutputFile(filePath, output, errorMessage))
    {
      return false;
    }

    output << std::setprecision(9);
    output << "frame_id,sensor_name,sensor_index,width,height,sample_index,u_index,v_index,"
              "x,y,z,distance_meters,flags,valid,hit,out_of_range\n";
    for(const HeightScanSensorGrid& sensor : frame.sensors)
    {
      for(size_t sampleIndex = 0; sampleIndex < sensor.samples.size(); ++sampleIndex)
      {
        const HeightScanSample& sample = sensor.samples[sampleIndex];
        output << csvFrameId << ',';
        WriteCsvString(output, sensor.name);
        output << ',' << sensor.sensorIndex << ',' << sensor.width << ',' << sensor.height << ',' << sampleIndex
               << ',' << sample.uIndex << ',' << sample.vIndex << ',' << sample.positionWs.x << ','
               << sample.positionWs.y << ',' << sample.positionWs.z << ',' << sample.distanceMeters << ','
               << sample.flags << ',' << (HasFlag(sample.flags, HeightScanSampleFlagValid) ? 1 : 0) << ','
               << (HasFlag(sample.flags, HeightScanSampleFlagHit) ? 1 : 0) << ','
               << (HasFlag(sample.flags, HeightScanSampleFlagOutOfRange) ? 1 : 0) << '\n';
      }
    }
    return static_cast<bool>(output);
  }
  catch(const std::exception& e)
  {
    if(errorMessage != nullptr)
    {
      *errorMessage = e.what();
    }
    return false;
  }
}

bool WriteManifestJson(const TrainingManifest& manifest,
                       const std::filesystem::path& filePath,
                       std::string* errorMessage)
{
  try
  {
    std::ofstream output;
    if(!OpenOutputFile(filePath, output, errorMessage))
    {
      return false;
    }

    output << "{\n";
    output << "  \"usd\": ";
    WriteJsonString(output, manifest.usdPath);
    output << ",\n  \"width\": " << manifest.width << ",\n";
    output << "  \"height\": " << manifest.height << ",\n";
    output << "  \"requested_frames\": " << manifest.requestedFrames << ",\n";
    output << "  \"rendered_frames\": " << manifest.renderedFrames << ",\n";
    output << "  \"mesh_count\": " << manifest.meshCount << ",\n";
    output << "  \"camera_count\": " << manifest.cameraCount << ",\n";
    output << "  \"lidar_sensor_count\": " << manifest.lidarSensorCount << ",\n";
    output << "  \"height_scan_sensor_count\": " << manifest.heightScanSensorCount << ",\n";
    output << "  \"frames\": [\n";
    for(size_t i = 0; i < manifest.frames.size(); ++i)
    {
      const FrameOutputManifest& frame = manifest.frames[i];
      output << "    {\n";
      output << "      \"frame\": " << frame.frameIndex << ",\n";
      output << "      \"lidar_csv\": ";
      WriteJsonString(output, frame.lidarCsv);
      output << ",\n      \"height_scan_csv\": ";
      WriteJsonString(output, frame.heightScanCsv);
      output << ",\n      \"preview_png\": ";
      WriteJsonString(output, frame.previewPng);
      output << ",\n      \"lidar_point_count\": " << frame.lidarPointCount << ",\n";
      output << "      \"height_scan_sample_count\": " << frame.heightScanSampleCount << "\n";
      output << "    }" << (i + 1 == manifest.frames.size() ? "\n" : ",\n");
    }
    output << "  ]\n";
    output << "}\n";
    return static_cast<bool>(output);
  }
  catch(const std::exception& e)
  {
    if(errorMessage != nullptr)
    {
      *errorMessage = e.what();
    }
    return false;
  }
}

std::string FormatFrameFileName(const std::string& prefix, uint64_t frameIndex, const std::string& extension)
{
  std::ostringstream output;
  output << prefix << '_' << std::setw(6) << std::setfill('0') << frameIndex << extension;
  return output.str();
}

} // namespace headless_training
