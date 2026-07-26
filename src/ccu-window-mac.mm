#include "ccu-window-mac.hpp"

#import <AppKit/AppKit.h>

void ccuActivateWindowMac(void *nsView) {
  @autoreleasepool {
    NSView *view = (__bridge NSView *)nsView;
    NSWindow *window = view.window;
    if (!window)
      return;
    // activateIgnoringOtherApps is what actually steals focus from OBS;
    // without it, orderFrontRegardless can still leave the window behind
    // the frontmost app's window.
    [NSApp activateIgnoringOtherApps:YES];
    [window makeKeyAndOrderFront:nil];
    [window orderFrontRegardless];
  }
}
