#pragma once
// Look of Whisker Scope (house style "Purple Soda Pop": dark grey panels, Barlow Condensed labels, JetBrains Mono numbers, PurrOne wood).
// Everything here is drawn in design pixels (the editor scales the whole content), so sizes match the approved mockup 1:1.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <BinaryData.h>
#include <map>
#include "Params.h"
#include "whiskerscope/Engine.h"

namespace wsui
{
constexpr int kW = 1360, kH = 920;       // design size of the window

struct Scheme { const char* name; juce::Colour l, r, m, acc; juce::Colour wf[7]; };
inline const Scheme& scheme (int i)
{
    static const Scheme S[4] = {
        { "Soda aqua / pink",  juce::Colour (0xff02EBBA), juce::Colour (0xffFF1383), juce::Colour (0xffA43DFF), juce::Colour (0xffFFD100), { juce::Colour (0xff06080a), juce::Colour (0xff1B0264), juce::Colour (0xff4F10FD), juce::Colour (0xffA43DFF), juce::Colour (0xffFF1383), juce::Colour (0xffFFD100), juce::Colour (0xffFEE400) } },
        { "Violet / orange",   juce::Colour (0xffA43DFF), juce::Colour (0xffFF9000), juce::Colour (0xff6CC9F5), juce::Colour (0xffFEE400), { juce::Colour (0xff06080a), juce::Colour (0xff3308AF), juce::Colour (0xff7900E3), juce::Colour (0xffCD92FE), juce::Colour (0xffFF9000), juce::Colour (0xffFFD100), juce::Colour (0xffFEE400) } },
        { "Ice / rose",        juce::Colour (0xff6CC9F5), juce::Colour (0xffD94995), juce::Colour (0xff02EBBA), juce::Colour (0xffFEEF5E), { juce::Colour (0xff06080a), juce::Colour (0xff2C2C62), juce::Colour (0xff398BC4), juce::Colour (0xff6CC9F5), juce::Colour (0xffD94995), juce::Colour (0xffFEEF5E), juce::Colour (0xffffffff) } },
        { "Mono",              juce::Colour (0xfff4f5f7), juce::Colour (0xffa9adb8), juce::Colour (0xffffffff), juce::Colour (0xff02EBBA), { juce::Colour (0xff06080a), juce::Colour (0xff2b2d34), juce::Colour (0xff5b5f6b), juce::Colour (0xffa9adb8), juce::Colour (0xfff4f5f7), juce::Colour (0xffffffff), juce::Colour (0xffffffff) } } };
    return S[juce::jlimit (0, 3, i)];
}
// The scheme every component paints with; the editor changes it when the Colours parameter changes.
struct Palette { const Scheme* s = &scheme (0); const Scheme& operator*() const { return *s; } const Scheme* operator->() const { return s; } };

namespace col
{
    inline const juce::Colour ground { 0xff0b0b0d }, panel { 0xff15161a }, panel2 { 0xff1c1d22 }, line { 0xff2b2d34 }, ink { 0xfff4f5f7 }, dim { 0xffa9adb8 }, disp { 0xff06080a }, grid { 0xff1b2523 },
        zero { 0xff3a3a78 }, key { 0xffd6d8de }, popup { 0xff0a0a0c }, hover { 0xff4a4d58 }, cb { 0xffffd100 }, mask { 0xffff1c6c };
}

//==============================================================================
// Fonts: Barlow Condensed (labels) and JetBrains Mono (numbers), both SIL Open Font License, shipped inside the plugin.
inline juce::Typeface::Ptr typefaceBarlow (int weight)
{
    static juce::Typeface::Ptr a = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedMedium_ttf, (size_t) BinaryData::BarlowCondensedMedium_ttfSize);
    static juce::Typeface::Ptr b = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, (size_t) BinaryData::BarlowCondensedSemiBold_ttfSize);
    static juce::Typeface::Ptr c = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, (size_t) BinaryData::BarlowCondensedBold_ttfSize);
    return weight >= 700 ? c : weight >= 600 ? b : a;
}
inline juce::Typeface::Ptr typefaceMono()
{
    static juce::Typeface::Ptr m = juce::Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoVariable_ttf, (size_t) BinaryData::JetBrainsMonoVariable_ttfSize);
    return m;
}
inline juce::Font label (float px, int weight = 500, float tracking = 0.04f) { return juce::Font (juce::FontOptions (typefaceBarlow (weight)).withPointHeight (px).withKerningFactor (tracking)); }
inline juce::Font mono (float px) { return juce::Font (juce::FontOptions (typefaceMono()).withPointHeight (px)); }
inline float textWidth (const juce::String& s, const juce::Font& f) { juce::GlyphArrangement ga; ga.addLineOfText (f, s, 0, 0); return ga.getBoundingBox (0, -1, true).getWidth(); }

