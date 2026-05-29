#pragma once

#include "plotjuggler_unitree_sdk2/unitree_message_flatten.h"

#include <PlotJuggler/datastreamer_base.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <QObject>

class QEvent;

namespace plotjuggler_unitree_sdk2
{

class DdsSubscriber
{
public:
  virtual ~DdsSubscriber() = default;
  virtual const std::string& key() const = 0;
  virtual void setActive(bool active) = 0;
  virtual bool isActive() const = 0;
};

struct TopicSelection
{
  std::string key;
  std::string topic;
  std::string type_name;
  bool enabled = false;
};

struct StreamConfig
{
  std::string network_interface;
  int domain_id = 0;
  int queue_length = 1;
  bool clear_existing_data = true;
  JoystickOutputMode joystick_output_mode = JoystickOutputMode::ParsedStructure;
  std::vector<TopicSelection> topics;
};

class UnitreeDataStreamer : public PJ::DataStreamer
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "facontidavide.PlotJuggler3.DataStreamer")
  Q_INTERFACES(PJ::DataStreamer)

public:
  UnitreeDataStreamer();
  ~UnitreeDataStreamer() override;

  const char* name() const override;
  const std::vector<QAction*>& availableActions() override;
  bool xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const override;
  bool xmlLoadState(const QDomElement& parent_element) override;
  bool start(QStringList* selected_datasources) override;
  void shutdown() override;
  bool isRunning() const override;

private:
  bool eventFilter(QObject* watched, QEvent* event) override;

  template <typename Msg> void addSubscriber(const TopicSelection& selection);

  bool configureTopics(StreamConfig* config);
  void showSettingsDialog();
  void installStreamingOptionsFilter();
  void loadDefaultSettings();
  void saveDefaultSettings() const;
  double elapsedSeconds() const;
  void appendSampleUnlocked(const std::string& series_name, double stamp, double value);
  void clearState();
  void leakSubscribersForProcessExit();

  std::atomic_bool running_{false};
  std::atomic_uint64_t total_messages_{0};
  std::chrono::steady_clock::time_point start_time_;
  std::mutex callback_mutex_;
  StreamConfig config_;
  std::vector<std::unique_ptr<DdsSubscriber>> subscribers_;
  QAction* settings_action_ = nullptr;
  std::vector<QAction*> actions_;
};

} // namespace plotjuggler_unitree_sdk2
