#include "unitree_datastreamer.h"

#include "plotjuggler_unitree_sdk2/unitree_message_flatten.h"

#include <unitree/idl/go2/LowState_.hpp>
#include <unitree/idl/go2/SportModeState_.hpp>
#include <unitree/idl/go2/WirelessController_.hpp>
#include <unitree/idl/hg/LowState_.hpp>
#include <unitree/idl/hg/SportModeState_.hpp>
#include <unitree/robot/channel/channel_factory.hpp>
#include <unitree/robot/channel/channel_subscriber.hpp>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <dds/dds.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <functional>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>

namespace plotjuggler_unitree_sdk2
{
namespace
{

struct TopicDefinition
{
  const char* key;
  const char* label;
  const char* topic;
  const char* type_name;
  bool default_enabled;
};

struct DiscoveredTopic
{
  QString topic;
  QString type_name;
  QString label;
  std::string key;
  bool supported = false;
  bool default_enabled = false;
};

constexpr const char* kSettingsGroup = "UnitreeSDK2DDS";
constexpr int kDiscoveryWaitMs = 800;

const std::vector<TopicDefinition>& topicDefinitions()
{
  static const std::vector<TopicDefinition> definitions = {
#define PJ_TOPIC_DEFINITION(MSG_TYPE, TYPE_NAME, LABEL) {TYPE_NAME, LABEL, "*", TYPE_NAME, true},
      PJ_UNITREE_SDK2_MESSAGE_TYPES(PJ_TOPIC_DEFINITION)
#undef PJ_TOPIC_DEFINITION
  };
  return definitions;
}

bool boolAttribute(const QDomElement& element, const QString& name, bool fallback)
{
  if (!element.hasAttribute(name))
  {
    return fallback;
  }
  const QString value = element.attribute(name).trimmed().toLower();
  return value == "1" || value == "true" || value == "yes";
}

std::unordered_map<std::string, bool> enabledTopicsByKey(const std::vector<TopicSelection>& topics)
{
  std::unordered_map<std::string, bool> enabled_by_key;
  for (const TopicSelection& selection : topics)
  {
    enabled_by_key[selection.key] = selection.enabled;
  }
  return enabled_by_key;
}

std::vector<TopicSelection>
topicSelections(const std::unordered_map<std::string, bool>& enabled_by_key = {})
{
  (void)enabled_by_key;
  std::vector<TopicSelection> selections;
  return selections;
}

std::string stripUnitreeTopicPrefix(const std::string& topic);

std::string canonicalTypeName(const std::string& type_name)
{
  std::string canonical;
  canonical.reserve(type_name.size());
  for (const unsigned char ch : type_name)
  {
    if (std::isalnum(ch) || ch == '_')
    {
      canonical.push_back(static_cast<char>(std::tolower(ch)));
    }
  }
  return canonical;
}

bool topicNamesMatch(const std::string& discovered_topic, const std::string& expected_topic)
{
  return stripUnitreeTopicPrefix(discovered_topic) == stripUnitreeTopicPrefix(expected_topic);
}

bool typeNamesMatch(const std::string& discovered_type, const std::string& expected_type)
{
  const std::string discovered = canonicalTypeName(discovered_type);
  const std::string expected = canonicalTypeName(expected_type);
  return !discovered.empty() && discovered == expected;
}

std::string topicSelectionKey(const std::string& topic, const std::string& type_name)
{
  return canonicalTypeName(type_name) + "@" + stripUnitreeTopicPrefix(topic);
}

const TopicDefinition* matchingTopicDefinition(const std::string& topic,
                                               const std::string& type_name)
{
  for (const TopicDefinition& definition : topicDefinitions())
  {
    const bool topic_matches =
        std::string(definition.topic) == "*" || topicNamesMatch(topic, definition.topic);
    if (topic_matches && typeNamesMatch(type_name, definition.type_name))
    {
      return &definition;
    }
  }
  return nullptr;
}

std::string xmlEscaped(const std::string& text)
{
  std::string escaped;
  escaped.reserve(text.size());
  for (const char ch : text)
  {
    switch (ch)
    {
    case '&':
      escaped += "&amp;";
      break;
    case '<':
      escaped += "&lt;";
      break;
    case '>':
      escaped += "&gt;";
      break;
    case '"':
      escaped += "&quot;";
      break;
    case '\'':
      escaped += "&apos;";
      break;
    default:
      escaped.push_back(ch);
      break;
    }
  }
  return escaped;
}

std::string cycloneddsConfigXml(const StreamConfig& config)
{
  if (config.network_interface.empty())
  {
    return {};
  }

  return "<CycloneDDS><Domain id=\"" + std::to_string(config.domain_id) +
         "\"><General><NetworkInterfaceAddress>" + xmlEscaped(config.network_interface) +
         "</NetworkInterfaceAddress></General></Domain></CycloneDDS>";
}

std::vector<std::pair<std::string, std::string>>
discoverPublishedDdsEndpoints(const StreamConfig& config)
{
  std::vector<std::pair<std::string, std::string>> endpoints;

  const std::string dds_config = cycloneddsConfigXml(config);
  dds_entity_t domain = 0;
  if (!dds_config.empty())
  {
    domain = dds_create_domain(static_cast<dds_domainid_t>(config.domain_id), dds_config.c_str());
  }

  const dds_entity_t participant =
      dds_create_participant(static_cast<dds_domainid_t>(config.domain_id), nullptr, nullptr);
  if (participant < 0)
  {
    if (domain > 0)
    {
      dds_delete(domain);
    }
    return endpoints;
  }

  const dds_entity_t reader =
      dds_create_reader(participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, nullptr, nullptr);
  if (reader < 0)
  {
    dds_delete(participant);
    if (domain > 0)
    {
      dds_delete(domain);
    }
    return endpoints;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(kDiscoveryWaitMs));

  constexpr uint32_t kBatchSize = 64;
  for (;;)
  {
    void* samples[kBatchSize] = {};
    dds_sample_info_t infos[kBatchSize] = {};
    const dds_return_t count = dds_take(reader, samples, infos, kBatchSize, kBatchSize);
    if (count <= 0)
    {
      break;
    }

    for (dds_return_t index = 0; index < count; ++index)
    {
      if (!infos[index].valid_data || samples[index] == nullptr)
      {
        continue;
      }

      const auto* endpoint = static_cast<const dds_builtintopic_endpoint_t*>(samples[index]);
      if (endpoint->topic_name == nullptr || endpoint->type_name == nullptr)
      {
        continue;
      }

      std::string topic = normalizeTopicPrefix(endpoint->topic_name);
      const std::string type_name = endpoint->type_name;
      if (topic.empty() || topic.rfind("DCPS", 0) == 0)
      {
        continue;
      }
      endpoints.emplace_back(std::move(topic), type_name);
    }

    dds_return_loan(reader, samples, static_cast<int32_t>(count));
    if (count < static_cast<dds_return_t>(kBatchSize))
    {
      break;
    }
  }

  dds_delete(participant);
  if (domain > 0)
  {
    dds_delete(domain);
  }
  return endpoints;
}

std::vector<DiscoveredTopic> discoverTopics(const StreamConfig& config)
{
  std::map<std::string, DiscoveredTopic> unique_topics;
  for (const auto& [topic, type_name] : discoverPublishedDdsEndpoints(config))
  {
    const TopicDefinition* definition = matchingTopicDefinition(topic, type_name);

    DiscoveredTopic discovered;
    discovered.topic = QString::fromStdString(topic);
    discovered.type_name = QString::fromStdString(type_name);
    if (definition)
    {
      discovered.key = topicSelectionKey(topic, type_name);
      discovered.label = QString::fromUtf8(definition->label);
      discovered.supported = true;
      discovered.default_enabled = definition->default_enabled;
    }
    else
    {
      discovered.label = "Unsupported";
    }

    const std::string unique_key = topic + "\n" + canonicalTypeName(type_name);
    unique_topics[unique_key] = std::move(discovered);
  }

  std::vector<DiscoveredTopic> topics;
  topics.reserve(unique_topics.size());
  for (auto& [key, topic] : unique_topics)
  {
    (void)key;
    topics.push_back(std::move(topic));
  }
  return topics;
}

std::string stripUnitreeTopicPrefix(const std::string& topic)
{
  const std::string normalized = normalizeTopicPrefix(topic);
  std::vector<std::string> tokens;
  size_t begin = 0;
  while (begin < normalized.size())
  {
    const size_t end = normalized.find('/', begin);
    tokens.push_back(normalized.substr(begin, end == std::string::npos ? end : end - begin));
    if (end == std::string::npos)
    {
      break;
    }
    begin = end + 1;
  }

  size_t first = 0;
  if (first < tokens.size() && tokens[first] == "unitree")
  {
    ++first;
  }
  if (first < tokens.size() && tokens[first] == "rt")
  {
    ++first;
  }

  std::string stripped;
  for (size_t index = first; index < tokens.size(); ++index)
  {
    if (!stripped.empty())
    {
      stripped.push_back('/');
    }
    stripped += tokens[index];
  }
  return stripped.empty() ? "unnamed" : stripped;
}

template <typename Msg> class TypedDdsSubscriber final : public DdsSubscriber
{
public:
  using MessageCallback = std::function<void(const Msg&)>;

  TypedDdsSubscriber(std::string key, const std::string& topic, int queue_length,
                     MessageCallback callback)
      : key_(std::move(key)), active_(std::make_shared<std::atomic_bool>(true)),
        subscriber_(std::make_shared<unitree::robot::ChannelSubscriber<Msg>>(topic))
  {
    subscriber_->InitChannel(
        [active = active_, callback = std::move(callback)](const void* raw_message)
        {
          if (raw_message && active->load(std::memory_order_acquire))
          {
            callback(*static_cast<const Msg*>(raw_message));
          }
        },
        queue_length);
  }

  ~TypedDdsSubscriber() override = default;

  const std::string& key() const override { return key_; }

  void setActive(bool active) override { active_->store(active, std::memory_order_release); }

  bool isActive() const override { return active_->load(std::memory_order_acquire); }

private:
  std::string key_;
  std::shared_ptr<std::atomic_bool> active_;
  unitree::robot::ChannelSubscriberPtr<Msg> subscriber_;
};

std::vector<std::unique_ptr<DdsSubscriber>>& leakedSubscribers()
{
  static auto* subscribers = new std::vector<std::unique_ptr<DdsSubscriber>>();
  return *subscribers;
}

class SettingsDialog : public QDialog
{
public:
  explicit SettingsDialog(const StreamConfig& previous, QWidget* parent = nullptr) : QDialog(parent)
  {
    previous_topics_ = previous.topics;

    setWindowTitle("Unitree SDK2 DDS Settings");
    auto* main_layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    network_interface_ = new QLineEdit(QString::fromStdString(previous.network_interface));
    network_interface_->setPlaceholderText("SDK default");
    form->addRow("Network interface", network_interface_);

    domain_id_ = new QSpinBox();
    domain_id_->setRange(0, 232);
    domain_id_->setValue(previous.domain_id);
    form->addRow("DDS domain id", domain_id_);

    clear_existing_data_ = new QCheckBox();
    clear_existing_data_->setChecked(previous.clear_existing_data);
    form->addRow("Clear existing data", clear_existing_data_);

    queue_length_ = new QSpinBox();
    queue_length_->setRange(0, 1024);
    queue_length_->setValue(previous.queue_length);
    form->addRow("DDS queue length", queue_length_);

    joystick_output_mode_ = new QComboBox();
    joystick_output_mode_->addItem("Parsed structure",
                                   static_cast<int>(JoystickOutputMode::ParsedStructure));
    joystick_output_mode_->addItem("Raw bytes", static_cast<int>(JoystickOutputMode::RawBytes));
    joystick_output_mode_->addItem("Raw and parsed",
                                   static_cast<int>(JoystickOutputMode::RawAndParsed));
    form->addRow("Joystick fields", joystick_output_mode_);
    main_layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    main_layout->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    restoreSelection(previous);
  }

