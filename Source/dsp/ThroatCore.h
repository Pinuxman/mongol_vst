#pragma once

#include "DspCommon.h"

namespace tengri
{
/**
    Monophonic sound source shared by the synth voices and the drone.

    timbre = 0 : khöömei throat voice (glottal pulse -> vowel formants -> whistling overtone)
    timbre = 1 : morin khuur (bowed horsehair saw -> trapezoid wooden body)
*/
class ThroatCore
{
public:
    struct Params
    {
        float timbre   = 0.0f; // 0 throat .. 1 horse-head fiddle
        float sub      = 0.4f; // kargyraa sub-octave growl
        float overtone = 0.6f; // sygyt whistle amount
        float harmonic = -1.0f; // fixed harmonic number, or < 0 for a wandering melody
        float vowel    = 0.25f;
    };

    explicit ThroatCore (uint32_t seed = 1) : noise (seed), rng (seed * 7919u + 13u) {}

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        glideCoef  = onePoleCoef (sr, 25.0f);
        harmCoef   = onePoleCoef (sr, 70.0f);
        noiseLp.set (sr, 2500.0f);
        for (int k = 0; k < numBody; ++k)
            body[k].set (sr, bodyFreq[k], bodyQ[k]);
        reset();
    }

    void reset()
    {
        osc.reset (0.0f);
        subPhase = whistlePhase = 0;
        for (auto& f : formant) f.reset();
        for (auto& f : body)    f.reset();
        over1.reset(); over2.reset();
        currentHarm = targetHarm = 8.0f;
        wanderTimer = 0;
        controlCounter = 0;
    }

    void setFrequency (float hz, bool immediate) noexcept
    {
        targetFreq = hz;
        if (immediate || freq <= 0.0f)
            freq = hz;
    }

    float process (const Params& p) noexcept
    {
        freq += glideCoef * (targetFreq - freq);

        // wandering overtone melody (the khöömei "tune" played on harmonics 6..12)
        if (p.harmonic >= 0.0f)
        {
            targetHarm = p.harmonic;
        }
        else
        {
            wanderTimer -= 1.0f / (float) sr;
            if (wanderTimer <= 0.0f)
            {
                static constexpr float melody[] = { 6, 8, 9, 10, 12, 10, 9, 8, 6, 8, 10, 9 };
                wanderIndex = (wanderIndex + 1 + (int) (rng.next() > 0.6f)) % 12;
                targetHarm  = melody[wanderIndex];
                wanderTimer = 0.45f + 0.9f * (0.5f + 0.5f * rng.next());
            }
        }
        currentHarm += harmCoef * (targetHarm - currentHarm);

        // vibrato (stronger for the bowed fiddle)
        vibPhase = wrap01 (vibPhase + 5.3f / (float) sr);
        const float vib = 1.0f + (0.0012f + 0.0040f * p.timbre) * std::sin (kTwoPi * vibPhase);
        const float f = freq * vib;

        if (--controlCounter <= 0)
            updateFilters (p, f);

        // glottal source: saw + narrow pulse, rich in upper harmonics
        auto [saw, pulse] = osc.process (f, sr, 0.12f);
        const float breath = noiseLp.process (noise.next());

        // kargyraa: period-doubling at f/2 (sub-harmonics between every partial)
        subPhase = wrap01 (subPhase + 0.5f * f / (float) sr);
        const float subSin = std::sin (kTwoPi * subPhase);
        const float growl  = 1.0f + 0.75f * p.sub * subSin;
        const float glottal = (0.45f * saw + 0.9f * pulse) * growl + 0.03f * breath;

        // --- throat path -----------------------------------------------------
        float vowelOut = 0;
        for (int k = 0; k < 3; ++k)
            vowelOut += formant[k].bandpass (glottal) * formantGain[k];

        const float ot = over2.bandpass (over1.bandpass (glottal)) * 7.0f;
        whistlePhase = wrap01 (whistlePhase + currentHarm * f / (float) sr);
        const float whistle = std::sin (kTwoPi * whistlePhase);

        const float throat = vowelOut * (1.1f - 0.5f * p.overtone)
                           + (ot + 0.12f * whistle) * p.overtone;

        // --- morin khuur path --------------------------------------------------
        const float bowSaw = saw * (0.9f + 0.25f * breath) + 0.05f * breath;
        float bodyOut = 0.25f * bowSaw;
        for (int k = 0; k < numBody; ++k)
            bodyOut += body[k].bandpass (bowSaw) * bodyGain[k];

        const float sub = fastTanh (2.5f * subSin) * 0.55f * p.sub;

        return 0.5f * lerp (throat, bodyOut * 0.9f, p.timbre) + 0.35f * sub;
    }

    float getHarmonicHz() const noexcept { return currentHarm * freq; }

private:
    void updateFilters (const Params& p, float f) noexcept
    {
        controlCounter = 32;

        const auto fm = vowelFormants (p.vowel);
        for (int k = 0; k < 3; ++k)
        {
            formant[k].set (sr, fm.f[k], 6.0f + 2.0f * (float) k);
            formantGain[k] = fm.g[k];
        }

        float centre = currentHarm * f;
        while (centre > 0.4f * (float) sr && centre > f)
            centre -= f;
        over1.set (sr, centre, 22.0f);
        over2.set (sr, centre, 22.0f);
    }

    static constexpr int numBody = 5;
    static constexpr float bodyFreq[numBody] = { 125.0f, 290.0f, 560.0f, 1250.0f, 2900.0f };
    static constexpr float bodyQ[numBody]    = { 2.0f, 3.0f, 4.0f, 3.0f, 2.0f };
    static constexpr float bodyGain[numBody] = { 0.55f, 1.0f, 0.8f, 0.45f, 0.22f };

    double sr = 44100;
    float freq = 110, targetFreq = 110, glideCoef = 0.01f;
    float subPhase = 0, whistlePhase = 0, vibPhase = 0;
    float currentHarm = 8, targetHarm = 8, harmCoef = 0.001f, wanderTimer = 0;
    int wanderIndex = 0, controlCounter = 0;

    BlepOsc osc;
    Noise noise, rng;
    OnePoleLP noiseLp;
    Svf formant[3], over1, over2;
    float formantGain[3] { 1, 0.6f, 0.3f };

    Svf body[numBody];
};

} // namespace tengri
