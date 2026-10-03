#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace tengri::pid
{
// global
inline constexpr auto mode   = "mode";
inline constexpr auto mix    = "mix";
inline constexpr auto output = "output";
inline constexpr auto root   = "root";
inline constexpr auto drone  = "drone";

// VOICE -> khöömei
inline constexpr auto kargyraa = "kargyraa";
inline constexpr auto overtone = "overtone";
inline constexpr auto harmonic = "harmonic";
inline constexpr auto sweep    = "sweep";
inline constexpr auto vowel    = "vowel";
inline constexpr auto throat   = "throat";

// STRING -> morin khuur
inline constexpr auto body     = "body";
inline constexpr auto sympathy = "sympathy";
inline constexpr auto bow      = "bow";
inline constexpr auto sustain  = "sustain";
inline constexpr auto grit     = "grit";
inline constexpr auto snap     = "snap";

// SPIRIT layer
inline constexpr auto drum  = "drum";
inline constexpr auto pulse = "pulse";
inline constexpr auto sync  = "sync";
inline constexpr auto space = "space";
inline constexpr auto echo  = "echo";

// SYNTH
inline constexpr auto synthLevel    = "synthLevel";
inline constexpr auto synthTimbre   = "synthTimbre";
inline constexpr auto synthSub      = "synthSub";
inline constexpr auto synthOvertone = "synthOvertone";
inline constexpr auto synthAttack   = "synthAttack";
inline constexpr auto synthRelease  = "synthRelease";
} // namespace tengri::pid

namespace tengri
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

const juce::StringArray& noteNames();
} // namespace tengri
