#pragma once
#include <juce_graphics/juce_graphics.h>

// Palettes picked by the project owner. Hex values sampled from the reference images.
// Use these names instead of retyping hex codes.
namespace catui::SodaPop
{
inline juce::Colour c(juce::uint32 argb) { return juce::Colour(argb); }

// "Soda Pop" (artsydee.com Procreate palette), 3 rows x 10 swatches.
// Row 1: greens then oranges and yellows
inline juce::Colour jade()       { return c(0xff009572); }
inline juce::Colour mint()       { return c(0xff02EBBA); }
inline juce::Colour aqua()       { return c(0xff01FCDC); }
inline juce::Colour mistGreen()  { return c(0xffBDF9EB); }
inline juce::Colour mistGrey()   { return c(0xffD9EBE9); }
inline juce::Colour tangerine()  { return c(0xffF57F01); }
inline juce::Colour orange()     { return c(0xffFF9000); }
inline juce::Colour amber()      { return c(0xffFFA600); }
inline juce::Colour gold()       { return c(0xffFFD100); }
inline juce::Colour lemon()      { return c(0xffFEE400); }
// Row 2: purples and blues
inline juce::Colour grape()      { return c(0xff4F009A); }
inline juce::Colour violet()     { return c(0xff7900E3); }
inline juce::Colour orchid()     { return c(0xffA43DFF); }
inline juce::Colour lilac()      { return c(0xffCD92FE); }
inline juce::Colour mistLilac()  { return c(0xffE1CBF4); }
inline juce::Colour midnight()   { return c(0xff1B0264); }
inline juce::Colour indigo()     { return c(0xff3308AF); }
inline juce::Colour electric()   { return c(0xff4F10FD); }
inline juce::Colour periwinkle() { return c(0xff774BF9); }
inline juce::Colour lavender()   { return c(0xffC0A9FE); }
// Row 3: reds and pinks
inline juce::Colour cherry()     { return c(0xffFE006D); }
inline juce::Colour hotPink()    { return c(0xffFF1383); }
inline juce::Colour bubblegum()  { return c(0xffFF7BB1); }
inline juce::Colour blush()      { return c(0xffFBBDD0); }
inline juce::Colour mistPink()   { return c(0xffFFE5EE); }
inline juce::Colour plum()       { return c(0xffAB0080); }
inline juce::Colour magenta()    { return c(0xffE900AE); }
inline juce::Colour fuchsia()    { return c(0xffFF00CF); }
inline juce::Colour candy()      { return c(0xffFE27D6); }
inline juce::Colour taffy()      { return c(0xffFE91E8); }

// Second palette: five swatches, yellow / sky / ocean / pink / navy.
inline juce::Colour popYellow()  { return c(0xffFEEF5E); }
inline juce::Colour popSky()     { return c(0xff6CC9F5); }
inline juce::Colour popOcean()   { return c(0xff398BC4); }
inline juce::Colour popPink()    { return c(0xffD94995); }
inline juce::Colour popNavy()    { return c(0xff2C2C62); }
}
