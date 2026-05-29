#pragma once

#include <PlotJuggler/toolbox_base.h>

namespace plotjuggler_unitree_sdk2
{

class UnitreeRobotViewWidget;

class UnitreeRobotViewToolbox : public PJ::ToolboxPlugin
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "facontidavide.PlotJuggler3.Toolbox")
  Q_INTERFACES(PJ::ToolboxPlugin)

public:
  UnitreeRobotViewToolbox();
  ~UnitreeRobotViewToolbox() override;

  const char* name() const override;
  void init(PJ::PlotDataMapRef& src_data, PJ::TransformsMap& transform_map) override;
  std::pair<QWidget*, WidgetType> providedWidget() const override;

public slots:
  bool onShowWidget() override;

private:
  UnitreeRobotViewWidget* widget_ = nullptr;
};

}  // namespace plotjuggler_unitree_sdk2
