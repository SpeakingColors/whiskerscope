#pragma once
// Layout helpers and the in-window dialog. Everything is drawn in design pixels like the rest of the editor.
#include "Look.h"
#include <functional>

namespace wsui
{
// Wrapping row layout like the mockup's CSS flex (gap row/column, items sit on the bottom of their row, "full" items take a whole row).
class Flow : public juce::Component
{
public:
    Flow (juce::Colour background, int padL, int padT, int padR, int padB, int gapX, int gapY) : bg (background), pl (padL), pt (padT), pr (padR), pb (padB), gx (gapX), gy (gapY) {}
    void add (juce::Component* c, bool full = false) { items.push_back ({ c, full }); addAndMakeVisible (c); if (getWidth() > 0) relayout(); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f); g.setColour (bg); g.fillRoundedRectangle (r, 4.0f); g.setColour (col::line); g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
    void resized() override { relayout(); }
    void relayout()
    {
        const int innerW = getWidth() - pl - pr; int y = pt, x = 0, rowH = 0; std::vector<It*> row;
        auto flush = [&] { for (auto* it : row) { auto b = it->c->getBounds(); it->c->setTopLeftPosition (b.getX(), y + rowH - b.getHeight()); } y += rowH + gy; row.clear(); x = 0; rowH = 0; };
        for (auto& it : items)
        {
            if (! it.c->isVisible()) continue;
            if (it.full) { if (! row.empty()) flush(); it.c->setBounds (pl, y, innerW, it.c->getHeight()); y += it.c->getHeight() + gy; continue; }
            const int w = it.c->getWidth();
            if (! row.empty() && x + gx + w > innerW) flush();
            const int px = row.empty() ? 0 : x + gx; it.c->setTopLeftPosition (pl + px, 0); x = px + w; rowH = std::max (rowH, it.c->getHeight()); row.push_back (&it);
        }
        if (! row.empty()) flush();
    }
private:
    struct It { juce::Component* c; bool full; };
    std::vector<It> items; juce::Colour bg; int pl, pt, pr, pb, gx, gy;
};

// "1  REFERENCE  the sound you want yours to sound like"
class GroupHead : public juce::Component
{
public:
    GroupHead (const juce::String& n, const juce::String& t, const juce::String& s, const Palette& p) : num (n), title (t), sub (s), pal (p) { setSize (100, 20); setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override
    {
        g.setColour (pal->acc); g.fillEllipse (0, 0, 20, 20); txt (g, num, 10, 15, juce::Colour (0xff111111), juce::Justification::centred, label (13.0f, 700, 0.0f));
        const auto tf = label (13.0f, 500, 0.16f); txt (g, title.toUpperCase(), 28, 15, col::ink, juce::Justification::left, tf);
        txt (g, sub, 28 + textWidth (title.toUpperCase(), tf) + 10, 15, col::dim, juce::Justification::left, label (12.5f, 500, 0.03f));
    }
private:
    juce::String num, title, sub; const Palette& pal;
};

// One line of mono text in the accent colour (status lines of the Shapes page).
class StatusText : public juce::Component
{
public:
    explicit StatusText (const Palette& p, juce::Colour fixed = {}) : pal (p), fixedColour (fixed) { setSize (100, 16); setInterceptsMouseClicks (false, false); }
    void setText (const juce::String& t) { if (t != text) { text = t; repaint(); } }
    void paint (juce::Graphics& g) override { mtxt (g, text, 0, 12, fixedColour.isTransparent() ? pal->acc : fixedColour, juce::Justification::left, 12.0f); }
private:
    juce::String text; const Palette& pal; juce::Colour fixedColour;
};

// Progress bar plus text, shown in place of the reference status line while a reference file is analysed.
class FileProgress : public juce::Component
{
public:
    explicit FileProgress (const Palette& p) : pal (p) { setSize (100, 16); setInterceptsMouseClicks (false, false); }
    void set (float f, const juce::String& t) { if (std::abs (f - frac) > 0.0005f || t != text) { frac = f; text = t; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        const juce::Rectangle<float> r (0.5f, 3.5f, 220.0f, 9.0f); g.setColour (juce::Colour (0xff0a0a0c)); g.fillRoundedRectangle (r, 4.5f); g.setColour (col::line); g.drawRoundedRectangle (r, 4.5f, 1.0f);
        g.setColour (pal->acc); g.fillRoundedRectangle (juce::Rectangle<float> (1.5f, 4.5f, juce::jmax (7.0f, 218.0f * frac), 7.0f), 3.5f);
        mtxt (g, text, 232.0f, 12.0f, pal->acc, juce::Justification::left, 12.0f);
    }
private:
    const Palette& pal; float frac = 0.0f; juce::String text;
};

// Text with **bold** parts, wrapped to a width.
class TextBlock : public juce::Component
{
public:
    enum Kind { Para, Head, Bullet };
    TextBlock (const juce::String& s, Kind k, const Palette& p) : kind (k), pal (p), raw (s) { setInterceptsMouseClicks (false, false); }
    int heightFor (int w)
    {
        width = w; build(); juce::TextLayout tl; tl.createLayout (as, (float) textW()); return (int) std::ceil (tl.getHeight()) + (kind == Head ? 4 : 2);
    }
    void paint (juce::Graphics& g) override
    {
        build(); if (kind == Bullet) { g.setColour (col::dim); g.fillEllipse (5, 9, 4, 4); }
        as.draw (g, juce::Rectangle<float> ((float) xOff(), 0, (float) textW(), (float) getHeight()));
    }
private:
    Kind kind; const Palette& pal; juce::String raw; int width = 400; juce::AttributedString as;
    int xOff() const { return kind == Bullet ? 18 : 0; }
    int textW() const { return std::max (20, width - xOff()); }
    void build()
    {
        as = juce::AttributedString(); as.setWordWrap (juce::AttributedString::byWord); as.setJustification (juce::Justification::topLeft);
        if (kind == Head) { as.append (raw.toUpperCase(), label (13.0f, 600, 0.2f), pal->l); return; }
        const auto reg = label (15.0f, 500, 0.02f), bold = label (15.0f, 700, 0.02f); const auto ink = juce::Colour (0xffd6d8de);
        auto parts = juce::StringArray::fromTokens (raw, "**", ""); bool b = false; for (auto& s : parts) { if (s.isNotEmpty()) as.append (s, b ? bold : reg, b ? col::ink : ink); b = ! b; }
        as.setLineSpacing (0.0f);
    }
};

//==============================================================================
// The dialog: dark scrim, centred card (560 wide, scrolls when long), title, blocks, buttons on the right. Escape or a click outside closes it.
class Modal : public juce::Component
{
public:
    struct Btn { juce::String label; bool primary = false; std::function<bool()> fn; };
    explicit Modal (const Palette& p) : pal (p) { setVisible (false); setWantsKeyboardFocus (true); addChildComponent (view); view.setScrollBarsShown (true, false); view.setViewedComponent (&body, false); view.setScrollBarThickness (8); }

