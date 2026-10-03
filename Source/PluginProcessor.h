#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "SynthVoice.h"
#include "dsp/PitchTracker.h"
#include "dsp/Spirit.h"
#include "dsp/ThroatCore.h"
#include "dsp/Transformers.h"

#ifndef JucePlugin_Name
 #define JucePlugin_Name "TENGRI"
#endif
#ifndef JucePlugin_IsSynth
 #define JucePlugin_IsSynth 0
#endif

class TengriProcessor : public juce::AudioProcessor
{
public:
    TengriProcessor();
    ~TengriProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    //==============================================================================
    /** Lock-free telemetry for the editor. */
    struct Telemetry
    {
        std::atomic<float> pitchHz { 0 }, voicing { 0 }, harmonicHz { 0 }, bowHz { 0 };
        std::atomic<float> inLevel { 0 }, outLevel { 0 };
        std::atomic<int> drumHits { 0 };
    } telemetry;

    static constexpr int scopeSize = 4096;
    /** Copies the most recent `num` output samples (mono) into dest. */
    void readScope (float* dest, int num) const noexcept;

private:
    static BusesProperties makeBuses();
    void updateSharedSynthParams();

    struct Raw
    {
        std::atomic<float>* mode; std::atomic<float>* mix; std::atomic<float>* output; std::atomic<float>* root; std::atomic<float>* drone;
        std::atomic<float>* kargyraa; std::atomic<float>* overtone; std::atomic<float>* harmonic; std::atomic<float>* sweep; std::atomic<float>* vowel; std::atomic<float>* throat;
        std::atomic<float>* body; std::atomic<float>* sympathy; std::atomic<float>* bow; std::atomic<float>* sustain; std::atomic<float>* grit; std::atomic<float>* snap;
        std::atomic<float>* drum; std::atomic<float>* pulse; std::atomic<float>* sync; std::atomic<float>* space; std::atomic<float>* echo;
        std::atomic<float>* synthLevel; std::atomic<float>* synthTimbre; std::atomic<float>* synthSub; std::atomic<float>* synthOvertone; std::atomic<float>* synthAttack; std::atomic<float>* synthRelease;
    } raw {};

    double sr = 44100;

    tengri::PitchTracker tracker;
    tengri::EnvelopeFollower envFollower;
    tengri::Analysis analysis;
    float voicingCoef = 0.001f, pitchCoef = 0.01f;

    tengri::VoiceTransformer voiceFx;
    tengri::StringTransformer stringFx;
    tengri::ThroatCore droneCore { 0xA11u };
    tengri::ShamanDrum drum;
    tengri::SteppeEcho echo;
    juce::Reverb reverb;

    juce::Synthesiser synth;
    tengri::SynthShared synthShared;

    juce::SmoothedValue<float> mixSmooth, gainSmooth, modeSmooth, droneSmooth, drumSmooth, synthLevelSmooth;

    juce::AudioBuffer<float> synthBuffer, wetBuffer;

    std::array<float, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TengriProcessor)
};
