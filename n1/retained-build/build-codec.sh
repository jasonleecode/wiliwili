#!/bin/sh
# Run on the Mac, using the retained ARM64 FFmpeg build tree.
set -eu
cd "$(dirname "$0")"
container=n1-hwdecode-workbench
docker start "$container" >/dev/null
docker cp patches/ffmpeg-v4l2-source-change.patch "$container":/tmp/wili-source-change.patch
docker cp patches/ffmpeg-v4l2-copy-fast.patch "$container":/tmp/wili-copy-fast.patch
docker exec "$container" sh -eu -c '
cd /build/ffmpeg/ffmpeg-5.1.9
if patch --dry-run -R -p1 < /tmp/wili-copy-fast.patch >/dev/null 2>&1; then
    : # Both patch layers are already applied.
elif patch --dry-run -p1 < /tmp/wili-source-change.patch >/dev/null 2>&1; then
    patch -p1 < /tmp/wili-source-change.patch
    patch -p1 < /tmp/wili-copy-fast.patch
elif patch --dry-run -R -p1 < /tmp/wili-source-change.patch >/dev/null 2>&1; then
    patch -p1 < /tmp/wili-copy-fast.patch
else
    echo "FFmpeg source does not match the retained reset2 baseline or the applied patch." >&2
    exit 1
fi
cd debian/standard
make -j4 libavcodec/libavcodec.so.59
cp libavcodec/libavcodec.so.59 /tmp/wili-safe.so
strip --strip-unneeded /tmp/wili-safe.so
'
docker cp "$container":/tmp/wili-safe.so output/libavcodec-wili-fast.so
shasum -a 256 output/libavcodec-wili-fast.so
