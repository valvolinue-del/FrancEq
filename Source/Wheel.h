#pragma once

#include "EqDesign.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace franceq
{

// One mouse-wheel detent in the units JUCE reports on Windows and Linux
// (about 50/256 to 60/256). Trackpads send smooth fractions of that.
inline constexpr float wheelNotch = 0.20f;

// Coarse frequency scroll. Shift, Cmd, or Ctrl uses wheelFineFactor.
inline constexpr float frequencyOctavesPerNotch = 1.0f / 6.0f;
inline constexpr float wheelFineFactor = 0.1f;

// Node bandwidth: the whole Q range (wide to narrow) in this many detents.
// The Q knob itself is left on JUCE's own wheel step.
inline constexpr float bandWheelNotches = 6.0f;

// JUCE Slider::getMouseWheelDelta uses this fraction of the normalised range.
inline constexpr float sliderWheelProportion = 0.15f;

inline bool wheelIsFine (const juce::ModifierKeys& mods) noexcept
{
    return mods.isShiftDown() || mods.isCommandDown() || mods.isCtrlDown();
}

inline float signedWheelDelta (const juce::MouseWheelDetails& wheel) noexcept
{
    const float dominant = std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? -wheel.deltaX : wheel.deltaY;
    return wheel.isReversed ? -dominant : dominant;
}

inline bool wheelIsHorizontal (const juce::MouseWheelDetails& wheel) noexcept
{
    return std::abs (wheel.deltaX) > std::abs (wheel.deltaY);
}

inline float notchesFromWheel (const juce::MouseWheelDetails& wheel) noexcept
{
    const float delta = signedWheelDelta (wheel);
    if (std::abs (delta) < 1.0e-6f)
        return 0.0f;

    // A physical click should be one notch on every platform. JUCE's raw delta
    // for that click is about 0.04 on some macOS paths and about 0.2 elsewhere.
    if (! wheel.isSmooth)
        return std::copysign (1.0f, delta);

    return juce::jlimit (-3.0f, 3.0f, delta / wheelNotch);
}

inline float proportionAfterNotches (float proportion, float notches, float fullRangeNotches) noexcept
{
    const float span = std::max (1.0f, fullRangeNotches);
    return juce::jlimit (0.0f, 1.0f, proportion + notches / span);
}

inline float frequencyAfterNotches (float frequency, float notches) noexcept
{
    const float next = frequency * std::pow (2.0f, notches * frequencyOctavesPerNotch);
    return juce::jlimit (minFrequency, maxFrequency, next);
}

} // namespace franceq