// Canvas style text: x is the anchor (left, centre or right), y is the baseline.
inline void txt (juce::Graphics& g, const juce::String& s, float x, float y, juce::Colour c, juce::Justification j, const juce::Font& f)
{
    g.setColour (c); g.setFont (f); g.drawSingleLineText (s, (int) std::lround (x), (int) std::lround (y), j);
}
inline const juce::Font& monoCached (float px) { static std::map<int, juce::Font> m; const int k = (int) std::lround (px * 10.0f); auto it = m.find (k); if (it == m.end()) it = m.emplace (k, mono (px)).first; return it->second; }
inline void mtxt (juce::Graphics& g, const juce::String& s, float x, float y, juce::Colour c, juce::Justification j, float px = 11.0f) { txt (g, s, x, y, c, j, monoCached (px)); }
inline juce::String fmtHz (double f) { return f >= 1000 ? juce::String (f / 1000.0, 2) + " kHz" : juce::String (f, 1) + " Hz"; }
inline juce::String fmtDb (double v) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; }
inline juce::String noteOf (double f) { return juce::String (ws::noteName (f)); }

//==============================================================================
// The PurrOne wood: dark #3d2210, warm #7b4a26 at 40 percent, mid #5a3419 at 75 percent, fixed seed vertical grain, thin black edge.
// (Same recipe as catui::drawWood in the cat-ui repo; kept here because that repo ships headers only.)
inline void drawWood (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (juce::Colour (0xff3d2210)); g.fillRect (r);
    juce::Random rnd (87); int x = r.getX();
    while (x < r.getRight())
    {
        const int w = 1 + rnd.nextInt (3); const bool warm = rnd.nextBool();
        g.setColour (warm ? juce::Colour (0xff7b4a26).withAlpha (0.4f) : juce::Colour (0xff5a3419).withAlpha (0.75f));
        g.fillRect (x, r.getY(), std::min (w, r.getRight() - x), r.getHeight()); x += w + rnd.nextInt (3);
    }
    g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawRect (r, 1);
}
inline void drawWoodCheeks (juce::Graphics& g, juce::Rectangle<int> bounds, int cheek)
{
    const auto l = bounds.removeFromLeft (cheek).withTrimmedLeft (0), r = bounds.removeFromRight (cheek);
    drawWood (g, l); drawWood (g, r);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.0f), (float) l.getRight() - 8, 0, juce::Colours::black.withAlpha (0.6f), (float) l.getRight(), 0, false)); g.fillRect (l.getRight() - 8, l.getY(), 8, l.getHeight());
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), (float) r.getX(), 0, juce::Colours::black.withAlpha (0.0f), (float) r.getX() + 8, 0, false)); g.fillRect (r.getX(), r.getY(), 8, r.getHeight());
}

