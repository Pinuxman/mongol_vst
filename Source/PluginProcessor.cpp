#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessor::BusesProperties TengriProcessor::makeBuses()
{
    // The instrument build keeps an (optional) input so hosts that allow it can
    // feed audio into the transformer as well; the FX build requires one.
    return juce::AudioProcessor::BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), ! JucePlugin_IsSynth)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
}

namespace
{
inline float safetyClip (float x) noexcept
{
    const float a = std::abs (x);
    if (a < 0.8f)
        return x;
    return std::copysign (0.8f + 0.2f * tengri::fastTanh ((a - 0.8f) * 5.0f), x);
}
} // namespace

TengriProcessor::TengriProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "TENGRI", tengri::createParameterLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    using namespace tengri;
    raw = { get (pid::mode), get (pid::mix), get (pid::output), get (pid::root), get (pid::drone),
            get (pid::kargyraa), get (pid::overtone), get (pid::harmonic), get (pid::sweep), get (pid::vowel), get (pid::throat),
            get (pid::body), get (pid::sympathy), get (pid::bow), get (pid::sustain), get (pid::grit), get (pid::snap),
            get (pid::drum), get (pid::pulse), get (pid::sync), get (pid::space), get (pid::echo),
            get (pid::synthLevel), get (pid::synthTimbre), get (pid::synthSub), get (pid::synthOvertone), get (pid::synthAttack), get (pid::synthRelease),
            get (pid::mute), get (pid::whistle) };

    for (uint32_t i = 0; i < 8; ++i)
        synth.addVoice (new TengriVoice (synthShared, 1000u + i * 77u));
    synth.addSound (new TengriSound());
}

bool TengriProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void TengriProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    tracker.prepare (sr);
    envFollower.set (sr, 3.0f, 90.0f);
    envFollower.reset();
    voicingCoef = tengri::onePoleCoef (sr, 40.0f);
    pitchCoef   = tengri::onePoleCoef (sr, 12.0f);
    analysis = {};

    voiceFx.prepare (sr);
    stringFx.prepare (sr);
    droneCore.prepare (sr);
    drum.prepare (sr);
    echo.prepare (sr);
    reverb.setSampleRate (sr);
    reverb.reset();

    synth.setCurrentPlaybackSampleRate (sr);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<tengri::TengriVoice*> (synth.getVoice (i)))
            v->prepare (sr);

    auto initSmooth = [this] (juce::SmoothedValue<float>& s, float value, double seconds)
    {
        s.reset (sr, seconds);
        s.setCurrentAndTargetValue (value);
    };
    initSmooth (mixSmooth, raw.mix->load(), 0.05);
    initSmooth (gainSmooth, juce::Decibels::decibelsToGain (raw.output->load()), 0.05);
    initSmooth (muteSmooth, raw.mute->load() > 0.5f ? 0.0f : 1.0f, 0.02);
    initSmooth (modeSmooth, raw.mode->load() > 0.5f ? 1.0f : 0.0f, 0.04);
    initSmooth (droneSmooth, raw.drone->load(), 0.3);
    initSmooth (drumSmooth, raw.drum->load(), 0.05);
    initSmooth (synthLevelSmooth, raw.synthLevel->load(), 0.05);

    synthBuffer.setSize (2, samplesPerBlock);
    wetBuffer.setSize (2, samplesPerBlock);
}

void TengriProcessor::updateSharedSynthParams()
{
    synthShared.core.timbre   = raw.synthTimbre->load();
    synthShared.core.sub      = raw.synthSub->load();
    synthShared.core.overtone = raw.synthOvertone->load();
    synthShared.core.harmonic = -1.0f;
    synthShared.core.vowel    = raw.vowel->load();
    synthShared.attack        = raw.synthAttack->load();
    synthShared.release       = raw.synthRelease->load();
}

void TengriProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int n      = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    if (n == 0 || numOut == 0)
        return;

    if (synthBuffer.getNumSamples() < n)
    {
        synthBuffer.setSize (2, n, false, false, true);
        wetBuffer.setSize (2, n, false, false, true);
    }

    // ---- synth (MIDI)
    updateSharedSynthParams();
    synthBuffer.clear (0, n);
    synth.renderNextBlock (synthBuffer, midi, 0, n);

    // ---- parameters
    const int rootPc = juce::jlimit (0, 11, (int) raw.root->load());

    tengri::VoiceTransformer::Params vp;
    vp.kargyraa = raw.kargyraa->load();
    vp.overtone = raw.overtone->load();
    vp.whistle  = raw.whistle->load();
    vp.harmonic = raw.harmonic->load();
    vp.sweep    = raw.sweep->load();
    vp.vowel    = raw.vowel->load();
    vp.throat   = raw.throat->load();

    tengri::StringTransformer::Params spar;
    spar.body     = raw.body->load();
    spar.sympathy = raw.sympathy->load();
    spar.bow      = raw.bow->load();
    spar.sustain  = raw.sustain->load();
    spar.grit     = raw.grit->load();
    spar.snap     = raw.snap->load();
    spar.rootPitchClass = rootPc;

    mixSmooth.setTargetValue (raw.mix->load());
    gainSmooth.setTargetValue (juce::Decibels::decibelsToGain (raw.output->load()));
    muteSmooth.setTargetValue (raw.mute->load() > 0.5f ? 0.0f : 1.0f);
    modeSmooth.setTargetValue (raw.mode->load() > 0.5f ? 1.0f : 0.0f);
    droneSmooth.setTargetValue (raw.drone->load());
    synthLevelSmooth.setTargetValue (raw.synthLevel->load());

    // ---- shaman drum timing
    float bpm = raw.pulse->load();
    double ppq = -1.0;
    const bool wantSync = raw.sync->load() > 0.5f;
    bool hostPlaying = true;
    if (wantSync)
    {
        if (auto* ph = getPlayHead())
        {
            if (auto pos = ph->getPosition())
            {
                if (auto b = pos->getBpm())         bpm = (float) *b;
                if (auto p = pos->getPpqPosition()) ppq = *p;
                hostPlaying = pos->getIsPlaying();
            }
        }
    }
    drum.setTiming (bpm, hostPlaying ? ppq : -1.0, wantSync && hostPlaying);
    drumSmooth.setTargetValue (wantSync && ! hostPlaying ? 0.0f : raw.drum->load());

    // ---- drone
    tengri::ThroatCore::Params droneParams;
    droneParams.sub      = 0.3f;
    droneParams.overtone = 0.55f;
    droneParams.vowel    = vp.vowel;
    droneParams.whistle  = vp.whistle;
    droneCore.setFrequency (tengri::midiToHz ((float) (36 + rootPc)), false);

    const float echoAmount = raw.echo->load();
    const float space      = raw.space->load();

    const float* inL = numIn > 0 ? buffer.getReadPointer (0) : nullptr;
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : inL;
    auto* wetL = wetBuffer.getWritePointer (0);
    auto* wetR = wetBuffer.getWritePointer (1);
    const auto* synL = synthBuffer.getReadPointer (0);
    const auto* synR = synthBuffer.getReadPointer (1);

    // dry signal is parked in the output channels while the wet bus is built
    auto* outL = buffer.getWritePointer (0);
    auto* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;

    float inPeak = 0;
    for (int i = 0; i < n; ++i)
    {
        const float l = inL != nullptr ? inL[i] : 0.0f;
        const float r = inR != nullptr ? inR[i] : l;
        const float mono = 0.5f * (l + r);
        inPeak = std::max (inPeak, std::abs (mono));

        // analysis
        tracker.push (mono);
        analysis.env = envFollower.process (mono);
        const bool voiced = tracker.isVoiced();
        analysis.voicing += voicingCoef * ((voiced ? 1.0f : 0.0f) - analysis.voicing);
        if (voiced)
            analysis.f0 += pitchCoef * (tracker.getFrequency() - analysis.f0);

        // transformers (both run so switching modes is seamless)
        const float vOut = voiceFx.process (mono, analysis, vp);
        const float sOut = stringFx.process (mono, analysis, spar);
        const float m    = modeSmooth.getNextValue();
        const float transformed = tengri::lerp (vOut, sOut, m);

        // drone follows the mode: throat in VOICE, horse-head fiddle in STRING
        float droneOut = 0;
        const float droneLevel = droneSmooth.getNextValue();
        if (droneLevel > 0.0001f)
        {
            droneParams.timbre = m;
            droneOut = droneCore.process (droneParams) * droneLevel * 0.45f;
        }

        float dl = 0, dr = 0;
        if (drum.process (drumSmooth.getNextValue(), dl, dr))
            telemetry.drumHits.fetch_add (1, std::memory_order_relaxed);

        const float mixV  = mixSmooth.getNextValue();
        const float synLv = synthLevelSmooth.getNextValue();

        float wl = transformed * mixV + synL[i] * synLv + droneOut + dl;
        float wr = transformed * mixV + synR[i] * synLv + droneOut + dr;
        echo.process (wl, wr, echoAmount);
        wetL[i] = wl;
        wetR[i] = wr;

        outL[i] = l * (1.0f - mixV);
        if (outR != nullptr)
            outR[i] = r * (1.0f - mixV);
    }

    // steppe / cave reverb
    juce::Reverb::Parameters rp;
    rp.roomSize   = 0.55f + 0.42f * space;
    rp.damping    = 0.45f;
    rp.wetLevel   = 0.55f * space;
    rp.dryLevel   = 1.0f;
    rp.width      = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);
    reverb.processStereo (wetL, wetR, n);

    float outPeak = 0;
    int w = scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        const float g = gainSmooth.getNextValue() * muteSmooth.getNextValue();
        float l = safetyClip ((outL[i] + wetL[i]) * g);
        float r = safetyClip (((outR != nullptr ? outR[i] : outL[i]) + wetR[i]) * g);

        if (outR != nullptr)
        {
            outL[i] = l;
            outR[i] = r;
        }
        else
        {
            outL[i] = 0.5f * (l + r);
        }

        const float mono = 0.5f * (l + r);
        outPeak = std::max (outPeak, std::abs (mono));
        scope[(size_t) w] = mono;
        w = (w + 1) & (scopeSize - 1);
    }
    scopeWrite.store (w, std::memory_order_release);

    for (int ch = 2; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    telemetry.inLevel.store (inPeak);
    telemetry.outLevel.store (outPeak);
    telemetry.voicing.store (analysis.voicing);
    telemetry.pitchHz.store (analysis.voicing > 0.5f ? analysis.f0 : 0.0f);
    telemetry.harmonicHz.store (voiceFx.getHarmonicHz());
    telemetry.bowHz.store (stringFx.getBowHz());
}

void TengriProcessor::readScope (float* dest, int num) const noexcept
{
    num = std::min (num, scopeSize);
    int r = (scopeWrite.load (std::memory_order_acquire) - num) & (scopeSize - 1);
    for (int i = 0; i < num; ++i)
    {
        dest[i] = scope[(size_t) r];
        r = (r + 1) & (scopeSize - 1);
    }
}

int TengriProcessor::getCurrentProgram()
{
    return juce::jlimit (0, getNumPrograms() - 1, (int) apvts.state.getProperty ("preset", 0));
}

void TengriProcessor::setCurrentProgram (int index)
{
    const auto& presets = tengri::factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;
    apvts.state.setProperty ("preset", index, nullptr);
    tengri::applyPreset (apvts, presets[(size_t) index]);
}

const juce::String TengriProcessor::getProgramName (int index)
{
    const auto& presets = tengri::factoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? juce::String (presets[(size_t) index].name) : juce::String();
}

juce::AudioProcessorEditor* TengriProcessor::createEditor()
{
    return new TengriEditor (*this);
}

void TengriProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TengriProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TengriProcessor();
}
