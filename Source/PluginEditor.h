#pragma once

#include "PluginProcessor.h"
#include "ui/Components.h"
#include "ui/TengriLookAndFeel.h"

class TengriEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TengriEditor (TengriProcessor&);
    ~TengriEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int editorWidth = 1000, editorHeight = 748;

private:
    void timerCallback() override;
    void updateModeVisibility();
    void drawCard (juce::Graphics&, juce::Rectangle<float> area, const juce::String& title,
                   const juce::String& subtitle, juce::Colour accent) const;
    void drawHeader (juce::Graphics&) const;
    void drawOrnament (juce::Graphics&, juce::Rectangle<float> band) const;
    void drawMeter (juce::Graphics&, const juce::String& name, float level, float x, float y) const;

    using Knobs = juce::OwnedArray<tengri::ui::DotKnob>;
    void addKnob (Knobs& group, const char* paramId, const char* label, const char* tip, juce::Colour accent);

    TengriProcessor& processor;
    tengri::ui::TengriLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };

    tengri::ui::ModeSwitch modeSwitch;
    tengri::ui::PresetBar presetBar;
    juce::ToggleButton muteButton { "MUTE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;
    tengri::ui::DotSpectrum spectrum;
    Knobs voiceKnobs, stringKnobs, synthKnobs, spiritKnobs;
    tengri::ui::DotSlider mixSlider, outSlider;
    juce::ToggleButton syncButton { "SYNC" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;

    juce::Rectangle<float> leftCard, synthCard, spiritCard;
    int lastMode = -1, lastDrumHits = 0;
    float drumFlash = 0, inLevel = 0, outLevel = 0;
    std::array<float, tengri::ui::DotSpectrum::fftSize> scopeBuffer {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TengriEditor)
};