//==============================================================================
// Look and feel: combo boxes, popup menus, info bubbles.
class WsLook : public juce::LookAndFeel_V4
{
public:
    WsLook()
    {
        setColour (juce::ComboBox::textColourId, col::ink); setColour (juce::ComboBox::backgroundColourId, col::popup); setColour (juce::ComboBox::outlineColourId, col::line);
        setColour (juce::PopupMenu::backgroundColourId, col::popup); setColour (juce::PopupMenu::textColourId, col::ink); setColour (juce::PopupMenu::highlightedBackgroundColourId, col::panel2);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff02EBBA));
        setColour (juce::TextEditor::backgroundColourId, col::popup); setColour (juce::TextEditor::textColourId, col::ink); setColour (juce::TextEditor::outlineColourId, col::line);
        setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff6CC9F5)); setColour (juce::TextEditor::highlightColourId, juce::Colour (0xff4F10FD));
        setColour (juce::CaretComponent::caretColourId, col::ink);
        setColour (juce::ScrollBar::thumbColourId, col::hover); setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::TooltipWindow::backgroundColourId, col::popup); setColour (juce::TooltipWindow::textColourId, col::ink); setColour (juce::TooltipWindow::outlineColourId, col::hover);
    }
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (col::popup); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (box.hasKeyboardFocus (true) ? juce::Colour (0xff6CC9F5) : box.isMouseOver() ? col::hover : col::line); g.drawRoundedRectangle (r, 3.0f, 1.0f);
        juce::Path t; const float cx = (float) w - 13.0f, cy = (float) h * 0.5f; t.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
        g.setColour (box.isEnabled() ? col::ink : col::dim); g.fillPath (t);
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return label (13.0f, 500, 0.04f); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& l) override { l.setBounds (7, 1, box.getWidth() - 24, box.getHeight() - 2); l.setFont (getComboBoxFont (box)); }
    juce::Font getPopupMenuFont() override { return label (15.0f, 500, 0.02f); }
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override { g.fillAll (col::popup); g.setColour (col::hover); g.drawRect (0, 0, w, h, 1); }
    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked, bool, const juce::String& text, const juce::String&, const juce::Drawable*, const juce::Colour*) override
    {
        if (isSeparator) { g.setColour (col::line); g.fillRect (area.reduced (6, 0).withHeight (1).withY (area.getCentreY())); return; }
        if (isHighlighted && isActive) { g.setColour (col::panel2); g.fillRect (area); }
        g.setColour (! isActive ? col::dim : (isHighlighted || isTicked) ? juce::Colour (0xff02EBBA) : col::ink); g.setFont (getPopupMenuFont());
        g.drawText (text, area.reduced (10, 0), juce::Justification::centredLeft, true);
    }
    // info bubbles
    juce::Rectangle<int> getTooltipBounds (const juce::String& tip, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override
    {
        const int w = 270; juce::String body = tip.fromFirstOccurrenceOf ("\n", false, false), head = tip.upToFirstOccurrenceOf ("\n", false, false);
        auto f = label (13.0f, 500, 0.02f); juce::AttributedString a; a.append (body, f, col::ink); a.setWordWrap (juce::AttributedString::byWord);
        juce::TextLayout tl; tl.createLayout (a, (float) w - 20.0f); const int h = 14 + (head.isNotEmpty() ? 17 : 0) + (int) std::ceil (tl.getHeight()) + 6;
        return juce::Rectangle<int> (screenPos.x > parentArea.getRight() - w - 20 ? screenPos.x - w - 10 : screenPos.x + 16, screenPos.y + 22, w, h).constrainedWithin (parentArea);
    }
    void drawTooltip (juce::Graphics& g, const juce::String& tip, int w, int h) override
    {
        g.setColour (col::popup); g.fillRoundedRectangle (0, 0, (float) w, (float) h, 4.0f); g.setColour (col::hover); g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 4.0f, 1.0f);
        const juce::String head = tip.upToFirstOccurrenceOf ("\n", false, false), body = tip.fromFirstOccurrenceOf ("\n", false, false); int y = 7;
        if (head.isNotEmpty()) { txt (g, head.toUpperCase(), 10, (float) y + 11, juce::Colour (0xff02EBBA), juce::Justification::left, label (12.0f, 600, 0.14f)); y += 17; }
        juce::AttributedString a; a.append (body, label (13.0f, 500, 0.02f), col::ink); a.setWordWrap (juce::AttributedString::byWord);
        a.draw (g, juce::Rectangle<float> (10.0f, (float) y, (float) w - 20.0f, (float) h - (float) y));
    }
};

//==============================================================================
// Button: plain, toggle (lit when on), tab, or the red Panic button. Text is drawn in capitals like the mockup.
class MBtn : public juce::Button
{
public:
    enum Kind { Plain, Toggle, Tab, Panic };
    MBtn (const juce::String& text, Kind k, const Palette& p) : juce::Button (text), kind (k), pal (p) { setClickingTogglesState (k == Toggle); }
    void setLit (bool b) { if (lit != b) { lit = b; repaint(); } }
    static int widthFor (const juce::String& t, Kind k) { return (int) std::ceil (textWidth (t.toUpperCase(), label (k == Tab ? 15.0f : 13.0f, 600, k == Tab ? 0.14f : 0.08f))) + (k == Tab ? 22 : 24); }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f); const bool on = kind == Toggle ? getToggleState() : lit;
        juce::Colour bg = col::panel2, bd = col::line, tx = col::ink;
        if (kind == Panic) { bg = lit ? juce::Colour (0xffff1c6c) : juce::Colour (0xff4a0f1f); bd = juce::Colour (0xffff1c6c); tx = lit ? juce::Colours::white : juce::Colour (0xffffd6e4); if (over || down) { bg = juce::Colour (0xffff1c6c); tx = juce::Colours::white; } }
        else if (on) { bg = juce::Colour (0xff0f2a26).interpolatedWith (pal->l, 0.0f); bd = pal->l; tx = pal->l; }
        else if (over) bd = col::hover;
        if (down && kind != Panic) bg = bg.brighter (0.08f);
        g.setColour (bg); g.fillRoundedRectangle (r, 3.0f); g.setColour (bd); g.drawRoundedRectangle (r, 3.0f, 1.0f);
        g.setColour (isEnabled() ? tx : tx.withAlpha (0.45f));
        const auto f = label (kind == Tab ? 15.0f : 13.0f, kind == Panic ? 700 : 600, kind == Tab ? 0.14f : 0.08f); g.setFont (f);
        g.drawText (getButtonText().toUpperCase(), getLocalBounds(), juce::Justification::centred, false);
    }