    // body building (call between begin() and show())
    void begin (const juce::String& t) { title = t; clearBody(); }
    void para (const juce::String& s) { add (new TextBlock (s, TextBlock::Para, pal), 10); }
    void head (const juce::String& s) { add (new TextBlock (s, TextBlock::Head, pal), 10); }
    void bullets (const juce::StringArray& l) { for (auto& s : l) add (new TextBlock (s, TextBlock::Bullet, pal), 3); }
    void custom (juce::Component* c) { owned.add (c); body.addAndMakeVisible (c); rows.push_back ({ c, 10, c->getHeight(), true }); }
    juce::TextEditor* input (const juce::String& text, int maxLen, bool multiline, bool readOnly = false, const juce::String& hint = {})
    {
        auto* e = new juce::TextEditor(); e->setMultiLine (multiline, true); e->setReturnKeyStartsNewLine (multiline); e->setReadOnly (readOnly); e->setFont (monoCached (13.0f)); e->setTextToShowWhenEmpty (hint, col::dim);
        e->setText (text, false); if (maxLen > 0) e->setInputRestrictions (maxLen); e->onEscapeKey = [this] { close(); }; e->setSize (400, multiline ? 150 : 30); e->setIndents (8, multiline ? 6 : 6);
        custom (e); return e;
    }
    void show (std::vector<Btn> buttons)
    {
        btns.clear(); buttonRow.clear();
        for (auto& b : buttons) { auto* mb = new MBtn (b.label, MBtn::Plain, pal); mb->setLit (b.primary); mb->setSize (MBtn::widthFor (b.label, MBtn::Plain), 30); auto fn = b.fn; mb->onClick = [this, fn] { if (! fn || fn()) close(); }; buttonRow.add (mb); addChildComponent (mb); btns.push_back (std::move (b)); }
        layoutCard(); setVisible (true); toFront (true); for (auto* b : buttonRow) b->setVisible (true); view.setVisible (true);
        bool focused = false; for (auto* c : owned) if (auto* te = dynamic_cast<juce::TextEditor*> (c)) { te->grabKeyboardFocus(); if (! te->isMultiLine()) te->selectAll(); focused = true; break; }
        if (! focused) grabKeyboardFocus();
    }
    void close() { setVisible (false); juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Modal> (this)] { if (safe != nullptr && ! safe->isVisible()) safe->clearBody(); }); if (onClosed) onClosed(); }
    bool isOpen() const { return isVisible(); }
    std::function<void()> onClosed;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xaa000000));
        g.setColour (juce::Colour (0x66000000)); g.fillRoundedRectangle (card.expanded (3).toFloat().translated (0, 8), 10.0f);
        g.setColour (col::panel); g.fillRoundedRectangle (card.toFloat(), 6.0f); g.setColour (col::hover); g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 6.0f, 1.0f);
        txt (g, title.toUpperCase(), (float) card.getX() + 18, (float) card.getY() + 16 + 20, col::ink, juce::Justification::left, label (22.0f, 600, 0.14f));
    }
    void resized() override { layoutCard(); }
    void mouseDown (const juce::MouseEvent& e) override { if (! card.contains (e.getPosition())) close(); }
    bool keyPressed (const juce::KeyPress& k) override { if (k == juce::KeyPress::escapeKey) { close(); return true; } return true; }
    void inputAttemptWhenModal() override {}

