#include "unitree_robot_view_toolbox.h"

#include <PlotJuggler/plotdata.h>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QOpenGLFunctions_2_0>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

#include <dlfcn.h>

#ifndef PJ_UNITREE_GO2_SOURCE_DIR
#define PJ_UNITREE_GO2_SOURCE_DIR ""
#endif

namespace plotjuggler_unitree_sdk2
{
namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kMaxViewPitch = kPi * 0.5 - 0.01;

QIcon makeCloseIcon(const QColor& color)
{
  QPixmap pixmap(18, 18);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.drawLine(QPointF(5.0, 5.0), QPointF(13.0, 13.0));
  painter.drawLine(QPointF(13.0, 5.0), QPointF(5.0, 13.0));
  return QIcon(pixmap);
}

struct Vec3
{
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

Vec3 operator+(const Vec3& lhs, const Vec3& rhs)
{
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vec3 operator-(const Vec3& lhs, const Vec3& rhs)
{
  return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vec3 operator*(const Vec3& value, double scale)
{
  return {value.x * scale, value.y * scale, value.z * scale};
}

double dot(const Vec3& lhs, const Vec3& rhs)
{
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs)
{
  return {
      lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.z * rhs.x - lhs.x * rhs.z,
      lhs.x * rhs.y - lhs.y * rhs.x,
  };
}

Vec3 normalized(Vec3 value)
{
  const double length = std::sqrt(dot(value, value));
  if (length < 1e-9)
  {
    return {0.0, 0.0, 1.0};
  }
  return value * (1.0 / length);
}

struct Mat4
{
  std::array<double, 16> m = {
      1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
  };

  static Mat4 identity() { return {}; }

  static Mat4 translation(const Vec3& value)
  {
    Mat4 result;
    result.m[3] = value.x;
    result.m[7] = value.y;
    result.m[11] = value.z;
    return result;
  }

  static Mat4 scale(const Vec3& value)
  {
    Mat4 result;
    result.m[0] = value.x;
    result.m[5] = value.y;
    result.m[10] = value.z;
    return result;
  }

  static Mat4 rotationX(double angle)
  {
    Mat4 result;
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    result.m[5] = c;
    result.m[6] = -s;
    result.m[9] = s;
    result.m[10] = c;
    return result;
  }

  static Mat4 rotationY(double angle)
  {
    Mat4 result;
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    result.m[0] = c;
    result.m[2] = s;
    result.m[8] = -s;
    result.m[10] = c;
    return result;
  }

  static Mat4 rotationZ(double angle)
  {
    Mat4 result;
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    result.m[0] = c;
    result.m[1] = -s;
    result.m[4] = s;
    result.m[5] = c;
    return result;
  }

  static Mat4 rpy(double roll, double pitch, double yaw)
  {
    return rotationZ(yaw) * rotationY(pitch) * rotationX(roll);
  }

  static Mat4 axisAngle(Vec3 axis, double angle)
  {
    axis = normalized(axis);
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    const double t = 1.0 - c;

    Mat4 result;
    result.m[0] = t * axis.x * axis.x + c;
    result.m[1] = t * axis.x * axis.y - s * axis.z;
    result.m[2] = t * axis.x * axis.z + s * axis.y;

    result.m[4] = t * axis.x * axis.y + s * axis.z;
    result.m[5] = t * axis.y * axis.y + c;
    result.m[6] = t * axis.y * axis.z - s * axis.x;

    result.m[8] = t * axis.x * axis.z - s * axis.y;
    result.m[9] = t * axis.y * axis.z + s * axis.x;
    result.m[10] = t * axis.z * axis.z + c;
    return result;
  }

  Vec3 transformPoint(const Vec3& point) const
  {
    return {
        m[0] * point.x + m[1] * point.y + m[2] * point.z + m[3],
        m[4] * point.x + m[5] * point.y + m[6] * point.z + m[7],
        m[8] * point.x + m[9] * point.y + m[10] * point.z + m[11],
    };
  }

  Mat4 operator*(const Mat4& other) const
  {
    Mat4 result;
    for (int row = 0; row < 4; ++row)
    {
      for (int col = 0; col < 4; ++col)
      {
        result.m[row * 4 + col] = 0.0;
        for (int k = 0; k < 4; ++k)
        {
          result.m[row * 4 + col] += m[row * 4 + k] * other.m[k * 4 + col];
        }
      }
    }
    return result;
  }
};

struct ModelConfig
{
  QString key;
  QString label;
  QString asset_directory;
  QString urdf_relative_path;
  QString package_name;
  int motor_count = 12;
  QString mapping_family;
};

struct Triangle
{
  Vec3 a;
  Vec3 b;
  Vec3 c;
};

struct Mesh
{
  QVector<Triangle> triangles;
  Vec3 bounds_min;
  Vec3 bounds_max;
  bool has_bounds = false;
  bool valid = false;
  mutable GLuint gl_display_list = 0;
  QString error;
};

struct LinkVisual
{
  QString mesh_filename;
  Mat4 origin;
  Vec3 scale = {1.0, 1.0, 1.0};
  QColor color = QColor(228, 232, 238, 255);
};

struct LinkModel
{
  QString name;
  QVector<LinkVisual> visuals;
};

struct JointModel
{
  QString name;
  QString type;
  QString parent;
  QString child;
  Vec3 axis = {0.0, 0.0, 1.0};
  Mat4 origin;
  int motor_index = -1;
};

struct RobotSample
{
  std::array<double, 35> q = {};
  int motor_count = 0;
  int expected_motor_count = 12;
  Vec3 rpy;
  bool has_imu = false;
  double latest_time = 0.0;
};

struct TimeRange
{
  double min = 0.0;
  double max = 0.0;
  bool valid = false;
};

QStringList splitNumbers(const QString& text)
{
  static const QRegularExpression whitespace("\\s+");
  return text.trimmed().split(whitespace, PJ::SkipEmptyParts);
}

Vec3 parseVec3(const QString& text, const Vec3& fallback = {})
{
  const QStringList parts = splitNumbers(text);
  if (parts.size() < 3)
  {
    return fallback;
  }
  return {parts[0].toDouble(), parts[1].toDouble(), parts[2].toDouble()};
}

Mat4 parseOrigin(const QDomElement& parent)
{
  const QDomElement origin = parent.firstChildElement("origin");
  if (origin.isNull())
  {
    return Mat4::identity();
  }
  const Vec3 xyz = parseVec3(origin.attribute("xyz"));
  const Vec3 rpy = parseVec3(origin.attribute("rpy"));
  return Mat4::translation(xyz) * Mat4::rpy(rpy.x, rpy.y, rpy.z);
}

Vec3 parseScale(const QString& text)
{
  const QStringList parts = splitNumbers(text);
  if (parts.size() < 3)
  {
    return {1.0, 1.0, 1.0};
  }
  return {parts[0].toDouble(), parts[1].toDouble(), parts[2].toDouble()};
}

QColor parseMaterialColor(const QDomElement& visual)
{
  const QDomElement material = visual.firstChildElement("material");
  if (material.isNull())
  {
    return QColor(228, 232, 238, 255);
  }

  const QDomElement color = material.firstChildElement("color");
  const QStringList rgba = splitNumbers(color.attribute("rgba"));
  if (rgba.size() < 3)
  {
    return QColor(228, 232, 238, 255);
  }

  QColor result;
  result.setRgbF(std::clamp(rgba[0].toDouble(), 0.0, 1.0), std::clamp(rgba[1].toDouble(), 0.0, 1.0),
                 std::clamp(rgba[2].toDouble(), 0.0, 1.0),
                 rgba.size() >= 4 ? std::clamp(rgba[3].toDouble(), 0.88, 1.0) : 1.0);
  return result;
}

QVector<double> parseDoubleArray(const QString& text, int reserve_hint)
{
  QVector<double> values;
  values.reserve(std::max(0, reserve_hint));
  const QByteArray bytes = text.toLatin1();
  const char* cursor = bytes.constData();

  while (*cursor != '\0')
  {
    char* end = nullptr;
    const double value = std::strtod(cursor, &end);
    if (end == cursor)
    {
      ++cursor;
      continue;
    }
    values.push_back(value);
    cursor = end;
  }
  return values;
}

QVector<int> parseIntArray(const QString& text, int reserve_hint = 0)
{
  QVector<int> values;
  values.reserve(std::max(0, reserve_hint));
  const QByteArray bytes = text.toLatin1();
  const char* cursor = bytes.constData();

  while (*cursor != '\0')
  {
    char* end = nullptr;
    const long value = std::strtol(cursor, &end, 10);
    if (end == cursor)
    {
      ++cursor;
      continue;
    }
    values.push_back(static_cast<int>(value));
    cursor = end;
  }
  return values;
}

QString withoutFragmentPrefix(QString value)
{
  if (value.startsWith('#'))
  {
    value.remove(0, 1);
  }
  return value;
}

Mat4 parseColladaMatrix(const QString& text)
{
  const QVector<double> values = parseDoubleArray(text, 16);
  if (values.size() < 16)
  {
    return Mat4::identity();
  }

  Mat4 result;
  for (int index = 0; index < 16; ++index)
  {
    result.m[static_cast<size_t>(index)] = values[index];
  }
  return result;
}

Mat4 parseColladaNodeTransform(const QDomElement& node)
{
  Mat4 result = Mat4::identity();
  for (QDomNode child = node.firstChild(); !child.isNull(); child = child.nextSibling())
  {
    const QDomElement element = child.toElement();
    if (element.isNull())
    {
      continue;
    }

    if (element.tagName() == "matrix")
    {
      result = result * parseColladaMatrix(element.text());
    }
    else if (element.tagName() == "translate")
    {
      result = result * Mat4::translation(parseVec3(element.text()));
    }
    else if (element.tagName() == "rotate")
    {
      const QStringList values = splitNumbers(element.text());
      if (values.size() >= 4)
      {
        result = result *
                 Mat4::axisAngle({values[0].toDouble(), values[1].toDouble(), values[2].toDouble()},
                                 values[3].toDouble() * kPi / 180.0);
      }
    }
    else if (element.tagName() == "scale")
    {
      result = result * Mat4::scale(parseScale(element.text()));
    }
  }
  return result;
}

void collectColladaGeometryTransformsRecursive(const QDomElement& node,
                                               const Mat4& parent_transform,
                                               QHash<QString, QVector<Mat4>>* transforms)
{
  const Mat4 node_transform = parent_transform * parseColladaNodeTransform(node);
  for (QDomNode child = node.firstChild(); !child.isNull(); child = child.nextSibling())
  {
    const QDomElement element = child.toElement();
    if (element.isNull())
    {
      continue;
    }

    if (element.tagName() == "instance_geometry")
    {
      const QString geometry_id = withoutFragmentPrefix(element.attribute("url"));
      if (!geometry_id.isEmpty())
      {
        (*transforms)[geometry_id].push_back(node_transform);
      }
    }
    else if (element.tagName() == "node")
    {
      collectColladaGeometryTransformsRecursive(element, node_transform, transforms);
    }
  }
}

QHash<QString, QVector<Mat4>> collectColladaGeometryTransforms(const QDomDocument& doc)
{
  QHash<QString, QVector<Mat4>> transforms;
  const QDomNodeList nodes = doc.elementsByTagName("visual_scene");
  for (int scene_index = 0; scene_index < nodes.size(); ++scene_index)
  {
    const QDomElement scene = nodes.at(scene_index).toElement();
    for (QDomNode child = scene.firstChild(); !child.isNull(); child = child.nextSibling())
    {
      const QDomElement node = child.toElement();
      if (node.tagName() == "node")
      {
        collectColladaGeometryTransformsRecursive(node, Mat4::identity(), &transforms);
      }
    }
  }
  return transforms;
}

QString ancestorGeometryId(QDomElement element)
{
  for (QDomNode node = element; !node.isNull(); node = node.parentNode())
  {
    const QDomElement candidate = node.toElement();
    if (candidate.tagName() == "geometry")
    {
      return candidate.attribute("id");
    }
  }
  return {};
}

void updateBounds(Mesh* mesh, const Vec3& point)
{
  if (!mesh->has_bounds)
  {
    mesh->bounds_min = point;
    mesh->bounds_max = point;
    mesh->has_bounds = true;
    return;
  }
  mesh->bounds_min.x = std::min(mesh->bounds_min.x, point.x);
  mesh->bounds_min.y = std::min(mesh->bounds_min.y, point.y);
  mesh->bounds_min.z = std::min(mesh->bounds_min.z, point.z);
  mesh->bounds_max.x = std::max(mesh->bounds_max.x, point.x);
  mesh->bounds_max.y = std::max(mesh->bounds_max.y, point.y);
  mesh->bounds_max.z = std::max(mesh->bounds_max.z, point.z);
}

void addTriangle(Mesh* mesh, const QVector<Vec3>& positions, int a, int b, int c,
                 const Mat4& transform = Mat4::identity())
{
  if (a < 0 || b < 0 || c < 0 || a >= positions.size() || b >= positions.size() ||
      c >= positions.size())
  {
    return;
  }

  const Vec3 transformed_a = transform.transformPoint(positions[a]);
  const Vec3 transformed_b = transform.transformPoint(positions[b]);
  const Vec3 transformed_c = transform.transformPoint(positions[c]);
  updateBounds(mesh, transformed_a);
  updateBounds(mesh, transformed_b);
  updateBounds(mesh, transformed_c);
  mesh->triangles.push_back({transformed_a, transformed_b, transformed_c});
}

bool loadColladaMesh(const QString& path, Mesh* mesh)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
  {
    mesh->error = QString("mesh not readable: %1").arg(path);
    return false;
  }

  QDomDocument doc;
  QString error_message;
  int error_line = 0;
  int error_column = 0;
  if (!doc.setContent(&file, false, &error_message, &error_line, &error_column))
  {
    mesh->error =
        QString("mesh XML error at %1:%2: %3").arg(error_line).arg(error_column).arg(error_message);
    return false;
  }

  const QHash<QString, QVector<Mat4>> geometry_transforms = collectColladaGeometryTransforms(doc);

  QHash<QString, QVector<Vec3>> positions_by_source;
  const QDomNodeList sources = doc.elementsByTagName("source");
  for (int index = 0; index < sources.size(); ++index)
  {
    const QDomElement source = sources.at(index).toElement();
    const QString source_id = source.attribute("id");
    const QDomElement float_array = source.firstChildElement("float_array");
    if (source_id.isEmpty() || float_array.isNull())
    {
      continue;
    }

    const int count = float_array.attribute("count").toInt();
    const QDomElement accessor =
        source.firstChildElement("technique_common").firstChildElement("accessor");
    const int stride = std::max(3, accessor.attribute("stride", "3").toInt());
    const QVector<double> values = parseDoubleArray(float_array.text(), count);

    QVector<Vec3> positions;
    positions.reserve(values.size() / stride);
    for (int value_index = 0; value_index + 2 < values.size(); value_index += stride)
    {
      positions.push_back({values[value_index], values[value_index + 1], values[value_index + 2]});
    }
    if (!positions.isEmpty())
    {
      positions_by_source.insert(source_id, positions);
    }
  }

  QHash<QString, QString> vertices_to_position_source;
  const QDomNodeList vertices_nodes = doc.elementsByTagName("vertices");
  for (int index = 0; index < vertices_nodes.size(); ++index)
  {
    const QDomElement vertices = vertices_nodes.at(index).toElement();
    const QString vertices_id = vertices.attribute("id");
    const QDomNodeList inputs = vertices.elementsByTagName("input");
    for (int input_index = 0; input_index < inputs.size(); ++input_index)
    {
      const QDomElement input = inputs.at(input_index).toElement();
      if (input.attribute("semantic") == "POSITION")
      {
        vertices_to_position_source.insert(vertices_id,
                                           withoutFragmentPrefix(input.attribute("source")));
      }
    }
  }

  auto positionSourceForPrimitive = [&](const QDomElement& primitive, int* vertex_offset,
                                        int* stride) -> QString
  {
    QString position_source;
    *vertex_offset = 0;
    *stride = 0;
    const QDomNodeList inputs = primitive.elementsByTagName("input");
    for (int input_index = 0; input_index < inputs.size(); ++input_index)
    {
      const QDomElement input = inputs.at(input_index).toElement();
      const int offset = input.attribute("offset", "0").toInt();
      *stride = std::max(*stride, offset + 1);
      if (input.attribute("semantic") != "VERTEX")
      {
        continue;
      }
      *vertex_offset = offset;
      const QString vertices_id = withoutFragmentPrefix(input.attribute("source"));
      position_source = vertices_to_position_source.value(vertices_id, vertices_id);
    }
    return position_source;
  };

  const QDomNodeList triangles = doc.elementsByTagName("triangles");
  for (int index = 0; index < triangles.size(); ++index)
  {
    const QDomElement triangle_element = triangles.at(index).toElement();
    const QString geometry_id = ancestorGeometryId(triangle_element);
    QVector<Mat4> transforms = geometry_transforms.value(geometry_id);
    if (transforms.isEmpty())
    {
      transforms.push_back(Mat4::identity());
    }

    int vertex_offset = 0;
    int stride = 0;
    const QString source_id = positionSourceForPrimitive(triangle_element, &vertex_offset, &stride);
    if (source_id.isEmpty() || stride <= 0 || !positions_by_source.contains(source_id))
    {
      continue;
    }

    const QVector<Vec3>& positions = positions_by_source[source_id];
    const int count_hint = triangle_element.attribute("count").toInt() * stride * 3;
    const QVector<int> indices =
        parseIntArray(triangle_element.firstChildElement("p").text(), count_hint);
    for (int cursor = 0; cursor + (3 * stride) <= indices.size(); cursor += 3 * stride)
    {
      for (const Mat4& transform : transforms)
      {
        addTriangle(mesh, positions, indices[cursor + vertex_offset],
                    indices[cursor + stride + vertex_offset],
                    indices[cursor + 2 * stride + vertex_offset], transform);
      }
    }
  }

  const QDomNodeList polylists = doc.elementsByTagName("polylist");
  for (int index = 0; index < polylists.size(); ++index)
  {
    const QDomElement polylist = polylists.at(index).toElement();
    const QString geometry_id = ancestorGeometryId(polylist);
    QVector<Mat4> transforms = geometry_transforms.value(geometry_id);
    if (transforms.isEmpty())
    {
      transforms.push_back(Mat4::identity());
    }

    int vertex_offset = 0;
    int stride = 0;
    const QString source_id = positionSourceForPrimitive(polylist, &vertex_offset, &stride);
    if (source_id.isEmpty() || stride <= 0 || !positions_by_source.contains(source_id))
    {
      continue;
    }

    const QVector<Vec3>& positions = positions_by_source[source_id];
    const QVector<int> vcounts = parseIntArray(polylist.firstChildElement("vcount").text());
    const QVector<int> indices = parseIntArray(polylist.firstChildElement("p").text());
    int cursor = 0;
    for (int vertex_count : vcounts)
    {
      if (cursor + vertex_count * stride > indices.size())
      {
        break;
      }
      if (vertex_count >= 3)
      {
        const int first = indices[cursor + vertex_offset];
        for (int vertex = 1; vertex + 1 < vertex_count; ++vertex)
        {
          for (const Mat4& transform : transforms)
          {
            addTriangle(mesh, positions, first, indices[cursor + vertex * stride + vertex_offset],
                        indices[cursor + (vertex + 1) * stride + vertex_offset], transform);
          }
        }
      }
      cursor += vertex_count * stride;
    }
  }

  mesh->valid = !mesh->triangles.isEmpty();
  if (!mesh->valid)
  {
    mesh->error = QString("mesh has no drawable triangles: %1").arg(path);
  }
  return mesh->valid;
}

float readFloatLE(const char* data)
{
  float value = 0.0F;
  std::memcpy(&value, data, sizeof(float));
  return value;
}

uint32_t readUInt32LE(const char* data)
{
  uint32_t value = 0;
  std::memcpy(&value, data, sizeof(uint32_t));
  return value;
}

bool loadBinaryStlMesh(const QByteArray& bytes, Mesh* mesh)
{
  if (bytes.size() < 84)
  {
    return false;
  }

  const uint32_t triangle_count = readUInt32LE(bytes.constData() + 80);
  const qsizetype expected_size = 84 + static_cast<qsizetype>(triangle_count) * 50;
  if (expected_size > bytes.size())
  {
    return false;
  }

  mesh->triangles.reserve(std::min<uint32_t>(triangle_count, 120000));
  const char* cursor = bytes.constData() + 84;
  for (uint32_t index = 0; index < triangle_count; ++index)
  {
    cursor += 12; // normal
    const Vec3 a = {readFloatLE(cursor), readFloatLE(cursor + 4), readFloatLE(cursor + 8)};
    cursor += 12;
    const Vec3 b = {readFloatLE(cursor), readFloatLE(cursor + 4), readFloatLE(cursor + 8)};
    cursor += 12;
    const Vec3 c = {readFloatLE(cursor), readFloatLE(cursor + 4), readFloatLE(cursor + 8)};
    cursor += 14; // third vertex + attribute byte count

    updateBounds(mesh, a);
    updateBounds(mesh, b);
    updateBounds(mesh, c);
    mesh->triangles.push_back({a, b, c});
  }
  return !mesh->triangles.isEmpty();
}

bool loadAsciiStlMesh(const QByteArray& bytes, Mesh* mesh)
{
  QVector<Vec3> vertices;
  vertices.reserve(3);
  const QList<QByteArray> lines = bytes.split('\n');
  for (const QByteArray& raw_line : lines)
  {
    const QByteArray line = raw_line.trimmed();
    if (!line.startsWith("vertex "))
    {
      continue;
    }
    const QList<QByteArray> parts = line.split(' ');
    QVector<double> values;
    values.reserve(3);
    for (const QByteArray& part : parts)
    {
      bool ok = false;
      const double value = part.toDouble(&ok);
      if (ok)
      {
        values.push_back(value);
      }
    }
    if (values.size() >= 3)
    {
      vertices.push_back({values[0], values[1], values[2]});
    }
    if (vertices.size() == 3)
    {
      updateBounds(mesh, vertices[0]);
      updateBounds(mesh, vertices[1]);
      updateBounds(mesh, vertices[2]);
      mesh->triangles.push_back({vertices[0], vertices[1], vertices[2]});
      vertices.clear();
    }
  }
  return !mesh->triangles.isEmpty();
}

bool looksLikeAsciiStl(const QByteArray& bytes)
{
  const QByteArray header = bytes.left(std::min<qsizetype>(bytes.size(), 4096)).trimmed().toLower();
  return header.startsWith("solid") && header.contains("facet") && header.contains("vertex");
}

void resetMeshGeometry(Mesh* mesh)
{
  mesh->triangles.clear();
  mesh->has_bounds = false;
}

bool loadStlMesh(const QString& path, Mesh* mesh)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
  {
    mesh->error = QString("mesh not readable: %1").arg(path);
    return false;
  }

  const QByteArray bytes = file.readAll();
  if (looksLikeAsciiStl(bytes))
  {
    if (!loadAsciiStlMesh(bytes, mesh))
    {
      resetMeshGeometry(mesh);
      loadBinaryStlMesh(bytes, mesh);
    }
  }
  else if (!loadBinaryStlMesh(bytes, mesh))
  {
    resetMeshGeometry(mesh);
    loadAsciiStlMesh(bytes, mesh);
  }

  mesh->valid = !mesh->triangles.isEmpty();
  if (!mesh->valid)
  {
    mesh->error = QString("STL has no drawable triangles: %1").arg(path);
  }
  return mesh->valid;
}

bool loadObjMesh(const QString& path, Mesh* mesh)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
  {
    mesh->error = QString("mesh not readable: %1").arg(path);
    return false;
  }

