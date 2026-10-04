// Offline render / smoke test: feeds synthetic voice, guitar and metal-ruler
// signals through TENGRI, writes WAVs, prints level / pitch stats and renders
// editor screenshots. Build with -DTENGRI_BUILD_TOOLS=ON.

#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 256;
using Signal = std::vector<float>;

Signal makeVoice (double seconds)
{
    // additive "aaa/ooo" voice: 130 Hz then 146 Hz, vibrato, phrase envelope
    Signal s ((size_t) (seconds * sampleRate));
    double phase = 0;
    for (size_t i = 0; i < s.size(); ++i)
    {
        const double t = (double) i / sampleRate;
        const double f0 = (t < seconds * 0.5 ? 130.0 : 146.0) * (1.0 + 0.006 * std::sin (2 * juce::MathConstants<double>::pi * 5.5 * t));
        phase += f0 / sampleRate;
        double v = 0;
        for (int h = 1; h <= 40; ++h)
        {
            const double fh = h * f0;
            const double formant = 1.0 / (1.0 + std::pow ((fh - 600.0) / 150.0, 2)) + 0.5 / (1.0 + std::pow ((fh - 1000.0) / 200.0, 2))
                                 + 0.15 / (1.0 + std::pow ((fh - 2600.0) / 300.0, 2));
            v += std::sin (2 * juce::MathConstants<double>::pi * h * phase) / h * (0.15 + formant);
        }
        const double env = std::min (1.0, t * 8.0) * std::min (1.0, (seconds - t) * 4.0);
        s[i] = (float) (0.12 * v * env);
    }
    return s;
}

Signal makeGuitar (double seconds)
{
    // Karplus-Strong plucks: A2, D3, E3, A2
    Signal s ((size_t) (seconds * sampleRate), 0.0f);
    const double notes[] = { 110.0, 146.83, 164.81, 110.0 };
    juce::Random rng (42);
    for (int n = 0; n < 4; ++n)
    {
        const size_t start = (size_t) (n * seconds / 4.0 * sampleRate);
        const int period = (int) std::round (sampleRate / notes[n]);
        std::vector<float> line ((size_t) period);
        for (auto& x : line) x = rng.nextFloat() * 2.0f - 1.0f;
        int idx = 0;
        for (size_t i = start; i < s.size(); ++i)
        {
            const int next = (idx + 1) % period;
            const float y = line[(size_t) idx];
            line[(size_t) idx] = 0.996f * 0.5f * (line[(size_t) idx] + line[(size_t) next]);
            idx = next;
            s[i] += 0.35f * y;
        }
    }
    return s;
}

Signal makeRuler (double seconds)
{
    // metal ruler twanged on a desk: buzzy, falling pitch, fast decay
    Signal s ((size_t) (seconds * sampleRate), 0.0f);
    for (int hit = 0; hit < (int) (seconds / 1.2); ++hit)
    {
        const size_t start = (size_t) (hit * 1.2 * sampleRate);
        double phase = 0;
        const double base = 200.0 + 40.0 * hit;
        for (size_t i = start; i < s.size(); ++i)
        {
            const double t = (double) (i - start) / sampleRate;
            const double f = base * (1.0 + 0.25 * std::exp (-t / 0.08));
            phase += f / sampleRate;
            const double x = std::sin (2 * juce::MathConstants<double>::pi * phase);
            const double buzz = std::tanh (4.0 * x) * 0.6 + 0.2 * std::sin (6 * juce::MathConstants<double>::pi * phase);
            s[i] += (float) (0.4 * buzz * std::exp (-t / 0.35));
        }
    }
    return s;
}

