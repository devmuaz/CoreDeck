#ifndef COREDECK_GUI_WIDGETS_TONE_H
#define COREDECK_GUI_WIDGETS_TONE_H

#include <cstdint>

namespace CoreDeck::Gui {
    enum class Tone : uint8_t {
        Neutral,
        Info,
        Positive,
        Warning,
        Negative,
    };
}

#endif // COREDECK_GUI_WIDGETS_TONE_H