  QVector<Vec3> vertices;
  while (!file.atEnd())
  {
    const QByteArray line = file.readLine().trimmed();
    if (line.startsWith("v "))
    {
      const QList<QByteArray> parts = line.split(' ');
      QVector<double> values;
      for (const QByteArray& part : parts)
      {
        bool ok = false;
        const double value = part.toDouble(&ok);
        if (ok)
        {
          values.push_back(value);
        }
      }
      if (values.size() >= 3)
      {
        vertices.push_back({values[0], values[1], values[2]});
      }
    }
    else if (line.startsWith("f "))
    {
      const QList<QByteArray> parts = line.split(' ');
      QVector<int> indices;
      for (int index = 1; index < parts.size(); ++index)
      {
        const QList<QByteArray> fields = parts[index].split('/');
        bool ok = false;
        int vertex_index = fields.value(0).toInt(&ok);
        if (!ok || vertex_index == 0)
        {
          continue;
        }
        if (vertex_index < 0)
        {
          vertex_index = vertices.size() + vertex_index;
        }
        else
        {
          --vertex_index;
        }
        indices.push_back(vertex_index);
      }
      if (indices.size() >= 3)
      {
        for (int fan = 1; fan + 1 < indices.size(); ++fan)
        {
          addTriangle(mesh, vertices, indices[0], indices[fan], indices[fan + 1]);
        }
      }
    }
  }

