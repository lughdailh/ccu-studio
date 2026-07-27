#pragma once

#ifdef __APPLE__

// Brings the native window backing an NSView to the front for real.
// Qt's raise()/activateWindow() go through Qt's own window-activation
// abstraction, which on macOS can be a no-op when another app (OBS
// itself) currently holds focus - the CCU dialog would show() but stay
// behind OBS's main window. This calls Cocoa directly instead.
void ccuActivateWindowMac(void *nsView);

// Lets AppKit enforce the window proportion while the user drags any resize
// edge. This avoids recursive Qt resize corrections and their layout jitter.
void ccuSetWindowAspectRatioMac(void *nsView, double ratio);

#endif
