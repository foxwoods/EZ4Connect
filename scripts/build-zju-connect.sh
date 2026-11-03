#!/bin/bash
# Build zju-connect from source (default: foxwoods/zju-connect, branch travelsky)
# Usage: ./scripts/build-zju-connect.sh <goos> <goarch> <output> [ref]
# Env: ZJU_CONNECT_REPO overrides the repository URL.

set -euo pipefail

GOOS_TARGET="$1"
GOARCH_TARGET="$2"
OUTPUT="$3"
REF="${4:-${ZJU_CONNECT_REF:-travelsky}}"
REPO="${ZJU_CONNECT_REPO:-https://github.com/foxwoods/zju-connect.git}"
SRC_DIR="zju-connect-src"

case "$OUTPUT" in
    /*) ;;
    *) OUTPUT="$PWD/$OUTPUT" ;;
esac

rm -rf "$SRC_DIR"
git clone --depth 1 --branch "$REF" "$REPO" "$SRC_DIR"

pushd "$SRC_DIR" > /dev/null
COMMIT_ID="$(git rev-parse --short HEAD)"
# EZ4Connect parses "-version" output with the prefix "ZJU Connect v"
VERSION="v${REF#v}"
CGO_ENABLED=0 GOOS="$GOOS_TARGET" GOARCH="$GOARCH_TARGET" \
    go build -v -trimpath -o "$OUTPUT" \
    -ldflags "-s -w -buildid= -X main.zjuConnectVersion=$VERSION -X main.CommitID=$COMMIT_ID" .
popd > /dev/null

rm -rf "$SRC_DIR"