  mesh->valid = !mesh->triangles.isEmpty();
  if (!mesh->valid)
  {
    mesh->error = QString("OBJ has no drawable triangles: %1").arg(path);
  }
  return mesh->valid;
}

QVector<Vec3> meshBoundsCorners(const Mesh& mesh)
{
  return {
      {mesh.bounds_min.x, mesh.bounds_min.y, mesh.bounds_min.z},
      {mesh.bounds_min.x, mesh.bounds_min.y, mesh.bounds_max.z},
      {mesh.bounds_min.x, mesh.bounds_max.y, mesh.bounds_min.z},
      {mesh.bounds_min.x, mesh.bounds_max.y, mesh.bounds_max.z},
      {mesh.bounds_max.x, mesh.bounds_min.y, mesh.bounds_min.z},
      {mesh.bounds_max.x, mesh.bounds_min.y, mesh.bounds_max.z},
      {mesh.bounds_max.x, mesh.bounds_max.y, mesh.bounds_min.z},
      {mesh.bounds_max.x, mesh.bounds_max.y, mesh.bounds_max.z},
  };
}

QString pluginDirectory()
{
  Dl_info info{};
  if (dladdr(reinterpret_cast<void*>(&pluginDirectory), &info) != 0 && info.dli_fname)
  {
    return QFileInfo(QString::fromLocal8Bit(info.dli_fname)).absolutePath();
  }
  return QCoreApplication::applicationDirPath();
}

QString sourceRobotsDirectory()
{
  const QString go2_source = QString::fromLocal8Bit(PJ_UNITREE_GO2_SOURCE_DIR);
  if (go2_source.isEmpty())
  {
    return {};
  }
  QFileInfo info(go2_source);
  if (info.fileName() == "go2_description")
  {
    return info.absoluteDir().absolutePath();
  }
  return info.absoluteFilePath();
}

const QVector<ModelConfig>& modelConfigs()
{
  static const QVector<ModelConfig> configs = {
      {"go2", "Go2", "go2_description", "urdf/go2_description.urdf", "go2_description", 12,
       "quadruped"},
      {"go2w", "Go2W", "go2w_description", "urdf/go2w_description.urdf", "go2w_description", 16,
       "quadruped_wheel"},
      {"b2", "B2", "b2_description", "urdf/b2_description.urdf", "b2_description", 12, "quadruped"},
      {"b2w", "B2W", "b2w_description", "urdf/b2w_description.urdf", "b2w_description", 16,
       "quadruped_wheel"},
      {"go1", "Go1", "go1_description", "urdf/go1.urdf", "go1_description", 12, "quadruped"},
      {"a1", "A1", "a1_description", "urdf/a1.urdf", "a1_description", 12, "quadruped"},
      {"aliengo", "Aliengo", "aliengo_description", "urdf/aliengo.urdf", "aliengo_description", 12,
       "quadruped"},
      {"b1", "B1", "b1_description", "xacro/b1.urdf", "b1_description", 12, "quadruped"},
      {"g1_29dof", "G1 29DOF", "g1_description", "g1_29dof.urdf", "g1_description", 29, "g1"},
      {"g1_23dof", "G1 23DOF", "g1_description", "g1_23dof.urdf", "g1_description", 29, "g1"},
      {"g1_d", "G1-D", "g1_d_description", "g1_d.urdf", "g1_d_description", 29, "g1d"},
      {"h1", "H1", "h1_description", "urdf/h1.urdf", "h1_description", 20, "h1"},
      {"h1_2", "H1-2", "h1_2_description", "h1_2.urdf", "h1_2_description", 27, "h1_2"},
      {"h2", "H2", "h2_description", "H2.urdf", "h2_description", 31, "h2"},
      {"r1", "R1", "r1_description", "R1.urdf", "r1_description", 31, "r1"},
      {"r1_air", "R1 AIR", "r1_air_description", "R1_AIR.urdf", "r1_air_description", 31, "r1"},
  };
  return configs;
}

const ModelConfig* findModelConfig(const QString& key)
{
  for (const ModelConfig& config : modelConfigs())
  {
    if (config.key == key)
    {
      return &config;
    }
  }
  return modelConfigs().isEmpty() ? nullptr : &modelConfigs().front();
}

QString resolveAssetRoot(const ModelConfig& config)
{
  const QString plugin_dir = pluginDirectory();
  const QString source_robots_dir = sourceRobotsDirectory();
  QStringList candidates = {
      QFileInfo(plugin_dir + "/assets/robots/" + config.asset_directory).absoluteFilePath(),
      QFileInfo(QCoreApplication::applicationDirPath() + "/assets/robots/" + config.asset_directory)
          .absoluteFilePath(),
  };
  if (!source_robots_dir.isEmpty())
  {
    candidates.push_back(
        QFileInfo(source_robots_dir + "/" + config.asset_directory).absoluteFilePath());
  }
  if (config.asset_directory == "go2_description")
  {
    candidates.push_back(QFileInfo(plugin_dir + "/assets/go2_description").absoluteFilePath());
    candidates.push_back(
        QFileInfo(QCoreApplication::applicationDirPath() + "/assets/go2_description")
            .absoluteFilePath());
    candidates.push_back(QString::fromLocal8Bit(PJ_UNITREE_GO2_SOURCE_DIR));
  }

  for (const QString& candidate : candidates)
  {
    if (candidate.isEmpty())
    {
      continue;
    }
    if (QFileInfo::exists(candidate + "/" + config.urdf_relative_path))
    {
      return candidate;
    }
  }
  return {};
}

QString normalizedJointKey(QString value)
{
  value = value.toLower();
  value.remove("_joint");
  value.remove("_link");
  value.remove("_");
  value.remove("-");
  return value;
}

void addMotorAlias(QHash<QString, int>* mapping, const QString& name, int index)
{
  mapping->insert(normalizedJointKey(name), index);
}

QHash<QString, int> buildMotorIndexMap(const QString& family)
{
  QHash<QString, int> mapping;
  auto add = [&](const QString& name, int index) { addMotorAlias(&mapping, name, index); };

  if (family == "quadruped" || family == "quadruped_wheel")
  {
    add("FR_hip", 0);
    add("FR_thigh", 1);
    add("FR_calf", 2);
    add("FL_hip", 3);
    add("FL_thigh", 4);
    add("FL_calf", 5);
    add("RR_hip", 6);
    add("RR_thigh", 7);
    add("RR_calf", 8);
    add("RL_hip", 9);
    add("RL_thigh", 10);
    add("RL_calf", 11);
    if (family == "quadruped_wheel")
    {
      add("FR_foot", 12);
      add("FL_foot", 13);
      add("RR_foot", 14);
      add("RL_foot", 15);
    }
    return mapping;
  }

  if (family == "h1")
  {
    add("right_hip_roll", 0);
    add("right_hip_pitch", 1);
    add("right_knee", 2);
    add("left_hip_roll", 3);
    add("left_hip_pitch", 4);
    add("left_knee", 5);
    add("waist_yaw", 6);
    add("torso", 6);
    add("left_hip_yaw", 7);
    add("right_hip_yaw", 8);
    add("left_ankle", 10);
    add("right_ankle", 11);
    add("right_shoulder_pitch", 12);
    add("right_shoulder_roll", 13);
    add("right_shoulder_yaw", 14);
    add("right_elbow", 15);
    add("left_shoulder_pitch", 16);
    add("left_shoulder_roll", 17);
    add("left_shoulder_yaw", 18);
    add("left_elbow", 19);
    return mapping;
  }

  if (family == "h1_2")
  {
    add("left_hip_yaw", 0);
    add("left_hip_pitch", 1);
    add("left_hip_roll", 2);
    add("left_knee", 3);
    add("left_ankle_pitch", 4);
    add("left_ankle", 4);
    add("left_ankle_roll", 5);
    add("right_hip_yaw", 6);
    add("right_hip_pitch", 7);
    add("right_hip_roll", 8);
    add("right_knee", 9);
    add("right_ankle_pitch", 10);
    add("right_ankle", 10);
    add("right_ankle_roll", 11);
    add("waist_yaw", 12);
    add("torso", 12);
    add("left_shoulder_pitch", 13);
    add("left_shoulder_roll", 14);
    add("left_shoulder_yaw", 15);
    add("left_elbow", 16);
    add("left_wrist_roll", 17);
    add("left_wrist_pitch", 18);
    add("left_wrist_yaw", 19);
    add("right_shoulder_pitch", 20);
    add("right_shoulder_roll", 21);
    add("right_shoulder_yaw", 22);
    add("right_elbow", 23);
    add("right_wrist_roll", 24);
    add("right_wrist_pitch", 25);
    add("right_wrist_yaw", 26);
    return mapping;
  }

  if (family == "h2" || family == "r1")
  {
    add("left_hip_pitch", 0);
    add("left_hip_roll", 1);
    add("left_hip_yaw", 2);
    add("left_knee", 3);
    add("left_ankle_pitch", 4);
    add("left_ankle_roll", 5);
    add("right_hip_pitch", 6);
    add("right_hip_roll", 7);
    add("right_hip_yaw", 8);
    add("right_knee", 9);
    add("right_ankle_pitch", 10);
    add("right_ankle_roll", 11);
    add("waist_roll", 12);
    add("waist_yaw", 13);
    add("left_shoulder_pitch", 15);
    add("left_shoulder_roll", 16);
    add("left_shoulder_yaw", 17);
    add("left_elbow", 18);
    add("left_wrist_roll", 19);
    add("right_shoulder_pitch", 22);
    add("right_shoulder_roll", 23);
    add("right_shoulder_yaw", 24);
    add("right_elbow", 25);
    add("right_wrist_roll", 26);
    add("head_pitch", 29);
    add("head_yaw", 30);
    return mapping;
  }

  if (family == "g1d")
  {
    add("waist_yaw", 12);
    add("torso", 12);
    add("waist_pitch", 14);
    add("yaw", 14);
    add("left_shoulder_pitch", 15);
    add("left_shoulder_roll", 16);
    add("left_shoulder_yaw", 17);
    add("left_elbow", 18);
    add("left_wrist_roll", 19);
    add("left_wrist_pitch", 20);
    add("left_wrist_yaw", 21);
    add("right_shoulder_pitch", 22);
    add("right_shoulder_roll", 23);
    add("right_shoulder_yaw", 24);
    add("right_elbow", 25);
    add("right_wrist_roll", 26);
    add("right_wrist_pitch", 27);
    add("right_wrist_yaw", 28);
    return mapping;
  }

  add("left_hip_pitch", 0);
  add("left_hip_roll", 1);
  add("left_hip_yaw", 2);
  add("left_knee", 3);
  add("left_ankle_pitch", 4);
  add("left_ankle_roll", 5);
  add("right_hip_pitch", 6);
  add("right_hip_roll", 7);
  add("right_hip_yaw", 8);
  add("right_knee", 9);
  add("right_ankle_pitch", 10);
  add("right_ankle_roll", 11);
  add("waist_yaw", 12);
  add("waist_roll", 13);
  add("waist_pitch", 14);
  add("left_shoulder_pitch", 15);
  add("left_shoulder_roll", 16);
  add("left_shoulder_yaw", 17);
  add("left_elbow", 18);
  add("left_wrist_roll", 19);
  add("left_wrist_pitch", 20);
  add("left_wrist_yaw", 21);
  add("right_shoulder_pitch", 22);
  add("right_shoulder_roll", 23);
  add("right_shoulder_yaw", 24);
  add("right_elbow", 25);
  add("right_wrist_roll", 26);
  add("right_wrist_pitch", 27);
  add("right_wrist_yaw", 28);
  return mapping;
}

class MeshCache
{
public:
  explicit MeshCache(QString asset_root, QString package_name)
      : asset_root_(std::move(asset_root)), package_name_(std::move(package_name))
  {
  }