  StreamConfig config() const
  {
    StreamConfig config;
    config.network_interface = network_interface_->text().trimmed().toStdString();
    config.domain_id = domain_id_->value();
    config.queue_length = queue_length_->value();
    config.clear_existing_data = clear_existing_data_->isChecked();
    config.joystick_output_mode =
        static_cast<JoystickOutputMode>(joystick_output_mode_->currentData().toInt());
    config.topics = previous_topics_;
    return config;
  }

private:
  void restoreSelection(const StreamConfig& previous)
  {
    const int joystick_index =
        joystick_output_mode_->findData(static_cast<int>(previous.joystick_output_mode));
    if (joystick_index >= 0)
    {
      joystick_output_mode_->setCurrentIndex(joystick_index);
    }
  }

  QLineEdit* network_interface_ = nullptr;
  QSpinBox* domain_id_ = nullptr;
  QSpinBox* queue_length_ = nullptr;
  QCheckBox* clear_existing_data_ = nullptr;
  QComboBox* joystick_output_mode_ = nullptr;
  std::vector<TopicSelection> previous_topics_;
};

class TopicSelectionDialog : public QDialog
{
public:
  TopicSelectionDialog(const StreamConfig& previous, std::vector<DiscoveredTopic> discovered_topics,
                       std::function<std::vector<DiscoveredTopic>()> refresh_topics,
                       QWidget* parent = nullptr)
      : QDialog(parent), previous_(previous), discovered_topics_(std::move(discovered_topics)),
        refresh_topics_(std::move(refresh_topics))
  {
    setWindowTitle("Unitree SDK2 DDS Topics");
    resize(780, 460);

    auto* main_layout = new QVBoxLayout(this);

    topics_ = new QTreeWidget();
    topics_->setColumnCount(3);
    topics_->setHeaderLabels({"Topic", "Message type", "Description"});
    topics_->setRootIsDecorated(false);
    topics_->setAlternatingRowColors(true);
    topics_->setSortingEnabled(false);
    topics_->header()->setStretchLastSection(true);
    topics_->header()->setSectionsClickable(true);
    topics_->header()->setSortIndicatorShown(true);
    topics_->header()->setSortIndicator(sort_column_, sort_order_);
    main_layout->addWidget(topics_, 1);

    auto* row = new QHBoxLayout();
    auto* refresh = new QPushButton("Refresh");
    auto* select_all = new QPushButton("Select All");
    auto* clear_all = new QPushButton("Clear All");
    row->addWidget(refresh);
    row->addWidget(select_all);
    row->addWidget(clear_all);
    row->addStretch(1);
    main_layout->addLayout(row);

    status_ = new QLabel();
    status_->setWordWrap(true);
    main_layout->addWidget(status_);

    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    main_layout->addWidget(buttons_);

    QObject::connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(refresh, &QPushButton::clicked, this, [this]() { refreshTopics(); });
    QObject::connect(select_all, &QPushButton::clicked, this,
                     [this]() { setAllTopicsChecked(Qt::Checked); });
    QObject::connect(clear_all, &QPushButton::clicked, this,
                     [this]() { setAllTopicsChecked(Qt::Unchecked); });
    QObject::connect(topics_, &QTreeWidget::itemChanged, this, [this]() { updateOkButton(); });
    QObject::connect(topics_->header(), &QHeaderView::sectionClicked, this,
                     [this](int section)
                     {
                       if (sort_column_ == section)
                       {
                         sort_order_ = (sort_order_ == Qt::AscendingOrder) ? Qt::DescendingOrder
                                                                           : Qt::AscendingOrder;
                       }
                       else
                       {
                         sort_column_ = section;
                         sort_order_ = Qt::AscendingOrder;
                       }
                       topics_->header()->setSortIndicator(sort_column_, sort_order_);
                       sortTopicItems();
                     });

    populate(enabledTopicsByKey(previous_.topics), !previous_.topics.empty());
    updateOkButton();
  }

