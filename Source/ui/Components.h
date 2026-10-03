#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DotMatrix.h"
#include "TengriLookAndFeel.h"

namespace tengri::ui
{
//==============================================================================
/** Rotary knob with a dotted ring, a small-caps label and a dot-matrix value. */
class DotKnob : public juce::Component
{
public:
    DotKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
             const juce::String& labelText, const juce::String& tooltip, juce::Colour accent)
        : label (labelText)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.setTooltip (tooltip);
        slider.setMouseDragSensitivity (220);
        addAndMakeVisible (slider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);

        if (auto* p = state.getParameter (paramId))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));

        slider.onValueChange = [this] { repaint(); };
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const int knob = std::min (r.getWidth(), r.getHeight() - 46);
        slider.setBounds (r.removeFromTop (knob).withSizeKeepingCentre (knob, knob));
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        r.removeFromTop (slider.getBottom() + 4.0f);

        g.setColour (palette::cream);
        g.setFont (labelFont (10.5f));
        g.drawText (label.toUpperCase(), r.removeFromTop (14.0f), juce::Justification::centred);

        const auto value = slider.getTextFromValue (slider.getValue()).toUpperCase().removeCharacters (" ");
        r.removeFromTop (4.0f);
        DotMatrix::drawCentred (g, value, r.removeFromTop (12.0f), 1.7f, palette::dim.brighter (0.25f));
    }

private:
    juce::String label;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
/** Horizontal dot-row slider (mix / output). */
class DotSlider : public juce::Component
{
public:
    DotSlider (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& labelText,
               juce::Colour accent)
        : label (labelText)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour (juce::Slider::trackColourId, accent);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
        if (auto* p = state.getParameter (paramId))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.onValueChange = [this] { repaint(); };
    }

    void resized() override
    {
        slider.setBounds (getLocalBounds().withTrimmedLeft (44).withTrimmedRight (62));
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (palette::cream);
        g.setFont (labelFont (10.5f));
        g.drawText (label.toUpperCase(), r.removeFromLeft (44.0f), juce::Justification::centredLeft);
        const auto value = slider.getTextFromValue (slider.getValue()).toUpperCase().removeCharacters (" ");
        DotMatrix::draw (g, value, r.getRight() - DotMatrix::width (value, 1.7f), r.getCentreY() - 6.0f, 1.7f, palette::dim);
    }

private:
    juce::String label;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
/** Pill-shaped two-segment switch, attached to a choice parameter. */
class ModeSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModeSwitch (juce::RangedAudioParameter& param, juce::StringArray segmentNames)
        : names (std::move (segmentNames)),
          attachment (param, [this] (float v) { index = juce::roundToInt (v); repaint(); })
    {
        attachment.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    int getIndex() const { return index; }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float radius = r.getHeight() * 0.5f;
        g.setColour (palette::card);
        g.fillRoundedRectangle (r, radius);
        g.setColour (palette::stroke);
        g.drawRoundedRectangle (r, radius, 1.0f);

        const float segW = r.getWidth() / (float) names.size();
        for (int i = 0; i < names.size(); ++i)
        {
            auto seg = juce::Rectangle<float> (r.getX() + segW * (float) i, r.getY(), segW, r.getHeight()).reduced (4.0f);
            const bool active = i == index;
            if (active)
            {
                g.setColour (palette::brick);
                g.fillRoundedRectangle (seg, seg.getHeight() * 0.5f);
            }
            DotMatrix::drawCentred (g, names[i], seg, 2.2f, active ? palette::cream : palette::dim);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = juce::jlimit (0, names.size() - 1, (int) (e.position.x / ((float) getWidth() / (float) names.size())));
        attachment.setValueAsCompleteGesture ((float) i);
    }

private:
    juce::StringArray names;
    juce::ParameterAttachment attachment;
    int index = 0;
};

//==============================================================================
/** Dot-matrix spectrum with a highlighted column for the sung / bowed harmonic. */
class DotSpectrum : public juce::Component
{
public:
    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder;

