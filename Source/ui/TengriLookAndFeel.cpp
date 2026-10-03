#include "TengriLookAndFeel.h"
#include "DotMatrix.h"

namespace tengri::ui
{
juce::Font labelFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain))
        .withExtraKerningFactor (0.12f);
}

TengriLookAndFeel::TengriLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, palette::background);
    setColour (juce::Slider::rotarySliderFillColourId, palette::brick);
    setColour (juce::Slider::trackColourId, palette::brick);
    setColour (juce::TooltipWindow::backgroundColourId, palette::cardRaised);
    setColour (juce::TooltipWindow::textColourId, palette::cream);
    setColour (juce::TooltipWindow::outlineColourId, palette::stroke);
}

void TengriLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float size  = std::min (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float ringR = size * 0.5f - 3.0f;
    const float dot   = juce::jlimit (2.0f, 4.0f, size * 0.05f);

    // dotted ring — Nothing glyph style
    const int numDots = size > 64 ? 31 : 25;
    const int lit = juce::roundToInt (pos * (float) (numDots - 1));
    for (int i = 0; i < numDots; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (numDots - 1);
        const auto p = centre.getPointOnCircumference (ringR, a);
        g.setColour (i <= lit ? palette::cream : palette::dotOff);
        const float d = (i == lit) ? dot * 1.5f : dot;
        g.fillEllipse (p.x - d * 0.5f, p.y - d * 0.5f, d, d);
    }

    // knob face
    const float faceR = ringR - dot * 2.6f;
    const bool hot = slider.isMouseOverOrDragging();
    g.setColour (hot ? palette::cardRaised.brighter (0.06f) : palette::cardRaised);
    g.fillEllipse (centre.x - faceR, centre.y - faceR, faceR * 2, faceR * 2);
    g.setColour (palette::stroke);
    g.drawEllipse (centre.x - faceR, centre.y - faceR, faceR * 2, faceR * 2, 1.0f);

    // indicator — a single red dot, like the Nothing recording light
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto ip = centre.getPointOnCircumference (faceR * 0.62f, angle);
    const float id = std::max (4.0f, faceR * 0.22f);
    g.setColour (accent);
    g.fillEllipse (ip.x - id * 0.5f, ip.y - id * 0.5f, id, id);
}

void TengriLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float, float, juce::Slider::SliderStyle, juce::Slider& slider)
{
    const auto accent = slider.findColour (juce::Slider::trackColourId);
    const auto r = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float pitch = 7.0f;
    const int numDots = std::max (2, (int) (r.getWidth() / pitch));
    const float startX = r.getX() + (r.getWidth() - (float) (numDots - 1) * pitch) * 0.5f;
    const float cy = r.getCentreY();
    const float frac = (sliderPos - r.getX()) / std::max (1.0f, r.getWidth());
    const int lit = juce::roundToInt (frac * (float) (numDots - 1));

    for (int i = 0; i < numDots; ++i)
    {
        const float d = i == lit ? 9.0f : 3.6f;
        g.setColour (i == lit ? accent : (i < lit ? palette::cream : palette::dotOff));
        g.fillEllipse (startX + (float) i * pitch - d * 0.5f, cy - d * 0.5f, d, d);
    }
}

void TengriLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? palette::cream : (highlighted ? palette::cardRaised : palette::card));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (on ? palette::cream : palette::stroke);
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);

    const float d = 6.0f;
    g.setColour (on ? palette::brick : palette::dotOff);
    g.fillEllipse (r.getX() + 10.0f, r.getCentreY() - d * 0.5f, d, d);

    DotMatrix::draw (g, b.getButtonText(), r.getX() + 22.0f, r.getCentreY() - DotMatrix::height (1.6f) * 0.5f, 1.6f,
                     on ? palette::background : palette::dim, juce::Colours::transparentBlack, 0.9f);
}

} // namespace tengri::ui
