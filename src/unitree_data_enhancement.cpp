#include "plotjuggler_unitree_sdk2/unitree_data_enhancement.h"

#include <algorithm>
#include <type_traits>

namespace plotjuggler_unitree_sdk2
{
namespace
{

std::string topicNamespace(const std::string& topic)
{
  const auto separator = topic.rfind('/');
  return separator == std::string::npos ? std::string{} : topic.substr(0, separator);
}

template <typename LowCmd, typename LowState>
void emitTorques(const std::string& topic, const LowCmd& command, const LowState& state,
                 const SampleSink& sink)
{
  if constexpr (std::is_same_v<LowCmd, unitree_hg::msg::dds_::LowCmd_>)
  {
    // PR and AB coordinates cannot be subtracted from each other.
    if (command.mode_pr() != state.mode_pr())
    {
      return;
    }
  }

  const auto count = std::min(command.motor_cmd().size(), state.motor_state().size());
  for (std::size_t index = 0; index < count; ++index)
  {
    const auto& cmd = command.motor_cmd()[index];
    const auto& feedback = state.motor_state()[index];
    const double tau_p = cmd.kp() * (static_cast<double>(cmd.q()) - feedback.q());
    const double tau_d = cmd.kd() * (static_cast<double>(cmd.dq()) - feedback.dq());
    const std::string prefix = topic + "/motor_state/" + (index < 10 ? "0" : "") +
                               std::to_string(index);
    sink(prefix + "/tau_des*", static_cast<double>(cmd.tau()) + tau_p + tau_d);
    sink(prefix + "/tau_des_p*", tau_p);
    sink(prefix + "/tau_des_d*", tau_d);
  }
}

} // namespace

void UnitreeDataEnhancement::clear()
{
  go_groups_.clear();
  hg_groups_.clear();
}

template <typename LowCmd, typename LowState>
void UnitreeDataEnhancement::updateCommand(TopicGroup<LowCmd, LowState>& group,
                                          const std::string& topic, const LowCmd& message)
{
  // Zero-order hold: a command affects only subsequent LowState samples.
  group.commands[topic] = message;
}

template <typename LowCmd, typename LowState>
void UnitreeDataEnhancement::updateState(TopicGroup<LowCmd, LowState>& group,
                                        const std::string& topic, const LowState& message,
                                        const SampleSink& sink)
{
  group.states[topic] = message;
  // Multiple state topics in one namespace are ambiguous; never pick one at random.
  if (group.states.size() == 1)
  {
    // One torque sample per state/motor. Prefer the sibling lowcmd topic when
    // other command sources (for example arm_sdk) share this namespace.
    const std::string topic_namespace = topicNamespace(topic);
    const std::string command_topic =
        topic_namespace.empty() ? "lowcmd" : topic_namespace + "/lowcmd";
    auto command = group.commands.find(command_topic);
    if (command == group.commands.end() && group.commands.size() == 1)
    {
      command = group.commands.begin();
    }
    if (command != group.commands.end())
    {
      emitTorques(topic, command->second, message, sink);
    }
  }
}

void UnitreeDataEnhancement::update(const std::string& topic,
                                    const unitree_go::msg::dds_::LowCmd_& message,
                                    const SampleSink&)
{
  updateCommand(go_groups_[topicNamespace(topic)], topic, message);
}

void UnitreeDataEnhancement::update(const std::string& topic,
                                    const unitree_go::msg::dds_::LowState_& message,
                                    const SampleSink& sink)
{
  updateState(go_groups_[topicNamespace(topic)], topic, message, sink);
}

void UnitreeDataEnhancement::update(const std::string& topic,
                                    const unitree_hg::msg::dds_::LowCmd_& message,
                                    const SampleSink&)
{
  updateCommand(hg_groups_[topicNamespace(topic)], topic, message);
}

void UnitreeDataEnhancement::update(const std::string& topic,
                                    const unitree_hg::msg::dds_::LowState_& message,
                                    const SampleSink& sink)
{
  updateState(hg_groups_[topicNamespace(topic)], topic, message, sink);
}

} // namespace plotjuggler_unitree_sdk2
