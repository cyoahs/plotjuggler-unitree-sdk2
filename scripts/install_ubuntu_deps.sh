#!/usr/bin/env bash
set -euo pipefail

sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential \
  ca-certificates \
  cmake \
  git \
  ninja-build \
  python3-dev \
  qtbase5-dev \
  libqt5opengl5-dev \
  libqt5serialport5-dev \
  libqt5svg5-dev \
  libqt5websockets5-dev \
  libqt5x11extras5-dev \
  liblua5.3-dev \
  liblz4-dev \
  libmosquitto-dev \
  libprotoc-dev \
  libzmq3-dev \
  libzstd-dev
