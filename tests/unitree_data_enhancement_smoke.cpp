#include "plotjuggler_unitree_sdk2/unitree_data_enhancement.h"

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace pju = plotjuggler_unitree_sdk2;

namespace
{

void require(bool condition, const std::string& message)
{
  if (!condition)
  {
    throw std::runtime_error(message);
  }
}

template <typename LowCmd, typename LowState> void checkTorques()
{
  pju::UnitreeDataEnhancement enhancement;
  std::map<std::string, double> samples;
  std::size_t emitted = 0;
  const pju::SampleSink sink = [&](const std::string& field, double value)
  {
    samples[field] = value;
    ++emitted;
  };

  LowCmd command;
  LowState state;
  auto& motor = command.motor_cmd()[0];
  motor.q() = 1.5F;
  motor.dq() = -0.5F;
  motor.kp() = 4.0F;
  motor.kd() = 2.0F;
  motor.tau() = 0.75F;
  state.motor_state()[0].q() = 0.5F;
  state.motor_state()[0].dq() = 0.25F;
  command.motor_cmd().back().tau() = -2.0F;

  enhancement.update("lowcmd", command, sink);
  require(samples.empty(), "A command alone must not produce torques");
  enhancement.update("lowstate", state, sink);
  require(samples.size() == command.motor_cmd().size() * 3, "Expected three fields per motor");
  require(samples.at("lowstate/motor_state/00/tau_des_p*") == 4.0, "Incorrect position term");
  require(samples.at("lowstate/motor_state/00/tau_des_d*") == -1.5, "Incorrect velocity term");
  require(samples.at("lowstate/motor_state/00/tau_des*") == 3.25, "Feed-forward torque must be added");
  require(samples.at("lowstate/motor_state/" + std::to_string(command.motor_cmd().size() - 1) +
                      "/tau_des*") == -2.0, "Last motor or zero gains handled incorrectly");

  samples.clear();
  state.motor_state()[0].q() = 2.0F;
  enhancement.update("lowstate", state, sink);
  require(samples.at("lowstate/motor_state/00/tau_des*") == -2.75,
          "State updates must recompute with the latest command");
  samples.clear();
  motor.tau() = -0.25F;
  enhancement.update("lowcmd", command, sink);
  require(samples.empty(), "Command updates must not emit samples or recompute past states");
  enhancement.update("lowstate", state, sink);
  require(samples.at("lowstate/motor_state/00/tau_des*") == -3.75,
          "The next state must use the updated command");

  enhancement.clear();
  samples.clear();
  enhancement.update("lowstate", state, sink);
  require(samples.empty(), "Clearing must discard cached commands");
  enhancement.update("lowcmd", command, sink);
  require(samples.empty(), "A new command must wait for the next state");
  enhancement.update("lowstate", state, sink);
  require(samples.at("lowstate/motor_state/00/tau_des*") == -3.75,
          "State-before-command arrival order failed");
  enhancement.clear();
  samples.clear();
  enhancement.update("lowcmd", command, sink);
  require(samples.empty(), "Clearing must discard cached states");

  enhancement.clear();
  enhancement.update("robot_a/lowcmd", command, sink);
  enhancement.update("robot_b/lowstate", state, sink);
  require(samples.empty(), "Different topic namespaces must not be combined");
  enhancement.update("robot_a/lowstate", state, sink);
  require(samples.at("robot_a/lowstate/motor_state/00/tau_des*") == -3.75,
          "Namespaced pair failed");

  samples.clear();
  motor.tau() = 5.0F;
  enhancement.update("robot_a/arm_sdk", command, sink);
  require(samples.empty(), "Custom command topics must also wait for the next state");
  emitted = 0;
  enhancement.update("robot_a/lowstate", state, sink);
  require(samples.at("robot_a/lowstate/motor_state/00/tau_des*") == -3.75 &&
              emitted == command.motor_cmd().size() * 3 && samples.size() == emitted,
          "State torque series must use lowcmd without duplicate samples from other commands");

  samples.clear();
  enhancement.update("robot_a/other_state", state, sink);
  enhancement.update("robot_a/lowcmd", command, sink);
  enhancement.update("robot_a/lowstate", state, sink);
  require(samples.empty(), "Ambiguous state topics must not be paired arbitrarily");

  enhancement.clear();
  enhancement.update("robot_a/control", command, sink);
  enhancement.update("robot_a/feedback", state, sink);
  require(samples.at("robot_a/feedback/motor_state/00/tau_des*") == 1.5,
          "A custom topic pair must emit under the actual state topic");
  samples.clear();
  enhancement.update("robot_a/other_control", command, sink);
  enhancement.update("robot_a/feedback", state, sink);
  require(samples.empty(), "Multiple custom command topics without lowcmd are ambiguous");
}

template <typename LowCmd, typename LowState> void checkZeroOrderHold()
{
  pju::UnitreeDataEnhancement enhancement;
  std::map<std::string, double> samples;
  std::size_t emitted = 0;
  const pju::SampleSink sink = [&](const std::string& field, double value)
  {
    samples[field] = value;
    ++emitted;
  };
  LowCmd command;
  LowState state;
  auto& motor = command.motor_cmd()[0];
  const auto fields_per_state = command.motor_cmd().size() * 3;

  // Several faster command updates between feedback samples: only the last one
  // takes effect, and all command fields are held together.
  enhancement.update("lowstate", state, sink);
  motor.tau() = 100.0F;
  enhancement.update("lowcmd", command, sink);
  motor.q() = 2.0F;
  motor.dq() = 3.0F;
  motor.kp() = 4.0F;
  motor.kd() = 5.0F;
  motor.tau() = 6.0F;
  enhancement.update("lowcmd", command, sink);
  require(emitted == 0, "Faster commands must not generate torque samples");
  state.motor_state()[0].q() = 1.0F;
  state.motor_state()[0].dq() = 1.0F;
  enhancement.update("lowstate", state, sink);
  require(emitted == fields_per_state && samples.at("lowstate/motor_state/00/tau_des*") == 20.0,
          "Feedback must sample the most recent command exactly once");

  // Faster feedback reuses the held command until another command is received.
  motor.tau() = -100.0F; // Local edits must not alter the cached message.
  state.motor_state()[0].q() = 1.5F;
  enhancement.update("lowstate", state, sink);
  require(emitted == 2 * fields_per_state && samples.at("lowstate/motor_state/00/tau_des*") == 18.0,
          "Faster feedback must reuse the held command");
  state.motor_state()[0].dq() = 2.0F;
  enhancement.update("lowstate", state, sink);
  require(emitted == 3 * fields_per_state && samples.at("lowstate/motor_state/00/tau_des*") == 13.0,
          "Held command must remain valid across multiple feedback samples");
}

void checkTypeIsolation()
{
  pju::UnitreeDataEnhancement enhancement;
  std::map<std::string, double> samples;
  const pju::SampleSink sink = [&](const std::string& field, double value)
  { samples[field] = value; };

  unitree_go::msg::dds_::LowCmd_ go_command;
  unitree_hg::msg::dds_::LowState_ hg_state;
  enhancement.update("lowcmd", go_command, sink);
  enhancement.update("lowstate", hg_state, sink);
  enhancement.update("sportmodestate", unitree_go::msg::dds_::SportModeState_{}, sink);
  require(samples.empty(), "Different SDK families or unrelated messages must not be combined");

  unitree_hg::msg::dds_::LowCmd_ hg_command;
  hg_command.mode_pr() = 1;
  enhancement.update("lowcmd", hg_command, sink);
  enhancement.update("lowstate", hg_state, sink);
  require(samples.empty(), "HG commands and states with different coordinates must not be combined");
  hg_state.mode_pr() = 1;
  enhancement.update("lowstate", hg_state, sink);
  require(!samples.empty(), "Matching HG coordinates should produce torques");
}

template <typename LowCmd, typename LowState> void checkEnhancementOptions()
{
  struct Sample
  {
    double stamp;
    double value;
  };
  for (bool pd_enabled : {false, true})
  {
    for (bool flatten_enabled : {false, true})
    {
      pju::UnitreeDataEnhancement enhancement;
      std::map<std::string, std::vector<Sample>> samples;
      const auto feed = [&](const std::string& topic, const auto& message, double stamp)
      {
        const auto sink = pju::withMotorFieldAliases(
            topic, flatten_enabled, [&](const std::string& series, double value)
            { samples[series].push_back({stamp, value}); });
        pju::flattenMessage(message, [&](const std::string& field, double value)
                            { sink(topic + "/" + field, value); });
        if (pd_enabled)
        {
          enhancement.update(topic, message, sink);
        }
      };
      LowCmd command;
      LowState state;
      command.motor_cmd()[0].q() = 1.5F;
      command.motor_cmd()[0].kp() = 4.0F;
      command.motor_cmd()[0].tau() = 0.75F;
      command.motor_cmd().back().kd() = 2.0F;
      state.motor_state()[0].q() = 0.5F;
      feed("robot/lowcmd", command, 1.0);
      feed("robot/lowstate", state, 2.0);

      require(samples.at("robot/lowcmd/motor_cmd/00/kp").back().value == 4.0 &&
                  samples.at("robot/lowstate/motor_state/00/q").back().value == 0.5,
              "Every option combination must retain the original motor fields");
      require(samples.count("motorstate*/q/00") == flatten_enabled &&
                  samples.count("motorcmd*/kp/00") == flatten_enabled,
              "Motor state and command aliases must depend only on Flatten");
      require(samples.count("robot/lowstate/motor_state/00/tau_des*") == pd_enabled &&
                  samples.count("motorstate*/tau_des*/00") == (pd_enabled && flatten_enabled),
              "PD torques and their aliases must respect the independent options");
      if (flatten_enabled)
      {
        const auto& q = samples.at("motorstate*/q/00").back();
        const auto& kp = samples.at("motorcmd*/kp/00").back();
        require(q.value == 0.5 && q.stamp == 2.0 && kp.value == 4.0 && kp.stamp == 1.0,
                "Aliases must preserve values and the source message timestamp");
        require(samples.at("motorcmd*/kd/" +
                           std::to_string(command.motor_cmd().size() - 1)).back().value == 2.0,
                "Flatten must include the last motor");
      }
      if (pd_enabled && flatten_enabled)
      {
        const auto& tau = samples.at("motorstate*/tau_des*/00");
        require(tau.size() == 1 && tau.back().value == 4.75 && tau.back().stamp == 2.0,
                "Flattened PD torque must preserve its derived marker and state timestamp");
      }
      state.motor_state()[0].q() = 1.0F;
      feed("robot/lowstate", state, 3.0);
      if (flatten_enabled)
      {
        const auto& q = samples.at("motorstate*/q/00");
        require(q.size() == 2 && q.back().value == 1.0 && q.back().stamp == 3.0,
                "Each source sample must append exactly one field alias");
      }
    }
  }
}

void checkNestedMotorAliases()
{
  std::map<std::string, double> samples;
  const auto sink = pju::withMotorFieldAliases(
      "robot/lowstate", true,
      [&](const std::string& series, double value) { samples[series] = value; });
  sink("robot/lowstate/motor_state/07/temperature/1", 42.0);
  require(samples.size() == 2 &&
              samples.at("motorstate*/temperature/07/1") == 42.0 &&
              samples.at("robot/lowstate/motor_state/07/temperature/1") == 42.0,
          "Nested fields must retain their subindices after the motor index");
  samples.clear();
  sink("robot/lowstate/imu_state/rpy/0", 1.0);
  sink("other/lowstate/motor_state/00/q", 2.0);
  sink("robot/lowstate/motor_state/length", 3.0);
  sink("robot/lowstate/motor_state/truncated/q", 4.0);
  require(samples.size() == 4, "Unrelated fields and array metadata must not acquire aliases");
}

} // namespace

int main()
{
  try
  {
    checkTorques<unitree_go::msg::dds_::LowCmd_, unitree_go::msg::dds_::LowState_>();
    checkTorques<unitree_hg::msg::dds_::LowCmd_, unitree_hg::msg::dds_::LowState_>();
    checkZeroOrderHold<unitree_go::msg::dds_::LowCmd_, unitree_go::msg::dds_::LowState_>();
    checkZeroOrderHold<unitree_hg::msg::dds_::LowCmd_, unitree_hg::msg::dds_::LowState_>();
    checkTypeIsolation();
    checkEnhancementOptions<unitree_go::msg::dds_::LowCmd_, unitree_go::msg::dds_::LowState_>();
    checkEnhancementOptions<unitree_hg::msg::dds_::LowCmd_, unitree_hg::msg::dds_::LowState_>();
    checkNestedMotorAliases();
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
