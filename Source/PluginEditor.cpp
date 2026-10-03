#include "PluginEditor.h"

using namespace tengri::ui;

namespace
{
juce::String ru (const char* utf8) { return juce::String::fromUTF8 (utf8); }

juce::String noteLabel (float hz)
{
    if (hz <= 0.0f)
        return "--";
    const int midi = juce::roundToInt (tengri::hzToMidi (hz));
    return tengri::noteNames()[((midi % 12) + 12) % 12] + juce::String (midi / 12 - 1);
}

// Soyombo crown: flame over sun over crescent moon
void drawSoyombo (juce::Graphics& g, float x, float y, float pitch)
{
    DotMatrix::drawBitmap (g, { "....#....", "...###...", "..#####..", "...###...", "....#....", "........." },
                           x, y, pitch, palette::terracotta);
    DotMatrix::drawBitmap (g, { "...###...", "..#####..", "...###..." }, x, y + pitch * 6, pitch, palette::ochre);
    DotMatrix::drawBitmap (g, { "#.......#", ".#.....#.", "..#####.." }, x, y + pitch * 10, pitch, palette::brick);
}
} // namespace

TengriEditor::TengriEditor (TengriProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      modeSwitch (*p.apvts.getParameter (tengri::pid::mode), { "VOICE", "STRING" }),
      mixSlider (p.apvts, tengri::pid::mix, "Mix", palette::brick),
      outSlider (p.apvts, tengri::pid::output, "Out", palette::ochre)
{
    setLookAndFeel (&lnf);
    using namespace tengri::pid;

    // VOICE -> khöömei
    addKnob (voiceKnobs, kargyraa, "Kargyraa", u8"Каргыраа — низкий рычащий субоктавный призвук (удвоение периода)", palette::brick);
    addKnob (voiceKnobs, overtone, "Overtone", u8"Сыгыт — свистящий обертон, поющий над голосом", palette::brick);
    addKnob (voiceKnobs, harmonic, "Harmonic", u8"Номер гармоники, на которой поёт обертон", palette::brick);
    addKnob (voiceKnobs, sweep, "Sweep", u8"Мелодия обертонов: скорость и размах движения", palette::brick);
    addKnob (voiceKnobs, vowel, "Vowel", u8"Гласная У-О-А-Э-И — форма рта", palette::brick);
    addKnob (voiceKnobs, throat, "Throat", u8"Сдавленное горло — плотность и насыщение", palette::brick);

    // STRING -> morin khuur
    addKnob (stringKnobs, body, "Body", u8"Корпус моринхуура — деревянная трапеция", palette::terracotta);
    addKnob (stringKnobs, sympathy, "Sympathy", u8"Симпатические струны, настроенные квартами от тоники", palette::terracotta);
    addKnob (stringKnobs, bow, "Bow", u8"Смычок из конского волоса — ресинтез по высоте входа", palette::terracotta);
    addKnob (stringKnobs, sustain, "Sustain", u8"Сколько смычок тянет ноту после затухания источника", palette::terracotta);
    addKnob (stringKnobs, grit, "Grit", u8"Канифоль и конский волос — шершавость", palette::terracotta);
    addKnob (stringKnobs, snap, "Snap", u8"Притяжение высоты смычка к монгольской пентатонике", palette::terracotta);

    // SYNTH
    addKnob (synthKnobs, synthLevel, "Level", u8"Громкость синтезатора (MIDI)", palette::ochre);
    addKnob (synthKnobs, synthTimbre, "Timbre", u8"Тембр: горловой голос ↔ моринхуур", palette::ochre);
    addKnob (synthKnobs, synthSub, "Sub", u8"Каргыраа — субоктава", palette::ochre);
    addKnob (synthKnobs, synthOvertone, "Overtone", u8"Свист обертона. Колесо модуляции выбирает гармонику вручную", palette::ochre);
    addKnob (synthKnobs, synthAttack, "Attack", u8"Атака", palette::ochre);
    addKnob (synthKnobs, synthRelease, "Release", u8"Затухание", palette::ochre);

    // SPIRIT
    addKnob (spiritKnobs, drum, "Drum", u8"Бубен шамана (хэц) с железными подвесками", palette::brick);
    addKnob (spiritKnobs, pulse, "Pulse", u8"Темп транса (SYNC — темп хоста)", palette::brick);
    addKnob (spiritKnobs, space, "Space", u8"Пространство: степь, юрта, пещера", palette::brick);
    addKnob (spiritKnobs, echo, "Echo", u8"Эхо над степью", palette::brick);
    addKnob (spiritKnobs, drone, "Drone", u8"Бурдон на тонике — горло или моринхуур, по режиму", palette::brick);
    addKnob (spiritKnobs, root, "Root", u8"Тоника: строй бурдона, симпатических струн и пентатоники", palette::brick);

    addAndMakeVisible (modeSwitch);
    addAndMakeVisible (spectrum);
    addAndMakeVisible (mixSlider);
    addAndMakeVisible (outSlider);
    addAndMakeVisible (syncButton);
    syncButton.setTooltip (ru (u8"Синхронизировать бубен с темпом и сеткой хоста"));
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, tengri::pid::sync, syncButton);

    modeSwitch.setTooltip (ru (u8"VOICE — голос в горловое пение · STRING — любая струна в моринхуур"));

    setSize (editorWidth, editorHeight);
    updateModeVisibility();
    startTimerHz (30);
}

