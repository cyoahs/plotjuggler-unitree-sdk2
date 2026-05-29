#pragma once

#include <functional>
#include <string>

#include <unitree/idl/go2/AudioData_.hpp>
#include <unitree/idl/go2/BmsCmd_.hpp>
#include <unitree/idl/go2/BmsState_.hpp>
#include <unitree/idl/go2/ConfigChangeStatus_.hpp>
#include <unitree/idl/go2/Error_.hpp>
#include <unitree/idl/go2/Go2FrontVideoData_.hpp>
#include <unitree/idl/go2/HeightMap_.hpp>
#include <unitree/idl/go2/IMUState_.hpp>
#include <unitree/idl/go2/InterfaceConfig_.hpp>
#include <unitree/idl/go2/LidarState_.hpp>
#include <unitree/idl/go2/LowCmd_.hpp>
#include <unitree/idl/go2/LowState_.hpp>
#include <unitree/idl/go2/MotorCmd_.hpp>
#include <unitree/idl/go2/MotorCmds_.hpp>
#include <unitree/idl/go2/MotorState_.hpp>
#include <unitree/idl/go2/MotorStates_.hpp>
#include <unitree/idl/go2/PathPoint_.hpp>
#include <unitree/idl/go2/Req_.hpp>
#include <unitree/idl/go2/Res_.hpp>
#include <unitree/idl/go2/SportModeCmd_.hpp>
#include <unitree/idl/go2/SportModeState_.hpp>
#include <unitree/idl/go2/TimeSpec_.hpp>
#include <unitree/idl/go2/UwbState_.hpp>
#include <unitree/idl/go2/UwbSwitch_.hpp>
#include <unitree/idl/go2/VoxelMapCompressed_.hpp>
#include <unitree/idl/go2/WirelessController_.hpp>
#include <unitree/idl/hg/AgvBmsState_.hpp>
#include <unitree/idl/hg/BmsCmd_.hpp>
#include <unitree/idl/hg/BmsState_.hpp>
#include <unitree/idl/hg/HandCmd_.hpp>
#include <unitree/idl/hg/HandState_.hpp>
#include <unitree/idl/hg/IMUState_.hpp>
#include <unitree/idl/hg/LowCmd_.hpp>
#include <unitree/idl/hg/LowState_.hpp>
#include <unitree/idl/hg/MainBoardState_.hpp>
#include <unitree/idl/hg/MotorCmd_.hpp>
#include <unitree/idl/hg/MotorState_.hpp>
#include <unitree/idl/hg/PressSensorState_.hpp>
#include <unitree/idl/hg/SportModeState_.hpp>
#include <unitree/idl/hg_doubleimu/doubleIMUState_.hpp>
#include <unitree/idl/ros2/Header_.hpp>
#include <unitree/idl/ros2/Imu_.hpp>
#include <unitree/idl/ros2/MapMetaData_.hpp>
#include <unitree/idl/ros2/OccupancyGrid_.hpp>
#include <unitree/idl/ros2/Odometry_.hpp>
#include <unitree/idl/ros2/Point32_.hpp>
#include <unitree/idl/ros2/PointCloud2_.hpp>
#include <unitree/idl/ros2/PointField_.hpp>
#include <unitree/idl/ros2/PointStamped_.hpp>
#include <unitree/idl/ros2/Point_.hpp>
#include <unitree/idl/ros2/Pose2D_.hpp>
#include <unitree/idl/ros2/PoseStamped_.hpp>
#include <unitree/idl/ros2/PoseWithCovarianceStamped_.hpp>
#include <unitree/idl/ros2/PoseWithCovariance_.hpp>
#include <unitree/idl/ros2/Pose_.hpp>
#include <unitree/idl/ros2/QuaternionStamped_.hpp>
#include <unitree/idl/ros2/Quaternion_.hpp>
#include <unitree/idl/ros2/String_.hpp>
#include <unitree/idl/ros2/Time_.hpp>
#include <unitree/idl/ros2/TwistStamped_.hpp>
#include <unitree/idl/ros2/TwistWithCovarianceStamped_.hpp>
#include <unitree/idl/ros2/TwistWithCovariance_.hpp>
#include <unitree/idl/ros2/Twist_.hpp>
#include <unitree/idl/ros2/Vector3_.hpp>