void setParam (TengriProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

struct Stats { float peak = 0, rms = 0, medianPitch = 0; bool finite = true; };

Stats run (TengriProcessor& p, const Signal& input, double seconds, const juce::File& wav,
           std::function<void (juce::MidiBuffer&, int64_t)> midiFn = {})
{
    p.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    p.prepareToPlay (sampleRate, blockSize);

    const auto total = (int64_t) (seconds * sampleRate);
    juce::AudioBuffer<float> out (2, (int) total);
    juce::AudioBuffer<float> block (2, blockSize);
    std::vector<float> pitches;

    for (int64_t pos = 0; pos < total; pos += blockSize)
    {
        const int n = (int) std::min<int64_t> (blockSize, total - pos);
        block.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i)
        {
            const float x = (size_t) (pos + i) < input.size() ? input[(size_t) (pos + i)] : 0.0f;
            block.setSample (0, i, x);
            block.setSample (1, i, x);
        }
        juce::MidiBuffer midi;
        if (midiFn) midiFn (midi, pos);
        p.processBlock (block, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, (int) pos, block, ch, 0, n);
        if (const float f = p.telemetry.pitchHz.load(); f > 0)
            pitches.push_back (f);
    }

    Stats st;
    double sum = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < out.getNumSamples(); ++i)
        {
            const float v = out.getSample (ch, i);
            if (! std::isfinite (v)) st.finite = false;
            st.peak = std::max (st.peak, std::abs (v));
            sum += (double) v * v;
        }
    st.rms = (float) std::sqrt (sum / (2.0 * out.getNumSamples()));
    if (! pitches.empty())
    {
        std::nth_element (pitches.begin(), pitches.begin() + (long) pitches.size() / 2, pitches.end());
        st.medianPitch = pitches[pitches.size() / 2];
    }

    wav.deleteFile();
    if (auto stream = std::unique_ptr<juce::OutputStream> (wav.createOutputStream()))
    {
        juce::WavAudioFormat fmt;
        if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (fmt.createWriterFor (stream.get(), sampleRate, 2, 24, {}, 0)))
        {
            stream.release();
            writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
        }
    }
    return st;
}

void print (const char* name, const Stats& s)
{
    std::printf ("%-28s peak %6.3f  rms %6.4f  pitch %7.1f Hz  %s\n", name, s.peak, s.rms, s.medianPitch,
                 s.finite ? "ok" : "NaN/INF!");
}

