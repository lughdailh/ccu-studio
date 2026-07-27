# CCU OBS 0.3.0

Test release for universal macOS (Apple Silicon and Intel) and Windows x64.

## Highlights

- Redesigned CCU interface with logo, custom icons and circular controls.
- 2×2 mosaic of 16:9 previews with a one-pixel gap.
- Right-click source assignment and visual guidance in empty previews.
- Live RGB histogram, waveform and vectorscope.
- Simultaneous frozen reference for all three scopes.
- Preview-only split original/corrected comparison.
- Eyedropper locked to the active camera, with magnifier and precise centre.
- Zoom view that uses the exact mosaic rectangle without moving controls.
- Adaptive 5:3 window limited to 85% of the available desktop area.
- Catalan and English interfaces.

## Install

### macOS

Close OBS, unzip the package and copy `obs-ccu.plugin` to:

```text
~/Library/Application Support/obs-studio/plugins/
```

This test build uses an ad-hoc signature and is not notarized.

### Windows

Close OBS, unzip the package and copy the `obs-ccu` folder to:

```text
%APPDATA%\obs-studio\plugins\
```

## Compatibility

- OBS Studio 32.2 or compatible.
- macOS 12 or later.
- Windows 10/11 x64.