  const Mesh* meshFor(const QString& mesh_filename)
  {
    const QString path = resolvePath(mesh_filename);
    if (path.isEmpty())
    {
      return nullptr;
    }

    const auto existing = meshes_.find(path);
    if (existing != meshes_.end())
    {
      return existing.value().get();
    }

    auto mesh = std::make_shared<Mesh>();
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "dae")
    {
      loadColladaMesh(path, mesh.get());
    }
    else if (suffix == "stl")
    {
      loadStlMesh(path, mesh.get());
    }
    else if (suffix == "obj")
    {
      loadObjMesh(path, mesh.get());
    }
    else
    {
      mesh->error = QString("unsupported mesh format: %1").arg(path);
    }
    const Mesh* result = mesh.get();
    meshes_.insert(path, mesh);
    return result;
  }

private:
  QString resolvePath(QString mesh_filename) const
  {
    const QString prefix = "package://" + package_name_ + "/";
    if (mesh_filename.startsWith(prefix))
    {
      mesh_filename = mesh_filename.mid(prefix.size());
    }
    else if (mesh_filename.startsWith("package://"))
    {
      const int package_separator = mesh_filename.indexOf('/', QString("package://").size());
      if (package_separator >= 0)
      {
        mesh_filename = mesh_filename.mid(package_separator + 1);
      }
    }
    if (mesh_filename.startsWith('/'))
    {
      return mesh_filename;
    }
    if (asset_root_.isEmpty())
    {
      return {};
    }
    return QFileInfo(asset_root_ + "/" + mesh_filename).absoluteFilePath();
  }

  QString asset_root_;
  QString package_name_;
  QHash<QString, std::shared_ptr<Mesh>> meshes_;
};

