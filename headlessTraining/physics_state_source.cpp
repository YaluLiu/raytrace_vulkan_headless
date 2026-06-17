#include "physics_state_source.h"

#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace headless_training
{
namespace
{
struct JsonValue
{
  enum class Type
  {
    Null,
    Object,
    Array,
    String,
    Number,
    Bool,
  };

  Type type{Type::Null};
  std::map<std::string, JsonValue> object;
  std::vector<JsonValue> array;
  std::string string;
  double number{0.0};
  bool boolean{false};
};

class JsonParser
{
public:
  explicit JsonParser(std::string text)
      : m_text(std::move(text))
  {
  }

  JsonValue parse()
  {
    JsonValue value = parseValue();
    skipWhitespace();
    if(m_pos != m_text.size())
    {
      fail("unexpected trailing content");
    }
    return value;
  }

private:
  std::string m_text;
  size_t m_pos{0};

  [[noreturn]] void fail(const std::string& message) const
  {
    throw std::runtime_error("invalid physics replay JSON near byte " + std::to_string(m_pos) + ": " + message);
  }

  void skipWhitespace()
  {
    while(m_pos < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_pos])) != 0)
    {
      ++m_pos;
    }
  }

  char peek()
  {
    skipWhitespace();
    if(m_pos >= m_text.size())
    {
      fail("unexpected end of input");
    }
    return m_text[m_pos];
  }

  bool consume(char expected)
  {
    skipWhitespace();
    if(m_pos < m_text.size() && m_text[m_pos] == expected)
    {
      ++m_pos;
      return true;
    }
    return false;
  }

  void expect(char expected)
  {
    if(!consume(expected))
    {
      fail(std::string("expected '") + expected + "'");
    }
  }

  JsonValue parseValue()
  {
    const char c = peek();
    if(c == '{')
    {
      return parseObject();
    }
    if(c == '[')
    {
      return parseArray();
    }
    if(c == '"')
    {
      JsonValue value;
      value.type = JsonValue::Type::String;
      value.string = parseString();
      return value;
    }
    if(c == '-' || std::isdigit(static_cast<unsigned char>(c)) != 0)
    {
      return parseNumber();
    }
    if(matchLiteral("true"))
    {
      JsonValue value;
      value.type = JsonValue::Type::Bool;
      value.boolean = true;
      return value;
    }
    if(matchLiteral("false"))
    {
      JsonValue value;
      value.type = JsonValue::Type::Bool;
      value.boolean = false;
      return value;
    }
    if(matchLiteral("null"))
    {
      return {};
    }
    fail("expected JSON value");
  }

  bool matchLiteral(const char* literal)
  {
    skipWhitespace();
    const std::string_view lit(literal);
    if(m_text.compare(m_pos, lit.size(), lit) == 0)
    {
      m_pos += lit.size();
      return true;
    }
    return false;
  }

  JsonValue parseObject()
  {
    JsonValue value;
    value.type = JsonValue::Type::Object;
    expect('{');
    if(consume('}'))
    {
      return value;
    }
    while(true)
    {
      if(peek() != '"')
      {
        fail("expected object key string");
      }
      std::string key = parseString();
      expect(':');
      value.object.emplace(std::move(key), parseValue());
      if(consume('}'))
      {
        return value;
      }
      expect(',');
    }
  }

  JsonValue parseArray()
  {
    JsonValue value;
    value.type = JsonValue::Type::Array;
    expect('[');
    if(consume(']'))
    {
      return value;
    }
    while(true)
    {
      value.array.push_back(parseValue());
      if(consume(']'))
      {
        return value;
      }
      expect(',');
    }
  }

  std::string parseString()
  {
    expect('"');
    std::string result;
    while(m_pos < m_text.size())
    {
      const char c = m_text[m_pos++];
      if(c == '"')
      {
        return result;
      }
      if(c != '\\')
      {
        result.push_back(c);
        continue;
      }
      if(m_pos >= m_text.size())
      {
        fail("unterminated escape sequence");
      }
      const char escaped = m_text[m_pos++];
      switch(escaped)
      {
        case '"':
        case '\\':
        case '/':
          result.push_back(escaped);
          break;
        case 'b':
          result.push_back('\b');
          break;
        case 'f':
          result.push_back('\f');
          break;
        case 'n':
          result.push_back('\n');
          break;
        case 'r':
          result.push_back('\r');
          break;
        case 't':
          result.push_back('\t');
          break;
        default:
          fail("unsupported string escape");
      }
    }
    fail("unterminated string");
  }

  JsonValue parseNumber()
  {
    skipWhitespace();
    const size_t start = m_pos;

    if(m_pos < m_text.size() && m_text[m_pos] == '-')
    {
      ++m_pos;
    }

    if(m_pos >= m_text.size())
    {
      fail("unterminated number");
    }

    if(m_text[m_pos] == '0')
    {
      ++m_pos;
      if(m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos])) != 0)
      {
        fail("JSON numbers cannot contain leading zeroes");
      }
    }
    else if(m_text[m_pos] >= '1' && m_text[m_pos] <= '9')
    {
      while(m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos])) != 0)
      {
        ++m_pos;
      }
    }
    else
    {
      fail("expected JSON number");
    }

    if(m_pos < m_text.size() && m_text[m_pos] == '.')
    {
      ++m_pos;
      if(m_pos >= m_text.size() || std::isdigit(static_cast<unsigned char>(m_text[m_pos])) == 0)
      {
        fail("JSON number fraction requires at least one digit");
      }
      while(m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos])) != 0)
      {
        ++m_pos;
      }
    }

    if(m_pos < m_text.size() && (m_text[m_pos] == 'e' || m_text[m_pos] == 'E'))
    {
      ++m_pos;
      if(m_pos < m_text.size() && (m_text[m_pos] == '+' || m_text[m_pos] == '-'))
      {
        ++m_pos;
      }
      if(m_pos >= m_text.size() || std::isdigit(static_cast<unsigned char>(m_text[m_pos])) == 0)
      {
        fail("JSON number exponent requires at least one digit");
      }
      while(m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos])) != 0)
      {
        ++m_pos;
      }
    }

    const std::string token = m_text.substr(start, m_pos - start);
    errno = 0;
    const double parsed = std::strtod(token.c_str(), nullptr);
    if(errno == ERANGE || !std::isfinite(parsed))
    {
      fail("JSON number is outside the finite double range");
    }

    JsonValue value;
    value.type = JsonValue::Type::Number;
    value.number = parsed;
    return value;
  }
};

