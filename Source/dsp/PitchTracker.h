#pragma once

#include "DspCommon.h"

namespace tengri
{
/**
    Monophonic YIN pitch tracker running on a decimated (~11 kHz) copy of the input.
    Range ~50 Hz .. 1 kHz, which covers voices, guitars, bass, morin khuur and
    the twang of a metal ruler on a table edge.
*/
class PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        decimation = std::max (1, (int) std::round (sampleRate / 11025.0));
        dsr        = (float) (sampleRate / decimation);
        window     = (int) std::round (dsr * 0.040f);   // 40 ms integration window
        tauMax     = (int) (dsr / 50.0f);
        tauMin     = std::max (2, (int) (dsr / 1100.0f));
        size       = window + tauMax + 2;
        hop        = std::max (16, (int) (dsr * 0.006f)); // ~6 ms

        ring.assign ((size_t) size, 0.0f);
        frame.assign ((size_t) size, 0.0f);
        diff.assign ((size_t) tauMax + 2, 0.0f);

        aa1.set (sampleRate, dsr * 0.42f, 0.54f);
        aa2.set (sampleRate, dsr * 0.42f, 1.31f);
        hp.set (sampleRate, 40.0f, 0.7f);
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        aa1.reset(); aa2.reset(); hp.reset();
        writePos = decCount = hopCount = 0;
        frequency = 0; confidence = 0; voiced = false;
    }

    void push (float x) noexcept
    {
        const float y = aa2.lowpass (aa1.lowpass (hp.highpass (x)));
        if (++decCount < decimation)
            return;
        decCount = 0;

        ring[(size_t) writePos] = y;
        if (++writePos >= size) writePos = 0;

        if (++hopCount >= hop)
        {
            hopCount = 0;
            analyse();
        }
    }

    float getFrequency() const noexcept  { return frequency; }
    float getConfidence() const noexcept { return confidence; }
    bool  isVoiced() const noexcept      { return voiced; }

private:
    void analyse() noexcept
    {
        for (int i = 0; i < size; ++i)
            frame[(size_t) i] = ring[(size_t) ((writePos + i) % size)];

        // energy gate on the newest window
        float energy = 0;
        const int start = size - window;
        for (int i = start; i < size; ++i)
            energy += frame[(size_t) i] * frame[(size_t) i];

        if (energy / (float) window < 1.0e-6f) // ~ -60 dBFS
        {
            voiced = false;
            confidence = 0;
            return;
        }

        // YIN difference function
        const float* f = frame.data();
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float sum = 0;
            for (int j = 0; j < window; ++j)
            {
                const float d = f[j] - f[j + tau];
                sum += d * d;
            }
            diff[(size_t) tau] = sum;
        }

        // cumulative mean normalised difference
        diff[0] = 1.0f;
        float running = 0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += diff[(size_t) tau];
            diff[(size_t) tau] = running > 0 ? diff[(size_t) tau] * (float) tau / running : 1.0f;
        }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (diff[(size_t) tau] < threshold)
            {
                while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau])
                    ++tau;
                best = tau;
                break;
            }
        }

        if (best < 0)
        {
            float minVal = 1e9f;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (diff[(size_t) tau] < minVal) { minVal = diff[(size_t) tau]; best = tau; }

            if (minVal > 0.35f)
            {
                voiced = false;
                confidence = 1.0f - std::min (minVal, 1.0f);
                return;
            }
        }

        // parabolic interpolation around the dip
        float t = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float a = diff[(size_t) best - 1], b = diff[(size_t) best], c = diff[(size_t) best + 1];
            const float denom = a - 2.0f * b + c;
            if (std::abs (denom) > 1.0e-9f)
                t += std::clamp (0.5f * (a - c) / denom, -1.0f, 1.0f);
        }

        frequency  = dsr / t;
        confidence = 1.0f - std::clamp (diff[(size_t) best], 0.0f, 1.0f);
        voiced     = true;
    }

    static constexpr float threshold = 0.15f;

    int decimation = 4, window = 441, tauMax = 220, tauMin = 10, size = 664, hop = 64;
    float dsr = 11025;
    std::vector<float> ring, frame, diff;
    int writePos = 0, decCount = 0, hopCount = 0;
    Svf aa1, aa2, hp;

    float frequency = 0, confidence = 0;
    bool voiced = false;
};

} // namespace tengri