class RobotModel
{
public:
  bool load(const ModelConfig& config)
  {
    config_ = config;
    asset_root_ = resolveAssetRoot(config);
    motor_count_ = config.motor_count;
    base_height_ = 0.0;
    valid_ = false;
    links_.clear();
    joints_.clear();
    children_by_parent_.clear();
    roots_.clear();

    if (asset_root_.isEmpty())
    {
      status_ = QString("%1 assets not found").arg(config.label);
      return false;
    }

    QFile file(asset_root_ + "/" + config.urdf_relative_path);
    if (!file.open(QIODevice::ReadOnly))
    {
      status_ = QString("URDF not readable: %1").arg(file.fileName());
      return false;
    }

    QDomDocument doc;
    QString error_message;
    int error_line = 0;
    int error_column = 0;
    if (!doc.setContent(&file, false, &error_message, &error_line, &error_column))
    {
      status_ = QString("URDF XML error at %1:%2: %3")
                    .arg(error_line)
                    .arg(error_column)
                    .arg(error_message);
      return false;
    }

    const QDomNodeList link_nodes = doc.elementsByTagName("link");
    for (int index = 0; index < link_nodes.size(); ++index)
    {
      const QDomElement link_element = link_nodes.at(index).toElement();
      LinkModel link;
      link.name = link_element.attribute("name");

      const QDomNodeList visuals = link_element.elementsByTagName("visual");
      for (int visual_index = 0; visual_index < visuals.size(); ++visual_index)
      {
        const QDomElement visual_element = visuals.at(visual_index).toElement();
        const QDomElement mesh_element =
            visual_element.firstChildElement("geometry").firstChildElement("mesh");
        if (mesh_element.isNull())
        {
          continue;
        }

        LinkVisual visual;
        visual.mesh_filename = mesh_element.attribute("filename");
        visual.origin = parseOrigin(visual_element);
        visual.scale = parseScale(mesh_element.attribute("scale"));
        visual.color = parseMaterialColor(visual_element);
        link.visuals.push_back(visual);
      }
      links_.insert(link.name, link);
    }

    const QHash<QString, int> motor_indices = buildMotorIndexMap(config.mapping_family);

    std::unordered_set<std::string> child_links;
    const QDomNodeList joint_nodes = doc.elementsByTagName("joint");
    for (int index = 0; index < joint_nodes.size(); ++index)
    {
      const QDomElement joint_element = joint_nodes.at(index).toElement();
      JointModel joint;
      joint.name = joint_element.attribute("name");
      joint.type = joint_element.attribute("type");
      joint.parent = joint_element.firstChildElement("parent").attribute("link");
      joint.child = joint_element.firstChildElement("child").attribute("link");
      joint.axis =
          parseVec3(joint_element.firstChildElement("axis").attribute("xyz"), {0.0, 0.0, 1.0});
      joint.origin = parseOrigin(joint_element);
      joint.motor_index = motor_indices.value(normalizedJointKey(joint.name), -1);
      child_links.insert(joint.child.toStdString());
      children_by_parent_.insert(joint.parent, joints_.size());
      joints_.push_back(joint);
    }

    for (auto it = links_.cbegin(); it != links_.cend(); ++it)
    {
      if (!child_links.count(it.key().toStdString()))
      {
        roots_.push_back(it.key());
      }
    }
    if (roots_.isEmpty() && links_.contains("base"))
    {
      roots_.push_back("base");
    }
    if (roots_.isEmpty() && links_.contains("pelvis"))
    {
      roots_.push_back("pelvis");
    }
    if (roots_.isEmpty() && links_.contains("AGV_link"))
    {
      roots_.push_back("AGV_link");
    }

    valid_ = !links_.isEmpty() && !joints_.isEmpty();
    if (valid_)
    {
      base_height_ = estimateBaseHeight();
      status_ = QString("%1 URDF loaded").arg(config.label);
    }
    else
    {
      status_ = QString("%1 URDF is empty").arg(config.label);
    }
    return valid_;
  }

  const QString& assetRoot() const { return asset_root_; }

  const QString& packageName() const { return config_.package_name; }

  const QString& label() const { return config_.label; }

  int motorCount() const { return motor_count_; }

  const QString& status() const { return status_; }

  bool isValid() const { return valid_; }

  const QHash<QString, LinkModel>& links() const { return links_; }

  const QVector<JointModel>& joints() const { return joints_; }

  QHash<QString, Mat4> computeTransforms(const RobotSample& sample, bool use_imu) const
  {
    Mat4 base_transform = Mat4::translation({0.0, 0.0, base_height_});
    if (use_imu && sample.has_imu)
    {
      base_transform = base_transform * Mat4::rpy(sample.rpy.x, sample.rpy.y, sample.rpy.z);
    }
    return computeTransformsWithBase(base_transform, sample);
  }

private:
  QHash<QString, Mat4> computeTransformsWithBase(const Mat4& base_transform,
                                                 const RobotSample& sample) const
  {
    QHash<QString, Mat4> transforms;
    for (const QString& root : roots_)
    {
      transforms.insert(root, base_transform);
      applyChildren(root, base_transform, sample, &transforms);
    }
    return transforms;
  }

  double estimateBaseHeight() const
  {
    RobotSample neutral;
    neutral.expected_motor_count = motor_count_;
    const QHash<QString, Mat4> transforms = computeTransformsWithBase(Mat4::identity(), neutral);
    if (transforms.isEmpty())
    {
      return 0.0;
    }

    double min_z = std::numeric_limits<double>::max();
    for (auto it = transforms.cbegin(); it != transforms.cend(); ++it)
    {
      min_z = std::min(min_z, it.value().transformPoint({0.0, 0.0, 0.0}).z);
    }
    return (min_z < 0.0) ? -min_z + 0.04 : 0.04;
  }

  void applyChildren(const QString& parent, const Mat4& parent_transform, const RobotSample& sample,
                     QHash<QString, Mat4>* transforms) const
  {
    const QList<int> child_joints = children_by_parent_.values(parent);
    for (int joint_index : child_joints)
    {
      const JointModel& joint = joints_[joint_index];
      double q = 0.0;
      if ((joint.type == "revolute" || joint.type == "continuous") && joint.motor_index >= 0)
      {
        q = sample.q[static_cast<size_t>(joint.motor_index)];
      }

      const Mat4 child_transform = parent_transform * joint.origin * Mat4::axisAngle(joint.axis, q);
      transforms->insert(joint.child, child_transform);
      applyChildren(joint.child, child_transform, sample, transforms);
    }
  }

  QString asset_root_;
  QString status_;
  ModelConfig config_;
  int motor_count_ = 12;
  double base_height_ = 0.0;
  bool valid_ = false;
  QHash<QString, LinkModel> links_;
  QVector<JointModel> joints_;
  QMultiHash<QString, int> children_by_parent_;
  QVector<QString> roots_;
};

struct Projection
{
  double yaw = -0.72;
  double pitch = 0.46;
  double scale = 1.0;
  QPointF offset;

  Vec3 rotate(const Vec3& point) const
  {
    const double cy = std::cos(yaw);
    const double sy = std::sin(yaw);
    const double cp = std::cos(pitch);
    const double sp = std::sin(pitch);

    const double x1 = cy * point.x - sy * point.y;
    const double y1 = sy * point.x + cy * point.y;
    const double z1 = point.z;
    return {x1, cp * y1 - sp * z1, sp * y1 + cp * z1};
  }

  QPointF project(const Vec3& point) const
  {
    const Vec3 rotated = rotate(point);
    return {offset.x() + rotated.x * scale, offset.y() - rotated.z * scale};
  }

  double depth(const Vec3& point) const { return rotate(point).y; }
};

Projection makeProjection(const QVector<Vec3>& points, const QRect& viewport, double yaw,
                          double pitch, double zoom)
{
  Projection projection;
  projection.yaw = yaw;
  projection.pitch = pitch;
  if (points.isEmpty() || viewport.width() <= 0 || viewport.height() <= 0)
  {
    projection.offset = viewport.center();
    return projection;
  }

  Vec3 min_bound = {std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
                    std::numeric_limits<double>::max()};
  Vec3 max_bound = {std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest(),
                    std::numeric_limits<double>::lowest()};
  for (const Vec3& point : points)
  {
    min_bound.x = std::min(min_bound.x, point.x);
    min_bound.y = std::min(min_bound.y, point.y);
    min_bound.z = std::min(min_bound.z, point.z);
    max_bound.x = std::max(max_bound.x, point.x);
    max_bound.y = std::max(max_bound.y, point.y);
    max_bound.z = std::max(max_bound.z, point.z);
  }

  const Vec3 center = {(min_bound.x + max_bound.x) * 0.5, (min_bound.y + max_bound.y) * 0.5,
                       (min_bound.z + max_bound.z) * 0.5};
  double radius = 0.125;
  for (const Vec3& point : points)
  {
    radius = std::max(radius, std::sqrt(dot(point - center, point - center)));
  }

  const double margin = 34.0;
  const double usable_width = std::max(1.0, static_cast<double>(viewport.width()) - 2.0 * margin);
  const double usable_height = std::max(1.0, static_cast<double>(viewport.height()) - 2.0 * margin);
  projection.scale = std::max(20.0, std::min(usable_width, usable_height) / (2.0 * radius)) * zoom;

  const Vec3 rotated_center = projection.rotate(center);
  projection.offset = {viewport.center().x() - rotated_center.x * projection.scale,
                       viewport.center().y() + rotated_center.z * projection.scale};
  return projection;
}

Mat4 worldToScreenMatrix(const Projection& projection)
{
  const double cy = std::cos(projection.yaw);
  const double sy = std::sin(projection.yaw);
  const double cp = std::cos(projection.pitch);
  const double sp = std::sin(projection.pitch);
  const double scale = projection.scale;

  Mat4 matrix;
  matrix.m = {
      scale * cy,
      -scale * sy,
      0.0,
      projection.offset.x(),
      -scale * sp * sy,
      -scale * sp * cy,
      -scale * cp,
      projection.offset.y(),
      scale * cp * sy,
      scale * cp * cy,
      -scale * sp,
      0.0,
      0.0,
      0.0,
      0.0,
      1.0,
  };
  return matrix;
}

std::array<GLdouble, 16> toOpenGlMatrix(const Mat4& matrix)
{
  std::array<GLdouble, 16> result{};
  for (int row = 0; row < 4; ++row)
  {
    for (int col = 0; col < 4; ++col)
    {
      result[static_cast<size_t>(col * 4 + row)] = matrix.m[static_cast<size_t>(row * 4 + col)];
    }
  }
  return result;
}

class RobotCanvas : public QOpenGLWidget, protected QOpenGLFunctions_2_0
{
public:
  explicit RobotCanvas(QWidget* parent = nullptr) : QOpenGLWidget(parent)
  {
    setMinimumSize(560, 390);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
    setContextMenuPolicy(Qt::NoContextMenu);
    setModelKey("go2");
  }

  void setData(PJ::PlotDataMapRef* data) { data_ = data; }