private:
    Kind kind; const Palette& pal; bool lit = false;
};

//==============================================================================
// Rotary knob with label and value, exactly the mockup knob: 44 px body, 54 px arc ring, tick, label under, value under that.
class MKnob : public juce::Slider
{
public:
    static constexpr int kWidth = 84, kHeight = 74, kTop = 0;     // component size; the 54 px ring overflows the top by 5 px (painting is unclipped)
    MKnob (const juce::String& lab, const char* fmtId, const Palette& p, int width = kWidth) : label_ (lab), fmt (fmtId), pal (p)
    {
        setName (lab);
        setSize (width, kHeight);
        setSliderStyle (juce::Slider::RotaryVerticalDrag); setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        setMouseDragSensitivity (160); setVelocityBasedMode (false); setWantsKeyboardFocus (true); setPaintingIsUnclipped (true); setScrollWheelEnabled (true);
    }
    void paint (juce::Graphics& g) override
    {
        const float cx = (float) getWidth() * 0.5f, cy = kTop + 22.0f, n = (float) valueToProportionOfLength (getValue());
        const float a0 = juce::degreesToRadians (-135.0f), a1 = juce::degreesToRadians (135.0f), a = a0 + (a1 - a0) * n;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff33363f), cx - 22 + 44 * 0.35f, kTop + 44 * 0.30f, juce::Colour (0xff23252c), cx - 22 + 44 * 0.35f + 29.5f, kTop + 44 * 0.30f, true));
        g.fillEllipse (cx - 22, cy - 22, 44, 44); g.setColour (juce::Colour (0xff3a3d48)); g.drawEllipse (cx - 21.5f, cy - 21.5f, 43, 43, 1.0f);
        juce::Path trk, val; trk.addCentredArc (cx, cy, 23, 23, 0, a0, a1, true);
        g.setColour (juce::Colour (0xff33353d)); g.strokePath (trk, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (n > 0.002f) { val.addCentredArc (cx, cy, 23, 23, 0, a0, a, true); g.setColour (pal->l); g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
        const float s = std::sin (a), c = std::cos (a);
        g.setColour (col::ink); g.drawLine (cx + 4 * s, cy - 4 * c, cx + 19 * s, cy - 19 * c, 2.0f);
        txt (g, label_.toUpperCase(), cx, (float) kTop + 44 + 2 + 10, col::dim, juce::Justification::centred, label (11.0f, 500, 0.14f));
        mtxt (g, juce::String (wsp::formatNum (fmt, (float) getValue())), cx, (float) kTop + 44 + 2 + 13 + 2 + 12, pal->l, juce::Justification::centred, 12.0f);
        if (hasKeyboardFocus (false)) { g.setColour (juce::Colour (0xff6CC9F5)); g.drawEllipse (cx - 27, cy - 27, 54, 54, 2.0f); }
    }
    bool hitTest (int x, int y) override { return x >= 0 && y >= 0 && x < getWidth() && y < getHeight(); }
    void setLabel (const juce::String& s) { label_ = s; repaint(); }
private:
    juce::String label_; const char* fmt; const Palette& pal;
};

// Label above a combo box (the mockup "fld").
class Field : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int kHeight = 46, kBoxTop = 15, kBoxHeight = 31;
    Field (const juce::String& lab, const juce::StringArray& items) : label_ (lab)
    {
        setName (lab);
        box.addItemList (items, 1); box.setSelectedItemIndex (0, juce::dontSendNotification); addAndMakeVisible (box);
    }
    juce::ComboBox box;
    void setTip (const juce::String& t) { setTooltip (t); box.setTooltip (t); }
    void paint (juce::Graphics& g) override { txt (g, label_.toUpperCase(), 0, 10.5f, col::dim, juce::Justification::left, label (11.0f, 500, 0.18f)); }
    void resized() override { box.setBounds (0, kBoxTop, getWidth(), kBoxHeight); }
    static int widthFor (const juce::StringArray& items, const juce::String& lab)
    {
        float w = 0; for (auto& s : items) w = std::max (w, textWidth (s, label (13.0f, 500, 0.04f)));
        return std::max ((int) std::ceil (w) + 38, (int) std::ceil (textWidth (lab.toUpperCase(), label (11.0f, 500, 0.18f))) + 2);
    }
private:
    juce::String label_;
};
}