    DotSpectrum() : fft (fftOrder), window (fftSize, juce::dsp::WindowingFunction<float>::hann) {}

    void setSampleRate (double s) { sampleRate = s; }

    /** `samples` must hold fftSize samples. */
    void push (const float* samples, float markerHz, const juce::String& line1, const juce::String& line2, juce::Colour accent)
    {
        std::copy (samples, samples + fftSize, fftData.begin());
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
        window.multiplyWithWindowingTable (fftData.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const int cols = numCols();
        levels.resize ((size_t) cols, 0.0f);
        for (int c = 0; c < cols; ++c)
        {
            const float f0 = freqForCol ((float) c), f1 = freqForCol ((float) c + 1.0f);
            int b0 = juce::jlimit (1, fftSize / 2 - 1, (int) (f0 / (float) sampleRate * fftSize));
            int b1 = juce::jlimit (b0 + 1, fftSize / 2, (int) (f1 / (float) sampleRate * fftSize) + 1);
            float m = 0;
            for (int b = b0; b < b1; ++b)
                m = std::max (m, fftData[(size_t) b]);
            const float db = juce::Decibels::gainToDecibels (m / (float) fftSize * 4.0f, -90.0f);
            const float level = juce::jlimit (0.0f, 1.0f, (db + 62.0f) / 56.0f);
            auto& l = levels[(size_t) c];
            l = level > l ? level : l * 0.86f + level * 0.14f;
        }

        marker = markerHz;
        text1 = line1;
        text2 = line2;
        accentColour = accent;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const int cols = numCols(), rows = numRows();
        const float d = 3.4f;
        const int markerCol = marker > 0 ? (int) colForFreq (marker) : -1;

        for (int c = 0; c < cols; ++c)
        {
            const float level = c < (int) levels.size() ? levels[(size_t) c] : 0.0f;
            const int litRows = juce::roundToInt (level * (float) rows);
            const bool isMarker = c == markerCol;

            for (int r = 0; r < rows; ++r)
            {
                const bool lit = r < litRows;
                juce::Colour col = palette::dotOff;
                if (lit)
                    col = isMarker ? accentColour : (r == litRows - 1 ? palette::cream : palette::cream.withAlpha (0.32f));
                else if (isMarker)
                    col = accentColour.withAlpha (0.25f);
                g.setColour (col);
                const float x = (float) c * pitch + (pitch - d) * 0.5f;
                const float y = (float) getHeight() - (float) (r + 1) * pitch + (pitch - d) * 0.5f;
                g.fillEllipse (x, y, d, d);
            }
        }

        // read-outs on a dark plate so they stay legible over the bars
        auto plate = [&] (const juce::String& s, float y, juce::Colour c)
        {
            if (s.isEmpty()) return;
            const float w = DotMatrix::width (s, 1.8f);
            g.setColour (palette::card.withAlpha (0.85f));
            g.fillRoundedRectangle (0.0f, y - 4.0f, w + 12.0f, 20.0f, 6.0f);
            DotMatrix::draw (g, s, 6.0f, y + 0.5f, 1.8f, c);
        };
        plate (text1, 4.0f, palette::cream);
        plate (text2, 26.0f, accentColour);
    }

private:
    static constexpr float pitch = 8.0f;
    static constexpr float fLow = 40.0f, fHigh = 9000.0f;

    int numCols() const { return std::max (1, (int) ((float) getWidth() / pitch)); }
    int numRows() const { return std::max (1, (int) ((float) getHeight() / pitch)); }
    float freqForCol (float c) const { return fLow * std::pow (fHigh / fLow, c / (float) numCols()); }
    float colForFreq (float f) const { return (float) numCols() * std::log (f / fLow) / std::log (fHigh / fLow); }

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, fftSize * 2> fftData {};
    std::vector<float> levels;
    double sampleRate = 44100;
    float marker = 0;
    juce::String text1, text2;
    juce::Colour accentColour = palette::brick;
};

} // namespace tengri::ui
