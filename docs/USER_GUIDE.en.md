# User Guide

## Requirements

- macOS 12 or later, or Windows 10/11 x64.
- OBS Studio 32.2 or a compatible version.
- One or more video sources already created in OBS.
- Camera automatic white balance disabled when possible.

## Install on macOS

Close OBS, unzip the macOS package and copy `ccu-studio.plugin` to:

```text
~/Library/Application Support/obs-studio/plugins/
```

Reopen OBS and look for `CCU Studio` under **Tools**.

## Install on Windows

1. Close OBS completely.
2. Unzip `ccu-studio-0.3.2-windows-x64.zip`.
3. Copy the complete `ccu-studio` folder to:

```text
%APPDATA%\obs-studio\plugins\
```

The resulting path must contain
`ccu-studio\bin\64bit\ccu-studio.dll` and `ccu-studio\data\`.
Reopen OBS and look for `CCU Studio` under **Tools**.

## Assign sources

1. Open **Tools → CCU Studio…**.
2. Right-click a preview and choose an OBS video source.
3. Repeat for up to four cameras.
4. Choose **No source** to clear a preview.

Empty previews display a subtle prompt explaining that sources are assigned
with a right-click.

## Active camera and controls

Select a camera by clicking its preview or buttons `1`–`4`. The one-pixel
gold border identifies the active camera. Only that camera responds to:

- Red, green and blue gain.
- Brightness.
- Contrast.
- Gamma.
- Saturation.
- Reset.

Corrections are stored in a `CCU Studio` filter attached to the original source,
so they also affect Program, recordings and streams.

## Eyedropper

1. Put a white or neutral grey reference under the actual lighting.
2. Select **Eyedropper**.
3. Move over the active camera preview.
4. Use the magnified reticle and white centre point to choose the sample.
5. Click inside the real video rectangle.

The eyedropper is locked to the active camera. Other previews reject picker
movement and clicks.

## Zoom

Select **Zoom** to show only the active camera. The enlarged preview occupies
exactly the same rectangle as the 2×2 mosaic, so the controls do not move.

## Scopes and frozen reference

The right panel provides an RGB histogram, waveform and vectorscope for the
active camera. Select **Freeze** to capture all three at once as an amber
reference, then select another camera to compare its live signal.

## Original/corrected comparison

Select **Original** to split the active preview:

- Original reconstruction on the left.
- Corrected image on the right.

This is preview-only and never disables the source filter or changes Program,
recording or streaming. Pixels already clipped to absolute black or white
cannot be reconstructed completely.

## Troubleshooting

- **Plugin missing from Tools:** verify the installation path and restart OBS.
- **No image:** right-click the preview and reassign an existing video source.
- **Eyedropper rejects a click:** click inside the actual image, not a
  letterbox bar or another camera.
- **Controls move but colour does not change:** confirm that the source owns a
  filter named `CCU Studio` (or `CCU OBS` when inherited from an earlier
  version).

## Instructions and support

The discreet **Instructions** action in the lower-left corner opens a built-in
guide in the active OBS language. The **Support** button is fixed at the foot
of that window and opens PayPal; donations are optional and help fund the
development of additional tools.

## License and credits

CCU Studio is distributed under `GPL-2.0-or-later`. Development was assisted
primarily by OpenAI Codex/ChatGPT and Claude, under human direction, testing,
and review. This acknowledgement does not imply affiliation with or
endorsement by OpenAI or Anthropic.

A project by **Lluís Bartra Homedes** for **Moiz i Bartra Produccions, SL**
and **El Català Emprenyat** ([emprenyat.cat](https://emprenyat.cat)).
Software conceived and created entirely in Catalonia.

Project website: [emprenyat.cat/obs/CCUstudio](https://emprenyat.cat/obs/CCUstudio)
