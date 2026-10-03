#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace tengri
{
constexpr float kPi    = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float midiToHz (float note) noexcept { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }
inline float hzToMidi (float hz) noexcept   { return 69.0f + 12.0f * std::log2 (std::max (hz, 1.0f) / 440.0f); }

/** Pade tanh approximation, accurate enough for saturation and cheap. */
inline float fastTanh (float x) noexcept
{
    x = std::clamp (x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

/** Coefficient for a one-pole smoother reaching ~63% in `ms` milliseconds. */
inline float onePoleCoef (double sampleRate, float ms) noexcept
{
    return 1.0f - std::exp (-1.0f / (std::max (ms, 0.01f) * 0.001f * (float) sampleRate));
}

inline float wrap01 (float p) noexcept { return p - std::floor (p); }

//==============================================================================
/** Zavalishin / Simper TPT state-variable filter. */
struct Svf
{
    float a1 = 0, a2 = 0, a3 = 0, k = 1;
    float ic1 = 0, ic2 = 0;

    void set (double sampleRate, float freq, float q) noexcept
    {
        freq = std::clamp (freq, 10.0f, 0.47f * (float) sampleRate);
        const float g = std::tan (kPi * freq / (float) sampleRate);
        k  = 1.0f / std::max (q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() noexcept { ic1 = ic2 = 0; }

    struct Out { float lp, bp, hp; };

    Out process (float v0) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, v1, v0 - k * v1 - v2 };
    }

    /** Band-pass normalised to unity gain at the centre frequency. */
    float bandpass (float x) noexcept { return k * process (x).bp; }
    float lowpass  (float x) noexcept { return process (x).lp; }
    float highpass (float x) noexcept { return process (x).hp; }
};

//==============================================================================
struct OnePoleLP
{
    float a = 1, z = 0;
    void set (double sampleRate, float freq) noexcept
    {
        a = 1.0f - std::exp (-kTwoPi * std::min (freq, 0.49f * (float) sampleRate) / (float) sampleRate);
    }
    float process (float x) noexcept { z += a * (x - z); return z; }
    void reset() noexcept { z = 0; }
};

//==============================================================================
struct EnvelopeFollower
{
    float att = 0, rel = 0, env = 0;
    void set (double sampleRate, float attackMs, float releaseMs) noexcept
    {
        att = onePoleCoef (sampleRate, attackMs);
        rel = onePoleCoef (sampleRate, releaseMs);
    }
    float process (float x) noexcept
    {
        const float a = std::abs (x);
        env += (a > env ? att : rel) * (a - env);
        return env;
    }
    void reset() noexcept { env = 0; }
};

//==============================================================================
struct Noise
{
    uint32_t state = 0x1234567u;
    explicit Noise (uint32_t seed = 0x1234567u) : state (seed) {}
    float next() noexcept
    {
        state = state * 1664525u + 1013904223u;
        return (float) (int32_t) state * (1.0f / 2147483648.0f);
    }
};

//==============================================================================
inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

/** Band-limited sawtooth (and narrow pulse, derived from two saws). */
struct BlepOsc
{
    float phase = 0;

    void reset (float p = 0) noexcept { phase = p; }

    /** Advances and returns {saw, pulse}. Pulse width in 0..1. */
    std::pair<float, float> process (float freq, double sampleRate, float pulseWidth) noexcept
    {
        const float dt = std::clamp (freq / (float) sampleRate, 0.0f, 0.45f);
        float saw = 2.0f * phase - 1.0f - polyBlep (phase, dt);

        float p2 = phase + pulseWidth;
        if (p2 >= 1.0f) p2 -= 1.0f;
        float saw2 = 2.0f * p2 - 1.0f - polyBlep (p2, dt);

        phase += dt;
        if (phase >= 1.0f) phase -= 1.0f;

        return { saw, saw - saw2 };
    }
};

//==============================================================================
struct DelayLine
{
    std::vector<float> buffer;
    int writePos = 0;

    void prepare (int maxSamples)
    {
        buffer.assign ((size_t) std::max (maxSamples + 4, 8), 0.0f);
        writePos = 0;
    }
    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    void push (float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        if (++writePos >= (int) buffer.size()) writePos = 0;
    }

    /** Reads `delay` samples behind the most recently pushed sample. */
    float read (float delay) noexcept
    {
        const int size = (int) buffer.size();
        delay = std::clamp (delay, 1.0f, (float) (size - 3));
        float pos = (float) writePos - delay;
        while (pos < 0) pos += (float) size;
        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % size;
        const float frac = pos - (float) i0;
        return buffer[(size_t) i0] + frac * (buffer[(size_t) i1] - buffer[(size_t) i0]);
    }
};

//==============================================================================
/** Mongolian (anhemitonic) pentatonic: do re mi so la. */
inline float snapToPentatonic (float hz, int rootPitchClass) noexcept
{
    static constexpr float degrees[] = { 0, 2, 4, 7, 9, 12 };
    const float m   = hzToMidi (hz) - (float) rootPitchClass;
    const float oct = std::floor (m / 12.0f);
    const float pc  = m - 12.0f * oct;

    float best = 0, bestDist = 100;
    for (float d : degrees)
    {
        const float dist = std::abs (pc - d);
        if (dist < bestDist) { bestDist = dist; best = d; }
    }
    return midiToHz ((float) rootPitchClass + 12.0f * oct + best);
}

//==============================================================================
/** Vowel formant table used by the throat-singing models (U -> O -> A -> E -> I). */
struct Formants { float f[3]; float g[3]; };

inline Formants vowelFormants (float v) noexcept
{
    static constexpr Formants table[] = {
        { { 325, 700, 2530 },  { 1.0f, 0.55f, 0.20f } }, // U
        { { 450, 800, 2830 },  { 1.0f, 0.70f, 0.22f } }, // O
        { { 700, 1220, 2600 }, { 1.0f, 0.60f, 0.30f } }, // A
        { { 550, 1770, 2490 }, { 1.0f, 0.55f, 0.35f } }, // E
        { { 300, 2300, 3000 }, { 1.0f, 0.40f, 0.35f } }, // I
    };
    v = std::clamp (v, 0.0f, 1.0f) * 4.0f;
    const int i  = std::min ((int) v, 3);
    const float t = v - (float) i;
    Formants out {};
    for (int k = 0; k < 3; ++k)
    {
        out.f[k] = lerp (table[i].f[k], table[i + 1].f[k], t);
        out.g[k] = lerp (table[i].g[k], table[i + 1].g[k], t);
    }
    return out;
}

} // namespace tengri
