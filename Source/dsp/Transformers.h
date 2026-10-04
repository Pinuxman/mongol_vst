#pragma once

#include "DspCommon.h"

namespace tengri
{
/** Shared analysis of the incoming signal, computed once per sample. */
struct Analysis
{
    float f0      = 110.0f; // smoothed fundamental (held when unvoiced)
    float env     = 0.0f;   // amplitude envelope
    float voicing = 0.0f;   // smoothed 0..1 "is there a pitch"
};

//==============================================================================
/**
    VOICE mode: turns any voice into khöömei / kargyraa / sygyt throat singing.

    - kargyraa : period doubling (AM at f0/2) + sub-octave growl
    - vowel    : U-O-A-E-I formant resonator (mouth shape)
    - overtone : razor-thin band-pass riding on harmonic N of the tracked pitch
                 plus a re-synthesised whistle, like the sygyt "flute" over the drone
    - whistle  : loudness of that whistle on its own (0..200 %)
    - sweep    : melodic movement of the whistle across harmonics 6..12
    - throat   : pressed-larynx saturation
*/
class VoiceTransformer
{
public:
    struct Params
    {
        float kargyraa = 0.5f, overtone = 0.6f, whistle = 1.0f, harmonic = 8.0f, sweep = 0.3f, vowel = 0.3f, throat = 0.3f;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        hp.set (sr, 70.0f, 0.7f);
        harmCoef = onePoleCoef (sr, 60.0f);
        reset();
    }

    void reset()
    {
        hp.reset();
        for (auto& f : formant) f.reset();
        ot1.reset(); ot2.reset();
        halfPhase = whistlePhase = stepTimer = 0;
        currentHarm = 8; stepIndex = 0; counter = 0;
    }

    float process (float x, const Analysis& a, const Params& p) noexcept
    {
        x = hp.highpass (x);

        // pressed throat
        const float drive = 1.0f + 12.0f * p.throat;
        const float xd = lerp (x, fastTanh (x * drive) * 0.45f, p.throat);

        // kargyraa: amplitude modulation at f0/2 creates sub-harmonics f0*(n + 1/2)
        halfPhase = wrap01 (halfPhase + 0.5f * a.f0 / (float) sr);
        const float s = std::sin (kTwoPi * halfPhase);
        const float growled = xd * (1.0f + 0.85f * p.kargyraa * s);
        const float sub = fastTanh (3.0f * s) * a.env * 1.6f * p.kargyraa * a.voicing;

        // overtone melody
        float targetHarm = p.harmonic;
        if (p.sweep > 0.01f)
        {
            static constexpr float offsets[] = { 0, 2, 1, 3, 4, 2, 1, -1, 0, 2, 4, 1 };
            stepTimer += (0.25f + 4.0f * p.sweep * p.sweep) / (float) sr;
            if (stepTimer >= 1.0f)
            {
                stepTimer -= 1.0f;
                stepIndex = (stepIndex + 1) % 12;
            }
            targetHarm += std::round (offsets[stepIndex] * std::min (1.0f, p.sweep * 2.5f));
        }
        targetHarm = std::max (targetHarm, 3.0f);
        currentHarm += harmCoef * (targetHarm - currentHarm);

        if (--counter <= 0)
            updateFilters (a.f0, p);

        float vowelOut = 0;
        for (int k = 0; k < 3; ++k)
            vowelOut += formant[k].bandpass (growled) * formantGain[k];
        const float shaped = 0.35f * growled + 1.5f * vowelOut;

        const float rich = growled + 0.4f * fastTanh (8.0f * growled);
        const float ot = ot2.bandpass (ot1.bandpass (rich)) * 9.0f;

        whistlePhase = wrap01 (whistlePhase + harmonicHz / (float) sr);
        const float whistle = std::sin (kTwoPi * whistlePhase) * a.env * 1.4f * a.voicing;

        return 0.26f * (shaped * (1.0f - 0.5f * p.overtone)
                        + (ot + whistle) * p.overtone * p.whistle * 1.2f
                        + sub);
    }

    float getHarmonicHz() const noexcept { return harmonicHz; }

private:
    void updateFilters (float f0, const Params& p) noexcept
    {
        counter = 32;
        const auto fm = vowelFormants (p.vowel);
        for (int k = 0; k < 3; ++k)
        {
            formant[k].set (sr, fm.f[k], 5.0f + 2.0f * (float) k);
            formantGain[k] = fm.g[k];
        }

        harmonicHz = currentHarm * f0;
        while (harmonicHz > 0.4f * (float) sr && harmonicHz > f0)
            harmonicHz -= f0;
        ot1.set (sr, harmonicHz, 18.0f);
        ot2.set (sr, harmonicHz, 18.0f);
    }

    double sr = 44100;
    Svf hp, formant[3], ot1, ot2;
    float formantGain[3] { 1, 0.6f, 0.3f };
    float halfPhase = 0, whistlePhase = 0, stepTimer = 0;
    float currentHarm = 8, harmCoef = 0.001f, harmonicHz = 880;
    int stepIndex = 0, counter = 0;
};

//==============================================================================
/** Karplus-Strong resonator used for sympathetic strings. */
struct SympatheticString
{
    DelayLine line;
    float lp = 0, period = 100, feedback = 0.99f;
    double sr = 44100;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        line.prepare ((int) (sampleRate / 25.0));
        lp = 0;
    }

    void tune (float hz, float decaySeconds) noexcept
    {
        period   = (float) sr / std::max (hz, 30.0f);
        // per-period gain for a -60 dB decay in decaySeconds
        feedback = std::min (0.9995f, std::pow (0.001f, period / ((float) sr * std::max (decaySeconds, 0.05f))));
    }