  void setModelKey(const QString& key)
  {
    const ModelConfig* config = findModelConfig(key);
    if (!config)
    {
      return;
    }

    model_key_ = config->key;
    model_.load(*config);
    mesh_cache_ = std::make_unique<MeshCache>(model_.assetRoot(), config->package_name);
    status_text_ = model_.status();
    update();
  }

  void setUseImu(bool value)
  {
    use_imu_ = value;
    update();
  }

  void setDrawMesh(bool value)
  {
    draw_mesh_ = value;
    update();
  }

  void setShowLabels(bool value)
  {
    show_labels_ = value;
    update();
  }

  void setLiveMode(bool value)
  {
    live_mode_ = value;
    update();
  }

  void setSelectedTime(double value)
  {
    selected_time_ = value;
    update();
  }

  TimeRange dataTimeRange() const
  {
    TimeRange range;
    auto addSeries = [&](const QString& name)
    {
      if (!data_)
      {
        return;
      }
      const auto it = data_->numeric.find(name.toStdString());
      if (it == data_->numeric.end() || it->second.size() == 0)
      {
        return;
      }
      const double first = it->second.front().x;
      const double last = it->second.back().x;
      if (!std::isfinite(first) || !std::isfinite(last))
      {
        return;
      }
      if (!range.valid)
      {
        range.min = std::min(first, last);
        range.max = std::max(first, last);
        range.valid = true;
        return;
      }
      range.min = std::min(range.min, std::min(first, last));
      range.max = std::max(range.max, std::max(first, last));
    };

    auto addLowStateSeries = [&](const QString& suffix)
    {
      addSeries("lowstate/" + suffix);
      addSeries("unitree/rt/lowstate/" + suffix);
    };

    for (int index = 0; index < model_.motorCount(); ++index)
    {
      addLowStateSeries(QString("motor_state/%1/q").arg(index, 2, 10, QLatin1Char('0')));
    }
    for (int index = 0; index < 3; ++index)
    {
      addLowStateSeries(QString("imu_state/rpy/%1").arg(index));
    }
    for (int index = 0; index < 4; ++index)
    {
      addLowStateSeries(QString("imu_state/quaternion/%1").arg(index));
    }
    return range;
  }

