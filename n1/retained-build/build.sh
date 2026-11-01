#!/bin/sh
# Run on the Mac. The retained ARM64 Bookworm container has matching N1 deps.
set -eu
cd "$(dirname "$0")"
docker start n1-wiliwili-builder >/dev/null
docker cp docker-build.sh n1-wiliwili-builder:/build-wiliwili.sh
docker exec n1-wiliwili-builder /build-wiliwili.sh