TengriEditor::~TengriEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TengriEditor::addKnob (Knobs& group, const char* paramId, const char* label, const char* tip, juce::Colour accent)
{
    auto* k = group.add (new DotKnob (processor.apvts, paramId, label, ru (tip), accent));
    addAndMakeVisible (k);
}

void TengriEditor::updateModeVisibility()
{
    const int mode = modeSwitch.getIndex();
    for (auto* k : voiceKnobs)  k->setVisible (mode == 0);
    for (auto* k : stringKnobs) k->setVisible (mode == 1);
    lastMode = mode;
    repaint();
}

//==============================================================================
void TengriEditor::resized()
{
    modeSwitch.setBounds (530, 24, 260, 46);

    leftCard   = { 24.0f, 118.0f, 468.0f, 558.0f };
    synthCard  = { 508.0f, 118.0f, 468.0f, 274.0f };
    spiritCard = { 508.0f, 402.0f, 468.0f, 274.0f };

    spectrum.setBounds (48, 200, 424, 152);

    auto layoutGrid = [] (Knobs& knobs, juce::Rectangle<int> area, int rows, int cols)
    {
        const int w = area.getWidth() / cols, h = area.getHeight() / rows;
        for (int i = 0; i < knobs.size(); ++i)
            knobs[i]->setBounds (area.getX() + (i % cols) * w, area.getY() + (i / cols) * h, w, h);
    };

    const juce::Rectangle<int> modeKnobArea (40, 370, 440, 256);
    layoutGrid (voiceKnobs, modeKnobArea, 2, 3);
    layoutGrid (stringKnobs, modeKnobArea, 2, 3);

    layoutGrid (synthKnobs, { 524, 186, 436, 204 }, 2, 3);
    layoutGrid (spiritKnobs, { 524, 470, 436, 204 }, 2, 3);

    mixSlider.setBounds (48, 638, 200, 28);
    outSlider.setBounds (268, 638, 200, 28);

    syncButton.setBounds (876, 420, 80, 26);
}

void TengriEditor::paint (juce::Graphics& g)
{
    g.fillAll (palette::background);

    drawHeader (g);
    drawOrnament (g, { 24.0f, 86.0f, 952.0f, 21.0f });

    const bool voice = modeSwitch.getIndex() == 0;
    drawCard (g, leftCard, voice ? "KHOOMEI" : "MORIN KHUUR",
              voice ? ru (u8"голос → горловое пение · хөөмий, каргыраа, сыгыт")
                    : ru (u8"струна → моринхуур · от железной линейки до гитары"),
              voice ? palette::brick : palette::terracotta);
    drawCard (g, synthCard, "SYNTH", ru (u8"синтезатор · midi · колесо = обертон"), palette::ochre);
    drawCard (g, spiritCard, "SPIRIT", ru (u8"шаманский слой · бубен, бурдон, степь"), palette::brick);

    // separator above the mix row
    g.setColour (palette::stroke);
    for (float x = 48.0f; x < 470.0f; x += 6.0f)
        g.fillEllipse (x, 630.0f, 1.6f, 1.6f);

    // drum pulse — ring of dots that flashes with every hit
    const juce::Point<float> c (spiritCard.getRight() - 128.0f, spiritCard.getY() + 33.0f);
    for (int i = 0; i < 10; ++i)
    {
        const auto pt = c.getPointOnCircumference (9.0f, juce::MathConstants<float>::twoPi * (float) i / 10.0f);
        g.setColour (palette::dotOff.interpolatedWith (palette::brick, drumFlash));
        g.fillEllipse (pt.x - 1.8f, pt.y - 1.8f, 3.6f, 3.6f);
    }
    g.setColour (palette::dotOff.interpolatedWith (palette::cream, drumFlash));
    g.fillEllipse (c.x - 3.0f, c.y - 3.0f, 6.0f, 6.0f);
}