#define PJ_UNITREE_SDK2_MESSAGE_TYPES(X)                                                           \
  X(unitree_go::msg::dds_::AudioData_, "unitree_go::msg::dds_::AudioData_", "Go2 AudioData")       \
  X(unitree_go::msg::dds_::BmsCmd_, "unitree_go::msg::dds_::BmsCmd_", "Go2 BmsCmd")                \
  X(unitree_go::msg::dds_::BmsState_, "unitree_go::msg::dds_::BmsState_", "Go2 BmsState")          \
  X(unitree_go::msg::dds_::ConfigChangeStatus_, "unitree_go::msg::dds_::ConfigChangeStatus_",      \
    "Go2 ConfigChangeStatus")                                                                      \
  X(unitree_go::msg::dds_::Error_, "unitree_go::msg::dds_::Error_", "Go2 Error")                   \
  X(unitree_go::msg::dds_::Go2FrontVideoData_, "unitree_go::msg::dds_::Go2FrontVideoData_",        \
    "Go2 FrontVideoData")                                                                          \
  X(unitree_go::msg::dds_::HeightMap_, "unitree_go::msg::dds_::HeightMap_", "Go2 HeightMap")       \
  X(unitree_go::msg::dds_::IMUState_, "unitree_go::msg::dds_::IMUState_", "Go2 IMUState")          \
  X(unitree_go::msg::dds_::InterfaceConfig_, "unitree_go::msg::dds_::InterfaceConfig_",            \
    "Go2 InterfaceConfig")                                                                         \
  X(unitree_go::msg::dds_::LidarState_, "unitree_go::msg::dds_::LidarState_", "Go2 LidarState")    \
  X(unitree_go::msg::dds_::LowCmd_, "unitree_go::msg::dds_::LowCmd_", "Go2 LowCmd")                \
  X(unitree_go::msg::dds_::LowState_, "unitree_go::msg::dds_::LowState_", "Go2 LowState")          \
  X(unitree_go::msg::dds_::MotorCmd_, "unitree_go::msg::dds_::MotorCmd_", "Go2 MotorCmd")          \
  X(unitree_go::msg::dds_::MotorCmds_, "unitree_go::msg::dds_::MotorCmds_", "Go2 MotorCmds")       \
  X(unitree_go::msg::dds_::MotorState_, "unitree_go::msg::dds_::MotorState_", "Go2 MotorState")    \
  X(unitree_go::msg::dds_::MotorStates_, "unitree_go::msg::dds_::MotorStates_", "Go2 MotorStates") \
  X(unitree_go::msg::dds_::PathPoint_, "unitree_go::msg::dds_::PathPoint_", "Go2 PathPoint")       \
  X(unitree_go::msg::dds_::Req_, "unitree_go::msg::dds_::Req_", "Go2 Req")                         \
  X(unitree_go::msg::dds_::Res_, "unitree_go::msg::dds_::Res_", "Go2 Res")                         \
  X(unitree_go::msg::dds_::SportModeCmd_, "unitree_go::msg::dds_::SportModeCmd_",                  \
    "Go2 SportModeCmd")                                                                            \
  X(unitree_go::msg::dds_::SportModeState_, "unitree_go::msg::dds_::SportModeState_",              \
    "Go2 SportModeState")                                                                          \
  X(unitree_go::msg::dds_::TimeSpec_, "unitree_go::msg::dds_::TimeSpec_", "Go2 TimeSpec")          \
  X(unitree_go::msg::dds_::UwbState_, "unitree_go::msg::dds_::UwbState_", "Go2 UwbState")          \
  X(unitree_go::msg::dds_::UwbSwitch_, "unitree_go::msg::dds_::UwbSwitch_", "Go2 UwbSwitch")       \
  X(unitree_go::msg::dds_::VoxelMapCompressed_, "unitree_go::msg::dds_::VoxelMapCompressed_",      \
    "Go2 VoxelMapCompressed")                                                                      \
  X(unitree_go::msg::dds_::WirelessController_, "unitree_go::msg::dds_::WirelessController_",      \
    "Go2 WirelessController")                                                                      \
  X(unitree_hg::msg::dds_::AgvBmsState_, "unitree_hg::msg::dds_::AgvBmsState_", "HG AgvBmsState")  \
  X(unitree_hg::msg::dds_::BmsCmd_, "unitree_hg::msg::dds_::BmsCmd_", "HG BmsCmd")                 \
  X(unitree_hg::msg::dds_::BmsState_, "unitree_hg::msg::dds_::BmsState_", "HG BmsState")           \
  X(unitree_hg::msg::dds_::HandCmd_, "unitree_hg::msg::dds_::HandCmd_", "HG HandCmd")              \
  X(unitree_hg::msg::dds_::HandState_, "unitree_hg::msg::dds_::HandState_", "HG HandState")        \
  X(unitree_hg::msg::dds_::IMUState_, "unitree_hg::msg::dds_::IMUState_", "HG IMUState")           \
  X(unitree_hg::msg::dds_::LowCmd_, "unitree_hg::msg::dds_::LowCmd_", "HG LowCmd")                 \
  X(unitree_hg::msg::dds_::LowState_, "unitree_hg::msg::dds_::LowState_", "HG LowState")           \
  X(unitree_hg::msg::dds_::MainBoardState_, "unitree_hg::msg::dds_::MainBoardState_",              \
    "HG MainBoardState")                                                                           \
  X(unitree_hg::msg::dds_::MotorCmd_, "unitree_hg::msg::dds_::MotorCmd_", "HG MotorCmd")           \
  X(unitree_hg::msg::dds_::MotorState_, "unitree_hg::msg::dds_::MotorState_", "HG MotorState")     \
  X(unitree_hg::msg::dds_::PressSensorState_, "unitree_hg::msg::dds_::PressSensorState_",          \
    "HG PressSensorState")                                                                         \
  X(unitree_hg::msg::dds_::SportModeState_, "unitree_hg::msg::dds_::SportModeState_",              \
    "HG SportModeState")                                                                           \
  X(unitree_hg_doubleimu::msg::dds_::doubleIMUState_,                                              \
    "unitree_hg_doubleimu::msg::dds_::doubleIMUState_", "HG doubleIMUState")                       \
  X(builtin_interfaces::msg::dds_::Time_, "builtin_interfaces::msg::dds_::Time_", "ROS2 Time")     \
  X(std_msgs::msg::dds_::Header_, "std_msgs::msg::dds_::Header_", "ROS2 Header")                   \
  X(std_msgs::msg::dds_::String_, "std_msgs::msg::dds_::String_", "ROS2 String")                   \
  X(geometry_msgs::msg::dds_::Point_, "geometry_msgs::msg::dds_::Point_", "ROS2 Point")            \
  X(geometry_msgs::msg::dds_::Point32_, "geometry_msgs::msg::dds_::Point32_", "ROS2 Point32")      \
  X(geometry_msgs::msg::dds_::PointStamped_, "geometry_msgs::msg::dds_::PointStamped_",            \
    "ROS2 PointStamped")                                                                           \
  X(geometry_msgs::msg::dds_::Pose_, "geometry_msgs::msg::dds_::Pose_", "ROS2 Pose")               \
  X(geometry_msgs::msg::dds_::Pose2D_, "geometry_msgs::msg::dds_::Pose2D_", "ROS2 Pose2D")         \
  X(geometry_msgs::msg::dds_::PoseStamped_, "geometry_msgs::msg::dds_::PoseStamped_",              \
    "ROS2 PoseStamped")                                                                            \
  X(geometry_msgs::msg::dds_::PoseWithCovariance_,                                                 \
    "geometry_msgs::msg::dds_::PoseWithCovariance_", "ROS2 PoseWithCovariance")                    \
  X(geometry_msgs::msg::dds_::PoseWithCovarianceStamped_,                                          \
    "geometry_msgs::msg::dds_::PoseWithCovarianceStamped_", "ROS2 PoseWithCovarianceStamped")      \
  X(geometry_msgs::msg::dds_::Quaternion_, "geometry_msgs::msg::dds_::Quaternion_",                \
    "ROS2 Quaternion")                                                                             \
  X(geometry_msgs::msg::dds_::QuaternionStamped_, "geometry_msgs::msg::dds_::QuaternionStamped_",  \
    "ROS2 QuaternionStamped")                                                                      \
  X(geometry_msgs::msg::dds_::Twist_, "geometry_msgs::msg::dds_::Twist_", "ROS2 Twist")            \
  X(geometry_msgs::msg::dds_::TwistStamped_, "geometry_msgs::msg::dds_::TwistStamped_",            \
    "ROS2 TwistStamped")                                                                           \
  X(geometry_msgs::msg::dds_::TwistWithCovariance_,                                                \
    "geometry_msgs::msg::dds_::TwistWithCovariance_", "ROS2 TwistWithCovariance")                  \
  X(geometry_msgs::msg::dds_::TwistWithCovarianceStamped_,                                         \
    "geometry_msgs::msg::dds_::TwistWithCovarianceStamped_", "ROS2 TwistWithCovarianceStamped")    \
  X(geometry_msgs::msg::dds_::Vector3_, "geometry_msgs::msg::dds_::Vector3_", "ROS2 Vector3")      \
  X(nav_msgs::msg::dds_::MapMetaData_, "nav_msgs::msg::dds_::MapMetaData_", "ROS2 MapMetaData")    \
  X(nav_msgs::msg::dds_::OccupancyGrid_, "nav_msgs::msg::dds_::OccupancyGrid_",                    \
    "ROS2 OccupancyGrid")                                                                          \
  X(nav_msgs::msg::dds_::Odometry_, "nav_msgs::msg::dds_::Odometry_", "ROS2 Odometry")             \
  X(sensor_msgs::msg::dds_::Imu_, "sensor_msgs::msg::dds_::Imu_", "ROS2 Imu")                      \
  X(sensor_msgs::msg::dds_::PointCloud2_, "sensor_msgs::msg::dds_::PointCloud2_",                  \
    "ROS2 PointCloud2")                                                                            \
  X(sensor_msgs::msg::dds_::PointField_, "sensor_msgs::msg::dds_::PointField_", "ROS2 PointField")

