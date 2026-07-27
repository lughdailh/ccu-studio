#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <version>"
  exit 1
fi

version="${1#v}"
tag="v$version"
dist_dir="$(cd "$(dirname "$0")/../dist" && pwd)"

mac_versioned="$dist_dir/ccu-studio-$version-macos-universal.zip"
win_versioned="$dist_dir/ccu-studio-$version-windows-x64.zip"
mac_stable="$dist_dir/ccu-studio-macos-universal.zip"
win_stable="$dist_dir/ccu-studio-windows-x64.zip"

for asset in "$mac_versioned" "$win_versioned"; do
  if [[ ! -f "$asset" ]]; then
    echo "Missing release asset: $asset" >&2
    exit 1
  fi
done

cp "$mac_versioned" "$mac_stable"
cp "$win_versioned" "$win_stable"

gh release upload "$tag" "$mac_stable" "$win_stable" --clobber

echo "Stable assets uploaded to $tag"
