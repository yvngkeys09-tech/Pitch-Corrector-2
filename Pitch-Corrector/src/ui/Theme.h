#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

/// VocalTune-style colour system. Every colour is resolved through the active
/// palette, so switching light/dark repaints the whole interface instantly.
namespace pcui {

struct Palette {
    juce::Colour bg, panel, panelSoft, well, line, text, dim, accent, accentDim,
                 accentText, white, blackKey, whiteKey, tintBlack, tintWhite, led;
};

inline const Palette kDark{
    {0xff232327}, // bg
    {0xff2e2e34}, // panel
    {0xff38383f}, // panelSoft
    {0xff141417}, // well (inset displays)
    {0xff4a4a52}, // line
    {0xffdcdce0}, // text
    {0xff8d8d96}, // dim
    {0xffc741cf}, // accent (magenta)
    {0xff6b2f70}, // accentDim
    {0xffffffff}, // accentText
    {0xffffffff}, // white
    {0xff1b1b1f}, // blackKey
    {0xffe9e9ed}, // whiteKey
    {0xff7b3a83}, // tintBlack
    {0xffddb4e2}, // tintWhite
    {0xff32cd32}, // led (ok)
};

inline const Palette kLight{
    {0xfff4f2f0}, // bg        warm paper white
    {0xffffffff}, // panel     raised cards
    {0xaffeae7e}, // panelSoft hover / inner card
    {0xffefedea}, // well      inset display
    {0xffdcd7d2}, // line      hairlines
    {0xff444444}, // text      graphite
    {0xff8a8a8a}, // dim       captions
    {0xffc741cf}, // accent    magenta stays the brand colour
    {0xffe3b8e8}, // accentDim soft magenta fill
    {0xffffffff}, // accentText
    {0xffffffff}, // white
    {0xff3a3a3e}, // blackKey
    {0xfffbfaf8}, // whiteKey
    {0xffb06ab5}, // tintBlack
    {0xfff0d9f3}, // tintWhite
    {0xff2fae4e}, // led
};

/// Mutable current palette (the editor flips it on theme change and repaints).
inline Palette& pal() noexcept { static Palette p = kDark; return p; }
inline void setPalette(bool dark) noexcept { pal() = dark ? kDark : kLight; }
inline bool isDark() noexcept { return pal().bg.getRed()   // cheap test
                                    < juce::uint8(0x80); }

// Convenience aliases bound to the *current* palette at use site.
#define PC_BG         pcui::pal().bg
#define PC_PANEL      pcui::pal().panel
#define PC_PANELSOFT  pcui::pal().panelSoft
#define PC_WELL       pcui::pal().well
#define PC_LINE       pcui::pal().line
#define PC_TEXT       pcui::pal().text
#define PC_DIM        pcui::pal().dim
#define PC_ACCENT     pcui::pal().accent
#define PC_ACCENTDIM  pcui::pal().accentDim
#define PC_ACCENTTEXT pcui::pal().accentText
#define PC_WHITE      pcui::pal().white

/// Back-compatible names: these now resolve through the active palette, so
/// every component that uses them repaints correctly in light *and* dark mode.
inline Colour kBg()         { return pal().bg; }
inline Colour kPanel()      { return pal().panel; }
inline Colour kPanelSoft()  { return pal().panelSoft; }
inline Colour kWell()       { return pal().well; }
inline Colour kLine()       { return pal().line; }
inline Colour kText()       { return pal().text; }
inline Colour kDim()        { return pal().dim; }
inline Colour kAccent()     { return pal().accent; }
inline Colour kAccentDim()  { return pal().accentDim; }
inline Colour kLed()        { return pal().led; }
inline Colour kBlackKey()   { return pal().blackKey; }
inline Colour kWhiteKey()   { return pal().whiteKey; }
inline Colour kTintBlack()  { return pal().tintBlack; }
inline Colour kTintWhite()  { return pal().tintWhite; }
}