  StreamConfig config() const
  {
    StreamConfig config = previous_;
    config.topics.clear();

    for (int index = 0; index < topics_->topLevelItemCount(); ++index)
    {
      const QTreeWidgetItem* item = topics_->topLevelItem(index);
      if (!item->data(0, SupportedRole).toBool())
      {
        continue;
      }
      config.topics.push_back({item->data(0, Qt::UserRole).toString().toStdString(),
                               item->data(0, TopicRole).toString().toStdString(),
                               item->data(0, TypeRole).toString().toStdString(),
                               item->checkState(0) == Qt::Checked});
    }
    return config;
  }

private:
  enum DataRole
  {
    TopicRole = Qt::UserRole + 1,
    TypeRole = Qt::UserRole + 2,
    SupportedRole = Qt::UserRole + 3
  };

  void populate(const std::unordered_map<std::string, bool>& enabled_by_key,
                bool has_explicit_enabled_state)
  {
    topics_->clear();

    int supported_count = 0;
    for (const DiscoveredTopic& discovered : discovered_topics_)
    {
      bool enabled = false;
      if (discovered.supported)
      {
        ++supported_count;
        const auto enabled_it = enabled_by_key.find(discovered.key);
        if (enabled_it != enabled_by_key.end())
        {
          enabled = enabled_it->second;
        }
        else
        {
          enabled = !has_explicit_enabled_state || discovered.default_enabled;
        }
      }

      auto* item = new QTreeWidgetItem(topics_);
      if (discovered.supported)
      {
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, enabled ? Qt::Checked : Qt::Unchecked);
      }
      else
      {
        item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
      }
      item->setText(0, discovered.topic);
      item->setText(1, discovered.type_name);
      item->setText(2, discovered.label);
      item->setData(0, Qt::UserRole, QString::fromStdString(discovered.key));
      item->setData(0, TopicRole, discovered.topic);
      item->setData(0, TypeRole, discovered.type_name);
      item->setData(0, SupportedRole, discovered.supported);
    }
    sortTopicItems();
    topics_->resizeColumnToContents(0);
    topics_->resizeColumnToContents(1);
    topics_->resizeColumnToContents(2);

