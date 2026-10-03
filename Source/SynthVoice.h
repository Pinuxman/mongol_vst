#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "dsp/ThroatCore.h"

namespace tengri
{
/** Values shared by every synth voice, refreshed by the processor once per block. */
struct SynthShared
{
    ThroatCore::Params core;
    float attack   = 0.08f;
    float release  = 0.9f;
    float modWheel = 0.0f; // CC1 picks the overtone by hand; 0 = wandering melody
};

struct TengriSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class TengriVoice : public juce::SynthesiserVoice
{
public:
    TengriVoice (SynthShared& sharedState, uint32_t seed) : shared (sharedState), core (seed) {}

    void prepare (double sampleRate)
    {
        core.prepare (sampleRate);
        adsr.setSampleRate (sampleRate);
    }

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<TengriSound*> (s) != nullptr; }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override
    {
        note = midiNote;
        bendSemitones = pitchWheelToSemitones (pitchWheel);
        const bool fresh = ! adsr.isActive();
        if (fresh)
            core.reset();
        core.setFrequency (currentHz(), fresh);

        adsr.setParameters ({ shared.attack, 0.35f, 0.8f, shared.release });
        adsr.noteOn();
        gain = 0.6f * (0.35f + 0.65f * velocity);

        const float pan = 0.5f + 0.28f * std::sin ((float) midiNote * 0.9f);
        panL = std::cos (pan * juce::MathConstants<float>::halfPi);
        panR = std::sin (pan * juce::MathConstants<float>::halfPi);
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            adsr.noteOff();
        }
        else
        {
            adsr.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int value) override
    {
        bendSemitones = pitchWheelToSemitones (value);
        core.setFrequency (currentHz(), false);
    }

    void controllerMoved (int controller, int value) override
    {
        if (controller == 1)
            shared.modWheel = (float) value / 127.0f;
    }

    using juce::SynthesiserVoice::renderNextBlock;

    void renderNextBlock (juce::AudioBuffer<float>& buffer, int start, int num) override
    {
        if (! isVoiceActive())
            return;

        auto params = shared.core;
        if (shared.modWheel > 0.01f)
            params.harmonic = std::round (4.0f + 12.0f * shared.modWheel);

        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = start; i < start + num; ++i)
        {
            const float s = core.process (params) * adsr.getNextSample() * gain;
            l[i] += s * panL;
            if (r != nullptr) r[i] += s * panR;

            if (! adsr.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    static float pitchWheelToSemitones (int value) { return 2.0f * ((float) value - 8192.0f) / 8192.0f; }
    float currentHz() const { return midiToHz ((float) note + bendSemitones); }

    SynthShared& shared;
    ThroatCore core;
    juce::ADSR adsr;
    int note = 60;
    float bendSemitones = 0, gain = 1, panL = 0.707f, panR = 0.707f;
};

} // namespace tengri