  QString statusText() const { return status_text_; }

protected:
  void initializeGL() override
  {
    initializeOpenGLFunctions();
    glClearColor(238.0F / 255.0F, 241.0F / 255.0F, 245.0F / 255.0F, 1.0F);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_FLAT);
  }

  void paintGL() override
  {
    glViewport(0, 0, width(), height());
    glClearColor(238.0F / 255.0F, 241.0F / 255.0F, 245.0F / 255.0F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!model_.isValid())
    {
      QPainter painter(this);
      painter.setRenderHint(QPainter::Antialiasing, true);
      painter.setPen(QColor(31, 41, 55));
      painter.drawText(rect(), Qt::AlignCenter, model_.status());
      status_text_ = model_.status();
      return;
    }

    const RobotSample sample = readSample();
    const QHash<QString, Mat4> transforms = model_.computeTransforms(sample, use_imu_);
    QVector<Vec3> framing_points = collectFramingPoints(transforms);
    const Projection projection = makeProjection(framing_points, rect().adjusted(8, 8, -8, -8),
                                                 view_yaw_, view_pitch_, view_zoom_);
    const Mat4 screen_matrix = worldToScreenMatrix(projection);

    beginOpenGlProjection();
    drawGroundGl(screen_matrix);
    if (draw_mesh_)
    {
      drawMeshesGl(transforms, screen_matrix);
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawAxesIcon(painter, projection);

    status_text_ = QString("%1  motors %2/%3  imu %4  t=%5s")
                       .arg(model_.label())
                       .arg(sample.motor_count)
                       .arg(sample.expected_motor_count)
                       .arg(sample.has_imu ? "ok" : "missing")
                       .arg(sample.latest_time, 0, 'f', 2);
    if (sample.motor_count == 0)
    {
      painter.setPen(QColor(31, 41, 55, 210));
      painter.drawText(rect().adjusted(18, 18, -18, -18), Qt::AlignLeft | Qt::AlignTop,
                       "waiting for lowstate samples");
    }
  }

  void mousePressEvent(QMouseEvent* event) override
  {
    last_mouse_pos_ = event->pos();
    if (event->button() == Qt::LeftButton)
    {
      if (const std::optional<AxisView> axis = axisAt(event->pos()))
      {
        alignViewToAxis(*axis);
        event->accept();
        return;
      }
      rotating_view_ = true;
      setCursor(Qt::ClosedHandCursor);
      event->accept();
      return;
    }
    QWidget::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent* event) override
  {
    const QPoint delta = event->pos() - last_mouse_pos_;
    last_mouse_pos_ = event->pos();
    if (rotating_view_)
    {
      view_yaw_ -= static_cast<double>(delta.x()) * 0.008;
      view_pitch_ = std::clamp(view_pitch_ - static_cast<double>(delta.y()) * 0.008, -kMaxViewPitch,
                               kMaxViewPitch);
      update();
      event->accept();
      return;
    }
    QWidget::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent* event) override
  {
    if (event->button() == Qt::LeftButton)
    {
      rotating_view_ = false;
    }
    if (!rotating_view_)
    {
      setCursor(Qt::OpenHandCursor);
    }
    QWidget::mouseReleaseEvent(event);
  }

  void mouseDoubleClickEvent(QMouseEvent* event) override
  {
    if (event->button() == Qt::LeftButton)
    {
      resetView();
      event->accept();
      return;
    }
    QWidget::mouseDoubleClickEvent(event);
  }

  void wheelEvent(QWheelEvent* event) override
  {
    const int delta = event->angleDelta().y();
    if (delta == 0)
    {
      QWidget::wheelEvent(event);
      return;
    }
    view_zoom_ = std::clamp(view_zoom_ * std::pow(1.0015, static_cast<double>(delta)), 0.25, 6.0);
    update();
    event->accept();
  }

private:
  void resetView()
  {
    view_yaw_ = -0.72;
    view_pitch_ = 0.46;
    view_zoom_ = 1.0;
    update();
  }

  enum class AxisView
  {
    X,
    Y,
    Z,
  };

  struct AxisTarget
  {
    AxisView view;
    QPointF end;
  };

  QPointF axesIconOrigin() const { return QPointF(width() - 58.0, 62.0); }

  QVector<AxisTarget> axesTargets() const
  {
    constexpr double kAxisLength = 34.0;
    Projection projection;
    projection.yaw = view_yaw_;
    projection.pitch = view_pitch_;

    auto makeTarget = [&](AxisView view, const Vec3& vector)
    {
      const Vec3 rotated = projection.rotate(vector);
      return AxisTarget{view, axesIconOrigin() +
                                  QPointF(rotated.x * kAxisLength, -rotated.z * kAxisLength)};
    };

    return {
        makeTarget(AxisView::X, {1.0, 0.0, 0.0}),
        makeTarget(AxisView::Y, {0.0, 1.0, 0.0}),
        makeTarget(AxisView::Z, {0.0, 0.0, 1.0}),
    };
  }

  static double pointDistanceSquared(const QPointF& lhs, const QPointF& rhs)
  {
    const QPointF delta = lhs - rhs;
    return delta.x() * delta.x() + delta.y() * delta.y();
  }

  static double distanceToSegmentSquared(const QPointF& point, const QPointF& start,
                                         const QPointF& end)
  {
    const QPointF segment = end - start;
    const double length_squared = segment.x() * segment.x() + segment.y() * segment.y();
    if (length_squared < 1e-9)
    {
      return pointDistanceSquared(point, start);
    }
    const QPointF relative = point - start;
    const double t = std::clamp(
        (relative.x() * segment.x() + relative.y() * segment.y()) / length_squared, 0.0, 1.0);
    const QPointF closest = start + segment * t;
    return pointDistanceSquared(point, closest);
  }

  std::optional<AxisView> axisAt(const QPointF& point) const
  {
    const QVector<AxisTarget> targets = axesTargets();
    const QPointF origin = axesIconOrigin();

    std::optional<AxisView> best_axis;
    double best_distance = 22.0 * 22.0;
    for (const AxisTarget& target : targets)
    {
      const double distance = pointDistanceSquared(point, target.end);
      if (distance < best_distance)
      {
        best_distance = distance;
        best_axis = target.view;
      }
    }

    if (best_axis)
    {
      return best_axis;
    }

    if (pointDistanceSquared(point, origin) < 11.0 * 11.0)
    {
      return std::nullopt;
    }

    best_distance = 9.0 * 9.0;
    for (const AxisTarget& target : targets)
    {
      const double distance = distanceToSegmentSquared(point, origin, target.end);
      if (distance < best_distance)
      {
        best_distance = distance;
        best_axis = target.view;
      }
    }
    return best_axis;
  }

  void alignViewToAxis(AxisView axis)
  {
    switch (axis)
    {
    case AxisView::X:
      view_yaw_ = kPi * 0.5;
      view_pitch_ = 0.0;
      break;
    case AxisView::Y:
      view_yaw_ = 0.0;
      view_pitch_ = 0.0;
      break;
    case AxisView::Z:
      view_yaw_ = 0.0;
      view_pitch_ = -kMaxViewPitch;
      break;
    }
    update();
  }

  bool numericAtTime(const QString& name, double time, double* value) const
  {
    if (!data_)
    {
      return false;
    }
    const auto it = data_->numeric.find(name.toStdString());
    if (it == data_->numeric.end() || it->second.size() == 0)
    {
      return false;
    }

    const std::optional<double> maybe_value = it->second.getYfromX(time);
    if (!maybe_value || !std::isfinite(*maybe_value))
    {
      return false;
    }
    *value = *maybe_value;
    return true;
  }

  bool latestNumeric(const QString& name, double* value, double* stamp) const
  {
    if (!data_)
    {
      return false;
    }
    const auto it = data_->numeric.find(name.toStdString());
    if (it == data_->numeric.end() || it->second.size() == 0)
    {
      return false;
    }

    const auto& point = it->second.back();
    if (!std::isfinite(point.y))
    {
      return false;
    }
    *value = point.y;
    if (stamp)
    {
      *stamp = point.x;
    }
    return true;
  }

  bool readNumeric(const QString& name, double* value, double* stamp) const
  {
    if (live_mode_)
    {
      return latestNumeric(name, value, stamp);
    }

    if (!numericAtTime(name, selected_time_, value))
    {
      return false;
    }
    if (stamp)
    {
      *stamp = selected_time_;
    }
    return true;
  }

  bool readLowStateNumeric(const QString& suffix, double* value, double* stamp) const
  {
    if (readNumeric("lowstate/" + suffix, value, stamp))
    {
      return true;
    }
    return readNumeric("unitree/rt/lowstate/" + suffix, value, stamp);
  }

  RobotSample readSample() const
  {
    RobotSample sample;
    sample.expected_motor_count = model_.motorCount();
    double stamp = 0.0;
    const int q_count = std::min(model_.motorCount(), static_cast<int>(sample.q.size()));
    for (int index = 0; index < q_count; ++index)
    {
      double value = 0.0;
      const QString field = QString("motor_state/%1/q").arg(index, 2, 10, QLatin1Char('0'));
      if (readLowStateNumeric(field, &value, &stamp))
      {
        sample.q[static_cast<size_t>(index)] = std::clamp(value, -4.0 * kPi, 4.0 * kPi);
        sample.latest_time = std::max(sample.latest_time, stamp);
        ++sample.motor_count;
      }
    }

    double roll = 0.0;
    double pitch = 0.0;
    double yaw = 0.0;
    double imu_stamp = 0.0;
    const bool has_rpy = readLowStateNumeric("imu_state/rpy/0", &roll, &imu_stamp) &&
                         readLowStateNumeric("imu_state/rpy/1", &pitch, &imu_stamp) &&
                         readLowStateNumeric("imu_state/rpy/2", &yaw, &imu_stamp);

    if (has_rpy)
    {
      sample.rpy = {roll, pitch, yaw};
      sample.has_imu = true;
      sample.latest_time = std::max(sample.latest_time, imu_stamp);
      return sample;
    }

    double qw = 1.0;
    double qx = 0.0;
    double qy = 0.0;
    double qz = 0.0;
    const bool has_quaternion = readLowStateNumeric("imu_state/quaternion/0", &qw, &imu_stamp) &&
                                readLowStateNumeric("imu_state/quaternion/1", &qx, &imu_stamp) &&
                                readLowStateNumeric("imu_state/quaternion/2", &qy, &imu_stamp) &&
                                readLowStateNumeric("imu_state/quaternion/3", &qz, &imu_stamp);
    if (has_quaternion)
    {
      const double norm = std::sqrt(qw * qw + qx * qx + qy * qy + qz * qz);
      if (norm > 1e-9)
      {
        qw /= norm;
        qx /= norm;
        qy /= norm;
        qz /= norm;
      }
      const double sinr_cosp = 2.0 * (qw * qx + qy * qz);
      const double cosr_cosp = 1.0 - 2.0 * (qx * qx + qy * qy);
      const double sinp = std::clamp(2.0 * (qw * qy - qz * qx), -1.0, 1.0);
      const double siny_cosp = 2.0 * (qw * qz + qx * qy);
      const double cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz);
      sample.rpy = {std::atan2(sinr_cosp, cosr_cosp), std::asin(sinp),
                    std::atan2(siny_cosp, cosy_cosp)};
      sample.has_imu = true;
      sample.latest_time = std::max(sample.latest_time, imu_stamp);
    }

    return sample;
  }

  QVector<Vec3> collectFramingPoints(const QHash<QString, Mat4>& transforms)
  {
    QVector<Vec3> points;
    points.reserve(transforms.size() + 128);
    for (auto it = transforms.cbegin(); it != transforms.cend(); ++it)
    {
      points.push_back(it.value().transformPoint({0.0, 0.0, 0.0}));
    }

    if (!draw_mesh_)
    {
      return points;
    }

    for (auto link_it = model_.links().cbegin(); link_it != model_.links().cend(); ++link_it)
    {
      const auto transform_it = transforms.find(link_it.key());
      if (transform_it == transforms.end())
      {
        continue;
      }
      for (const LinkVisual& visual : link_it.value().visuals)
      {
        if (!mesh_cache_)
        {
          continue;
        }
        const Mesh* mesh = mesh_cache_->meshFor(visual.mesh_filename);
        if (!mesh || !mesh->valid)
        {
          continue;
        }
        const Mat4 visual_transform =
            transform_it.value() * visual.origin * Mat4::scale(visual.scale);
        for (const Vec3& corner : meshBoundsCorners(*mesh))
        {
          points.push_back(visual_transform.transformPoint(corner));
        }
      }
    }
    return points;
  }

  void beginOpenGlProjection()
  {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, static_cast<GLdouble>(width()), static_cast<GLdouble>(height()), 0.0, -10000.0,
            10000.0);
    glMatrixMode(GL_MODELVIEW);
  }

  void setOpenGlMatrix(const Mat4& matrix)
  {
    const std::array<GLdouble, 16> gl_matrix = toOpenGlMatrix(matrix);
    glLoadMatrixd(gl_matrix.data());
  }

  void drawGroundGl(const Mat4& screen_matrix)
  {
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    setOpenGlMatrix(screen_matrix);

    glColor4ub(148, 163, 184, 255);
    glLineWidth(1.0F);
    glBegin(GL_LINES);
    for (double offset = -0.6; offset <= 0.601; offset += 0.1)
    {
      glVertex3d(-0.65, offset, 0.0);
      glVertex3d(0.65, offset, 0.0);
      glVertex3d(offset, -0.45, 0.0);
      glVertex3d(offset, 0.45, 0.0);
    }
    glEnd();
  }

  void compileMeshDisplayList(const Mesh* mesh)
  {
    if (!mesh || mesh->gl_display_list != 0)
    {
      return;
    }

    mesh->gl_display_list = glGenLists(1);
    if (mesh->gl_display_list == 0)
    {
      return;
    }

    glNewList(mesh->gl_display_list, GL_COMPILE);
    glBegin(GL_TRIANGLES);
    for (const Triangle& triangle : mesh->triangles)
    {
      const Vec3 normal = normalized(cross(triangle.b - triangle.a, triangle.c - triangle.a));
      glNormal3d(normal.x, normal.y, normal.z);
      glVertex3d(triangle.a.x, triangle.a.y, triangle.a.z);
      glVertex3d(triangle.b.x, triangle.b.y, triangle.b.z);
      glVertex3d(triangle.c.x, triangle.c.y, triangle.c.z);
    }
    glEnd();
    glEndList();
  }

  void drawMeshGeometryGl(const Mesh* mesh)
  {
    compileMeshDisplayList(mesh);
    if (mesh->gl_display_list != 0)
    {
      glCallList(mesh->gl_display_list);
      return;
    }

    glBegin(GL_TRIANGLES);
    for (const Triangle& triangle : mesh->triangles)
    {
      const Vec3 normal = normalized(cross(triangle.b - triangle.a, triangle.c - triangle.a));
      glNormal3d(normal.x, normal.y, normal.z);
      glVertex3d(triangle.a.x, triangle.a.y, triangle.a.z);
      glVertex3d(triangle.b.x, triangle.b.y, triangle.b.z);
      glVertex3d(triangle.c.x, triangle.c.y, triangle.c.z);
    }
    glEnd();
  }

  void drawMeshesGl(const QHash<QString, Mat4>& transforms, const Mat4& screen_matrix)
  {
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_NORMALIZE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

    const GLfloat global_ambient[] = {0.48F, 0.48F, 0.48F, 1.0F};
    const GLfloat main_ambient[] = {0.12F, 0.12F, 0.12F, 1.0F};
    const GLfloat main_diffuse[] = {0.45F, 0.45F, 0.45F, 1.0F};
    const GLfloat main_position[] = {-0.25F, -0.35F, 0.9F, 0.0F};
    const GLfloat fill_ambient[] = {0.0F, 0.0F, 0.0F, 1.0F};
    const GLfloat fill_diffuse[] = {0.18F, 0.18F, 0.18F, 1.0F};
    const GLfloat fill_position[] = {0.65F, 0.45F, 0.55F, 0.0F};

    glLoadIdentity();
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_ambient);
    glLightfv(GL_LIGHT0, GL_AMBIENT, main_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, main_diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, main_position);
    glLightfv(GL_LIGHT1, GL_AMBIENT, fill_ambient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, fill_diffuse);
    glLightfv(GL_LIGHT1, GL_POSITION, fill_position);

    for (auto link_it = model_.links().cbegin(); link_it != model_.links().cend(); ++link_it)
    {
      const auto transform_it = transforms.find(link_it.key());
      if (transform_it == transforms.end())
      {
        continue;
      }

      for (const LinkVisual& visual : link_it.value().visuals)
      {
        if (!mesh_cache_)
        {
          continue;
        }
        const Mesh* mesh = mesh_cache_->meshFor(visual.mesh_filename);
        if (!mesh || !mesh->valid)
        {
          continue;
        }

        const Mat4 transform = transform_it.value() * visual.origin * Mat4::scale(visual.scale);
        setOpenGlMatrix(screen_matrix * transform);
        glColor4f(static_cast<GLfloat>(visual.color.redF()),
                  static_cast<GLfloat>(visual.color.greenF()),
                  static_cast<GLfloat>(visual.color.blueF()), 1.0F);
        drawMeshGeometryGl(mesh);
      }
    }
  }

  void drawSkeleton(QPainter& painter, const QHash<QString, Mat4>& transforms,
                    const Projection& projection)
  {
    painter.setPen(QPen(QColor(255, 180, 86), 3));
    painter.setBrush(QColor(255, 180, 86));

    for (const JointModel& joint : model_.joints())
    {
      const auto parent_it = transforms.find(joint.parent);
      const auto child_it = transforms.find(joint.child);
      if (parent_it == transforms.end() || child_it == transforms.end())
      {
        continue;
      }

      const Vec3 parent = parent_it.value().transformPoint({0.0, 0.0, 0.0});
      const Vec3 child = child_it.value().transformPoint({0.0, 0.0, 0.0});
      const QPointF parent_point = projection.project(parent);
      const QPointF child_point = projection.project(child);
      painter.drawLine(parent_point, child_point);
      if (joint.motor_index >= 0)
      {
        painter.drawEllipse(child_point, 3.5, 3.5);
      }

      if (show_labels_ && joint.motor_index >= 0)
      {
        painter.setPen(QColor(226, 232, 240));
        painter.drawText(child_point + QPointF(5.0, -5.0), joint.name);
        painter.setPen(QPen(QColor(255, 180, 86), 3));
      }
    }
  }

  void drawAxesIcon(QPainter& painter, const Projection& projection)
  {
    constexpr double kAxisLength = 34.0;
    const QPointF origin(width() - 58.0, 62.0);
    struct Axis
    {
      Vec3 vector;
      QColor color;
      QString label;
      double depth = 0.0;
    };
    QVector<Axis> axes = {
        {{1.0, 0.0, 0.0}, QColor(239, 68, 68), "X", 0.0},
        {{0.0, 1.0, 0.0}, QColor(34, 197, 94), "Y", 0.0},
        {{0.0, 0.0, 1.0}, QColor(59, 130, 246), "Z", 0.0},
    };
    for (Axis& axis : axes)
    {
      axis.depth = projection.depth(axis.vector);
    }
    std::sort(axes.begin(), axes.end(),
              [](const Axis& lhs, const Axis& rhs) { return lhs.depth > rhs.depth; });

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(31, 41, 55, 95), 1));
    painter.setBrush(QColor(255, 255, 255, 170));
    painter.drawEllipse(origin, 7.0, 7.0);

    for (const Axis& axis : axes)
    {
      const Vec3 rotated = projection.rotate(axis.vector);
      const QPointF end = origin + QPointF(rotated.x * kAxisLength, -rotated.z * kAxisLength);
      painter.setPen(QPen(axis.color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      painter.drawLine(origin, end);
      painter.setBrush(axis.color);
      painter.drawEllipse(end, 5.8, 5.8);
      painter.setPen(axis.color.lighter(130));
      painter.drawText(end + QPointF(5.0, -5.0), axis.label);
    }
  }

  PJ::PlotDataMapRef* data_ = nullptr;
  RobotModel model_;
  std::unique_ptr<MeshCache> mesh_cache_;
  QString model_key_;
  bool use_imu_ = true;
  bool draw_mesh_ = true;
  bool show_labels_ = false;
  bool live_mode_ = true;
  double selected_time_ = 0.0;
  double view_yaw_ = -0.72;
  double view_pitch_ = 0.46;
  double view_zoom_ = 1.0;
  QPoint last_mouse_pos_;
  bool rotating_view_ = false;
  mutable QString status_text_;
};

} // namespace

