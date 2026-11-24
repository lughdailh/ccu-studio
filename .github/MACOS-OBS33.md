# macOS / OBS 33.0.3

Run `bash .github/scripts/build-macos-obs33.sh` with Xcode 26.5+ and CMake 3.28+.
The script builds the official OBS 33.0.3 SDK and uses the SHA-256 pinned
2026-10-23 universal OBS/Qt dependencies in `macos-dependencies.json`.
Set `OBS33_SDK_ROOT` to reuse a local SDK directory between checkouts.

Output: `dist/ccu-studio-0.3.3-macos-universal.zip` (macOS 13+, arm64 + x86_64).
No user OBS installation or configuration is changed by this build script.

The package is ad-hoc signed, not notarized. The verifier checks both slices,
OBS/Qt linkage, exported module entry points, rpaths, resources, version,
minimum macOS version, signature and ZIP integrity. It does not run OBS.
Use OBS's own libraries at `@executable_path/../Frameworks`; do not bundle Qt.
The previous Windows build configuration is separate and unchanged.

For installation, close OBS, extract the ZIP and copy `ccu-studio.plugin`
from the extracted folder into `~/Library/Application Support/obs-studio/plugins/`.

Runtime acceptance still requires loading inside OBS 33.0.3, exercising the UI
and actual sources/recording, and testing on physical Intel and Apple Silicon Macs.
A successful CI build or module entry-point check alone is not that validation.