    if (discovered_topics_.empty())
    {
      status_->setText("No DDS publications discovered. Check the DDS domain/interface, "
                       "start the publisher, then press Refresh.");
    }
    else
    {
      status_->setText(QString("%1 DDS publication(s) discovered, %2 supported by this plugin.")
                           .arg(discovered_topics_.size())
                           .arg(supported_count));
    }
  }

  void sortTopicItems()
  {
    QList<QTreeWidgetItem*> items;
    while (topics_->topLevelItemCount() > 0)
    {
      items.push_back(topics_->takeTopLevelItem(0));
    }

    std::stable_sort(items.begin(), items.end(),
                     [this](const QTreeWidgetItem* lhs, const QTreeWidgetItem* rhs)
                     {
                       const bool lhs_supported = lhs->data(0, SupportedRole).toBool();
                       const bool rhs_supported = rhs->data(0, SupportedRole).toBool();
                       if (lhs_supported != rhs_supported)
                       {
                         return lhs_supported;
                       }

                       int cmp = QString::localeAwareCompare(lhs->text(sort_column_),
                                                             rhs->text(sort_column_));
                       if (cmp == 0 && sort_column_ != 0)
                       {
                         cmp = QString::localeAwareCompare(lhs->text(0), rhs->text(0));
                       }
                       if (cmp == 0)
                       {
                         cmp = QString::localeAwareCompare(lhs->text(1), rhs->text(1));
                       }
                       return sort_order_ == Qt::AscendingOrder ? cmp < 0 : cmp > 0;
                     });

    topics_->addTopLevelItems(items);
  }

