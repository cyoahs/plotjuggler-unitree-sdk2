#include "plotjuggler_unitree_sdk2/unitree_message_flatten.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace plotjuggler_unitree_sdk2
{
namespace
{

template <typename Value>
void addScalar(const SampleSink& sink, const std::string& name, Value value)
{
  sink(name, static_cast<double>(value));
}

template <typename Array>
void addArray(const SampleSink& sink, const std::string& name, const Array& values)
{
  for (std::size_t index = 0; index < values.size(); ++index)
  {
    sink(name + "/" + std::to_string(index), static_cast<double>(values[index]));
  }
}

std::string twoDigits(std::size_t index)
{
  std::ostringstream stream;
  stream << std::setw(2) << std::setfill('0') << index;
  return stream.str();
}

std::string joinPrefix(const std::string& prefix, const char* field)
{
  if (prefix.empty())
  {
    return field;
  }
  return prefix + "/" + field;
}

std::string joinPrefix(const std::string& prefix, std::size_t index)
{
  if (prefix.empty())
  {
    return twoDigits(index);
  }
  return prefix + "/" + twoDigits(index);
}

template <typename> struct IsStdArray : std::false_type
{
};

template <typename Value, std::size_t Size>
struct IsStdArray<std::array<Value, Size>> : std::true_type
{
};

template <typename> struct IsStdVector : std::false_type
{
};

template <typename Value, typename Allocator>
struct IsStdVector<std::vector<Value, Allocator>> : std::true_type
{
};

template <typename> struct IsStdString : std::false_type
{
};

template <typename Traits, typename Allocator>
struct IsStdString<std::basic_string<char, Traits, Allocator>> : std::true_type
{
};

#define PJ_DECLARE_HAS_FIELD(FIELD)                                                                \
  template <typename Msg, typename = void> struct HasField_##FIELD : std::false_type               \
  {                                                                                                \
  };                                                                                               \
  template <typename Msg>                                                                          \
  struct HasField_##FIELD<Msg, std::void_t<decltype(std::declval<const Msg&>().FIELD())>>          \
      : std::true_type                                                                             \
  {                                                                                                \
  };

PJ_DECLARE_HAS_FIELD(accelerometer)
PJ_DECLARE_HAS_FIELD(adc_reel)
PJ_DECLARE_HAS_FIELD(angular)
PJ_DECLARE_HAS_FIELD(angular_velocity)
PJ_DECLARE_HAS_FIELD(angular_velocity_covariance)
PJ_DECLARE_HAS_FIELD(bandwidth)
PJ_DECLARE_HAS_FIELD(base_pitch)
PJ_DECLARE_HAS_FIELD(base_roll)
PJ_DECLARE_HAS_FIELD(base_yaw)
PJ_DECLARE_HAS_FIELD(battery_percentage)
PJ_DECLARE_HAS_FIELD(bit_flag)
PJ_DECLARE_HAS_FIELD(bms_cmd)
PJ_DECLARE_HAS_FIELD(bms_state)
PJ_DECLARE_HAS_FIELD(bmsstate)
PJ_DECLARE_HAS_FIELD(bmsvoltage)
PJ_DECLARE_HAS_FIELD(body)
PJ_DECLARE_HAS_FIELD(body_height)
PJ_DECLARE_HAS_FIELD(bq_ntc)
PJ_DECLARE_HAS_FIELD(buttons)
PJ_DECLARE_HAS_FIELD(cell_vol)
PJ_DECLARE_HAS_FIELD(channel)
PJ_DECLARE_HAS_FIELD(child_frame_id)
PJ_DECLARE_HAS_FIELD(cloud_frequency)
PJ_DECLARE_HAS_FIELD(cloud_packet_loss_rate)
PJ_DECLARE_HAS_FIELD(cloud_scan_num)
PJ_DECLARE_HAS_FIELD(cloud_size)
PJ_DECLARE_HAS_FIELD(cmd)
PJ_DECLARE_HAS_FIELD(cmds)
PJ_DECLARE_HAS_FIELD(com_rotation_speed)
PJ_DECLARE_HAS_FIELD(content)
PJ_DECLARE_HAS_FIELD(count)
PJ_DECLARE_HAS_FIELD(covariance)
PJ_DECLARE_HAS_FIELD(crc)
PJ_DECLARE_HAS_FIELD(current)
PJ_DECLARE_HAS_FIELD(cycle)
PJ_DECLARE_HAS_FIELD(data)
PJ_DECLARE_HAS_FIELD(datatype)
PJ_DECLARE_HAS_FIELD(ddq)
PJ_DECLARE_HAS_FIELD(ddq_raw)
PJ_DECLARE_HAS_FIELD(device_v)
PJ_DECLARE_HAS_FIELD(dirty_percentage)
PJ_DECLARE_HAS_FIELD(distance_est)
PJ_DECLARE_HAS_FIELD(docking_status)
PJ_DECLARE_HAS_FIELD(dq)
PJ_DECLARE_HAS_FIELD(dq_raw)
PJ_DECLARE_HAS_FIELD(enabled)
PJ_DECLARE_HAS_FIELD(enabled_from_app)
PJ_DECLARE_HAS_FIELD(error)
PJ_DECLARE_HAS_FIELD(error_code)
PJ_DECLARE_HAS_FIELD(error_state)
PJ_DECLARE_HAS_FIELD(euler)
PJ_DECLARE_HAS_FIELD(fan)
PJ_DECLARE_HAS_FIELD(fan_frequency)
PJ_DECLARE_HAS_FIELD(fan_state)
PJ_DECLARE_HAS_FIELD(fields)
PJ_DECLARE_HAS_FIELD(firmware_version)
PJ_DECLARE_HAS_FIELD(fn)
PJ_DECLARE_HAS_FIELD(foot_force)
PJ_DECLARE_HAS_FIELD(foot_force_est)
PJ_DECLARE_HAS_FIELD(foot_position_body)
PJ_DECLARE_HAS_FIELD(foot_raise_height)
PJ_DECLARE_HAS_FIELD(foot_speed_body)
PJ_DECLARE_HAS_FIELD(frame_id)
PJ_DECLARE_HAS_FIELD(frame_reserve)
PJ_DECLARE_HAS_FIELD(fsm_id)
PJ_DECLARE_HAS_FIELD(fsm_mode)
PJ_DECLARE_HAS_FIELD(gait_type)
PJ_DECLARE_HAS_FIELD(gpio)
PJ_DECLARE_HAS_FIELD(gyroscope)
PJ_DECLARE_HAS_FIELD(head)
PJ_DECLARE_HAS_FIELD(header)
PJ_DECLARE_HAS_FIELD(height)
PJ_DECLARE_HAS_FIELD(imu_frequency)
PJ_DECLARE_HAS_FIELD(imu_packet_loss_rate)
PJ_DECLARE_HAS_FIELD(imu_rpy)
PJ_DECLARE_HAS_FIELD(imu_state)
PJ_DECLARE_HAS_FIELD(info)
PJ_DECLARE_HAS_FIELD(is_bigendian)
PJ_DECLARE_HAS_FIELD(is_charging)
PJ_DECLARE_HAS_FIELD(is_dc_connected)
PJ_DECLARE_HAS_FIELD(is_dense)
PJ_DECLARE_HAS_FIELD(joy_mode)
PJ_DECLARE_HAS_FIELD(joystick)
PJ_DECLARE_HAS_FIELD(kd)
PJ_DECLARE_HAS_FIELD(keys)
PJ_DECLARE_HAS_FIELD(kp)
PJ_DECLARE_HAS_FIELD(led)
PJ_DECLARE_HAS_FIELD(level_flag)
PJ_DECLARE_HAS_FIELD(linear)
PJ_DECLARE_HAS_FIELD(linear_acceleration)
PJ_DECLARE_HAS_FIELD(linear_acceleration_covariance)
PJ_DECLARE_HAS_FIELD(lost)
PJ_DECLARE_HAS_FIELD(lx)
PJ_DECLARE_HAS_FIELD(ly)
PJ_DECLARE_HAS_FIELD(manufacturer_date)
PJ_DECLARE_HAS_FIELD(map_load_time)
PJ_DECLARE_HAS_FIELD(mcu_ntc)
PJ_DECLARE_HAS_FIELD(mode)
PJ_DECLARE_HAS_FIELD(mode_machine)
PJ_DECLARE_HAS_FIELD(mode_pr)
PJ_DECLARE_HAS_FIELD(motor_cmd)
PJ_DECLARE_HAS_FIELD(motor_state)
PJ_DECLARE_HAS_FIELD(motorstate)
PJ_DECLARE_HAS_FIELD(name)
PJ_DECLARE_HAS_FIELD(nanosec)
PJ_DECLARE_HAS_FIELD(off)
PJ_DECLARE_HAS_FIELD(offset)
PJ_DECLARE_HAS_FIELD(orientation)
PJ_DECLARE_HAS_FIELD(orientation_covariance)
PJ_DECLARE_HAS_FIELD(orientation_est)
PJ_DECLARE_HAS_FIELD(origin)
PJ_DECLARE_HAS_FIELD(path_point)
PJ_DECLARE_HAS_FIELD(pitch_est)
PJ_DECLARE_HAS_FIELD(point)
PJ_DECLARE_HAS_FIELD(point_step)
PJ_DECLARE_HAS_FIELD(pose)
PJ_DECLARE_HAS_FIELD(position)
PJ_DECLARE_HAS_FIELD(power_a)
PJ_DECLARE_HAS_FIELD(power_v)
PJ_DECLARE_HAS_FIELD(press_sensor_state)
PJ_DECLARE_HAS_FIELD(pressure)
PJ_DECLARE_HAS_FIELD(progress)
PJ_DECLARE_HAS_FIELD(q)
PJ_DECLARE_HAS_FIELD(q_raw)
PJ_DECLARE_HAS_FIELD(quaternion)
PJ_DECLARE_HAS_FIELD(range_obstacle)
PJ_DECLARE_HAS_FIELD(reserve)
PJ_DECLARE_HAS_FIELD(resolution)
PJ_DECLARE_HAS_FIELD(row_step)
PJ_DECLARE_HAS_FIELD(rpy)
PJ_DECLARE_HAS_FIELD(rx)
PJ_DECLARE_HAS_FIELD(ry)
PJ_DECLARE_HAS_FIELD(sdk_version)
PJ_DECLARE_HAS_FIELD(sec)
PJ_DECLARE_HAS_FIELD(sensor)
PJ_DECLARE_HAS_FIELD(serial_buffer_read)
PJ_DECLARE_HAS_FIELD(serial_buffer_size)
PJ_DECLARE_HAS_FIELD(serial_recv_stamp)
PJ_DECLARE_HAS_FIELD(sn)
PJ_DECLARE_HAS_FIELD(soc)
PJ_DECLARE_HAS_FIELD(software_version)
PJ_DECLARE_HAS_FIELD(soh)
PJ_DECLARE_HAS_FIELD(source)
PJ_DECLARE_HAS_FIELD(speed_level)
PJ_DECLARE_HAS_FIELD(src_size)
PJ_DECLARE_HAS_FIELD(stamp)
PJ_DECLARE_HAS_FIELD(state)
PJ_DECLARE_HAS_FIELD(states)
PJ_DECLARE_HAS_FIELD(status)
PJ_DECLARE_HAS_FIELD(sys_rotation_speed)
PJ_DECLARE_HAS_FIELD(system_v)
PJ_DECLARE_HAS_FIELD(t_from_start)
PJ_DECLARE_HAS_FIELD(tag_pitch)
PJ_DECLARE_HAS_FIELD(tag_roll)
PJ_DECLARE_HAS_FIELD(tag_yaw)
PJ_DECLARE_HAS_FIELD(task_id)
PJ_DECLARE_HAS_FIELD(task_time)
PJ_DECLARE_HAS_FIELD(tau)
PJ_DECLARE_HAS_FIELD(tau_est)
PJ_DECLARE_HAS_FIELD(temperature)
PJ_DECLARE_HAS_FIELD(temperature_ntc1)
PJ_DECLARE_HAS_FIELD(temperature_ntc2)
PJ_DECLARE_HAS_FIELD(theta)
PJ_DECLARE_HAS_FIELD(tick)
PJ_DECLARE_HAS_FIELD(time_frame)
PJ_DECLARE_HAS_FIELD(twist)
PJ_DECLARE_HAS_FIELD(uuid)
PJ_DECLARE_HAS_FIELD(value)
PJ_DECLARE_HAS_FIELD(velocity)
PJ_DECLARE_HAS_FIELD(version)
PJ_DECLARE_HAS_FIELD(version_high)
PJ_DECLARE_HAS_FIELD(version_low)
PJ_DECLARE_HAS_FIELD(video180p)
PJ_DECLARE_HAS_FIELD(video360p)
PJ_DECLARE_HAS_FIELD(video720p)
PJ_DECLARE_HAS_FIELD(vol)
PJ_DECLARE_HAS_FIELD(vx)
PJ_DECLARE_HAS_FIELD(vy)
PJ_DECLARE_HAS_FIELD(vyaw)
PJ_DECLARE_HAS_FIELD(w)
PJ_DECLARE_HAS_FIELD(width)
PJ_DECLARE_HAS_FIELD(wireless_remote)
PJ_DECLARE_HAS_FIELD(x)
PJ_DECLARE_HAS_FIELD(y)
PJ_DECLARE_HAS_FIELD(yaw)
PJ_DECLARE_HAS_FIELD(yaw_est)
PJ_DECLARE_HAS_FIELD(yaw_speed)
PJ_DECLARE_HAS_FIELD(z)

#undef PJ_DECLARE_HAS_FIELD

template <typename Msg>
void flattenGeneric(const Msg& msg, const std::string& prefix, const MessageFlattenOptions& options,
                    const SampleSink& sink);

template <typename Value>
void flattenValue(const SampleSink& sink, const std::string& name, const Value& value,
                  const MessageFlattenOptions& options)
{
  using Decayed = std::decay_t<Value>;
  constexpr std::size_t kMaxVariableSequenceSamples = 256;

  if constexpr (std::is_arithmetic<Decayed>::value)
  {
    addScalar(sink, name, value);
  }
  else if constexpr (IsStdString<Decayed>::value)
  {
    addScalar(sink, name + "/length", value.size());
  }
  else if constexpr (IsStdArray<Decayed>::value)
  {
    for (std::size_t index = 0; index < value.size(); ++index)
    {
      flattenValue(sink, joinPrefix(name, index), value[index], options);
    }
  }
  else if constexpr (IsStdVector<Decayed>::value)
  {
    addScalar(sink, name + "/size", value.size());
    const std::size_t limit = std::min(value.size(), kMaxVariableSequenceSamples);
    for (std::size_t index = 0; index < limit; ++index)
    {
      flattenValue(sink, joinPrefix(name, index), value[index], options);
    }
    if (value.size() > limit)
    {
      addScalar(sink, name + "/truncated", value.size() - limit);
    }
  }
  else
  {
    flattenGeneric(value, name, options, sink);
  }
}

#define PJ_FLATTEN_FIELD(FIELD)                                                                    \
  if constexpr (HasField_##FIELD<Msg>::value)                                                      \
  {                                                                                                \
    flattenValue(sink, joinPrefix(prefix, #FIELD), msg.FIELD(), options);                          \
  }

template <typename Msg>
void flattenGeneric(const Msg& msg, const std::string& prefix, const MessageFlattenOptions& options,
                    const SampleSink& sink)
{
  PJ_FLATTEN_FIELD(accelerometer)
  PJ_FLATTEN_FIELD(adc_reel)
  PJ_FLATTEN_FIELD(angular)
  PJ_FLATTEN_FIELD(angular_velocity)
  PJ_FLATTEN_FIELD(angular_velocity_covariance)
  PJ_FLATTEN_FIELD(bandwidth)
  PJ_FLATTEN_FIELD(base_pitch)
  PJ_FLATTEN_FIELD(base_roll)
  PJ_FLATTEN_FIELD(base_yaw)
  PJ_FLATTEN_FIELD(battery_percentage)
  PJ_FLATTEN_FIELD(bit_flag)
  PJ_FLATTEN_FIELD(bms_cmd)
  PJ_FLATTEN_FIELD(bms_state)
  PJ_FLATTEN_FIELD(bmsstate)
  PJ_FLATTEN_FIELD(bmsvoltage)
  PJ_FLATTEN_FIELD(body)
  PJ_FLATTEN_FIELD(body_height)
  PJ_FLATTEN_FIELD(bq_ntc)
  PJ_FLATTEN_FIELD(buttons)
  PJ_FLATTEN_FIELD(cell_vol)
  PJ_FLATTEN_FIELD(channel)
  PJ_FLATTEN_FIELD(child_frame_id)
  PJ_FLATTEN_FIELD(cloud_frequency)
  PJ_FLATTEN_FIELD(cloud_packet_loss_rate)
  PJ_FLATTEN_FIELD(cloud_scan_num)
  PJ_FLATTEN_FIELD(cloud_size)
  PJ_FLATTEN_FIELD(cmd)
  PJ_FLATTEN_FIELD(cmds)
  PJ_FLATTEN_FIELD(com_rotation_speed)
  PJ_FLATTEN_FIELD(content)
  PJ_FLATTEN_FIELD(count)
  PJ_FLATTEN_FIELD(covariance)
  PJ_FLATTEN_FIELD(crc)
  PJ_FLATTEN_FIELD(current)
  PJ_FLATTEN_FIELD(cycle)
  PJ_FLATTEN_FIELD(data)
  PJ_FLATTEN_FIELD(datatype)
  PJ_FLATTEN_FIELD(ddq)
  PJ_FLATTEN_FIELD(ddq_raw)
  PJ_FLATTEN_FIELD(device_v)
  PJ_FLATTEN_FIELD(dirty_percentage)
  PJ_FLATTEN_FIELD(distance_est)
  PJ_FLATTEN_FIELD(docking_status)
  PJ_FLATTEN_FIELD(dq)
  PJ_FLATTEN_FIELD(dq_raw)
  PJ_FLATTEN_FIELD(enabled)
  PJ_FLATTEN_FIELD(enabled_from_app)
  PJ_FLATTEN_FIELD(error)
  PJ_FLATTEN_FIELD(error_code)
  PJ_FLATTEN_FIELD(error_state)
  PJ_FLATTEN_FIELD(euler)
  PJ_FLATTEN_FIELD(fan)
  PJ_FLATTEN_FIELD(fan_frequency)
  PJ_FLATTEN_FIELD(fan_state)
  PJ_FLATTEN_FIELD(fields)
  PJ_FLATTEN_FIELD(firmware_version)
  PJ_FLATTEN_FIELD(fn)
  PJ_FLATTEN_FIELD(foot_force)
  PJ_FLATTEN_FIELD(foot_force_est)
  PJ_FLATTEN_FIELD(foot_position_body)
  PJ_FLATTEN_FIELD(foot_raise_height)
  PJ_FLATTEN_FIELD(foot_speed_body)
  PJ_FLATTEN_FIELD(frame_id)
  PJ_FLATTEN_FIELD(frame_reserve)
  PJ_FLATTEN_FIELD(fsm_id)
  PJ_FLATTEN_FIELD(fsm_mode)
  PJ_FLATTEN_FIELD(gait_type)
  PJ_FLATTEN_FIELD(gpio)
  PJ_FLATTEN_FIELD(gyroscope)
  PJ_FLATTEN_FIELD(head)
  PJ_FLATTEN_FIELD(header)
  PJ_FLATTEN_FIELD(height)
  PJ_FLATTEN_FIELD(imu_frequency)
  PJ_FLATTEN_FIELD(imu_packet_loss_rate)
  PJ_FLATTEN_FIELD(imu_rpy)
  PJ_FLATTEN_FIELD(imu_state)
  PJ_FLATTEN_FIELD(info)
  PJ_FLATTEN_FIELD(is_bigendian)
  PJ_FLATTEN_FIELD(is_charging)
  PJ_FLATTEN_FIELD(is_dc_connected)
  PJ_FLATTEN_FIELD(is_dense)
  PJ_FLATTEN_FIELD(joy_mode)
  PJ_FLATTEN_FIELD(joystick)
  PJ_FLATTEN_FIELD(kd)
  PJ_FLATTEN_FIELD(keys)
  PJ_FLATTEN_FIELD(kp)
  PJ_FLATTEN_FIELD(led)
  PJ_FLATTEN_FIELD(level_flag)
  PJ_FLATTEN_FIELD(linear)
  PJ_FLATTEN_FIELD(linear_acceleration)
  PJ_FLATTEN_FIELD(linear_acceleration_covariance)
  PJ_FLATTEN_FIELD(lost)
  PJ_FLATTEN_FIELD(lx)
  PJ_FLATTEN_FIELD(ly)
  PJ_FLATTEN_FIELD(manufacturer_date)
  PJ_FLATTEN_FIELD(map_load_time)
  PJ_FLATTEN_FIELD(mcu_ntc)
  PJ_FLATTEN_FIELD(mode)
  PJ_FLATTEN_FIELD(mode_machine)
  PJ_FLATTEN_FIELD(mode_pr)
  PJ_FLATTEN_FIELD(motor_cmd)
  PJ_FLATTEN_FIELD(motor_state)
  PJ_FLATTEN_FIELD(motorstate)
  PJ_FLATTEN_FIELD(name)
  PJ_FLATTEN_FIELD(nanosec)
  PJ_FLATTEN_FIELD(off)
  PJ_FLATTEN_FIELD(offset)
  PJ_FLATTEN_FIELD(orientation)
  PJ_FLATTEN_FIELD(orientation_covariance)
  PJ_FLATTEN_FIELD(orientation_est)
  PJ_FLATTEN_FIELD(origin)
  PJ_FLATTEN_FIELD(path_point)
  PJ_FLATTEN_FIELD(pitch_est)
  PJ_FLATTEN_FIELD(point)
  PJ_FLATTEN_FIELD(point_step)
  PJ_FLATTEN_FIELD(pose)
  PJ_FLATTEN_FIELD(position)
  PJ_FLATTEN_FIELD(power_a)
  PJ_FLATTEN_FIELD(power_v)
  PJ_FLATTEN_FIELD(press_sensor_state)
  PJ_FLATTEN_FIELD(pressure)
  PJ_FLATTEN_FIELD(progress)
  PJ_FLATTEN_FIELD(q)
  PJ_FLATTEN_FIELD(q_raw)
  PJ_FLATTEN_FIELD(quaternion)
  PJ_FLATTEN_FIELD(range_obstacle)
  PJ_FLATTEN_FIELD(reserve)
  PJ_FLATTEN_FIELD(resolution)
  PJ_FLATTEN_FIELD(row_step)
  PJ_FLATTEN_FIELD(rpy)
  PJ_FLATTEN_FIELD(rx)
  PJ_FLATTEN_FIELD(ry)
  PJ_FLATTEN_FIELD(sdk_version)
  PJ_FLATTEN_FIELD(sec)
  PJ_FLATTEN_FIELD(sensor)
  PJ_FLATTEN_FIELD(serial_buffer_read)
  PJ_FLATTEN_FIELD(serial_buffer_size)
  PJ_FLATTEN_FIELD(serial_recv_stamp)
  PJ_FLATTEN_FIELD(sn)
  PJ_FLATTEN_FIELD(soc)
  PJ_FLATTEN_FIELD(software_version)
  PJ_FLATTEN_FIELD(soh)
  PJ_FLATTEN_FIELD(source)
  PJ_FLATTEN_FIELD(speed_level)
  PJ_FLATTEN_FIELD(src_size)
  PJ_FLATTEN_FIELD(stamp)
  PJ_FLATTEN_FIELD(state)
  PJ_FLATTEN_FIELD(states)
  PJ_FLATTEN_FIELD(status)
  PJ_FLATTEN_FIELD(sys_rotation_speed)
  PJ_FLATTEN_FIELD(system_v)
  PJ_FLATTEN_FIELD(t_from_start)
  PJ_FLATTEN_FIELD(tag_pitch)
  PJ_FLATTEN_FIELD(tag_roll)
  PJ_FLATTEN_FIELD(tag_yaw)
  PJ_FLATTEN_FIELD(task_id)
  PJ_FLATTEN_FIELD(task_time)
  PJ_FLATTEN_FIELD(tau)
  PJ_FLATTEN_FIELD(tau_est)
  PJ_FLATTEN_FIELD(temperature)
  PJ_FLATTEN_FIELD(temperature_ntc1)
  PJ_FLATTEN_FIELD(temperature_ntc2)
  PJ_FLATTEN_FIELD(theta)
  PJ_FLATTEN_FIELD(tick)
  PJ_FLATTEN_FIELD(time_frame)
  PJ_FLATTEN_FIELD(twist)
  PJ_FLATTEN_FIELD(uuid)
  PJ_FLATTEN_FIELD(value)
  PJ_FLATTEN_FIELD(velocity)
  PJ_FLATTEN_FIELD(version)
  PJ_FLATTEN_FIELD(version_high)
  PJ_FLATTEN_FIELD(version_low)
  PJ_FLATTEN_FIELD(video180p)
  PJ_FLATTEN_FIELD(video360p)
  PJ_FLATTEN_FIELD(video720p)
  PJ_FLATTEN_FIELD(vol)
  PJ_FLATTEN_FIELD(vx)
  PJ_FLATTEN_FIELD(vy)
  PJ_FLATTEN_FIELD(vyaw)
  PJ_FLATTEN_FIELD(w)
  PJ_FLATTEN_FIELD(width)
  PJ_FLATTEN_FIELD(wireless_remote)
  PJ_FLATTEN_FIELD(x)
  PJ_FLATTEN_FIELD(y)
  PJ_FLATTEN_FIELD(yaw)
  PJ_FLATTEN_FIELD(yaw_est)
  PJ_FLATTEN_FIELD(yaw_speed)
  PJ_FLATTEN_FIELD(z)

  if constexpr (HasField_sec<Msg>::value && HasField_nanosec<Msg>::value)
  {
    addScalar(sink, joinPrefix(prefix, "seconds"),
              msg.sec() + static_cast<double>(msg.nanosec()) * 1e-9);
  }
}

#undef PJ_FLATTEN_FIELD

bool wantsRawJoystick(JoystickOutputMode mode)
{
  return mode == JoystickOutputMode::RawBytes || mode == JoystickOutputMode::RawAndParsed;
}

bool wantsParsedJoystick(JoystickOutputMode mode)
{
  return mode == JoystickOutputMode::ParsedStructure || mode == JoystickOutputMode::RawAndParsed;
}

struct JoystickButton
{
  const char* name;
  uint16_t mask;
};

const std::array<JoystickButton, 16>& joystickButtons()
{
  static const std::array<JoystickButton, 16> buttons = {{
      {"R1", 1u << 0},
      {"L1", 1u << 1},
      {"start", 1u << 2},
      {"select", 1u << 3},
      {"R2", 1u << 4},
      {"L2", 1u << 5},
      {"F1", 1u << 6},
      {"F2", 1u << 7},
      {"A", 1u << 8},
      {"B", 1u << 9},
      {"X", 1u << 10},
      {"Y", 1u << 11},
      {"up", 1u << 12},
      {"right", 1u << 13},
      {"down", 1u << 14},
      {"left", 1u << 15},
  }};
  return buttons;
}

void addJoystickButtons(const SampleSink& sink, const std::string& prefix, uint16_t keys)
{
  addScalar(sink, prefix + "/value", keys);
  for (const JoystickButton& button : joystickButtons())
  {
    addScalar(sink, prefix + "/" + button.name, (keys & button.mask) ? 1 : 0);
  }
}

uint16_t readUInt16Le(const uint8_t* data)
{
  return static_cast<uint16_t>(data[0]) |
         static_cast<uint16_t>(static_cast<uint16_t>(data[1]) << 8);
}

float readFloat(const uint8_t* data)
{
  float value = 0.0F;
  std::memcpy(&value, data, sizeof(value));
  return value;
}

template <typename Array>
void addWirelessRemote(const SampleSink& sink, const std::string& prefix, const Array& bytes,
                       JoystickOutputMode mode)
{
  if (wantsRawJoystick(mode))
  {
    addArray(sink, prefix, bytes);
  }

  if (!wantsParsedJoystick(mode) || bytes.size() < 24)
  {
    return;
  }

  addScalar(sink, prefix + "/head/0", bytes[0]);
  addScalar(sink, prefix + "/head/1", bytes[1]);
  addJoystickButtons(sink, prefix + "/buttons", readUInt16Le(&bytes[2]));
  addScalar(sink, prefix + "/axes/lx", readFloat(&bytes[4]));
  addScalar(sink, prefix + "/axes/rx", readFloat(&bytes[8]));
  addScalar(sink, prefix + "/axes/ry", readFloat(&bytes[12]));
  addScalar(sink, prefix + "/axes/L2", readFloat(&bytes[16]));
  addScalar(sink, prefix + "/axes/ly", readFloat(&bytes[20]));
}

void addWirelessController(const unitree_go::msg::dds_::WirelessController_& msg,
                           JoystickOutputMode mode, const SampleSink& sink)
{
  if (wantsRawJoystick(mode))
  {
    addScalar(sink, "lx", msg.lx());
    addScalar(sink, "ly", msg.ly());
    addScalar(sink, "rx", msg.rx());
    addScalar(sink, "ry", msg.ry());
    addScalar(sink, "keys", msg.keys());
  }

  if (wantsParsedJoystick(mode))
  {
    addScalar(sink, "joystick/axes/lx", msg.lx());
    addScalar(sink, "joystick/axes/ly", msg.ly());
    addScalar(sink, "joystick/axes/rx", msg.rx());
    addScalar(sink, "joystick/axes/ry", msg.ry());
    addJoystickButtons(sink, "joystick/buttons", msg.keys());
  }
}

template <typename IMU>
void flattenImu(const IMU& msg, const std::string& prefix, const SampleSink& sink)
{
  addArray(sink, prefix + "/quaternion", msg.quaternion());
  addArray(sink, prefix + "/gyroscope", msg.gyroscope());
  addArray(sink, prefix + "/accelerometer", msg.accelerometer());
  addArray(sink, prefix + "/rpy", msg.rpy());
  addScalar(sink, prefix + "/temperature", msg.temperature());
}

void flattenGoMotorState(const unitree_go::msg::dds_::MotorState_& motor, const std::string& prefix,
                         const SampleSink& sink)
{
  addScalar(sink, prefix + "/mode", motor.mode());
  addScalar(sink, prefix + "/q", motor.q());
  addScalar(sink, prefix + "/dq", motor.dq());
  addScalar(sink, prefix + "/ddq", motor.ddq());
  addScalar(sink, prefix + "/tau_est", motor.tau_est());
  addScalar(sink, prefix + "/q_raw", motor.q_raw());
  addScalar(sink, prefix + "/dq_raw", motor.dq_raw());
  addScalar(sink, prefix + "/ddq_raw", motor.ddq_raw());
  addScalar(sink, prefix + "/temperature", motor.temperature());
  addScalar(sink, prefix + "/lost", motor.lost());
  addArray(sink, prefix + "/reserve", motor.reserve());
}

void flattenHgMotorState(const unitree_hg::msg::dds_::MotorState_& motor, const std::string& prefix,
                         const SampleSink& sink)
{
  addScalar(sink, prefix + "/mode", motor.mode());
  addScalar(sink, prefix + "/q", motor.q());
  addScalar(sink, prefix + "/dq", motor.dq());
  addScalar(sink, prefix + "/ddq", motor.ddq());
  addScalar(sink, prefix + "/tau_est", motor.tau_est());
  addArray(sink, prefix + "/temperature", motor.temperature());
  addScalar(sink, prefix + "/vol", motor.vol());
  addArray(sink, prefix + "/sensor", motor.sensor());
  addScalar(sink, prefix + "/motorstate", motor.motorstate());
  addArray(sink, prefix + "/reserve", motor.reserve());
}

void flattenBmsState(const unitree_go::msg::dds_::BmsState_& bms, const std::string& prefix,
                     const SampleSink& sink)
{
  addScalar(sink, prefix + "/version_high", bms.version_high());
  addScalar(sink, prefix + "/version_low", bms.version_low());
  addScalar(sink, prefix + "/status", bms.status());
  addScalar(sink, prefix + "/soc", bms.soc());
  addScalar(sink, prefix + "/current", bms.current());
  addScalar(sink, prefix + "/cycle", bms.cycle());
  addArray(sink, prefix + "/bq_ntc", bms.bq_ntc());
  addArray(sink, prefix + "/mcu_ntc", bms.mcu_ntc());
  addArray(sink, prefix + "/cell_vol", bms.cell_vol());
}

void flattenPathPoint(const unitree_go::msg::dds_::PathPoint_& point, const std::string& prefix,
                      const SampleSink& sink)
{
  addScalar(sink, prefix + "/t_from_start", point.t_from_start());
  addScalar(sink, prefix + "/x", point.x());
  addScalar(sink, prefix + "/y", point.y());
  addScalar(sink, prefix + "/yaw", point.yaw());
  addScalar(sink, prefix + "/vx", point.vx());
  addScalar(sink, prefix + "/vy", point.vy());
  addScalar(sink, prefix + "/vyaw", point.vyaw());
}

} // namespace

std::string normalizeTopicPrefix(const std::string& topic)
{
  std::string normalized;
  normalized.reserve(topic.size());
  bool previous_was_separator = false;
  for (char ch : topic)
  {
    const bool separator = (ch == '/' || ch == ' ' || ch == '\t');
    if (separator)
    {
      if (!normalized.empty() && !previous_was_separator)
      {
        normalized.push_back('/');
      }
      previous_was_separator = true;
    }
    else
    {
      normalized.push_back(ch);
      previous_was_separator = false;
    }
  }
  while (!normalized.empty() && normalized.back() == '/')
  {
    normalized.pop_back();
  }
  return normalized.empty() ? "unnamed" : normalized;
}

void flattenMessage(const unitree_go::msg::dds_::LowState_& msg,
                    const MessageFlattenOptions& options, const SampleSink& sink)
{
  addArray(sink, "head", msg.head());
  addScalar(sink, "level_flag", msg.level_flag());
  addScalar(sink, "frame_reserve", msg.frame_reserve());
  addArray(sink, "sn", msg.sn());
  addArray(sink, "version", msg.version());
  addScalar(sink, "bandwidth", msg.bandwidth());
  flattenImu(msg.imu_state(), "imu_state", sink);

  const auto& motor_state = msg.motor_state();
  for (std::size_t index = 0; index < motor_state.size(); ++index)
  {
    flattenGoMotorState(motor_state[index], "motor_state/" + twoDigits(index), sink);
  }

  flattenBmsState(msg.bms_state(), "bms_state", sink);
  addArray(sink, "foot_force", msg.foot_force());
  addArray(sink, "foot_force_est", msg.foot_force_est());
  addScalar(sink, "tick", msg.tick());
  addWirelessRemote(sink, "wireless_remote", msg.wireless_remote(), options.joystick_output_mode);
  addScalar(sink, "bit_flag", msg.bit_flag());
  addScalar(sink, "adc_reel", msg.adc_reel());
  addScalar(sink, "temperature_ntc1", msg.temperature_ntc1());
  addScalar(sink, "temperature_ntc2", msg.temperature_ntc2());
  addScalar(sink, "power_v", msg.power_v());
  addScalar(sink, "power_a", msg.power_a());
  addArray(sink, "fan_frequency", msg.fan_frequency());
  addScalar(sink, "reserve", msg.reserve());
  addScalar(sink, "crc", msg.crc());
}

void flattenMessage(const unitree_go::msg::dds_::SportModeState_& msg, const MessageFlattenOptions&,
                    const SampleSink& sink)
{
  addScalar(sink, "stamp/sec", msg.stamp().sec());
  addScalar(sink, "stamp/nanosec", msg.stamp().nanosec());
  addScalar(sink, "stamp/seconds", msg.stamp().sec() + 1e-9 * msg.stamp().nanosec());
  addScalar(sink, "error_code", msg.error_code());
  flattenImu(msg.imu_state(), "imu_state", sink);
  addScalar(sink, "mode", msg.mode());
  addScalar(sink, "progress", msg.progress());
  addScalar(sink, "gait_type", msg.gait_type());
  addScalar(sink, "foot_raise_height", msg.foot_raise_height());
  addArray(sink, "position", msg.position());
  addScalar(sink, "body_height", msg.body_height());
  addArray(sink, "velocity", msg.velocity());
  addScalar(sink, "yaw_speed", msg.yaw_speed());
  addArray(sink, "range_obstacle", msg.range_obstacle());
  addArray(sink, "foot_force", msg.foot_force());
  addArray(sink, "foot_position_body", msg.foot_position_body());
  addArray(sink, "foot_speed_body", msg.foot_speed_body());

  const auto& path_points = msg.path_point();
  for (std::size_t index = 0; index < path_points.size(); ++index)
  {
    flattenPathPoint(path_points[index], "path_point/" + twoDigits(index), sink);
  }
}

void flattenMessage(const unitree_go::msg::dds_::WirelessController_& msg,
                    const MessageFlattenOptions& options, const SampleSink& sink)
{
  addWirelessController(msg, options.joystick_output_mode, sink);
}

void flattenMessage(const unitree_hg::msg::dds_::LowState_& msg,
                    const MessageFlattenOptions& options, const SampleSink& sink)
{
  addArray(sink, "version", msg.version());
  addScalar(sink, "mode_pr", msg.mode_pr());
  addScalar(sink, "mode_machine", msg.mode_machine());
  addScalar(sink, "tick", msg.tick());
  flattenImu(msg.imu_state(), "imu_state", sink);

  const auto& motor_state = msg.motor_state();
  for (std::size_t index = 0; index < motor_state.size(); ++index)
  {
    flattenHgMotorState(motor_state[index], "motor_state/" + twoDigits(index), sink);
  }

  addWirelessRemote(sink, "wireless_remote", msg.wireless_remote(), options.joystick_output_mode);
  addArray(sink, "reserve", msg.reserve());
  addScalar(sink, "crc", msg.crc());
}

void flattenMessage(const unitree_hg::msg::dds_::SportModeState_& msg, const MessageFlattenOptions&,
                    const SampleSink& sink)
{
  addScalar(sink, "fsm_id", msg.fsm_id());
  addScalar(sink, "fsm_mode", msg.fsm_mode());
  addScalar(sink, "task_id", msg.task_id());
  addScalar(sink, "task_time", msg.task_time());
}

void flattenMessage(const unitree_go::msg::dds_::LowState_& msg, const SampleSink& sink)
{
  flattenMessage(msg, MessageFlattenOptions{}, sink);
}

void flattenMessage(const unitree_go::msg::dds_::SportModeState_& msg, const SampleSink& sink)
{
  flattenMessage(msg, MessageFlattenOptions{}, sink);
}

void flattenMessage(const unitree_go::msg::dds_::WirelessController_& msg, const SampleSink& sink)
{
  flattenMessage(msg, MessageFlattenOptions{}, sink);
}

void flattenMessage(const unitree_hg::msg::dds_::LowState_& msg, const SampleSink& sink)
{
  flattenMessage(msg, MessageFlattenOptions{}, sink);
}

void flattenMessage(const unitree_hg::msg::dds_::SportModeState_& msg, const SampleSink& sink)
{
  flattenMessage(msg, MessageFlattenOptions{}, sink);
}

template <typename Msg>
void flattenMessage(const Msg& msg, const MessageFlattenOptions& options, const SampleSink& sink)
{
  flattenGeneric(msg, "", options, sink);
}

template <typename Msg> void flattenMessage(const Msg& msg, const SampleSink& sink)
{
  flattenMessage(msg, MessageFlattenOptions{}, sink);
}

#define PJ_INSTANTIATE_FLATTEN(MSG_TYPE, TYPE_NAME, LABEL)                                         \
  template void flattenMessage<MSG_TYPE>(const MSG_TYPE&, const MessageFlattenOptions&,            \
                                         const SampleSink&);                                       \
  template void flattenMessage<MSG_TYPE>(const MSG_TYPE&, const SampleSink&);

PJ_UNITREE_SDK2_MESSAGE_TYPES(PJ_INSTANTIATE_FLATTEN)

#undef PJ_INSTANTIATE_FLATTEN

} // namespace plotjuggler_unitree_sdk2
