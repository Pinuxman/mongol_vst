#pragma once

#include "DspCommon.h"

namespace tengri
{
/**
    Khets — the shaman's frame drum, with the iron pendants (jingles) of the
    shaman's costume. Plays a steady trance pulse with an accent on the
    first of every pair of beats ("DUM-dum, DUM-dum").
*/
class ShamanDrum
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        headBp.set (sr, 190.0f, 1.3f);
        slapHp.set (sr, 1100.0f, 0.8f);
        jingleHpL.set (sr, 5500.0f, 0.9f);
        jingleHpR.set (sr, 6500.0f, 0.9f);
        reset();
    }

    void reset()
    {
        t = 10.0f; amp = 0; phase = 0; beatPhase = 0; lastBeat = -1; beatCount = 0;
        headBp.reset(); slapHp.reset(); jingleHpL.reset(); jingleHpR.reset();
    }

    /** Call once per block; ppq < 0 means "free-running". */
    void setTiming (float bpm, double ppqAtBlockStart, bool synced) noexcept
    {
        tempo = std::clamp (bpm, 20.0f, 400.0f);
        if (synced && ppqAtBlockStart >= 0.0)
            beatPhase = ppqAtBlockStart;
        isSynced = synced;
    }

    /** Returns true on the sample a hit is triggered. */
    bool process (float level, float& outL, float& outR) noexcept
    {
        bool hit = false;
        beatPhase += (double) tempo / 60.0 / sr;
        const auto beat = (int64_t) std::floor (beatPhase);
        if (beat != lastBeat)
        {
            if (lastBeat >= 0 || ! isSynced)
            {
                trigger ((beatCount++ % 2 == 0) ? 1.0f : 0.62f);
                hit = level > 0.001f;
            }
            lastBeat = beat;
        }

        if (t > 2.0f || level <= 0.0001f)
        {
            t += 1.0f / (float) sr;
            outL = outR = 0;
            return hit;
        }

        t += 1.0f / (float) sr;
        const float pitch = 46.0f + 75.0f * std::exp (-t / 0.035f);
        phase = wrap01 (phase + pitch / (float) sr);
        const float boom  = std::sin (kTwoPi * phase) * std::exp (-t / 0.38f);
        const float head  = headBp.bandpass (noise.next()) * std::exp (-t / 0.06f) * 1.8f;
        const float slap  = slapHp.highpass (noise.next()) * std::exp (-t / 0.012f) * 0.35f;

        const float rattle = 0.55f + 0.45f * std::sin (kTwoPi * 23.0f * t);
        const float jEnv   = std::exp (-t / 0.2f) * rattle * 0.22f;
        const float jl = jingleHpL.highpass (noise.next()) * jEnv;
        const float jr = jingleHpR.highpass (noise.next()) * jEnv;

        const float mono = (boom * 0.9f + head + slap) * amp * level * 0.25f;
        outL = mono + jl * amp * level * 0.6f;
        outR = mono + jr * amp * level * 0.6f;
        return hit;
    }

private:
    void trigger (float velocity) noexcept
    {
        t = 0; amp = velocity;
    }

    double sr = 44100, beatPhase = 0;
    int64_t lastBeat = -1, beatCount = 0;
    float tempo = 120, t = 10, amp = 0, phase = 0;
    bool isSynced = false;
    Noise noise { 0xD00D };
    Svf headBp, slapHp, jingleHpL, jingleHpR;
};

//==============================================================================
/** Ping-pong echo across the steppe, darkening with every repeat. */
class SteppeEcho
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        left.prepare ((int) (sampleRate * 1.2));
        right.prepare ((int) (sampleRate * 1.2));
        lpL.set (sr, 3200.0f);
        lpR.set (sr, 2600.0f);
        reset();
    }

    void reset()
    {
        left.reset(); right.reset(); lpL.reset(); lpR.reset();
    }

    void process (float& l, float& r, float amount) noexcept
    {
        const float dl = left.read ((float) sr * 0.375f);
        const float dr = right.read ((float) sr * 0.5f);
        const float fb = 0.2f + 0.45f * amount;
        left.push (l + lpL.process (dr) * fb);
        right.push (r + lpR.process (dl) * fb);
        l += dl * amount * 0.8f;
        r += dr * amount * 0.8f;
    }

private:
    double sr = 44100;
    DelayLine left, right;
    OnePoleLP lpL, lpR;
};

} // namespace tengri
