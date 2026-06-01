#include "plotjuggler_unitree_sdk2/unitree_message_flatten.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace pju = plotjuggler_unitree_sdk2;

namespace
{

struct Sample
{
  std::string field;
  double value;
};

bool hasField(const std::vector<Sample>& samples, const std::string& field)
{
  return std::any_of(samples.begin(), samples.end(),
                     [&](const Sample& sample) { return sample.field == field; });
}

bool hasValue(const std::vector<Sample>& samples, const std::string& field, double value)
{
  return std::any_of(samples.begin(), samples.end(), [&](const Sample& sample) {
    return sample.field == field && sample.value == value;
  });
}

}  // namespace

int main()
{
  unitree_go::msg::dds_::LowState_ go_lowstate;
  go_lowstate.tick() = 42;
  go_lowstate.imu_state().rpy()[2] = 1.25F;
  go_lowstate.motor_state()[0].q() = 0.5F;
  go_lowstate.bms_state().soc() = 87;
  go_lowstate.wireless_remote()[2] = 0x00;
  go_lowstate.wireless_remote()[3] = 0x01;
  const float joystick_lx = 0.75F;
  std::memcpy(&go_lowstate.wireless_remote()[4], &joystick_lx, sizeof(joystick_lx));

  std::vector<Sample> go_samples;
  pju::flattenMessage(go_lowstate, [&](const std::string& field, double value) {
    go_samples.push_back({field, value});
  });

  if (!hasField(go_samples, "tick") ||
      !hasField(go_samples, "imu_state/rpy/2") ||
      !hasField(go_samples, "motor_state/00/q") ||
      !hasField(go_samples, "bms_state/soc") ||
      !hasValue(go_samples, "wireless_remote/buttons/A", 1.0) ||
      !hasValue(go_samples, "wireless_remote/axes/lx", joystick_lx))
  {
    std::cerr << "Go2 LowState flattening missed expected fields\n";
    return 1;
  }

  unitree_go::msg::dds_::WirelessController_ joystick_msg;
  joystick_msg.lx() = 0.25F;
  joystick_msg.keys() = 1u << 9;

  std::vector<Sample> joystick_samples;
  pju::flattenMessage(joystick_msg, [&](const std::string& field, double value) {
    joystick_samples.push_back({field, value});
  });

  if (!hasValue(joystick_samples, "joystick/buttons/B", 1.0) ||
      !hasValue(joystick_samples, "joystick/axes/lx", 0.25))
  {
    std::cerr << "WirelessController parsing missed expected fields\n";
    return 1;
  }

  std::vector<Sample> raw_joystick_samples;
  pju::flattenMessage(joystick_msg,
                      pju::MessageFlattenOptions{pju::JoystickOutputMode::RawBytes},
                      [&](const std::string& field, double value) {
                        raw_joystick_samples.push_back({field, value});
                      });

  if (!hasField(raw_joystick_samples, "keys") ||
      hasField(raw_joystick_samples, "joystick/buttons/B"))
  {
    std::cerr << "WirelessController raw mode did not preserve raw-only fields\n";
    return 1;
  }

  unitree_hg::msg::dds_::SportModeState_ hg_sport;
  hg_sport.fsm_id() = 3;
  hg_sport.task_time() = 9.5F;

  std::vector<Sample> hg_samples;
  pju::flattenMessage(hg_sport, [&](const std::string& field, double value) {
    hg_samples.push_back({field, value});
  });

  if (!hasField(hg_samples, "fsm_id") || !hasField(hg_samples, "task_time"))
  {
    std::cerr << "HG SportModeState flattening missed expected fields\n";
    return 1;
  }

  std_msgs::msg::dds_::String_ json_string;
  json_string.data(R"({"state":{"x":1.5,"enabled":true},"samples":[2,-3.25],"label":"skip"})");

  std::vector<Sample> json_samples;
  pju::flattenMessage(json_string, [&](const std::string& field, double value) {
    json_samples.push_back({field, value});
  });

  if (!hasField(json_samples, "data/length") ||
      !hasValue(json_samples, "data/json/state/x", 1.5) ||
      !hasValue(json_samples, "data/json/state/enabled", 1.0) ||
      !hasValue(json_samples, "data/json/samples/00", 2.0) ||
      !hasValue(json_samples, "data/json/samples/01", -3.25) ||
      hasField(json_samples, "data/json/label"))
  {
    std::cerr << "JSON string flattening missed expected numeric fields\n";
    return 1;
  }

  std_msgs::msg::dds_::String_ plain_string;
  plain_string.data("not json");

  std::vector<Sample> plain_samples;
  pju::flattenMessage(plain_string, [&](const std::string& field, double value) {
    plain_samples.push_back({field, value});
  });

  if (!hasField(plain_samples, "data/length") || hasField(plain_samples, "data/json"))
  {
    std::cerr << "Plain string should not produce JSON fields\n";
    return 1;
  }

  if (pju::normalizeTopicPrefix("/rt//lowstate/") != "rt/lowstate")
  {
    std::cerr << "Topic normalization failed\n";
    return 1;
  }

  return 0;
}
