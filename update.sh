#!/bin/sh
# Replaces the esp-serial-flasher core in src/ with the given upstream release.
# Everything in src/ except our EspSerialFlasher.* files is upstream.
# Usage: ./update.sh v2.1.0
set -e
VERSION=$1
if [ -z "$VERSION" ]; then
    echo "usage: $0 <release tag, e.g. v2.1.0>" >&2
    exit 1
fi
REPO=https://github.com/espressif/esp-serial-flasher.git
ROOT=$(cd "$(dirname "$0")" && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

git -c advice.detachedHead=false clone --quiet --depth 1 --branch "$VERSION" "$REPO" "$TMP"

find "$ROOT/src" -mindepth 1 -maxdepth 1 ! -name 'EspSerialFlasher.*' -exec rm -rf {} +
cp -r "$TMP"/src/. "$TMP"/include/. "$TMP"/private_include/. "$ROOT/src/"
cp "$TMP"/LICENSE "$ROOT/LICENSE"
echo "esp-serial-flasher $VERSION" > "$ROOT/UPSTREAM_VERSION"
sed -i "s/^version=.*/version=${VERSION#v}/" "$ROOT/library.properties"
echo "Updated to esp-serial-flasher $VERSION"