void snapshot (TengriProcessor& p, int mode, const juce::File& png)
{
    setParam (p, tengri::pid::mode, (float) mode);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setVisible (true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
    const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    png.deleteFile();
    juce::FileOutputStream os (png);
    juce::PNGImageFormat().writeImageToStream (img, os);
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    auto voice = makeVoice (6.0), guitar = makeGuitar (6.0), ruler = makeRuler (4.8);
    using namespace tengri;

    {
        TengriProcessor p;
        print ("dry voice (mix 0)", (setParam (p, pid::mix, 0.0f), run (p, voice, 6.0, outDir.getChildFile ("00_voice_dry.wav"))));
    }
    {
        TengriProcessor p;
        setParam (p, pid::mix, 0.0f);
        print ("dry guitar (mix 0)", run (p, guitar, 6.0, outDir.getChildFile ("00_guitar_dry.wav")));
    }
    {
        TengriProcessor p;
        setParam (p, pid::mix, 0.0f);
        print ("dry ruler (mix 0)", run (p, ruler, 4.8, outDir.getChildFile ("00_ruler_dry.wav")));
    }
    {
        TengriProcessor p;
        setParam (p, pid::mode, 0);
        print ("VOICE  -> khoomei", run (p, voice, 7.0, outDir.getChildFile ("01_voice_khoomei.wav")));
    }
    {
        TengriProcessor p;
        setParam (p, pid::mode, 1);
        print ("STRING guitar -> morin", run (p, guitar, 7.0, outDir.getChildFile ("02_guitar_morin.wav")));
    }
    {
        TengriProcessor p;
        setParam (p, pid::mode, 1);
        setParam (p, pid::sustain, 2.5f);
        print ("STRING ruler -> morin", run (p, ruler, 6.0, outDir.getChildFile ("03_ruler_morin.wav")));
    }
    {
        TengriProcessor p;
        setParam (p, pid::drum, 0.7f);
        setParam (p, pid::drone, 0.5f);
        setParam (p, pid::space, 0.6f);
        auto midiFn = [] (juce::MidiBuffer& m, int64_t pos)
        {
            if (pos == 0) { m.addEvent (juce::MidiMessage::noteOn (1, 50, 0.9f), 0); m.addEvent (juce::MidiMessage::noteOn (1, 57, 0.7f), 0); }
            if (pos <= (int64_t) (3.0 * sampleRate) && pos + blockSize > (int64_t) (3.0 * sampleRate))
            {
                m.addEvent (juce::MidiMessage::noteOff (1, 50), 0);
                m.addEvent (juce::MidiMessage::noteOff (1, 57), 0);
                m.addEvent (juce::MidiMessage::noteOn (1, 45, 1.0f), 1);
            }
            if (pos <= (int64_t) (5.0 * sampleRate) && pos + blockSize > (int64_t) (5.0 * sampleRate))
                m.addEvent (juce::MidiMessage::noteOff (1, 45), 0);
        };
        print ("SYNTH + drum + drone", run (p, {}, 7.0, outDir.getChildFile ("04_synth_spirit.wav"), midiFn));
    }
    {
        TengriProcessor p;
        setParam (p, pid::synthTimbre, 1.0f);
        auto midiFn = [] (juce::MidiBuffer& m, int64_t pos)
        {
            if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 45, 0.9f), 0);
            if (pos <= (int64_t) (3.0 * sampleRate) && pos + blockSize > (int64_t) (3.0 * sampleRate))
                m.addEvent (juce::MidiMessage::noteOff (1, 45), 0);
        };
        print ("SYNTH morin timbre", run (p, {}, 5.0, outDir.getChildFile ("05_synth_morin.wav"), midiFn));
    }

    {
        TengriProcessor p;
        setParam (p, pid::drum, 1.0f);
        print ("DRUM only (100%)", run (p, {}, 4.0, outDir.getChildFile ("06_drum.wav")));
    }
    {
        TengriProcessor p;
        setParam (p, pid::drone, 1.0f);
        print ("DRONE only (100%)", run (p, {}, 4.0, outDir.getChildFile ("07_drone.wav")));
    }
    {
        TengriProcessor p;
        auto midiFn = [] (juce::MidiBuffer& m, int64_t pos)
        {
            if (pos == 0) for (int nn : { 50, 57, 62 }) m.addEvent (juce::MidiMessage::noteOn (1, nn, 1.0f), 0);
        };
        print ("SYNTH throat chord vel 127", run (p, {}, 4.0, outDir.getChildFile ("08_synth_chord.wav"), midiFn));
    }

    // WHISTLE: energy around the sung harmonic (H10 of 130 Hz = 1300 Hz) must follow the knob
    for (float w : { 0.0f, 1.0f, 2.0f })
    {
        TengriProcessor p;
        setParam (p, pid::harmonic, 10.0f);
        setParam (p, pid::sweep, 0.0f);
        setParam (p, pid::whistle, w);
        setParam (p, pid::space, 0.0f);
        setParam (p, pid::echo, 0.0f);
        const auto name = "whistle_" + juce::String (juce::roundToInt (w * 100)) + ".wav";
        print (("VOICE whistle " + juce::String (juce::roundToInt (w * 100)) + "%").toRawUTF8(), run (p, voice, 3.0, outDir.getChildFile (name)));
    }
    {
        TengriProcessor p;
        setParam (p, pid::mute, 1.0f);
        setParam (p, pid::drum, 1.0f);
        print ("MUTE on (voice + drum)", run (p, voice, 3.0, outDir.getChildFile ("mute.wav")));
    }
    {
        TengriProcessor p;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            const auto st = run (p, i % 2 == 0 ? voice : guitar, 2.0, outDir.getChildFile ("preset.wav"));
            std::printf ("preset %02d %-16s peak %6.3f  rms %6.4f  %s\n", i + 1, p.getProgramName (i).toRawUTF8(),
                         st.peak, st.rms, st.finite ? "ok" : "NaN!");
        }
        outDir.getChildFile ("preset.wav").deleteFile();
    }

    // UI
    {
        TengriProcessor p;
        p.setCurrentProgram (1);
        setParam (p, pid::drum, 0.5f);
        run (p, voice, 2.4, outDir.getChildFile ("ui_feed.wav"));
        snapshot (p, 0, outDir.getChildFile ("ui_voice.png"));
        setParam (p, pid::mode, 1);
        run (p, guitar, 1.0, outDir.getChildFile ("ui_feed.wav"));
        snapshot (p, 1, outDir.getChildFile ("ui_string.png"));
        outDir.getChildFile ("ui_feed.wav").deleteFile();
    }

    std::printf ("written to %s\n", outDir.getFullPathName().toRawUTF8());
    return 0;
}
