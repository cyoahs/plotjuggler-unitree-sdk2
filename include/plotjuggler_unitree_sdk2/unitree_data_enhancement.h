#pragma once

#include "plotjuggler_unitree_sdk2/unitree_message_flatten.h"

#include <map>
#include <string>

namespace plotjuggler_unitree_sdk2
{

// Call under the streamer's callback mutex. Sinks receive complete series paths.
// Pair messages of the same SDK family and topic namespace. LowCmd updates only
// replace the held command; emit exclusively on LowState at its receive timestamp.
class UnitreeDataEnhancement
{
public:
  void clear();

  void update(const std::string& topic, const unitree_go::msg::dds_::LowCmd_& message,
              const SampleSink& sink);
  void update(const std::string& topic, const unitree_go::msg::dds_::LowState_& message,
              const SampleSink& sink);
  void update(const std::string& topic, const unitree_hg::msg::dds_::LowCmd_& message,
              const SampleSink& sink);
  void update(const std::string& topic, const unitree_hg::msg::dds_::LowState_& message,
              const SampleSink& sink);

  template <typename Msg>
  void update(const std::string&, const Msg&, const SampleSink&)
  {
  }

private:
  template <typename LowCmd, typename LowState> struct TopicGroup
  {
    std::map<std::string, LowCmd> commands;
    std::map<std::string, LowState> states;
  };

  template <typename LowCmd, typename LowState>
  static void updateCommand(TopicGroup<LowCmd, LowState>& group, const std::string& topic,
                            const LowCmd& message);
  template <typename LowCmd, typename LowState>
  static void updateState(TopicGroup<LowCmd, LowState>& group, const std::string& topic,
                          const LowState& message, const SampleSink& sink);

  std::map<std::string, TopicGroup<unitree_go::msg::dds_::LowCmd_,
                                  unitree_go::msg::dds_::LowState_>> go_groups_;
  std::map<std::string, TopicGroup<unitree_hg::msg::dds_::LowCmd_,
                                  unitree_hg::msg::dds_::LowState_>> hg_groups_;
};

} // namespace plotjuggler_unitree_sdk2
