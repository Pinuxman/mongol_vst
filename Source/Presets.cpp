#include "Presets.h"
#include "Parameters.h"

namespace tengri
{
const std::vector<Preset>& factoryPresets()
{
    using namespace pid;
    static const std::vector<Preset> presets {
        { "INIT", {} },

        { "STEPPE KHOOMEI",
          { { mode, 0 }, { kargyraa, 0.35f }, { overtone, 0.7f }, { whistle, 1.0f }, { harmonic, 10 }, { sweep, 0.35f },
            { vowel, 0.3f }, { throat, 0.25f }, { drone, 0.2f }, { space, 0.45f }, { echo, 0.2f } } },

        { "DEEP KARGYRAA",
          { { mode, 0 }, { kargyraa, 0.95f }, { overtone, 0.3f }, { whistle, 0.5f }, { harmonic, 6 }, { sweep, 0.1f },
            { vowel, 0.15f }, { throat, 0.6f }, { space, 0.4f }, { echo, 0.1f } } },

        { "SYGYT FLUTE",
          { { mode, 0 }, { kargyraa, 0.15f }, { overtone, 1.0f }, { whistle, 1.6f }, { harmonic, 12 }, { sweep, 0.55f },
            { vowel, 0.75f }, { throat, 0.2f }, { space, 0.5f }, { echo, 0.35f } } },

        { "SOFT WHISTLE",
          { { mode, 0 }, { kargyraa, 0.4f }, { overtone, 0.6f }, { whistle, 0.35f }, { harmonic, 8 }, { sweep, 0.2f },
            { vowel, 0.3f }, { throat, 0.2f }, { space, 0.35f } } },

        { "MORIN KHUUR",
          { { mode, 1 }, { body, 0.8f }, { sympathy, 0.5f }, { bow, 0.55f }, { sustain, 1.5f }, { grit, 0.3f }, { snap, 0.6f },
            { space, 0.4f }, { echo, 0.15f } } },

        { "IRON RULER",
          { { mode, 1 }, { body, 0.7f }, { sympathy, 0.65f }, { bow, 0.8f }, { sustain, 2.8f }, { grit, 0.4f }, { snap, 1.0f },
            { space, 0.5f }, { echo, 0.25f } } },

        { "HORSE GALLOP",
          { { mode, 1 }, { body, 0.75f }, { sympathy, 0.4f }, { bow, 0.6f }, { sustain, 0.6f }, { grit, 0.45f }, { snap, 0.8f },
            { drum, 0.5f }, { pulse, 176 }, { synthTimbre, 1.0f }, { space, 0.3f } } },

        { "SHAMAN TRANCE",
          { { mode, 0 }, { kargyraa, 0.6f }, { overtone, 0.6f }, { whistle, 0.9f }, { sweep, 0.45f }, { drum, 0.65f },
            { pulse, 200 }, { drone, 0.45f }, { space, 0.65f }, { echo, 0.3f } } },

        { "ETERNAL SKY",
          { { synthTimbre, 0.2f }, { synthSub, 0.5f }, { synthOvertone, 0.8f }, { synthAttack, 1.2f }, { synthRelease, 4.0f },
            { drone, 0.25f }, { space, 0.75f }, { echo, 0.35f } } },
    };
    return presets;
}

void applyPreset (juce::AudioProcessorValueTreeState& state, const Preset& preset)
{
    for (auto* p : state.processor.getParameters())
    {
        auto* param = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (param == nullptr || param->getParameterID() == pid::mute)
            continue;

        float normalised = param->getDefaultValue();
        for (auto& [id, value] : preset.values)
            if (param->getParameterID() == id)
                normalised = param->convertTo0to1 (value);

        param->beginChangeGesture();
        param->setValueNotifyingHost (normalised);
        param->endChangeGesture();
    }
}
} // namespace tengri