  std::unordered_map<std::string, bool> checkedTopicsByKey() const
  {
    std::unordered_map<std::string, bool> checked_by_key;
    for (int index = 0; index < topics_->topLevelItemCount(); ++index)
    {
      const QTreeWidgetItem* item = topics_->topLevelItem(index);
      if (!item->data(0, SupportedRole).toBool())
      {
        continue;
      }
      checked_by_key[item->data(0, Qt::UserRole).toString().toStdString()] =
          item->checkState(0) == Qt::Checked;
    }
    return checked_by_key;
  }

  void refreshTopics()
  {
    if (!refresh_topics_)
    {
      return;
    }

    const auto checked_by_key = checkedTopicsByKey();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    discovered_topics_ = refresh_topics_();
    QApplication::restoreOverrideCursor();
    populate(checked_by_key, true);
    updateOkButton();
  }

  void setAllTopicsChecked(Qt::CheckState state)
  {
    for (int index = 0; index < topics_->topLevelItemCount(); ++index)
    {
      QTreeWidgetItem* item = topics_->topLevelItem(index);
      if (item->data(0, SupportedRole).toBool())
      {
        item->setCheckState(0, state);
      }
    }
    updateOkButton();
  }

  void updateOkButton()
  {
    bool has_checked = false;
    for (int index = 0; index < topics_->topLevelItemCount(); ++index)
    {
      const QTreeWidgetItem* item = topics_->topLevelItem(index);
      if (item->data(0, SupportedRole).toBool() && item->checkState(0) == Qt::Checked)
      {
        has_checked = true;
        break;
      }
    }
    if (auto* ok = buttons_->button(QDialogButtonBox::Ok))
    {
      ok->setEnabled(has_checked);
    }
  }