    float process (float in) noexcept
    {
        const float y = line.read (period);
        lp += 0.55f * (y - lp);
        line.push (in + lp * feedback);
        return y;
    }
};

//==============================================================================
/**
    STRING mode: turns any plucked/struck/scraped string or bar (guitar, bass,
    violin, a metal ruler on the edge of a desk, a rubber band...) into a
    morin khuur — the horse-head fiddle.

    - body     : trapezoid wooden box resonances
    - sympathy : four Karplus-Strong strings tuned to the root in fourths
    - bow      : horsehair bow re-synthesis following the tracked pitch
    - sustain  : how long the bow keeps singing after the source decays
    - grit     : rosin / horsehair rasp
    - snap     : pull the bow pitch onto the Mongolian pentatonic scale
*/
class StringTransformer
{
public:
    struct Params
    {
        float body = 0.7f, sympathy = 0.5f, bow = 0.5f, sustain = 1.2f, grit = 0.3f, snap = 0.5f;
        int rootPitchClass = 2;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        hp.set (sr, 35.0f, 0.7f);
        noiseLp.set (sr, 3200.0f);
        for (int k = 0; k < numBody; ++k)
            body[k].set (sr, bodyFreq[k], bodyQ[k]);
        for (auto& s : strings) s.prepare (sr);
        attackCoef = onePoleCoef (sr, 45.0f);
        glideCoef  = onePoleCoef (sr, 35.0f);
        reset();
    }

    void reset()
    {
        hp.reset();
        for (auto& b : body) b.reset();
        for (auto& s : strings) s.line.reset();
        osc.reset();
        bowEnv = 0; bowFreq = 110; vibPhase = 0; lastTuneRoot = -1; lastSustain = -1;
    }

    float process (float x, const Analysis& a, const Params& p) noexcept
    {
        x = hp.highpass (x);

        if (p.rootPitchClass != lastTuneRoot || std::abs (p.sustain - lastSustain) > 0.01f)
            retune (p.rootPitchClass, p.sustain);

        // ---- horsehair bow re-synthesis
        const float snapped = snapToPentatonic (a.f0, p.rootPitchClass);
        const float target  = a.f0 * std::pow (snapped / a.f0, p.snap);
        bowFreq += glideCoef * (target - bowFreq);

        const float drive = a.voicing > 0.5f ? std::min (1.0f, a.env * 2.5f) : 0.0f;
        if (drive > bowEnv) bowEnv += attackCoef * (drive - bowEnv);
        else                bowEnv *= releaseMul;

        vibPhase = wrap01 (vibPhase + 5.3f / (float) sr);
        const float vib = 1.0f + 0.0035f * std::sin (kTwoPi * vibPhase) * bowEnv;
        const float saw = osc.process (bowFreq * vib, sr, 0.3f).first;
        const float n = noiseLp.process (noise.next());
        const float bowSig = (saw * (0.85f + 0.5f * p.grit * n) + n * 0.1f * p.grit) * bowEnv * 0.6f;

        // ---- source + bow into the wooden body
        const float s = x * (1.0f - 0.7f * p.bow) + bowSig * p.bow;
        float b = 0.3f * s;
        for (int k = 0; k < numBody; ++k)
            b += body[k].bandpass (s) * bodyGain[k];
        float out = lerp (s, b * 1.3f, p.body);

        // ---- rosin rasp
        const float rasp = fastTanh (out * (1.0f + 7.0f * p.grit) + 0.15f * p.grit) - fastTanh (0.15f * p.grit);
        out = lerp (out, rasp * 0.6f, p.grit * 0.6f);

        // ---- sympathetic strings
        const float excite = (x + bowSig * 0.3f * p.bow) * 0.2f;
        float sym = 0;
        for (auto& str : strings)
            sym += str.process (excite);

        return 0.42f * (out + sym * p.sympathy * 0.8f);
    }

    float getBowHz() const noexcept { return bowFreq; }

private:
    void retune (int root, float sustain) noexcept
    {
        lastTuneRoot = root;
        lastSustain  = sustain;
        releaseMul   = std::exp (-1.0f / (std::max (sustain, 0.02f) * (float) sr));

        // morin khuur strings are a fourth apart; add octaves for shimmer
        static constexpr int intervals[numStrings] = { 0, 5, 12, 17 };
        for (int k = 0; k < numStrings; ++k)
            strings[k].tune (midiToHz ((float) (36 + root + intervals[k])), 0.8f + 1.4f * sustain);
    }

    static constexpr int numBody = 5, numStrings = 4;
    static constexpr float bodyFreq[numBody] = { 125.0f, 290.0f, 560.0f, 1250.0f, 2900.0f };
    static constexpr float bodyQ[numBody]    = { 2.0f, 3.0f, 4.0f, 3.0f, 2.0f };
    static constexpr float bodyGain[numBody] = { 0.55f, 1.0f, 0.8f, 0.45f, 0.22f };

    double sr = 44100;
    Svf hp, body[numBody];
    SympatheticString strings[numStrings];
    BlepOsc osc;
    Noise noise { 0xBEEF };
    OnePoleLP noiseLp;
    float bowEnv = 0, bowFreq = 110, vibPhase = 0;
    float attackCoef = 0.01f, glideCoef = 0.01f, releaseMul = 0.9999f;
    int lastTuneRoot = -1;
    float lastSustain = -1;
};

} // namespace tengri
