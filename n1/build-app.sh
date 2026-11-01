#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=${1:-/tmp/wiliwili-n1-build}
stage=${2:-/tmp/wiliwili-n1-stage}
cmake -S "$root" -B "$build" -DPLATFORM_DESKTOP=ON -DUSE_GLES2=ON \
 -DUSE_SYSTEM_CURL=ON -DUSE_SYSTEM_OPENCC=ON -DINSTALL=ON \
 -DCMAKE_INSTALL_PREFIX=/opt/wiliwili -DCMAKE_BUILD_TYPE=Release \
 -DBRLS_UNITY_BUILD=ON -DCMAKE_UNITY_BUILD_BATCH_SIZE=4 -DN1_LOW_MEMORY=ON
cmake --build "$build" -j4
DESTDIR="$stage" cmake --install "$build"
ln -sfn /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc \
 "$stage/opt/wiliwili/share/wiliwili/resources/font/switch_font.ttf"
