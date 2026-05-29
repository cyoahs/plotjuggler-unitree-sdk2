# PlotJuggler Unitree SDK2 Plugin

[中文](README.zh.md) | English

[PlotJuggler](https://github.com/facontidavide/PlotJuggler) Unitree SDK2 Plugin.

This repository provides two plugins:

- `Unitree SDK2 DDS`: a DataStreamer plugin that subscribes to DDS topics through `unitree_sdk2` and flattens strongly typed messages into numeric PlotJuggler series.
- `Unitree Robot View`: a Toolbox plugin that renders Unitree robot posture from `lowstate` IMU and motor position series already loaded into PlotJuggler.

## Usage

### 1. Download the Plugin

Download the Linux x86_64 bundle from GitHub Releases:

```text
plotjuggler-unitree-sdk2-<version>-linux-x86_64.tar.gz
```

Extract it to any directory, for example:

```bash
mkdir -p ~/plotjuggler_plugins
tar -xzf plotjuggler-unitree-sdk2-<version>-linux-x86_64.tar.gz \
  -C ~/plotjuggler_plugins
```

The extracted directory is the plugin folder that PlotJuggler should load:

```text
~/plotjuggler_plugins/plotjuggler-unitree-sdk2-<version>-linux-x86_64/
```

### 2. Add the Plugin Folder in PlotJuggler

Open PlotJuggler, open `Preferences`, select the `Plugins` tab, click `Add` in the `Plugin folders` list, choose the extracted directory, confirm, and restart PlotJuggler.

After restart, you should see:

- `Unitree SDK2 DDS` in the Streaming panel
- `Unitree Robot View` in the `Tools` menu

You can also load the plugin folder temporarily from the command line without changing Preferences:

```bash
plotjuggler --plugin_folders "$HOME/plotjuggler_plugins/plotjuggler-unitree-sdk2-<version>-linux-x86_64"
```

### 3. Stream Unitree DDS Data

In the PlotJuggler Streaming panel, select `Unitree SDK2 DDS`. Use the gear button to configure DDS network interface, domain id, queue length, and joystick field mode.

<img src="docs/images/unitree-sdk2-dds-settings.png" alt="Unitree SDK2 DDS settings dialog" width="360">

Press `Start` to scan DDS publications and select topics from the discovered list. Supported types are sorted first and selected by default. If the robot or DDS publisher starts later, press `Refresh` to scan again. After confirmation, the plugin subscribes to the selected topics and writes PlotJuggler series.

`Joystick fields` defaults to `Parsed structure`. In this mode, Unitree joystick data is decoded into fields such as `wireless_remote/buttons/*`, `wireless_remote/axes/*`, `joystick/buttons/*`, and `joystick/axes/*` instead of only exposing raw bytes or key bitmasks.

Leading `unitree/` and `rt/` topic path components are removed from series names, for example:

```text
lowstate/imu_state/rpy/0
lowstate/motor_state/00/q
sportmodestate/velocity/0
```

The time axis is local elapsed seconds from the moment streaming starts.

### 4. Open the Robot Posture View

First stream `lowstate` data with `Unitree SDK2 DDS`, then open `Tools` / `Unitree Robot View`. Robot View does not subscribe to DDS by itself; it consumes the time series already loaded into PlotJuggler.

![Unitree Robot View showing a G1 posture replay](docs/images/unitree-robot-view-g1.png)

Input series:

- `lowstate/imu_state/rpy/*` or `lowstate/imu_state/quaternion/*`
- `lowstate/motor_state/NN/q`

Controls:

- Drag with the left mouse button to rotate the camera.
- Use the mouse wheel to zoom.
- Double-click to reset the camera.
- Click the top-right axis icon to align the view to an axis.
- Keep `Live` checked to follow the latest data, or uncheck it and drag the timeline to replay older posture data.

## Supported Messages

Topic support is determined by DDS type, not by fixed topic names. Built-in support covers top-level Unitree SDK2 IDL message types under:

- `unitree_go`
- `unitree_hg`
- `unitree_hg_doubleimu`
- ROS2-style `std_msgs`, `geometry_msgs`, `sensor_msgs`, and `nav_msgs`

Numeric fields are flattened recursively. Strings are exported as `/length`. Large variable-length sequences such as images, point clouds, and maps are size-limited to avoid flooding PlotJuggler.

To add new message types, update:

```text
include/plotjuggler_unitree_sdk2/unitree_message_flatten.h
```

## Robot View Models and Motor Order

Motor order follows Unitree SDK2 `LowState.motor_state[]`, not URDF joint order. Go2 example:

```text
FR_hip, FR_thigh, FR_calf,
FL_hip, FL_thigh, FL_calf,
RR_hip, RR_thigh, RR_calf,
RL_hip, RL_thigh, RL_calf
```

The current bundle supports A1, Aliengo, B1, B2, B2W, Go1, Go2, Go2W, G1 23DOF, G1 29DOF, G1-D, H1, H1-2, H2, R1, and R1 AIR.

## Dependency Policy

The repository tracks project source, CI configuration, and required dependency declarations only. Build trees, install trees, generated bundles, deb caches, local sysroots, and PlotJuggler source checkouts are ignored.

Required dependencies:

- `third_party/unitree_sdk2`: Git submodule for Unitree SDK2, DDS type definitions, and DDS runtime libraries.
- `third_party/unitree_ros_assets`: Git submodule for Robot View URDF and mesh assets.
- Qt: system development package dependency.
- PlotJuggler: build-time development prefix dependency. It must provide `plotjugglerConfig.cmake` and `plotjuggler_base`; it is not a repository submodule.

## Local Development

### 1. Install System Dependencies

On Ubuntu 22.04/24.04:

```bash
scripts/install_ubuntu_deps.sh
```

This installs CMake, Ninja, Qt5, PlotJuggler build dependencies, and the base development packages used by CI.

### 2. Initialize Submodules

```bash
git submodule update --init --depth 1 \
  third_party/unitree_sdk2 \
  third_party/unitree_ros_assets
```

### 3. Prepare a PlotJuggler Development Prefix

If the Snap PlotJuggler package is available, reuse its development prefix:

```bash
export PLOTJUGGLER_PREFIX=/snap/plotjuggler/current/usr/local
```

If no development prefix is available, build PlotJuggler `3.17.2` into `.deps/plotjuggler-3.17.2`:

```bash
scripts/build_plotjuggler_dev.sh
export PLOTJUGGLER_PREFIX="$PWD/.deps/plotjuggler-3.17.2"
```

### 4. Configure, Build, and Test

Recommended local build:

```bash
scripts/build_local_bundle.sh
```

Equivalent CMake preset commands:

```bash
cmake --preset dev
cmake --build --preset dev --parallel 4
ctest --preset dev
```

Build only the message flattener smoke test, without Qt or PlotJuggler:

```bash
cmake --preset smoke
cmake --build --preset smoke --parallel 4
ctest --preset smoke
```

### 5. Common CMake Options

- `UNITREE_SDK2_ROOT=/path/to/unitree_sdk2`: override the default submodule.
- `PJ_UNITREE_BUNDLE_DIRECTORY=/path/to/bundle`: choose the runtime bundle output directory.
- `PJ_UNITREE_BUILD_BUNDLE=OFF`: build the shared libraries only, without copying the runtime bundle.
- `PJ_UNITREE_COPY_ROBOT_ASSETS=OFF`: skip URDF/mesh copying, useful when working only on the DDS plugin.
- `PJ_PLUGIN_INSTALL_DIRECTORY=/path/to/plugin_dir`: install plugins to a specific PlotJuggler plugin directory with `cmake --install`.

## Build Outputs

Raw build outputs:

```text
build/dev/libplotjuggler_unitree_sdk2.so
build/dev/libplotjuggler_unitree_robot_view.so
```

Default runtime bundle:

```text
bundle/plotjuggler_unitree_sdk2/
  libplotjuggler_unitree_sdk2.so
  libplotjuggler_unitree_robot_view.so
  libddsc.so
  libddsc.so.0
  libddscxx.so
  libddscxx.so.0
  assets/robots/
  licenses/
```

The bundle uses `$ORIGIN` RPATH for the DDS runtime libraries copied from Unitree SDK2. Qt and PlotJuggler libraries are intentionally not bundled; runtime users should rely on their installed PlotJuggler distribution.

## Packaging and Releases

Create a release package after building:

```bash
scripts/package_bundle.sh 0.1.0
```

Outputs:

```text
dist/plotjuggler-unitree-sdk2-0.1.0-linux-x86_64.tar.gz
dist/plotjuggler-unitree-sdk2-0.1.0-linux-x86_64.tar.gz.sha256
```

GitHub Actions:

- `.github/workflows/ci.yml`: builds with the `ci` preset, runs tests, and uploads a bundle artifact. CI uses a Release build and 2 parallel compile jobs to match the standard `ubuntu-22.04` GitHub-hosted runner resource profile.
- `.github/workflows/release.yml`: builds and publishes a GitHub Release when a `v*` tag is pushed.

Release:

```bash
git tag v0.1.0
git push origin v0.1.0
```

## Licenses

Project source code is licensed under the permissive MIT License; see `LICENSE`. It allows copying, modification, redistribution, and commercial use, provided that the copyright and license notice are retained.

Unitree SDK2 and Unitree ROS assets are BSD-3-Clause licensed. The release bundle copies:

- `licenses/unitree_sdk2_LICENSE`
- `licenses/unitree_sdk2_thirdparty/`
- `licenses/unitree_ros_LICENSE`
