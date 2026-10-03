#include "Parameters.h"

namespace tengri
{
const juce::StringArray& noteNames()
{
    static const juce::StringArray names { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names;
}

namespace
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
    using Attr   = juce::AudioParameterFloatAttributes;

    juce::String percent (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

    juce::String seconds (float v, int)
    {
        return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + "MS"
                        : juce::String (v, 1) + "S";
    }

    void addPercent (Layout& layout, const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, 1.0f), def,
            Attr().withStringFromValueFunction (percent)));
    }

    void addTime (Layout& layout, const char* id, const char* name, float lo, float hi, float def)
    {
        juce::NormalisableRange<float> range (lo, hi);
        range.setSkewForCentre (std::sqrt (lo * hi) * 1.5f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, def,
            Attr().withStringFromValueFunction (seconds)));
    }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    Layout layout;

    // ---- global
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { pid::mode, 1 }, "Mode", juce::StringArray { "Voice", "String" }, 0));
    addPercent (layout, pid::mix, "Mix", 0.85f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::output, 1 }, "Output", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        Attr().withStringFromValueFunction ([] (float v, int)
        {
            return (v > 0 ? "+" : "") + juce::String (v, 1) + "DB";
        })));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { pid::root, 1 }, "Root", noteNames(), 2)); // D
    addPercent (layout, pid::drone, "Drone", 0.0f);

    // ---- voice
    addPercent (layout, pid::kargyraa, "Kargyraa", 0.5f);
    addPercent (layout, pid::overtone, "Overtone", 0.6f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::harmonic, 1 }, "Harmonic", juce::NormalisableRange<float> (4.0f, 16.0f, 1.0f), 8.0f,
        Attr().withStringFromValueFunction ([] (float v, int) { return "H" + juce::String (juce::roundToInt (v)); })));
    addPercent (layout, pid::sweep, "Sweep", 0.3f);
    addPercent (layout, pid::vowel, "Vowel", 0.3f);
    addPercent (layout, pid::throat, "Throat", 0.3f);

    // ---- string
    addPercent (layout, pid::body, "Body", 0.7f);
    addPercent (layout, pid::sympathy, "Sympathy", 0.45f);
    addPercent (layout, pid::bow, "Bow", 0.5f);
    addTime (layout, pid::sustain, "Sustain", 0.05f, 6.0f, 1.2f);
    addPercent (layout, pid::grit, "Grit", 0.3f);
    addPercent (layout, pid::snap, "Snap", 0.5f);

    // ---- spirit
    addPercent (layout, pid::drum, "Drum", 0.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::pulse, 1 }, "Pulse", juce::NormalisableRange<float> (40.0f, 240.0f, 1.0f), 120.0f,
        Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + "BPM"; })));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { pid::sync, 1 }, "Sync", false));
    addPercent (layout, pid::space, "Space", 0.35f);
    addPercent (layout, pid::echo, "Echo", 0.2f);

    // ---- synth
    addPercent (layout, pid::synthLevel, "Synth Level", 0.7f);
    addPercent (layout, pid::synthTimbre, "Synth Timbre", 0.0f);
    addPercent (layout, pid::synthSub, "Synth Sub", 0.4f);
    addPercent (layout, pid::synthOvertone, "Synth Overtone", 0.6f);
    addTime (layout, pid::synthAttack, "Synth Attack", 0.002f, 4.0f, 0.08f);
    addTime (layout, pid::synthRelease, "Synth Release", 0.02f, 8.0f, 0.9f);

    return layout;
}

} // namespace tengri
