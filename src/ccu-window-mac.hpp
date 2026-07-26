#pragma once

#ifdef __APPLE__

// Brings the native window backing an NSView to the front for real.
// Qt's raise()/activateWindow() go through Qt's own window-activation
// abstraction, which on macOS can be a no-op when another app (OBS
// itself) currently holds focus - the CCU dialog would show() but stay
// behind OBS's main window. This calls Cocoa directly instead.
void ccuActivateWindowMac(void *nsView);

#endif
