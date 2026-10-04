#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace tengri
{
/** Factory presets. Any parameter a preset doesn't name goes back to its default; MUTE is never touched. */
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values; // real (not normalised) values
};

const std::vector<Preset>& factoryPresets();

void applyPreset (juce::AudioProcessorValueTreeState& state, const Preset& preset);
} // namespace tengri