namespace plotjuggler_unitree_sdk2
{

using SampleSink = std::function<void(const std::string& field_name, double value)>;

enum class JoystickOutputMode
{
  RawBytes,
  ParsedStructure,
  RawAndParsed,
};

struct MessageFlattenOptions
{
  JoystickOutputMode joystick_output_mode = JoystickOutputMode::ParsedStructure;
};

void flattenMessage(const unitree_go::msg::dds_::LowState_& msg, const SampleSink& sink);
void flattenMessage(const unitree_go::msg::dds_::SportModeState_& msg, const SampleSink& sink);
void flattenMessage(const unitree_go::msg::dds_::WirelessController_& msg, const SampleSink& sink);

void flattenMessage(const unitree_hg::msg::dds_::LowState_& msg, const SampleSink& sink);
void flattenMessage(const unitree_hg::msg::dds_::SportModeState_& msg, const SampleSink& sink);

void flattenMessage(const unitree_go::msg::dds_::LowState_& msg,
                    const MessageFlattenOptions& options, const SampleSink& sink);
void flattenMessage(const unitree_go::msg::dds_::SportModeState_& msg,
                    const MessageFlattenOptions& options, const SampleSink& sink);
void flattenMessage(const unitree_go::msg::dds_::WirelessController_& msg,
                    const MessageFlattenOptions& options, const SampleSink& sink);

void flattenMessage(const unitree_hg::msg::dds_::LowState_& msg,
                    const MessageFlattenOptions& options, const SampleSink& sink);
void flattenMessage(const unitree_hg::msg::dds_::SportModeState_& msg,
                    const MessageFlattenOptions& options, const SampleSink& sink);

template <typename Msg> void flattenMessage(const Msg& msg, const SampleSink& sink);

template <typename Msg>
void flattenMessage(const Msg& msg, const MessageFlattenOptions& options, const SampleSink& sink);

std::string normalizeTopicPrefix(const std::string& topic);

} // namespace plotjuggler_unitree_sdk2
