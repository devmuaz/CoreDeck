//
// Created by AbdulMuaz Aqeel on 27/09/2026.
//

#import <Cocoa/Cocoa.h>

#include "theme.h"

namespace CoreDeck {
void SetCocoaWindowAppearance(void *nativeWindow, const bool light) {
  if (nativeWindow == nullptr) {
    return;
  }

  auto *window = static_cast<NSWindow *>(nativeWindow);
  const NSAppearanceName name =
      light ? NSAppearanceNameAqua : NSAppearanceNameDarkAqua;
  window.appearance = [NSAppearance appearanceNamed:name];
}
} // namespace CoreDeck