  StreamConfig previous_;
  std::vector<DiscoveredTopic> discovered_topics_;
  std::function<std::vector<DiscoveredTopic>()> refresh_topics_;
  QTreeWidget* topics_ = nullptr;
  QLabel* status_ = nullptr;
  QDialogButtonBox* buttons_ = nullptr;
  int sort_column_ = 0;
  Qt::SortOrder sort_order_ = Qt::AscendingOrder;
};

} // namespace

UnitreeDataStreamer::UnitreeDataStreamer()
{
  config_.domain_id = 0;
  config_.queue_length = 1;
  config_.clear_existing_data = true;
  config_.joystick_output_mode = JoystickOutputMode::ParsedStructure;
  loadDefaultSettings();

  settings_action_ = new QAction("Settings", this);
  QObject::connect(settings_action_, &QAction::triggered, this,
                   &UnitreeDataStreamer::showSettingsDialog);
  actions_.push_back(settings_action_);
}

UnitreeDataStreamer::~UnitreeDataStreamer()
{
  shutdown();
  leakSubscribersForProcessExit();
}

const char* UnitreeDataStreamer::name() const { return "Unitree SDK2 DDS"; }

const std::vector<QAction*>& UnitreeDataStreamer::availableActions()
{
  installStreamingOptionsFilter();
  return actions_;
}

bool UnitreeDataStreamer::eventFilter(QObject* watched, QEvent* event)
{
  if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease)
  {
    auto* widget = qobject_cast<QWidget*>(watched);
    if (widget && widget->objectName() == "buttonStreamingOptions")
    {
      auto* combo =
          widget->window() ? widget->window()->findChild<QComboBox*>("comboStreaming") : nullptr;
      if (combo && combo->currentText() == QString::fromUtf8(name()))
      {
        event->accept();
        if (event->type() == QEvent::MouseButtonRelease)
        {
          QTimer::singleShot(0, this, &UnitreeDataStreamer::showSettingsDialog);
        }
        return true;
      }
    }
  }

  return QObject::eventFilter(watched, event);
}

bool UnitreeDataStreamer::xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const
{
  QDomElement config = doc.createElement("unitree_sdk2_dds");
  config.setAttribute("network_interface", QString::fromStdString(config_.network_interface));
  config.setAttribute("domain_id", config_.domain_id);
  config.setAttribute("queue_length", config_.queue_length);
  config.setAttribute("clear_existing_data", config_.clear_existing_data);
  config.setAttribute("joystick_output_mode", static_cast<int>(config_.joystick_output_mode));

  QDomElement topics = doc.createElement("topics");
  for (const TopicSelection& selection : config_.topics)
  {
    QDomElement topic = doc.createElement("topic");
    topic.setAttribute("key", QString::fromStdString(selection.key));
    topic.setAttribute("name", QString::fromStdString(selection.topic));
    topic.setAttribute("type", QString::fromStdString(selection.type_name));
    topic.setAttribute("enabled", selection.enabled);
    topics.appendChild(topic);
  }
  config.appendChild(topics);
  parent_element.appendChild(config);
  return true;
}

