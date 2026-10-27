#!/bin/sh
# usage: export_toolchain.sh <xbps-bin-dir> <host-pkgs-dir> <dest>
set -e
XBPS_BIN=$1; REPO=$2; DEST=$3
sum=$(ls -l --time-style=+%s "$REPO"/xbinutils-* "$REPO"/xgcc-* "$REPO"/libstdc++-v3-* | sha256sum)
[ "$(cat "$DEST/.sum" 2>/dev/null)" = "$sum" ] && exit 0
rm -rf "$DEST"; mkdir -p "$DEST"
PATH="$XBPS_BIN:$PATH" XBPS_ARCH=invalid XBPS_TARGET_ARCH=$(uname -m) \
    xbps-install -y -r "$DEST" -R "$REPO" xbinutils xgcc libstdc++-v3
echo "$sum" > "$DEST/.sum"