const JsonValue& RequireMember(const JsonValue& value, const std::string& name)
{
  if(value.type != JsonValue::Type::Object)
  {
    throw std::runtime_error("expected JSON object while reading member " + name);
  }
  const auto it = value.object.find(name);
  if(it == value.object.end())
  {
    throw std::runtime_error("physics replay missing required member: " + name);
  }
  return it->second;
}

const JsonValue* OptionalMember(const JsonValue& value, const std::string& name)
{
  if(value.type != JsonValue::Type::Object)
  {
    return nullptr;
  }
  const auto it = value.object.find(name);
  return it == value.object.end() ? nullptr : &it->second;
}

std::string RequireString(const JsonValue& value, const std::string& name)
{
  const JsonValue& member = RequireMember(value, name);
  if(member.type != JsonValue::Type::String)
  {
    throw std::runtime_error("physics replay member must be a string: " + name);
  }
  return member.string;
}

bool OptionalBool(const JsonValue& value, const std::string& name, bool fallback)
{
  const JsonValue* member = OptionalMember(value, name);
  if(member == nullptr)
  {
    return fallback;
  }
  if(member->type != JsonValue::Type::Bool)
  {
    throw std::runtime_error("physics replay member must be a bool: " + name);
  }
  return member->boolean;
}

std::array<double, 16> RequireMatrixRows(const JsonValue& value)
{
  const JsonValue& matrix = RequireMember(value, "matrix");
  if(matrix.type != JsonValue::Type::Array || matrix.array.size() != 16)
  {
    throw std::runtime_error("physics replay matrix must be an array with 16 numbers");
  }

  std::array<double, 16> rows{};
  for(size_t i = 0; i < rows.size(); ++i)
  {
    if(matrix.array[i].type != JsonValue::Type::Number)
    {
      throw std::runtime_error("physics replay matrix entries must be numbers");
    }
    rows[i] = matrix.array[i].number;
  }
  return rows;
}

struct ReplayFrame
{
  std::vector<InstancePoseUpdate> updates;
};

class ReplayPhysicsStateSource final : public PhysicsStateSource
{
public:
  explicit ReplayPhysicsStateSource(std::vector<ReplayFrame> frames)
      : m_frames(std::move(frames))
  {
  }

  bool nextFrame(std::vector<InstancePoseUpdate>& updates) override
  {
    if(m_nextFrame >= m_frames.size())
    {
      updates.clear();
      return false;
    }
    updates = m_frames[m_nextFrame].updates;
    ++m_nextFrame;
    return true;
  }

private:
  std::vector<ReplayFrame> m_frames;
  size_t m_nextFrame{0};
};

std::string ReadTextFile(const std::filesystem::path& path)
{
  std::ifstream input(path);
  if(!input)
  {
    throw std::runtime_error("failed to open physics replay: " + path.string());
  }
  std::ostringstream content;
  content << input.rdbuf();
  return content.str();
}
} // namespace

std::unique_ptr<PhysicsStateSource> LoadPhysicsReplay(const std::filesystem::path& replayPath)
{
  JsonParser parser(ReadTextFile(replayPath));
  const JsonValue root = parser.parse();
  const JsonValue& framesValue = RequireMember(root, "frames");
  if(framesValue.type != JsonValue::Type::Array)
  {
    throw std::runtime_error("physics replay frames must be an array");
  }

  std::vector<ReplayFrame> frames;
  frames.reserve(framesValue.array.size());
  for(const JsonValue& frameValue : framesValue.array)
  {
    const JsonValue& instancesValue = RequireMember(frameValue, "instances");
    if(instancesValue.type != JsonValue::Type::Array)
    {
      throw std::runtime_error("physics replay frame instances must be an array");
    }

    ReplayFrame frame;
    frame.updates.reserve(instancesValue.array.size());
    for(const JsonValue& instanceValue : instancesValue.array)
    {
      InstancePoseUpdate update;
      update.name = RequireString(instanceValue, "name");
      update.worldTransform = MakeEngineTransformFromUsdRows(RequireMatrixRows(instanceValue));
      update.visible = OptionalBool(instanceValue, "visible", true);
      frame.updates.push_back(update);
    }
    frames.push_back(std::move(frame));
  }

  return std::make_unique<ReplayPhysicsStateSource>(std::move(frames));
}

} // namespace headless_training
