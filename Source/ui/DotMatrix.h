#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

namespace tengri::ui
{
/**
    5x7 dot-matrix type, the signature of the Nothing OS look.
    Supports A-Z, 0-9 and  . : - + % # / < > * (centred dot) and space.
*/
class DotMatrix
{
public:
    using Glyph = std::array<uint8_t, 7>; // 5 bits per row, MSB = leftmost

    static const Glyph* glyph (juce::juce_wchar c)
    {
        static const auto table = buildTable();
        c = juce::CharacterFunctions::toUpperCase (c);
        auto it = table.find (c);
        return it != table.end() ? &it->second : nullptr;
    }

    static float width (const juce::String& text, float pitch)
    {
        return text.isEmpty() ? 0.0f : ((float) text.length() * 6.0f - 1.0f) * pitch;
    }

    static float height (float pitch) { return 7.0f * pitch; }

    /** Draws text with its top-left at (x, y). Unlit dots are drawn only when `off` is opaque. */
    static void draw (juce::Graphics& g, const juce::String& text, float x, float y, float pitch,
                      juce::Colour on, juce::Colour off = juce::Colours::transparentBlack, float dotScale = 0.78f)
    {
        const float d = pitch * dotScale;
        const float inset = (pitch - d) * 0.5f;
        for (auto c : text)
        {
            if (auto* gl = glyph (c))
            {
                for (int row = 0; row < 7; ++row)
                    for (int col = 0; col < 5; ++col)
                    {
                        const bool lit = ((*gl)[(size_t) row] >> (4 - col)) & 1;
                        if (! lit && off.isTransparent())
                            continue;
                        g.setColour (lit ? on : off);
                        g.fillEllipse (x + (float) col * pitch + inset, y + (float) row * pitch + inset, d, d);
                    }
            }
            x += 6.0f * pitch;
        }
    }

    static void drawCentred (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float pitch, juce::Colour on)
    {
        draw (g, text, area.getCentreX() - width (text, pitch) * 0.5f, area.getCentreY() - height (pitch) * 0.5f, pitch, on);
    }

    /** Draws an arbitrary bitmap given as rows of '#' / '.' characters. */
    static void drawBitmap (juce::Graphics& g, std::initializer_list<const char*> rows, float x, float y, float pitch,
                            juce::Colour on, float dotScale = 0.78f)
    {
        const float d = pitch * dotScale;
        const float inset = (pitch - d) * 0.5f;
        g.setColour (on);
        int r = 0;
        for (auto* row : rows)
        {
            for (int c = 0; row[c] != 0; ++c)
                if (row[c] == '#')
                    g.fillEllipse (x + (float) c * pitch + inset, y + (float) r * pitch + inset, d, d);
            ++r;
        }
    }

private:
    static std::map<juce::juce_wchar, Glyph> buildTable()
    {
        const std::pair<juce::juce_wchar, std::array<const char*, 7>> src[] = {
            { 'A', { ".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" } },
            { 'B', { "####.", "#...#", "#...#", "####.", "#...#", "#...#", "####." } },
            { 'C', { ".###.", "#...#", "#....", "#....", "#....", "#...#", ".###." } },
            { 'D', { "####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####." } },
            { 'E', { "#####", "#....", "#....", "####.", "#....", "#....", "#####" } },
            { 'F', { "#####", "#....", "#....", "####.", "#....", "#....", "#...." } },
            { 'G', { ".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####" } },
            { 'H', { "#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" } },
            { 'I', { ".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###." } },
            { 'J', { "..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.." } },
            { 'K', { "#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#" } },
            { 'L', { "#....", "#....", "#....", "#....", "#....", "#....", "#####" } },
            { 'M', { "#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#" } },
            { 'N', { "#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#" } },
            { 'O', { ".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." } },
            { 'P', { "####.", "#...#", "#...#", "####.", "#....", "#....", "#...." } },
            { 'Q', { ".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#" } },
            { 'R', { "####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#" } },
            { 'S', { ".####", "#....", "#....", ".###.", "....#", "....#", "####." } },
            { 'T', { "#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.." } },
            { 'U', { "#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." } },
            { 'V', { "#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.." } },
            { 'W', { "#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#." } },
            { 'X', { "#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#" } },
            { 'Y', { "#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.." } },
            { 'Z', { "#####", "....#", "...#.", "..#..", ".#...", "#....", "#####" } },
            { '0', { ".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###." } },
            { '1', { "..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###." } },
            { '2', { ".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####" } },
            { '3', { "#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###." } },
            { '4', { "...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#." } },
            { '5', { "#####", "#....", "####.", "....#", "....#", "#...#", ".###." } },
            { '6', { "..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###." } },
            { '7', { "#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..." } },
            { '8', { ".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###." } },
            { '9', { ".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.." } },
            { '.', { ".....", ".....", ".....", ".....", ".....", ".....", "..#.." } },
            { ':', { ".....", "..#..", ".....", ".....", ".....", "..#..", "....." } },
            { '-', { ".....", ".....", ".....", ".###.", ".....", ".....", "....." } },
            { '+', { ".....", "..#..", "..#..", "#####", "..#..", "..#..", "....." } },
            { '%', { "##..#", "##..#", "...#.", "..#..", ".#...", "#..##", "#..##" } },
            { '#', { ".#.#.", ".#.#.", "#####", ".#.#.", "#####", ".#.#.", ".#.#." } },
            { '/', { "....#", "....#", "...#.", "..#..", ".#...", "#....", "#...." } },
            { '<', { "...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#." } },
            { '>', { ".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#..." } },
            { '*', { ".....", ".....", ".....", "..#..", ".....", ".....", "....." } },
            { ' ', { ".....", ".....", ".....", ".....", ".....", ".....", "....." } },
        };

        std::map<juce::juce_wchar, Glyph> table;
        for (auto& [ch, rows] : src)
        {
            Glyph gl {};
            for (size_t r = 0; r < 7; ++r)
                for (int c = 0; c < 5; ++c)
                    if (rows[r][c] == '#')
                        gl[r] |= (uint8_t) (1 << (4 - c));
            table[ch] = gl;
        }
        return table;
    }
};

} // namespace tengri::ui