bool UnitreeDataStreamer::xmlLoadState(const QDomElement& parent_element)
{
  const QDomElement config = parent_element.firstChildElement("unitree_sdk2_dds");
  if (config.isNull())
  {
    return false;
  }

  config_.network_interface = config.attribute("network_interface").toStdString();
  config_.domain_id = config.attribute("domain_id", "0").toInt();
  config_.queue_length = config.attribute("queue_length", "1").toInt();
  config_.clear_existing_data = boolAttribute(config, "clear_existing_data", true);
  config_.joystick_output_mode = static_cast<JoystickOutputMode>(
      config
          .attribute("joystick_output_mode",
                     QString::number(static_cast<int>(JoystickOutputMode::ParsedStructure)))
          .toInt());

  config_.topics.clear();
  const QDomElement topics = config.firstChildElement("topics");
  for (QDomElement topic = topics.firstChildElement("topic"); !topic.isNull();
       topic = topic.nextSiblingElement("topic"))
  {
    config_.topics.push_back(
        {topic.attribute("key").toStdString(), topic.attribute("name").toStdString(),
         topic.attribute("type").toStdString(), boolAttribute(topic, "enabled", true)});
  }
  if (config_.topics.empty())
  {
    config_.topics = topicSelections();
  }
  return true;
}

bool UnitreeDataStreamer::configureTopics(StreamConfig* config)
{
  QApplication::setOverrideCursor(Qt::WaitCursor);
  std::vector<DiscoveredTopic> discovered_topics = discoverTopics(*config);
  QApplication::restoreOverrideCursor();

  const StreamConfig discovery_config = *config;
  TopicSelectionDialog dialog(*config, std::move(discovered_topics),
                              [discovery_config]() { return discoverTopics(discovery_config); });
  if (dialog.exec() != QDialog::Accepted)
  {
    return false;
  }
  *config = dialog.config();

  const auto enabled_count =
      std::count_if(config->topics.begin(), config->topics.end(),
                    [](const TopicSelection& topic) { return topic.enabled; });
  if (enabled_count == 0)
  {
    QMessageBox::warning(nullptr, "Unitree SDK2 DDS", "Select at least one DDS topic.");
    return false;
  }

  return true;
}

void UnitreeDataStreamer::showSettingsDialog()
{
  SettingsDialog dialog(config_);
  if (dialog.exec() != QDialog::Accepted)
  {
    return;
  }

  config_ = dialog.config();
  saveDefaultSettings();
}

void UnitreeDataStreamer::installStreamingOptionsFilter()
{
  for (QWidget* window : QApplication::topLevelWidgets())
  {
    const auto buttons = window->findChildren<QWidget*>("buttonStreamingOptions");
    for (QWidget* button : buttons)
    {
      button->removeEventFilter(this);
      button->installEventFilter(this);
    }
  }
}

void UnitreeDataStreamer::loadDefaultSettings()
{
  QSettings settings;
  const QString group = kSettingsGroup;

  config_.network_interface =
      settings.value(group + "/network_interface", QString()).toString().toStdString();
  config_.domain_id = settings.value(group + "/domain_id", 0).toInt();
  config_.queue_length = settings.value(group + "/queue_length", 1).toInt();
  config_.clear_existing_data = settings.value(group + "/clear_existing_data", true).toBool();
  config_.joystick_output_mode = static_cast<JoystickOutputMode>(
      settings
          .value(group + "/joystick_output_mode",
                 static_cast<int>(JoystickOutputMode::ParsedStructure))
          .toInt());

  config_.topics.clear();
}

void UnitreeDataStreamer::saveDefaultSettings() const
{
  QSettings settings;
  const QString group = kSettingsGroup;

  settings.setValue(group + "/network_interface",
                    QString::fromStdString(config_.network_interface));
  settings.setValue(group + "/domain_id", config_.domain_id);
  settings.setValue(group + "/queue_length", config_.queue_length);
  settings.setValue(group + "/clear_existing_data", config_.clear_existing_data);
  settings.setValue(group + "/joystick_output_mode",
                    static_cast<int>(config_.joystick_output_mode));

  QStringList enabled_topics;
  for (const TopicSelection& selection : config_.topics)
  {
    if (selection.enabled)
    {
      enabled_topics.push_back(QString::fromStdString(selection.key));
    }
  }
  settings.setValue(group + "/enabled_topics", enabled_topics);
}

