#!/bin/bash
# Reproducible universal build. Does not install into the user's OBS profile.
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"
sdk_root="${OBS33_SDK_ROOT:-$repo_root/.deps/obs33}"
build_dir="${OBS33_BUILD_DIR:-$repo_root/build_macos_obs33}"
mkdir -p "$sdk_root" "$repo_root/dist"
sdk_root="$(cd "$sdk_root" && pwd)"
obs_version=33.0.3
deps_version=2026-10-23
source_archive="$sdk_root/$obs_version.tar.gz"
source_dir="$sdk_root/obs-studio-$obs_version"
verify_hash() {
  printf '%s  %s\n' "$2" "$1" | shasum -a 256 --check
}
fetch() {
  if [[ ! -f "$2" ]]; then curl -fL --retry 3 "$1" -o "$2"; fi
  verify_hash "$2" "$3"
}
lock_value() { python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["dependencies"][sys.argv[2]]["hashes"]["macos"])' .github/macos-dependencies.json "$1"; }
fetch "https://github.com/obsproject/obs-studio/archive/refs/tags/$obs_version.tar.gz" "$source_archive" "$(lock_value obs-studio)"
if [[ ! -d "$source_dir" ]]; then tar -xf "$source_archive" -C "$sdk_root"; fi
prebuilt="$source_dir/.deps/obs-deps-$deps_version-universal"
qt6="$source_dir/.deps/obs-deps-qt6-$deps_version-universal"
# The upstream bootstrap downloads and checks its own pinned dependency archives.
cmake -S "$source_dir" -B "$sdk_root/build" -G Xcode \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  "-DCMAKE_OSX_SYSROOT=$(xcrun --sdk macosx --show-sdk-path)" \
  "-DCMAKE_PREFIX_PATH=$prebuilt;$qt6" \
  -DENABLE_CORE_MODULES=OFF -DENABLE_FRONTEND=OFF -DENABLE_BROWSER=OFF \
  -DOBS_VERSION_OVERRIDE="$obs_version"
verify_hash "$source_dir/.deps/macos-deps-$deps_version-universal.tar.xz" "$(lock_value prebuilt)"
verify_hash "$source_dir/.deps/macos-deps-qt6-$deps_version-universal.tar.xz" "$(lock_value qt6)"
cmake --build "$sdk_root/build" --config Release --target obs-frontend-api --parallel 8
cmake --install "$sdk_root/build" --config Release --component Development --prefix "$sdk_root/install"

cmake_args=( -S . -B "$build_dir" -G Xcode
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
  "-DCMAKE_OSX_SYSROOT=$(xcrun --sdk macosx --show-sdk-path)"
  "-DCMAKE_PREFIX_PATH=$sdk_root/install;$prebuilt;$qt6"
  "-DCMAKE_FRAMEWORK_PATH=$sdk_root/install/Frameworks"
  "-DOBS_SDK_PATH=$sdk_root/install"
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO )
if [[ -f buildspec.json ]]; then
  name=rec-time-monitor
  version="$(python3 -c 'import json; print(json.load(open("buildspec.json"))["version"])')"
  cmake_args+=( -DENABLE_FRONTEND_API=ON -DENABLE_QT=ON "-DPLUGIN_BUILD_NUMBER=${GITHUB_RUN_NUMBER:-1}" )
  targets=(rec-time-monitor)
else
  name=ccu-studio
  version="$(sed -nE 's/project\(obs-ccu VERSION ([0-9.]+).*/\1/p' CMakeLists.txt)"
  cmake_args+=( -DBUILD_TESTING=ON )
  targets=(obs-ccu scope-data-tests)
fi
cmake "${cmake_args[@]}"
cmake --build "$build_dir" --config Release --target "${targets[@]}" --parallel 8
if [[ "$name" == ccu-studio ]]; then
  ctest --test-dir "$build_dir" -C Release -R '^scope-data-tests$' --output-on-failure
fi
# Keep bundles off iCloud/FileProvider folders: FinderInfo may reappear after signing.
stage="$(mktemp -d "${TMPDIR:-/tmp}/obs33-package.XXXXXX")"
trap 'rm -rf "$stage"' EXIT
ditto "$build_dir/Release/$name.plugin" "$stage/$name.plugin"
cp LICENSE "$stage/LICENSE"
cat > "$stage/INSTALL.txt" <<EOF
$name $version — macOS 13+, OBS $obs_version
Universal: Intel x86_64 + Apple Silicon arm64.
Close OBS. Copy $name.plugin from this extracted folder to:
~/Library/Application Support/obs-studio/plugins/
Built with official OBS dependencies $deps_version. Uses OBS's own libraries and Qt.
Ad-hoc signed; not notarized. Compilation and static package checks do not prove runtime behavior.
EOF
xattr -cr "$stage/$name.plugin"
codesign --force --sign - "$stage/$name.plugin"
python3 .github/scripts/verify-macos-bundle.py "$stage/$name.plugin" "$version" > "$stage/verification.json"
archive="$repo_root/dist/$name-$version-macos-universal.zip"
ditto -c -k --sequesterRsrc --keepParent "$stage/$name.plugin" "$archive"
# Include installation and provenance beside the top-level plugin.
(cd "$stage" && zip -q "$archive" INSTALL.txt LICENSE verification.json)
unzip -t "$archive"
shasum -a 256 "$archive" > "$archive.sha256"
echo "Built: $archive"