private:
    struct Row { juce::Component* c; int gapAbove; int h; bool fixedH; };
    const Palette& pal; juce::String title; juce::Rectangle<int> card; juce::OwnedArray<juce::Component> owned; std::vector<Row> rows; juce::Component body; juce::Viewport view;
    std::vector<Btn> btns; juce::OwnedArray<MBtn> buttonRow; static constexpr int cardW = 560, padX = 18;

    void add (TextBlock* t, int gap) { owned.add (t); body.addAndMakeVisible (t); rows.push_back ({ t, gap, 0, false }); }
    void clearBody() { view.setViewedComponent (nullptr, false); body.removeAllChildren(); rows.clear(); owned.clear(); buttonRow.clear(); view.setViewedComponent (&body, false); }
    void layoutCard()
    {
        if (rows.empty() && buttonRow.isEmpty()) { card = {}; return; }
        const int innerW = cardW - 2 * padX - 8; int y = 0;
        for (auto& r : rows) { if (&r != &rows.front()) y += r.gapAbove; if (! r.fixedH) { r.h = dynamic_cast<TextBlock*> (r.c)->heightFor (innerW); r.c->setBounds (0, y, innerW, r.h); } else r.c->setBounds (0, y, innerW, r.h); y += r.h; }
        const int bodyH = y; const int titleH = 44, btnH = 30 + 10 + 16, maxH = std::max (200, getHeight() - 60), viewH = std::min (bodyH, maxH - titleH - btnH);
        body.setSize (innerW, bodyH); const int h = titleH + viewH + btnH + 4; card = juce::Rectangle<int> (cardW, h).withCentre (getLocalBounds().getCentre());
        view.setBounds (card.getX() + padX, card.getY() + titleH, cardW - 2 * padX, viewH); view.setScrollBarsShown (bodyH > viewH, false);
        int bx = card.getRight() - padX; for (int i = buttonRow.size() - 1; i >= 0; --i) { auto* b = buttonRow[i]; bx -= b->getWidth(); b->setTopLeftPosition (bx, card.getBottom() - 16 - 30); bx -= 8; }
        repaint();
    }
};
}
