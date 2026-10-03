#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace tengri::ui
{
/** Brick / clay / steppe-gold palette over Nothing-style monochrome surfaces. */
namespace palette
{
    inline const juce::Colour background { 0xff1c120f }; // burnt umber night
    inline const juce::Colour card       { 0xff28180f }; // fired clay, dark
    inline const juce::Colour cardRaised { 0xff33201a };
    inline const juce::Colour stroke     { 0xff4a2d22 };
    inline const juce::Colour dotOff     { 0xff3f281f };
    inline const juce::Colour cream      { 0xfff3e6d1 }; // felt of the ger
    inline const juce::Colour dim        { 0xff9e7b68 };
    inline const juce::Colour brick      { 0xffc4492e }; // red brick / flag red
    inline const juce::Colour terracotta { 0xffdd6c48 };
    inline const juce::Colour ochre      { 0xffe2a64a }; // soyombo gold
    inline const juce::Colour ornament   { 0xff5c2f22 };
} // namespace palette

juce::Font labelFont (float height = 11.0f, bool bold = true);

class TengriLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TengriLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

} // namespace tengri::ui