bool UnitreeDataStreamer::start(QStringList* selected_datasources)
{
  (void)selected_datasources;

  if (running_)
  {
    return true;
  }

  StreamConfig new_config = config_;
  if (!configureTopics(&new_config))
  {
    return false;
  }

  if (new_config.clear_existing_data)
  {
    {
      std::lock_guard<std::mutex> lock(mutex());
      dataMap().clear();
    }
    Q_EMIT clearBuffers();
  }

  try
  {
    unitree::robot::ChannelFactory::Instance()->Init(new_config.domain_id,
                                                     new_config.network_interface);

    clearState();
    config_ = std::move(new_config);
    saveDefaultSettings();
    start_time_ = std::chrono::steady_clock::now();
    total_messages_ = 0;
    running_ = true;

    for (const TopicSelection& selection : config_.topics)
    {
      if (!selection.enabled)
      {
        continue;
      }

      bool subscribed = false;
#define PJ_ADD_TYPED_SUBSCRIBER(MSG_TYPE, TYPE_NAME, LABEL)                                        \
  if (!subscribed && typeNamesMatch(selection.type_name, TYPE_NAME))                               \
  {                                                                                                \
    addSubscriber<MSG_TYPE>(selection);                                                            \
    subscribed = true;                                                                             \
  }
      PJ_UNITREE_SDK2_MESSAGE_TYPES(PJ_ADD_TYPED_SUBSCRIBER)
#undef PJ_ADD_TYPED_SUBSCRIBER

      if (!subscribed)
      {
        throw std::runtime_error("Unsupported topic selection: " + selection.topic + " [" +
                                 selection.type_name + "]");
      }
    }
  }
  catch (const std::exception& error)
  {
    shutdown();
    QMessageBox::critical(nullptr, "Unitree SDK2 DDS", error.what());
    return false;
  }

  return true;
}

void UnitreeDataStreamer::shutdown()
{
  running_ = false;
  clearState();

  std::lock_guard<std::mutex> lock(callback_mutex_);
}

bool UnitreeDataStreamer::isRunning() const { return running_; }

template <typename Msg> void UnitreeDataStreamer::addSubscriber(const TopicSelection& selection)
{
  for (const auto& subscriber : subscribers_)
  {
    if (subscriber && subscriber->key() == selection.key)
    {
      subscriber->setActive(true);
      return;
    }
  }

  const std::string topic_prefix = stripUnitreeTopicPrefix(selection.topic);
  const int queue_length = std::max(0, config_.queue_length);

  subscribers_.push_back(std::make_unique<TypedDdsSubscriber<Msg>>(
      selection.key, selection.topic, queue_length,
      [this, topic_prefix](const Msg& message)
      {
        if (!running_)
        {
          return;
        }

        std::lock_guard<std::mutex> callback_lock(callback_mutex_);
        if (!running_)
        {
          return;
        }

        const double stamp = elapsedSeconds();
        {
          std::lock_guard<std::mutex> lock(mutex());
          const MessageFlattenOptions flatten_options{config_.joystick_output_mode};
          flattenMessage(message, flatten_options,
                         [this, &topic_prefix, stamp](const std::string& field, double value)
                         { appendSampleUnlocked(topic_prefix + "/" + field, stamp, value); });

          const uint64_t count = ++total_messages_;
          appendSampleUnlocked(topic_prefix + "/_message_count", stamp, static_cast<double>(count));
        }
        Q_EMIT dataReceived();
      }));
}

double UnitreeDataStreamer::elapsedSeconds() const
{
  const auto now = std::chrono::steady_clock::now();
  const auto elapsed = std::chrono::duration<double>(now - start_time_);
  return elapsed.count();
}

void UnitreeDataStreamer::appendSampleUnlocked(const std::string& series_name, double stamp,
                                               double value)
{
  dataMap().getOrCreateNumeric(series_name).pushBack(PJ::PlotData::Point{stamp, value});
}

void UnitreeDataStreamer::clearState()
{
  for (auto& subscriber : subscribers_)
  {
    if (subscriber)
    {
      subscriber->setActive(false);
    }
  }
}

void UnitreeDataStreamer::leakSubscribersForProcessExit()
{
  auto& leaked = leakedSubscribers();
  leaked.reserve(leaked.size() + subscribers_.size());
  for (auto& subscriber : subscribers_)
  {
    if (subscriber)
    {
      subscriber->setActive(false);
      leaked.push_back(std::move(subscriber));
    }
  }
  subscribers_.clear();
}

} // namespace plotjuggler_unitree_sdk2