void TengriEditor::drawHeader (juce::Graphics& g) const
{
    drawSoyombo (g, 26.0f, 22.0f, 3.4f);
    DotMatrix::draw (g, "TENGRI", 70.0f, 26.0f, 6.0f, palette::cream);

    g.setFont (labelFont (11.0f));
    g.setColour (palette::cream);
    g.drawText ("MONGOL SHAMAN ENGINE", 296, 28, 220, 16, juce::Justification::centredLeft);
    g.setFont (labelFont (11.0f, false));
    g.setColour (palette::dim);
    g.drawText (ru (u8"монгольский шаманский движок"), 296, 46, 220, 16, juce::Justification::centredLeft);

    drawMeter (g, "IN", inLevel, 818.0f, 32.0f);
    drawMeter (g, "OUT", outLevel, 818.0f, 54.0f);
}

void TengriEditor::drawMeter (juce::Graphics& g, const juce::String& name, float level, float x, float y) const
{
    DotMatrix::draw (g, name, x, y, 1.6f, palette::dim);
    const int numDots = 16;
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    const int lit = juce::roundToInt (juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) * (float) numDots);
    for (int i = 0; i < numDots; ++i)
    {
        juce::Colour col = palette::dotOff;
        if (i < lit)
            col = i >= numDots - 2 ? palette::brick : (i >= numDots - 5 ? palette::ochre : palette::cream);
        g.setColour (col);
        g.fillEllipse (x + 34.0f + (float) i * 7.5f, y + 1.5f, 4.2f, 4.2f);
    }
}

void TengriEditor::drawOrnament (juce::Graphics& g, juce::Rectangle<float> band) const
{
    // khee — the continuous Mongolian meander (alkhan khee), drawn in dots
    static const char* unit[] = { ".######.", ".#....#.", ".#.##.#.", ".#..#.#.", ".####.#.", "......#.", "########" };
    const float pitch = band.getHeight() / 7.0f;
    const float unitW = 8.0f * pitch;
    const int count = (int) (band.getWidth() / unitW);
    const float startX = band.getX() + (band.getWidth() - (float) count * unitW) * 0.5f;
    const float d = pitch * 0.62f;

    g.setColour (palette::ornament);
    for (int u = 0; u < count; ++u)
        for (int r = 0; r < 7; ++r)
            for (int c = 0; c < 8; ++c)
                if (unit[r][c] == '#')
                    g.fillEllipse (startX + (float) u * unitW + (float) c * pitch, band.getY() + (float) r * pitch, d, d);
}

void TengriEditor::drawCard (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title,
                             const juce::String& subtitle, juce::Colour accent) const
{
    g.setColour (palette::card);
    g.fillRoundedRectangle (area, 24.0f);
    g.setColour (palette::stroke);
    g.drawRoundedRectangle (area.reduced (0.5f), 24.0f, 1.0f);

    // accent dot + dot-matrix title
    g.setColour (accent);
    g.fillEllipse (area.getX() + 24.0f, area.getY() + 26.0f, 8.0f, 8.0f);
    DotMatrix::draw (g, title, area.getX() + 42.0f, area.getY() + 20.0f, 2.8f, palette::cream);

    g.setColour (palette::dim);
    g.setFont (labelFont (11.5f, false));
    g.drawText (subtitle, (int) area.getX() + 24, (int) area.getY() + 46, (int) area.getWidth() - 48, 16,
                juce::Justification::centredLeft);
}

//==============================================================================
void TengriEditor::timerCallback()
{
    if (modeSwitch.getIndex() != lastMode)
        updateModeVisibility();

    auto& t = processor.telemetry;
    const float f0 = t.pitchHz.load();

    processor.readScope (scopeBuffer.data(), (int) scopeBuffer.size());
    spectrum.setSampleRate (processor.getSampleRate() > 0 ? processor.getSampleRate() : 44100.0);

    juce::String line1 = f0 > 0 ? "F0 " + noteLabel (f0) + " " + juce::String (juce::roundToInt (f0)) + "HZ" : "F0 --";
    juce::String line2;
    float marker = 0;
    if (lastMode == 0)
    {
        marker = f0 > 0 ? t.harmonicHz.load() : 0.0f;
        if (marker > 0)
            line2 = "H" + juce::String (juce::roundToInt (marker / f0)) + " " + juce::String (juce::roundToInt (marker)) + "HZ";
    }
    else
    {
        marker = t.bowHz.load();
        if (f0 > 0)
            line2 = "BOW " + noteLabel (marker) + " " + juce::String (juce::roundToInt (marker)) + "HZ";
    }
    spectrum.push (scopeBuffer.data(), marker, line1, line2, palette::ochre);

    const int hits = t.drumHits.load();
    if (hits != lastDrumHits)
    {
        lastDrumHits = hits;
        drumFlash = 1.0f;
    }
    else
    {
        drumFlash *= 0.82f;
    }

    inLevel  = std::max (t.inLevel.load(),  inLevel * 0.85f);
    outLevel = std::max (t.outLevel.load(), outLevel * 0.85f);

    repaint (800, 24, 190, 50);
    repaint (spiritCard.toNearestInt().withHeight (60));
}