class UnitreeRobotViewWidget : public QWidget
{
public:
  explicit UnitreeRobotViewWidget(QWidget* parent = nullptr) : QWidget(parent)
  {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(10);

    close_button_ = new QToolButton();
    close_button_->setIcon(makeCloseIcon(QColor(0, 0, 0)));
    close_button_->setIconSize(QSize(18, 18));
    close_button_->setFixedSize(26, 26);
    close_button_->setToolTip("Close");
    close_button_->setAutoRaise(false);
    close_button_->setStyleSheet("QToolButton { border: 1px solid #000000; border-radius: 4px; "
                                 "background-color: palette(button); }");
    toolbar->addWidget(close_button_);

    model_selector_ = new QComboBox();
    for (const ModelConfig& config : modelConfigs())
    {
      const bool available = !resolveAssetRoot(config).isEmpty();
      model_selector_->addItem(available ? config.label : config.label + " (missing assets)",
                               config.key);
    }
    const int go2_index = model_selector_->findData("go2");
    if (go2_index >= 0)
    {
      model_selector_->setCurrentIndex(go2_index);
    }
    toolbar->addWidget(model_selector_);

    use_imu_ = new QCheckBox("IMU");
    use_imu_->setChecked(true);
    toolbar->addWidget(use_imu_);

    draw_mesh_ = new QCheckBox("Mesh");
    draw_mesh_->setChecked(true);
    toolbar->addWidget(draw_mesh_);

    live_ = new QCheckBox("Live");
    live_->setChecked(true);
    toolbar->addWidget(live_);

    toolbar->addStretch(1);
    status_ = new QLabel();
    toolbar->addWidget(status_);
    layout->addLayout(toolbar);

    canvas_ = new RobotCanvas(this);
    layout->addWidget(canvas_, 1);

    auto* timeline_layout = new QHBoxLayout();
    timeline_layout->setSpacing(8);
    time_min_ = new QLabel("--");
    time_current_ = new QLabel("--");
    time_max_ = new QLabel("--");
    time_slider_ = new QSlider(Qt::Horizontal);
    time_slider_->setRange(0, kTimelineSteps);
    time_slider_->setEnabled(false);
    timeline_layout->addWidget(time_min_);
    timeline_layout->addWidget(time_slider_, 1);
    timeline_layout->addWidget(time_max_);
    timeline_layout->addWidget(time_current_);
    layout->addLayout(timeline_layout);

    QObject::connect(use_imu_, &QCheckBox::toggled, canvas_,
                     [this](bool checked) { canvas_->setUseImu(checked); });
    QObject::connect(draw_mesh_, &QCheckBox::toggled, canvas_,
                     [this](bool checked) { canvas_->setDrawMesh(checked); });
    QObject::connect(model_selector_, QOverload<int>::of(&QComboBox::currentIndexChanged), canvas_,
                     [this](int index)
                     {
                       const QString key = model_selector_->itemData(index).toString();
                       if (!key.isEmpty())
                       {
                         canvas_->setModelKey(key);
                         updateTimeline();
                       }
                     });
    QObject::connect(live_, &QCheckBox::toggled, canvas_,
                     [this](bool checked)
                     {
                       canvas_->setLiveMode(checked);
                       updateTimeline();
                     });
    QObject::connect(time_slider_, &QSlider::valueChanged, canvas_,
                     [this](int position)
                     {
                       const TimeRange range = canvas_->dataTimeRange();
                       if (!range.valid)
                       {
                         return;
                       }
                       live_->setChecked(false);
                       const double ratio =
                           static_cast<double>(position) / static_cast<double>(kTimelineSteps);
                       const double time = range.min + ratio * (range.max - range.min);
                       canvas_->setSelectedTime(time);
                       updateTimelineLabels(range, time);
                     });
    QObject::connect(&timer_, &QTimer::timeout, canvas_,
                     [this]()
                     {
                       canvas_->update();
                       updateTimeline();
                       status_->setText(canvas_->statusText());
                     });
    timer_.setInterval(50);
    timer_.start();
  }

  void setData(PJ::PlotDataMapRef* data) { canvas_->setData(data); }

  QToolButton* closeButton() const { return close_button_; }

private:
  static constexpr int kTimelineSteps = 10000;

  QString formatTime(double value) const
  {
    if (!std::isfinite(value))
    {
      return "--";
    }
    return QString::number(value, 'f', 3);
  }

  void updateTimelineLabels(const TimeRange& range, double current_time)
  {
    time_min_->setText(formatTime(range.min));
    time_max_->setText(formatTime(range.max));
    time_current_->setText(formatTime(current_time));
  }

  void updateTimeline()
  {
    const TimeRange range = canvas_->dataTimeRange();
    time_slider_->setEnabled(range.valid && range.max > range.min);
    if (!range.valid)
    {
      time_min_->setText("--");
      time_max_->setText("--");
      time_current_->setText("--");
      return;
    }

    double current_time = range.max;
    if (!live_->isChecked())
    {
      const double ratio =
          static_cast<double>(time_slider_->value()) / static_cast<double>(kTimelineSteps);
      current_time = range.min + ratio * (range.max - range.min);
      canvas_->setSelectedTime(current_time);
    }
    else
    {
      const bool previous = time_slider_->blockSignals(true);
      time_slider_->setValue(kTimelineSteps);
      time_slider_->blockSignals(previous);
    }

    updateTimelineLabels(range, current_time);
  }

  RobotCanvas* canvas_ = nullptr;
  QToolButton* close_button_ = nullptr;
  QComboBox* model_selector_ = nullptr;
  QCheckBox* use_imu_ = nullptr;
  QCheckBox* draw_mesh_ = nullptr;
  QCheckBox* live_ = nullptr;
  QLabel* status_ = nullptr;
  QLabel* time_min_ = nullptr;
  QLabel* time_current_ = nullptr;
  QLabel* time_max_ = nullptr;
  QSlider* time_slider_ = nullptr;
  QTimer timer_;
};

UnitreeRobotViewToolbox::UnitreeRobotViewToolbox() = default;

UnitreeRobotViewToolbox::~UnitreeRobotViewToolbox() = default;

const char* UnitreeRobotViewToolbox::name() const { return "Unitree Robot View"; }

void UnitreeRobotViewToolbox::init(PJ::PlotDataMapRef& src_data, PJ::TransformsMap& transform_map)
{
  (void)transform_map;
  if (!widget_)
  {
    widget_ = new UnitreeRobotViewWidget();
    QObject::connect(widget_->closeButton(), &QToolButton::clicked, this,
                     [this]() { Q_EMIT closed(); });
  }
  widget_->setData(&src_data);
}

std::pair<QWidget*, UnitreeRobotViewToolbox::WidgetType>
UnitreeRobotViewToolbox::providedWidget() const
{
  return {widget_, PJ::ToolboxPlugin::FIXED};
}

bool UnitreeRobotViewToolbox::onShowWidget() { return true; }

} // namespace plotjuggler_unitree_sdk2
