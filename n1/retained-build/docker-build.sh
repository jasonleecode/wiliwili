#!/bin/sh
set -eu
# Bookworm arm64; cached Debian packages copied from the matching N1.
for package in /out/debs/*.deb; do dpkg-deb -x "$package" /; done
cp /out/runtime-libs/* /usr/lib/aarch64-linux-gnu/
ldconfig
cmake -S /src -B /build/wiliwili -G 'Unix Makefiles' \
  -DPLATFORM_DESKTOP=ON -DUSE_GLES2=ON -DUSE_SYSTEM_CURL=ON \
  -DUSE_SYSTEM_OPENCC=ON -DINSTALL=ON -DCMAKE_INSTALL_PREFIX=/opt/wiliwili \
  -DCMAKE_BUILD_TYPE=Release -DBRLS_UNITY_BUILD=ON -DN1_LOW_MEMORY=ON \
  -DCMAKE_UNITY_BUILD_BATCH_SIZE=4 \
  -DMPV_INCLUDE_DIR=/usr/include \
  -DMPV_LIBRARY_mpv=/usr/lib/aarch64-linux-gnu/libmpv.so.2
cmake --build /build/wiliwili -j4
DESTDIR=/out/stage cmake --install /build/wiliwili
# Use the Chinese font already installed on the N1.
ln -sf /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc /out/stage/opt/wiliwili/share/wiliwili/resources/font/switch_font.ttf
strip /out/stage/opt/wiliwili/bin/wiliwili
# Include the isolated decoder when it has been built on the Mac.
if [ -f /out/libavcodec-wili-fast.so ]; then
  mkdir -p /out/stage/opt/n1-wiliwili/lib
  cp /out/libavcodec-wili-fast.so /out/stage/opt/n1-wiliwili/lib/libavcodec-fast-20261101.so
  ln -sfn libavcodec-fast-20261101.so /out/stage/opt/n1-wiliwili/lib/libavcodec.so.59
fi
tar -C /out/stage -czf /out/wiliwili-1.6.0-n1-arm64.tar.gz opt
sha256sum /out/wiliwili-1.6.0-n1-arm64.tar.gz > /out/SHA256SUMS
