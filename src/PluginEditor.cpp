#include "PluginEditor.h"
#include "Views.h"
#include "Controls.h"
#include "catui/RenderGate.h"

#ifndef WS_VERSION
 #define WS_VERSION "0.2.0"
#endif

using APVTS = juce::AudioProcessorValueTreeState;

namespace wsui
{
//==============================================================================
// Meters column: level bars, readouts, correlation. Child controls (Metering, RMS length, Peak reset) are added by Content.
class MeterPanel : public juce::Component
{
public:
    explicit MeterPanel (Env& e) : env (e) { setOpaque (false); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f); g.setColour (col::panel2); g.fillRoundedRectangle (r, 4.0f); g.setColour (col::line); g.drawRoundedRectangle (r, 4.0f, 1.0f);
        auto& P = env.p; const auto& S = *env.pal; const bool two = P.page == 3; const juce::String mm (P.choiceRaw ("mmode")); const bool lufs = mm == "lufs";
        txt (g, two ? "IN / OUT" : "LEVEL", 11, 21, col::dim, juce::Justification::left, label (12.0f, 500, 0.2f));
        txt (g, "CLIP", 210, 21, col::dim, juce::Justification::right, label (12.0f, 500, 0.2f));
        const bool clip = env.anyClip(); const juce::Colour ledC = clip ? juce::Colour (0xffff2d95) : juce::Colour (0xff3a1020);
        if (clip) { g.setColour (ledC.withAlpha (0.35f)); g.fillEllipse (211, 11, 16, 16); } g.setColour (ledC); g.fillEllipse (214, 14, 10, 10);
        paintLevels (g, S, lufs, two);
        const auto& o = env.out; auto fd = [] (float v) { return v <= -119 ? juce::String ("-inf") : juce::String (v, 1); };
        std::vector<std::pair<juce::String, juce::String>> ro;
        if (lufs) ro = { { "Moment.", fd (P.lufsOut.momentary()) }, { "Short", fd (P.lufsOut.shortTerm()) }, { "Integr.", fd (P.lufsOut.integrated()) }, { "Max M", fd (P.lufsOut.maxMomentary()) } };
        else
        {
            const float crest = std::max (o.pk[0], o.pk[1]) - std::max (o.rms[0], o.rms[1]);
            ro = { { "Peak L", fd (o.pk[0]) }, { "Peak R", fd (o.pk[1]) }, { "RMS L", fd (o.rms[0]) }, { "RMS R", fd (o.rms[1]) }, { "Crest dB", env.silent ? juce::String ("-") : juce::String (crest, 1) }, { "Held", fd (std::max (o.hold[0], o.hold[1])) } };
        }
        drawRo (g, ro, 349);
        if (lufs) mtxt (g, "LUFS = loudness, K-weighted", 11, 349 + 17 * 2 + 4, col::dim, juce::Justification::left, 11);
        txt (g, "CORRELATION", 11, 549, col::dim, juce::Justification::left, label (12.0f, 500, 0.2f));
        mtxt (g, env.silent ? juce::String ("-") : juce::String (env.corr, 2), 224, 549, S.acc, juce::Justification::right, 11);
        paintCorr (g, S);
        std::vector<std::pair<juce::String, juce::String>> r2 = { { "Width", env.silent ? juce::String ("-") : juce::String ((int) std::lround (env.width)) + " %" }, { "Bal.", env.silent ? juce::String ("-") : juce::String (env.bal > 0 ? "L " : "R ") + juce::String (std::abs (env.bal), 1) },
                                                                  { "Mono dB", env.silent ? juce::String ("-") : juce::String (env.monoLoss, 1) }, { "Corr.", env.silent ? juce::String ("-") : juce::String (env.corr, 2) } };
        drawRo (g, r2, 619);
        const int v = env.verdict; const juce::String msg = v < 0 ? "No signal" : v == 2 ? "Phase problem: weak in mono" : v == 1 ? "Check mono: some loss" : "Mono safe"; const juce::Colour vc = v < 0 ? col::dim : v == 2 ? S.r : v == 1 ? S.acc : S.l;
        txt (g, msg.toUpperCase(), (float) getWidth() * 0.5f, 660, vc, juce::Justification::centred, label (14.0f, 500, 0.08f));
    }
private:
    Env& env;
    void drawRo (juce::Graphics& g, const std::vector<std::pair<juce::String, juce::String>>& v, float y0)
    {
        for (size_t i = 0; i < v.size(); ++i)
        {
            const float x0 = (i % 2 == 0) ? 11.0f : 11.0f + 101.5f + 10.0f, x1 = x0 + 101.5f, yy = y0 + 4.0f + 17.0f * (float) (i / 2);
            mtxt (g, v[i].first, x0, yy, col::dim, juce::Justification::left, 11); mtxt (g, v[i].second, x1, yy, env.pal->acc, juce::Justification::right, 11);
        }
    }
    static juce::Colour lvlCol (float d, float warn, float hot)
    {
        const juce::Colour blue (47, 125, 255), purple (178, 77, 255), pink (255, 45, 149);
        if (d < warn - 3) return blue; if (d < warn) return blue.interpolatedWith (purple, (d - (warn - 3)) / 3); if (d < hot - 2) return purple; if (d < hot) return purple.interpolatedWith (pink, (d - (hot - 2)) / 2); return pink;
    }
    struct Ch { float fill, mark, hold; bool clipped; const char* lab; juce::Colour col; };
    void neonBar (juce::Graphics& g, float x, float bw, float top, float ph, const Ch& ch, float warn, float hot)
    {
        const float seg = 1.5f; auto Y = [&] (float d) { return top + ph * (1.0f - (juce::jlimit (-60.0f, 0.0f, d) + 60.0f) / 60.0f); };
        for (int i = 0; i < 40; ++i)
        {
            const float d0 = -60.0f + (float) i * seg, d1 = d0 + seg, y1 = Y (d1), y0 = Y (d0), hh = std::max (1.0f, y0 - y1 - 1); const auto c = lvlCol ((d0 + d1) / 2, warn, hot);
            const bool lit = ch.fill >= d0 + seg * 0.5f, isMark = ch.mark > -119 && ch.mark >= d0 && ch.mark < d1 && ch.mark > ch.fill + 0.5f;
            if (lit) { g.setColour (c.withAlpha (0.22f)); g.fillRect (x - 1, y1, bw + 2, hh + 2); g.setColour (c); } else if (isMark) { g.setColour (juce::Colours::white.withAlpha (0.25f)); g.fillRect (x - 1, y1, bw + 2, hh + 2); g.setColour (juce::Colours::white.withAlpha (0.93f)); } else g.setColour (c.withAlpha (0.11f));
            g.fillRect (x, y1 + 1, bw, hh);
        }
        if (ch.hold > -59) { const auto hc = ch.clipped ? juce::Colour (255, 45, 149) : lvlCol (ch.hold, warn, hot); g.setColour (hc.withAlpha (0.3f)); g.fillRect (x - 1, Y (ch.hold) - 2, bw + 2, 4.0f); g.setColour (hc); g.fillRect (x, Y (ch.hold) - 1, bw, 2.0f); }
        if (ch.clipped) { g.setColour (juce::Colour (255, 45, 149).withAlpha (0.3f)); g.fillRect (x - 2, top - 12, bw + 4, 10.0f); }
        g.setColour (ch.clipped ? juce::Colour (255, 45, 149) : juce::Colour (255, 45, 149).withAlpha (0.13f)); g.fillRect (x, top - 10, bw, 6.0f);
        mtxt (g, ch.lab, x + bw / 2, top + ph + 14, ch.col, juce::Justification::horizontallyCentred, 12);
    }
    void paintLevels (juce::Graphics& g, const Scheme& S, bool lufs, bool two)
    {
        auto& P = env.p; const float W = 213, H = 303, top = 24, bot = H - 20, ph = bot - top, warn = lufs ? -14.0f : (float) env.st.warnDb, hot = lufs ? -8.0f : -0.5f;
        g.saveState(); g.setOrigin (11, 32); g.reduceClipRegion (0, 0, 213, 303);
        auto Y = [&] (float d) { return top + ph * (1.0f - (juce::jlimit (-60.0f, 0.0f, d) + 60.0f) / 60.0f); };
        g.fillAll (col::disp);
        const juce::String mm (P.choiceRaw ("mmode"));
        auto chans = [&] (const Lv& o, bool isIn) -> std::array<Ch, 2>
        {
            if (lufs) { auto& L = isIn ? P.lufsIn : P.lufsOut; return { Ch { L.momentary(), -120, L.maxMomentary(), false, "M", S.l }, Ch { L.shortTerm(), -120, -120, false, "S", S.r } }; }
            std::array<Ch, 2> a; for (int c = 0; c < 2; ++c) a[(size_t) c] = Ch { mm == "peak" ? o.disp[c] : o.rms[c], mm == "both" ? o.disp[c] : -120.0f, o.hold[c], o.clip[c], c ? "R" : "L", c ? S.r : S.l }; return a;
        };
        const auto A = chans (env.out, false); const float sw = 26; float bw; float xs[4] {};
        if (two) { bw = std::floor (((W - 12 - sw) / 2 - 3) / 2); xs[0] = 4; xs[1] = 4 + bw + 3; xs[2] = 4 + 2 * bw + 3 + 2 + sw + 2; xs[3] = 4 + 3 * bw + 6 + 2 + sw + 2; }
        else { bw = std::floor ((W - 40) / 2); xs[0] = 4; xs[1] = W - bw - 4; }
        g.setColour (juce::Colour (0xff0a0d11)); for (int i = 0; i < (two ? 4 : 2); ++i) g.fillRect (xs[i], top - 12, bw, ph + 12);
        const float cx = two ? 4 + 2 * bw + 3 + 1 + sw / 2 + 1 : W / 2;
        if (two)
        {
            const auto B = chans (env.in, true); neonBar (g, xs[0], bw, top, ph, B[0], warn, hot); neonBar (g, xs[1], bw, top, ph, B[1], warn, hot); neonBar (g, xs[2], bw, top, ph, A[0], warn, hot); neonBar (g, xs[3], bw, top, ph, A[1], warn, hot);
            mtxt (g, "IN", xs[0] + bw + 1.5f, 9, juce::Colours::white, juce::Justification::horizontallyCentred, 11); mtxt (g, "OUT", xs[2] + bw + 1.5f, 9, juce::Colours::white, juce::Justification::horizontallyCentred, 11);
        }
        else { neonBar (g, xs[0], bw, top, ph, A[0], warn, hot); neonBar (g, xs[1], bw, top, ph, A[1], warn, hot); mtxt (g, lufs ? "LUFS" : "dBFS", W / 2, 9, col::key, juce::Justification::horizontallyCentred, 10); }
        const int last = two ? 3 : 1;
        for (int d = 0; d >= -60; d -= 6)
        {
            const float y = Y ((float) d); mtxt (g, juce::String (d), cx, y + 3, juce::Colour (0xffe9d9a0), juce::Justification::horizontallyCentred, 10);
            g.setColour (juce::Colours::white.withAlpha (0.1f)); g.fillRect (xs[0], std::floor (y), bw, 1.0f); g.fillRect (xs[last], std::floor (y), bw, 1.0f);
        }
        if (lufs) { const float li = P.lufsOut.integrated(); if (li > -100) { const float a = two ? xs[2] : xs[0], b = two ? xs[3] + bw : xs[1] + bw; g.setColour (S.acc); g.fillRect (a, Y (li) - 0.25f, b - a, 1.5f); mtxt (g, "I", a - 2, Y (li) - 3, S.acc, juce::Justification::right, 10); } }
        g.restoreState();
    }
    void paintCorr (juce::Graphics& g, const Scheme& S)
    {
        g.setColour (col::disp); g.fillRoundedRectangle (11.0f, 561.0f, 213.0f, 46.0f, 3.0f); g.setColour (col::line); g.drawRoundedRectangle (11.5f, 561.5f, 212.0f, 45.0f, 3.0f, 1.0f);
        g.saveState(); g.setOrigin (12, 562); const float w2 = 211; auto X = [&] (float v) { return 8 + (w2 - 16) * (v + 1) / 2; };
        g.setColour (juce::Colour (0xff101418)); g.fillRect (8.0f, 18.0f, w2 - 16, 12.0f);
        if (! env.silent) { g.setColour (env.corr >= 0 ? S.l : S.r); g.fillRect (std::min (X (0), X (env.corr)), 18.0f, std::abs (X (env.corr) - X (0)), 12.0f); }
        for (float v : { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f })
        {
            g.setColour (juce::Colours::white.withAlpha (0.33f)); g.fillRect (std::floor (X (v)), 14.0f, 1.0f, 20.0f);
            mtxt (g, v == 0 ? juce::String ("0") : (v > 0 ? "+" : "") + juce::String (v, (v == 0.5f || v == -0.5f) ? 1 : 0), X (v), 44, S.acc, v == -1 ? juce::Justification::left : v == 1 ? juce::Justification::right : juce::Justification::horizontallyCentred, 9);
        }
        mtxt (g, "Antiphase", 8, 11, S.r, juce::Justification::left, 9); mtxt (g, "Stereo", w2 / 2, 11, col::key, juce::Justification::horizontallyCentred, 9); mtxt (g, "Mono", w2 - 8, 11, S.l, juce::Justification::right, 9);
        g.restoreState();
    }
};

//==============================================================================
// The preset name box in the header.
class PresetBox : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PresetBox (const Palette& p) : pal (p) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    std::function<void()> onClick; juce::String text;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f); g.setColour (juce::Colour (0xff0a0a0c)); g.fillRoundedRectangle (r, 3.0f); g.setColour (isMouseOver() ? col::hover : col::line); g.drawRoundedRectangle (r, 3.0f, 1.0f);
        g.setColour (col::ink); g.setFont (monoCached (14.0f)); g.drawText (text, 12, 0, getWidth() - 36, getHeight(), juce::Justification::centredLeft, true);
        juce::Path t; const float cx = (float) getWidth() - 14.0f, cy = (float) getHeight() * 0.5f; t.addTriangle (cx - 3.5f, cy - 2, cx + 3.5f, cy - 2, cx, cy + 3); g.setColour (pal->l); g.fillPath (t);
    }
    void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
private:
    const Palette& pal;
};

//==============================================================================
class Content : public juce::Component
{
public:
    Content (WhiskerScopeEditor& ed, WhiskerScopeProcessor& pr);
    ~Content() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override { grabKeyboardFocus(); }
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& k) override;
    void setPage (int p);
    void tickNow() { tick(); }                         // used by the screenshot tool
    Env& envRef() { return env; }
    void showSettings(); void showAbout(); void showGuide();
    void startFrames() { gate.start (settings.eco ? 12 : 30); }

private:
    class Sugg;
    WhiskerScopeEditor& editor; WhiskerScopeProcessor& p; APVTS& apvts; Env env; WhiskerScopeProcessor::Settings settings;
    WsLook look; std::unique_ptr<juce::TooltipWindow> tips; catui::RenderGate gate { *this };
    std::vector<std::unique_ptr<APVTS::ComboBoxAttachment>> combos; std::vector<std::unique_ptr<APVTS::SliderAttachment>> sliders; std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttons;
    juce::OwnedArray<juce::Component> owned;

    // header
    juce::Path logoPath; MBtn *bPrev = nullptr, *bNext = nullptr, *bSave = nullptr, *bMore = nullptr, *bPanic = nullptr, *bGuide = nullptr, *bSettings = nullptr, *bAbout = nullptr; PresetBox presetBox { env.pal };
    // rig
    MKnob *kGain = nullptr, *kScGain = nullptr; Field* fColor = nullptr;
    // tabs and pages
    std::unique_ptr<MBtn> tabs[4]; std::unique_ptr<ViewBase> views[4]; std::unique_ptr<Flow> panels[4]; Flow* shapesGroups[3] {};
    std::unique_ptr<MeterPanel> meters; Field *fMeter = nullptr, *fRms = nullptr; MBtn* bPkReset = nullptr;
    Modal modal { env.pal };
    int shownPage = -1; double lastTick = 0; int labelTick = 0; juce::String lastSync; int lastColor = -1; bool lastMuted = false;
    // shapes
    Field* fShape = nullptr; MBtn *bCapRef = nullptr, *bCanRef = nullptr, *bCapMine = nullptr, *bCanMine = nullptr, *bRefOn = nullptr; StatusText *stRef = nullptr, *stMine = nullptr, *stAuto = nullptr; Sugg* sugg = nullptr;
    std::vector<ws::Shape> shapeList; juce::StringArray factoryNames;
    Flow* scopeFlow = nullptr; juce::Component *hostDur = nullptr, *hostFreq = nullptr, *hostPer = nullptr, *hostDiv = nullptr;

    const wsp::ChoiceDef& cd (const char* id) const { for (auto& c : wsp::choices()) if (std::strcmp (c.id, id) == 0) return c; jassertfalse; return wsp::choices()[0]; }
    const wsp::NumDef& nd (const char* id) const { for (auto& c : wsp::nums()) if (std::strcmp (c.id, id) == 0) return c; jassertfalse; return wsp::nums()[0]; }
    const wsp::FlagDef& fd (const char* id) const { for (auto& c : wsp::flags()) if (std::strcmp (c.id, id) == 0) return c; jassertfalse; return wsp::flags()[0]; }
    Field* mkField (const char* id, const char* lab, int w);
    MKnob* mkKnob (const char* id, const char* lab, int w = MKnob::kWidth);
    MBtn* mkToggle (const char* id, const char* lab, int w);
    MBtn* mkButton (const juce::String& text, int w, const juce::String& tipHead, const juce::String& tip, std::function<void()> fn, MBtn::Kind k = MBtn::Plain);
    template <class T> T* keep (T* c) { owned.add (c); return c; }
    void buildHeader(); void buildRig(); void buildPages(); void buildShapes(); void buildMeters();
    void layoutFixed();
    void resized() override;
    void tick();
    void syncFromProcessor();
    void updateShapesStatus();
    void panic();
    void resetPeaksAll() { env.resetPeaks(); static_cast<SpecView*> (views[0].get())->resetPeaks(); }
    void refreshShapeList (const juce::String& select = {});
    void presetMenu(); void saveDialog(); void moreDialog(); void note (const juce::String& s);
    void copyDialog (const juce::String& text, const juce::String& what);
    void importDialog (bool preset);
    void relayoutScope();
    void applySettings();
    friend class WhiskerScopeEditor;
};

// Suggestion line under the Match group.
class Content::Sugg : public juce::Component
{
public:
    explicit Sugg (Content& c) : owner (c) { setSize (900, 18); setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override
    {
        auto& P = owner.p; const auto& V = P.shapes(); const auto& S = *owner.env.pal; const int ch = std::string (P.choiceRaw ("shChan")) == "s" ? 1 : 0; const float* arr = V.eq[ch]; const auto lab = label (15.0f, 500, 0.03f);
        float x = 0; const float y = 13;
        auto put = [&] (const juce::String& s, juce::Colour c, const juce::Font& f) { txt (g, s, x, y, c, juce::Justification::left, f); x += textWidth (s, f); };
        if (! V.hasRefShown) { put (V.hasRef ? "Reference is switched off. Turn it on to get suggestions." : "Pick or capture a reference shape to get suggestions.", col::ink, lab); return; }
        std::vector<int> idx; for (int k = 0; k < ws::kBands; ++k) { const float a = std::abs (arr[k]), pa = k > 0 ? std::abs (arr[k - 1]) : 0.0f, na = k < ws::kBands - 1 ? std::abs (arr[k + 1]) : 0.0f; if (a >= 1.5f && a >= pa && a >= na) idx.push_back (k); }
        std::sort (idx.begin(), idx.end(), [&] (int a, int b) { return std::abs (arr[a]) > std::abs (arr[b]); });
        std::vector<int> pick; for (int i : idx) { bool ok = true; for (int j : pick) if (std::abs (j - i) < 4) ok = false; if (ok && pick.size() < 4) pick.push_back (i); } std::sort (pick.begin(), pick.end());
        const juce::String name = ch == 0 ? "Mid" : "Side";
        if (pick.empty()) { put (name + " already close to the reference. No move bigger than 1.5 dB.", col::ink, lab); return; }
        put (name + " EQ moves: ", col::ink, lab); bool first = true;
        for (int i : pick)
        {
            if (! first) put (juce::String::fromUTF8 (" \xc2\xb7 "), col::ink, lab);
            first = false; const double f = ws::bandCentreHz (i);
            put (juce::String (arr[i] > 0 ? "Boost " : "Cut ") + (f >= 1000 ? juce::String (f / 1000.0, 1) + " kHz" : juce::String ((int) std::lround (f)) + " Hz") + " " + (arr[i] > 0 ? "+" : "") + juce::String (arr[i], 1) + " dB", arr[i] > 0 ? S.l : S.r, label (15.0f, 700, 0.03f));
        }
    }
private:
    Content& owner;
};

//==============================================================================
Content::Content (WhiskerScopeEditor& ed, WhiskerScopeProcessor& pr) : editor (ed), p (pr), apvts (pr.apvts), env (pr), settings (WhiskerScopeProcessor::loadSettings())
{
    setSize (WhiskerScopeEditor::kDesignW, WhiskerScopeEditor::kDesignH); setWantsKeyboardFocus (true); setOpaque (true);
    setLookAndFeel (&look); env.st = settings; env.pal.s = &scheme (p.choiceIndex ("color"));
    buildHeader(); buildRig(); buildPages(); buildMeters();
    addChildComponent (modal); modal.onClosed = [this] { grabKeyboardFocus(); };
    tips = std::make_unique<juce::TooltipWindow> (this, 1000); tips->setLookAndFeel (&look);
    applySettings(); setPage (juce::jlimit (0, 3, p.page)); layoutFixed(); refreshShapeList();
    gate.onFrame = [this] { tick(); }; startFrames();
}

Content::~Content() { gate.stop(); if (tips) tips->setLookAndFeel (nullptr); setLookAndFeel (nullptr); p.setShapesWatched (false); }

void Content::applySettings() { if (tips) tips->setMillisecondsBeforeTipAppears (settings.bubbles ? 1000 : 1000000000); env.st = settings; }

//------------------------------------------------------------------------------
Field* Content::mkField (const char* id, const char* lab, int w)
{
    auto& d = cd (id); juce::StringArray items; for (auto* s : d.labels) items.add (s); const juce::String l = lab ? lab : d.label;
    auto* f = keep (new Field (l, items)); f->setSize (w > 0 ? w : Field::widthFor (items, l), Field::kHeight); f->setTip (l + "\n" + d.tip);
    combos.push_back (std::make_unique<APVTS::ComboBoxAttachment> (apvts, id, f->box)); return f;
}
MKnob* Content::mkKnob (const char* id, const char* lab, int w)
{
    auto& d = nd (id); const juce::String l = lab ? lab : d.label; auto* k = keep (new MKnob (l, d.fmt, env.pal, w));
    sliders.push_back (std::make_unique<APVTS::SliderAttachment> (apvts, id, *k)); k->setDoubleClickReturnValue (true, d.def); k->setTooltip (l + "\n" + d.tip + " Double-click resets."); return k;
}
MBtn* Content::mkToggle (const char* id, const char* lab, int w)
{
    auto& d = fd (id); const juce::String l = lab ? lab : d.label; auto* b = keep (new MBtn (l, MBtn::Toggle, env.pal)); b->setSize (w, 30); b->setTooltip (l + "\n" + d.tip);
    buttons.push_back (std::make_unique<APVTS::ButtonAttachment> (apvts, id, *b)); return b;
}
MBtn* Content::mkButton (const juce::String& text, int w, const juce::String& tipHead, const juce::String& tip, std::function<void()> fn, MBtn::Kind k)
{
    auto* b = keep (new MBtn (text, k, env.pal)); b->setSize (w > 0 ? w : MBtn::widthFor (text, k), 30); if (tip.isNotEmpty()) b->setTooltip ((tipHead.isNotEmpty() ? tipHead : text) + "\n" + tip); b->onClick = std::move (fn); return b;
}

void Content::buildHeader()
{
    logoPath = juce::Drawable::parseSVGPath ("M4.5 5C10 7 17 11 21 14c3-.5 7-2 11-2s8 1.5 11 2C47 11 55 7 60 5.5L61 24C61 40 48 50 32 58 16 50 3 40 3.7 24z");
    auto step = [this] (int d) { auto a = p.allPresets(); int i = 0; for (size_t k = 0; k < a.size(); ++k) if (a[k].name == p.presetName) i = (int) k; p.loadPreset (a[(size_t) (((i + d) % (int) a.size() + (int) a.size()) % (int) a.size())].name); };
    bPrev = mkButton (juce::String::fromUTF8 ("\xe2\x80\xb9"), 28, "Previous preset", "Previous preset.", [step] { step (-1); });
    bNext = mkButton (juce::String::fromUTF8 ("\xe2\x80\xba"), 29, "Next preset", "Next preset.", [step] { step (1); });
    bSave = mkButton ("Save", 62, "Save preset", "Save the current settings as your own preset (stays on this device).", [this] { saveDialog(); });
    bMore = mkButton (juce::String::fromUTF8 ("\xe2\x8b\xaf"), 37, "More", "Rename, delete, export or import presets.", [this] { moreDialog(); });
    bPanic = mkButton ("Panic", 69, "Panic", "PANIC: mutes the output at once, cancels any capture and turns the matching EQ off. Press again to unmute. Shortcut: P.", [this] { panic(); }, MBtn::Panic);
    bGuide = mkButton ("Guide", 68, "Guide", "Quick guide: what each view and control does.", [this] { showGuide(); });
    bSettings = mkButton ("Settings", 98, "Settings", "Info bubbles, colours, power saving, meter warning level, window size.", [this] { showSettings(); });
    bAbout = mkButton ("About & Support", 166, "About & Support", "About Studio 87, privacy, installing, and ways to support the project.", [this] { showAbout(); });
    for (auto* b : { bPrev, bNext, bSave, bMore, bPanic, bGuide, bSettings, bAbout }) addAndMakeVisible (b);
    presetBox.setTooltip ("Presets\nOpen the preset list: factory presets and your own."); presetBox.onClick = [this] { presetMenu(); }; addAndMakeVisible (presetBox);
}

void Content::buildRig()
{
    kGain = mkKnob ("gain", "Input gain"); kScGain = mkKnob ("scGain", "SC gain"); fColor = mkField ("color", nullptr, 144);
    addAndMakeVisible (kGain); addAndMakeVisible (kScGain); addAndMakeVisible (fColor);
}

void Content::buildPages()
{
    const auto flowPanel = [&] (int i, int h) { panels[i] = std::make_unique<Flow> (col::panel2, 11, 9, 11, 9, 14, 8); panels[i]->setSize (1026, h); addChildComponent (*panels[i]); return panels[i].get(); };
    { auto* f = flowPanel (0, 118);
      f->add (mkField ("ana", nullptr, 101)); f->add (mkField ("fft", nullptr, 77)); f->add (mkField ("speed", nullptr, 87)); f->add (mkField ("range", nullptr, 83)); f->add (mkField ("slope", nullptr, 102));
      f->add (mkField ("pkSpd", nullptr, 95)); f->add (mkField ("pkN", "Peak labels", 96)); f->add (mkField ("maskDb", nullptr, 118)); f->add (mkField ("bands", nullptr, 104)); f->add (mkField ("scale", nullptr, 77));
      f->add (mkToggle ("style", nullptr, 56)); f->add (mkToggle ("wf", nullptr, 111)); f->add (mkToggle ("rot", nullptr, 82)); f->add (mkToggle ("hold", nullptr, 112));
      f->add (mkButton ("Peak reset", 118, "Peak reset", "Clears the held peaks and the clip light. Shortcut: R.", [this] { resetPeaksAll(); }));
      f->add (mkToggle ("fzSpec", nullptr, 81)); }
    { auto* f = flowPanel (1, 92); scopeFlow = f;
      f->add (mkField ("chan", nullptr, 102)); f->add (mkField ("sync", nullptr, 105));
      f->add (hostDur = mkKnob ("dur", nullptr)); f->add (hostFreq = mkKnob ("freq", nullptr)); f->add (hostPer = mkKnob ("periods", nullptr)); f->add (hostDiv = mkField ("div", nullptr, 104));
      f->add (mkKnob ("trig", nullptr)); f->add (mkField ("pkSpd", nullptr, 95)); f->add (mkField ("pkN", nullptr, 86)); f->add (mkToggle ("trans", nullptr, 118)); f->add (mkToggle ("fzScope", nullptr, 82)); }
    { auto* f = flowPanel (2, 92); f->add (mkKnob ("pers", nullptr)); f->add (mkKnob ("zoom", nullptr)); f->add (mkToggle ("fzSt", nullptr, 82)); }
    buildShapes();
    views[0] = std::make_unique<SpecView> (env); views[1] = std::make_unique<ScopeView> (env); views[2] = std::make_unique<StereoView> (env); views[3] = std::make_unique<ShapesView> (env);
    static const char* const names[4] = { "Spectrum", "Scope", "Stereo field", "Shapes" };
    static const char* const tp[4] = { "Spectrum analyser with optional waterfall. Shortcut: 1.", "Waveform scope. Shortcut: 2.", "Stereo field: goniometer, phase correlation and mono check. Shortcut: 3.", "Freeze the spectrum shape of a song or lead, save it, and see the EQ that moves your sound toward it. Shortcut: 4." };
    for (int i = 0; i < 4; ++i)
    {
        addChildComponent (*views[i]); tabs[i] = std::make_unique<MBtn> (names[i], MBtn::Tab, env.pal); tabs[i]->setTooltip (juce::String (names[i]) + "\n" + tp[i]); tabs[i]->setSize (252, 37);
        tabs[i]->onClick = [this, i] { setPage (i); }; addAndMakeVisible (*tabs[i]);
    }
}

void Content::buildMeters()
{
    meters = std::make_unique<MeterPanel> (env); addAndMakeVisible (*meters); meters->setInterceptsMouseClicks (false, true);
    fMeter = mkField ("mmode", nullptr, 213); fRms = mkField ("rms", nullptr, 213); meters->addAndMakeVisible (fMeter); meters->addAndMakeVisible (fRms);
    bPkReset = mkButton ("Peak reset", 213, "Peak reset", "Clears held peaks, the clip light and the crest figure. Shortcut: R.", [this] { resetPeaksAll(); }); meters->addAndMakeVisible (bPkReset);
}

void Content::buildShapes()
{
    panels[3] = std::make_unique<Flow> (col::panel2, 0, 0, 0, 0, 0, 0); panels[3]->setSize (1026, 403); addChildComponent (*panels[3]);
    auto group = [&] (int i, const char* num, const char* title, const char* sub) { auto* g = keep (new Flow (juce::Colour (0xff121317), 10, 8, 10, 9, 10, 6)); shapesGroups[i] = g; panels[3]->addAndMakeVisible (g); g->add (keep (new GroupHead (num, title, sub, env.pal)), true); return g; };
    auto* g1 = group (0, "1", "Reference", "the sound you want yours to sound like"); auto* g2 = group (1, "2", "Mine", "your own sound; skip this and the live sound is used"); auto* g3 = group (2, "3", "Match", "the EQ that moves mine toward the reference, with level control");
    bCapRef = mkButton ("Capture reference", 188, "Capture reference", "Play the song or lead you want to sound like, press this, let it play, then press again. Its average spectrum shape and loudness are frozen as the reference.", [this] { if (p.shapes().capturing) { p.stopCapture(); note (p.takeNote()); } else p.startCapture (true); }); g1->add (bCapRef);
    bCanRef = mkButton ("Cancel", 70, "Cancel capture", "Throw the running capture away and keep what you had before.", [this] { p.cancelCapture(); }); g1->add (bCanRef); bCanRef->setVisible (false);
    { juce::StringArray none; fShape = keep (new Field ("Or pick a saved shape", none)); fShape->setSize (185, Field::kHeight); fShape->setTip ("Shape library\nStarting-point shapes plus the ones you saved."); g1->add (fShape); }
    g1->add (mkButton ("Use as reference", 172, "Use as reference", "Loads the chosen shape as the target.", [this] { const int id = fShape->box.getSelectedId() - 1000; if (id >= 0 && id < (int) shapeList.size()) p.useAsReference (shapeList[(size_t) id]); }));
    g1->add (mkButton ("Save as...", 101, "Save reference as", "Keeps the current reference (shape and loudness) in your shape library on this device.", [this]
    {
        if (! p.shapes().hasRef) { note ("There is no reference yet. Capture one or pick a shape first."); return; }
        const juce::String cur = juce::String (p.shapes().ref.name); modal.begin ("Save shape"); modal.para ("Name for this shape:"); auto* in = modal.input (cur == "Captured reference" ? juce::String ("My shape") : cur, 40, false);
        modal.show ({ { "Cancel" }, { "Save", true, [this, in] { auto n = WhiskerScopeProcessor::cleanName (in->getText()); if (n.isEmpty()) return false; auto s = p.shapes().ref; s.name = n.toStdString(); if (! p.saveUserShape (s)) { juce::MessageManager::callAsync ([this] { note ("The shape could not be saved. Check that your user folder is writable."); }); return true; } p.renameReference (n); refreshShapeList (n); return true; } } });
    }));
    g1->add (mkButton ("Manage...", 101, "Manage shapes", "Delete a saved shape, export the reference as text, or import a shape you were sent.", [this]
    {
        const int id = fShape->box.getSelectedId() - 1000; const bool valid = id >= 0 && id < (int) shapeList.size(); const juce::String nm = valid ? juce::String (shapeList[(size_t) id].name) : juce::String(); const bool user = valid && ! factoryNames.contains (nm);
        modal.begin ("Shapes"); modal.para ("Selected in the library: **" + nm + "**" + (user ? "" : " (starting points cannot be deleted)"));
        modal.show ({ { "Delete selected", false, [this, user, nm] { if (! user) return false; p.deleteUserShape (nm); refreshShapeList(); return true; } },
                      { "Export reference", false, [this] { if (! p.shapes().hasRef) return false; juce::MessageManager::callAsync ([this] { copyDialog (WhiskerScopeProcessor::shapeToText (p.shapes().ref), "The shape"); }); return true; } },
                      { "Import", false, [this] { juce::MessageManager::callAsync ([this] { importDialog (false); }); return true; } }, { "Close" } });
    }));
    bRefOn = mkToggle ("shRefOn", nullptr, 140); g1->add (bRefOn);
    g1->add (mkButton ("Rename...", 100, "Rename reference", "Gives the current reference a name of your choice.", [this]
    {
        if (! p.shapes().hasRef) { note ("There is no reference to rename."); return; }
        modal.begin ("Rename reference"); modal.para ("New name:"); auto* in = modal.input (juce::String (p.shapes().ref.name), 40, false);
        modal.show ({ { "Cancel" }, { "Rename", true, [this, in] { auto n = WhiskerScopeProcessor::cleanName (in->getText()); if (n.isEmpty()) return false; p.renameReference (n); return true; } } });
    }));
    g1->add (mkButton ("Clear reference", 169, "Clear reference", "Removes the reference. Mine, the EQ settings and everything else stay. Save it first (Save as) if you want to keep it.", [this]
    {
        if (! p.shapes().hasRef) return; modal.begin ("Clear reference?"); modal.para ("The reference **" + juce::String (p.shapes().ref.name) + "** will be removed from the plugin. A saved copy in your library stays.");
        modal.show ({ { "Cancel" }, { "Clear", true, [this] { p.clearReference(); return true; } } });
    }));
    g1->add (mkField ("shRefTilt", "Ref tilt", 98)); stRef = keep (new StatusText (env.pal)); g1->add (stRef, true);
    bCapMine = mkButton ("Capture mine", 137, "Capture mine", "Play your own sound, press this, let it play, then press again. Its shape is frozen so you can compare while you move on.", [this] { if (p.shapes().capturing) { p.stopCapture(); note (p.takeNote()); } else p.startCapture (false); }); g2->add (bCapMine);
    bCanMine = mkButton ("Cancel", 70, "Cancel capture", "Throw the running capture away and keep what you had before.", [this] { p.cancelCapture(); }); g2->add (bCanMine); bCanMine->setVisible (false);
    g2->add (mkButton ("Clear mine", 111, "Clear mine", "Forget the captured shape of your sound and compare the live sound again.", [this] { p.clearMine(); }));
    g2->add (mkField ("shLen", nullptr, 128)); stMine = keep (new StatusText (env.pal)); g2->add (stMine, true);
    g3->add (mkField ("shChan", nullptr, 67)); g3->add (mkField ("shSmooth", nullptr, 110)); g3->add (mkKnob ("shStrength", nullptr, 76)); g3->add (mkKnob ("shLimit", nullptr, 76)); g3->add (mkToggle ("shHear", nullptr, 72));
    { auto* k = mkKnob ("gain", "Input", 76); k->setTooltip ("Input\nLevel into the plugin (same knob as the Input gain at the top). Double-click resets."); g3->add (k); }
    g3->add (mkKnob ("shOut", nullptr, 76)); g3->add (mkField ("shAuto", nullptr, 153)); stAuto = keep (new StatusText (env.pal)); stAuto->setSize (190, 16); g3->add (stAuto);
    g3->add (mkButton ("Copy EQ points", 150, "Copy EQ points", "Copies the matching EQ as a list of bells (frequency, gain, Q) for Mid and Side, to type into any EQ.", [this]
    {
        const auto& V = p.shapes(); juce::StringArray lines; const double fs = p.sampleRateNow.load();
        for (int ch = 0; ch < 2; ++ch)
        {
            float gains[ws::kBells]; ws::designBells (V.eq[ch], fs, gains); lines.add (juce::String (ch == 0 ? "Mid" : "Side") + " EQ (bells, Q " + juce::String (ws::kBellQ, 1) + "):");
            bool any = false; for (int i = 0; i < ws::kBells; ++i) if (std::abs (gains[i]) >= 0.1f) { any = true; const double f = ws::bellHz (i); lines.add ("  " + (f >= 1000 ? juce::String (f / 1000.0, 2) + " kHz" : juce::String (f, 0) + " Hz") + ": " + (gains[i] >= 0 ? "+" : "") + juce::String (gains[i], 1) + " dB"); }
            if (! any) lines.add ("  no moves");
        }
        copyDialog (lines.joinIntoString ("\n"), "The EQ points");
    }));
    sugg = keep (new Sugg (*this)); g3->add (sugg, true);
}

//------------------------------------------------------------------------------
void Content::layoutFixed()
{
    bPrev->setBounds (390, 17, 28, 30); presetBox.setBounds (425, 16, 306, 32); bNext->setBounds (738, 17, 29, 30); bSave->setBounds (774, 17, 62, 30); bMore->setBounds (842, 17, 37, 30);
    bPanic->setBounds (896, 17, 69, 30); bGuide->setBounds (972, 17, 68, 30); bSettings->setBounds (1047, 17, 98, 30); bAbout->setBounds (1150, 17, 166, 30);
    kGain->setTopLeftPosition (834, 83); kScGain->setTopLeftPosition (926, 83); fColor->setTopLeftPosition (1018, 99);
    for (int i = 0; i < 4; ++i) tabs[i]->setTopLeftPosition (44 + i * 258, 202);
    static const int panelH[4] = { 118, 92, 92, 403 };
    for (int i = 0; i < 4; ++i) { const int top = 874 - panelH[i]; panels[i]->setBounds (44, top, 1026, panelH[i]); views[i]->setBounds (44, 247, 1026, top - 8 - 247); }
    shapesGroups[0]->setBounds (11, 10, 563, 196); shapesGroups[1]->setBounds (583, 10, 431, 196); shapesGroups[2]->setBounds (11, 216, 1003, 178);
    for (int i = 0; i < 3; ++i) shapesGroups[i]->relayout();
    for (int i = 0; i < 3; ++i) panels[i]->relayout();
    meters->setBounds (1080, 202, 236, 672); fMeter->setTopLeftPosition (11, 400); fRms->setTopLeftPosition (11, 448); bPkReset->setTopLeftPosition (11, 501);
    modal.setBounds (getLocalBounds());
    relayoutScope();
}

void Content::resized() { if (meters != nullptr) layoutFixed(); }

void Content::relayoutScope()
{
    const juce::String s (p.choiceRaw ("sync"));
    hostDur->setVisible (s == "free" || s == "trig"); hostFreq->setVisible (s == "freq"); hostPer->setVisible (s == "freq" || s == "pitch"); hostDiv->setVisible (s == "beat");
    scopeFlow->relayout(); lastSync = s;
}

//------------------------------------------------------------------------------
void Content::paint (juce::Graphics& g)
{
    const int W = getWidth(), H = getHeight(); g.fillAll (juce::Colours::black);
    { g.saveState(); g.reduceClipRegion (30, 0, W - 60, H); g.setColour (col::panel); g.fillRect (30, 0, W - 60, H);
      g.addTransform (juce::AffineTransform::scale (1.0f, 1.35f, 680.0f, 0.0f)); juce::ColourGradient grad (juce::Colour (0xff1e1f23), 680.0f, 0.0f, col::panel, 680.0f, 680.0f * 0.6f, false); g.setGradientFill (grad); g.fillRect (30, 0, W - 60, 700); g.restoreState(); }
    drawWoodCheeks (g, getLocalBounds(), 30);
    { juce::Path lp (logoPath); lp.applyTransform (juce::AffineTransform::scale (38.0f / 64.0f).translated (44, 14)); g.setColour (juce::Colour (0xfff4f5f7)); g.fillPath (lp);
      juce::Path eyes = juce::Drawable::parseSVGPath ("M11.3 25.8C20 26 25 31 27 36.5 21 37.5 15 36 12.5 31zM52.7 25.8C44 26 39 31 37 36.5 43 37.5 49 36 51.5 31z"); eyes.applyTransform (juce::AffineTransform::scale (38.0f / 64.0f).translated (44, 14)); g.setColour (col::panel); g.fillPath (eyes); }
    txt (g, "WHISKER SCOPE", 92, 36, col::ink, juce::Justification::left, label (28.0f, 700, 0.12f));
    txt (g, "STUDIO 87", 92, 52, env.pal->acc, juce::Justification::left, label (12.0f, 500, 0.3f));
    g.setColour (col::line); g.fillRect (44, 64, 1272, 1);
    { auto r = juce::Rectangle<float> (44, 75, 1272, 117).reduced (0.5f); g.setColour (col::panel2); g.fillRoundedRectangle (r, 4.0f); g.setColour (col::line); g.drawRoundedRectangle (r, 4.0f, 1.0f); }
    txt (g, "INPUT", 55, 126, col::dim, juce::Justification::left, label (12.0f, 500, 0.2f));
    const bool scOn = p.scEnabled.load(), scSeen = p.scSeen.load();
    txt (g, "SIDECHAIN (LAYER B)", 177, 109, col::dim, juce::Justification::left, label (11.0f, 500, 0.18f));
    { g.setColour ((scSeen ? col::cb : col::dim).withAlpha (scSeen ? 1.0f : 0.5f)); g.fillEllipse (178, 122, 10, 10);
      txt (g, scSeen ? "Receiving sound: drawn in yellow on the displays" : scOn ? "Connected, waiting for sound" : "Not connected. Route a track to the sidechain input of this plugin in your DAW.", 196, 132, scSeen ? col::cb : col::dim, juce::Justification::left, label (14.0f, 500, 0.03f)); }
    if (p.muted.load()) txt (g, "PANIC: output muted. Press Panic again to unmute.", 177, 160, juce::Colour (0xffff4f8f), juce::Justification::left, label (14.0f, 700, 0.06f));
    mtxt (g, "BPM " + (p.hostBpm.load() > 1 ? juce::String ((int) std::lround (p.hostBpm.load())) : juce::String ("-")), 55, 180, env.pal->acc, juce::Justification::left, 13);
    txt (g, "Reads the audio of this track. Sound passes unchanged unless Input gain, the matching EQ or Output gain is used.", 1305, 180, col::dim, juce::Justification::right, label (12.0f, 500, 0.04f));
    g.setColour (col::line); g.fillRect (44, 886, 1272, 1);
    { const auto f = label (12.0f, 500, 0.16f); txt (g, "STUDIO 87", 44, 906, col::ink, juce::Justification::left, f); const float w = textWidth ("STUDIO 87", f);
      txt (g, juce::String::fromUTF8 (" \xc2\xb7 WHISKER SCOPE V") + juce::String (WS_VERSION).upToLastOccurrenceOf (".", false, false), 44 + w, 906, col::dim, juce::Justification::left, f);
      txt (g, "WINDOW " + juce::String ((int) std::lround (editor.scaleNow() * 100.0)) + " %", 680, 906, col::dim, juce::Justification::horizontallyCentred, f); }
}

void Content::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto q = e.getPosition();
    if (q.x < 30 || q.x > getWidth() - 30) editor.resetSize (false);
    else if (q.y >= 890 && q.x > 600 && q.x < 760) editor.resetSize (true);
}

bool Content::keyPressed (const juce::KeyPress& k)
{
    if (modal.isOpen() || k.getModifiers().isAnyModifierKeyDown()) return false;
    const auto c = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
    if (c == 'p') { panic(); return true; }
    if (c == 'r') { resetPeaksAll(); return true; }
    if (c >= '1' && c <= '4') { setPage ((int) (c - '1')); return true; }
    if (c == 'f') { static const char* const ids[3] = { "fzSpec", "fzScope", "fzSt" }; const int pg = p.page; const int idx = pg == 0 ? 0 : pg == 1 ? 1 : pg == 2 ? 2 : -1; if (idx >= 0) p.setParam (ids[idx], p.param (ids[idx]) > 0.5f ? 0.0f : 1.0f); return true; }
    return false;
}

void Content::setPage (int i)
{
    i = juce::jlimit (0, 3, i); p.page = i; if (i == shownPage) return; shownPage = i; p.setShapesWatched (i == 3);
    for (int k = 0; k < 4; ++k) { views[k]->setVisible (k == i); panels[k]->setVisible (k == i); tabs[k]->setLit (k == i); }
    if (auto* s = dynamic_cast<StereoView*> (views[2].get())) s->clearTrail();
    if (auto* s = dynamic_cast<SpecView*> (views[0].get())) s->clearHistory();
    repaint();
}

void Content::panic()
{
    const bool now = ! p.muted.load(); p.muted.store (now);
    if (now) { p.cancelCapture(); if (p.param ("shHear") > 0.5f) p.setParam ("shHear", 0.0f); resetPeaksAll(); }
    bPanic->setLit (now); lastMuted = now; repaint (0, 75, getWidth(), 117);
}

void Content::note (const juce::String& s)
{
    if (s.isEmpty()) return; modal.begin ("Note"); modal.para (s); modal.show ({ { "Close" } });
}

void Content::copyDialog (const juce::String& text, const juce::String& what)
{
    juce::SystemClipboard::copyTextToClipboard (text);
    modal.begin ("Copied"); modal.para (what + " copied to the clipboard."); modal.show ({ { "Close" } });
}

void Content::importDialog (bool preset)
{
    modal.begin (preset ? "Import preset" : "Import shape"); modal.para (preset ? "Paste exported preset text. Anything that does not look like a preset is rejected." : "Paste exported shape text. Anything that does not look like a shape is rejected.");
    auto* ta = modal.input ({}, 0, true, false, preset ? "Paste exported preset text" : "Paste the shape text here");
    auto bad = [ta] { ta->setColour (juce::TextEditor::outlineColourId, juce::Colour (0xffFF1383)); ta->setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xffFF1383)); ta->repaint(); return false; };
    modal.show ({ { "Cancel" }, { "Import", true, [this, ta, preset, bad]
    {
        if (preset) { juce::String nn; if (! p.importPresetText (ta->getText(), nn)) return bad(); p.loadPreset (nn); return true; }
        ws::Shape s; if (! WhiskerScopeProcessor::shapeFromText (ta->getText(), s)) return bad();
        p.useAsReference (s); return true;
    } } });
}

void Content::presetMenu()
{
    juce::PopupMenu m; auto all = p.allPresets(); m.addSectionHeader ("Factory");
    for (size_t i = 0; i < all.size(); ++i) if (all[i].factory) m.addItem ((int) i + 1, all[i].name, true, all[i].name == p.presetName);
    bool anyUser = false; for (size_t i = 0; i < all.size(); ++i) if (! all[i].factory) { if (! anyUser) { m.addSectionHeader ("Mine"); anyUser = true; } m.addItem ((int) i + 1, all[i].name, true, all[i].name == p.presetName); }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBox).withMinimumWidth (presetBox.getWidth()), [this, all] (int r) { if (r > 0 && r <= (int) all.size()) p.loadPreset (all[(size_t) (r - 1)].name); });
}

void Content::saveDialog()
{
    const bool mine = p.userPresetExists (p.presetName);
    modal.begin ("Save preset"); modal.para ("Name for this preset. It is kept on this device."); auto* in = modal.input (mine ? p.presetName : juce::String ("My preset"), 40, false);
    modal.show ({ { "Cancel" }, { "Save", true, [this, in]
    {
        const auto n = WhiskerScopeProcessor::cleanName (in->getText()); if (n.isEmpty()) return false;
        if (p.userPresetExists (n)) { juce::MessageManager::callAsync ([this, n] { modal.begin ("Overwrite preset?"); modal.para ("A preset called **" + n + "** already exists. Replace it?"); modal.show ({ { "Cancel" }, { "Replace", true, [this, n] { p.saveUserPreset (n); return true; } } }); }); return true; }
        if (! p.saveUserPreset (n)) juce::MessageManager::callAsync ([this] { note ("The preset could not be saved. Check that your user folder is writable."); });
        return true;
    } } });
}

void Content::moreDialog()
{
    const bool mine = p.userPresetExists (p.presetName); const juce::String cur = p.presetName;
    modal.begin ("Presets"); modal.para ("Current: **" + cur + "**" + (mine ? "" : " (factory presets cannot be renamed or deleted)"));
    modal.show ({ { "Rename", false, [this, mine, cur] { if (! mine) return false; juce::MessageManager::callAsync ([this, cur] { modal.begin ("Rename preset"); auto* in = modal.input (cur, 40, false); modal.show ({ { "Cancel" }, { "Rename", true, [this, in, cur] { const auto n = WhiskerScopeProcessor::cleanName (in->getText()); if (n.isEmpty() || p.userPresetExists (n)) return false; p.renameUserPreset (cur, n); return true; } } }); }); return true; } },
                  { "Delete", false, [this, mine, cur] { if (! mine) return false; p.deleteUserPreset (cur); return true; } },
                  { "Export", false, [this, cur] { juce::MessageManager::callAsync ([this, cur] { copyDialog (p.presetToText (cur), "The preset"); }); return true; } },
                  { "Import", false, [this] { juce::MessageManager::callAsync ([this] { importDialog (true); }); return true; } }, { "Close" } });
}

//------------------------------------------------------------------------------
void Content::refreshShapeList (const juce::String& select)
{
    auto& box = fShape->box; const int curId = box.getSelectedId();
    const juce::String keepName = select.isNotEmpty() ? select : (curId >= 1000 && curId - 1000 < (int) shapeList.size() ? juce::String (shapeList[(size_t) (curId - 1000)].name) : juce::String());
    shapeList = p.factoryShapes(); const size_t nf = shapeList.size(); for (auto& s : p.userShapes()) shapeList.push_back (s);
    box.clear (juce::dontSendNotification); box.addSectionHeading ("Starting points (made by us, not measured from a song)");
    for (size_t i = 0; i < nf; ++i) box.addItem (juce::String (shapeList[i].name), 1000 + (int) i);
    if (shapeList.size() > nf) { box.addSectionHeading ("My shapes"); for (size_t i = nf; i < shapeList.size(); ++i) box.addItem (juce::String (shapeList[i].name), 1000 + (int) i); }
    int sel = 1000; if (keepName.isNotEmpty()) for (size_t i = 0; i < shapeList.size(); ++i) if (juce::String (shapeList[i].name) == keepName) sel = 1000 + (int) i;
    box.setSelectedId (sel, juce::dontSendNotification);
    factoryNames.clear(); for (size_t i = 0; i < nf; ++i) factoryNames.add (juce::String (shapeList[i].name));
}

void Content::updateShapesStatus()
{
    const auto& V = p.shapes(); const bool refOn = p.param ("shRefOn") > 0.5f; const bool lufsRef = V.hasRef && V.ref.hasLufs();
    stRef->setText ("Reference: " + (V.hasRef ? juce::String (V.ref.name) + (refOn ? "" : " [OFF]") + (lufsRef ? "  " + juce::String (V.ref.lufs, 1) + " LUFS" : "  (no loudness stored)") : juce::String ("none yet")));
    stMine->setText ("Mine: " + (V.hasMine ? "captured" + (V.mine.hasLufs() ? "  " + juce::String (V.mine.lufs, 1) + " LUFS" : juce::String()) : V.hasLive ? juce::String ("live (following the sound)") : juce::String ("waiting for sound")));
    const juce::String mode (p.choiceRaw ("shAuto")); const float ad = p.autoDb.load();
    stAuto->setText (mode == "off" ? juce::String ("Auto level off") : (mode == "ref" && ! lufsRef) ? juce::String ("Auto: no ref loudness") : "Auto gain " + juce::String (ad >= 0 ? "+" : "") + juce::String (ad, 1) + " dB" + (p.autoHeld.load() ? ", held" : ""));
    const bool capRef = V.capturing && V.captureIsRef, capMine = V.capturing && ! V.captureIsRef;
    bCapRef->setButtonText (capRef ? "Stop and keep" : "Capture reference"); bCapRef->setLit (capRef); bCapRef->setEnabled (! capMine);
    bCapMine->setButtonText (capMine ? "Stop and keep" : "Capture mine"); bCapMine->setLit (capMine); bCapMine->setEnabled (! capRef);
    if (bCanRef->isVisible() != capRef) { bCanRef->setVisible (capRef); shapesGroups[0]->relayout(); }
    if (bCanMine->isVisible() != capMine) { bCanMine->setVisible (capMine); shapesGroups[1]->relayout(); }
    if (! V.capturing) { const auto n = p.takeNote(); if (n.isNotEmpty()) note (n); }
    sugg->repaint();
}

void Content::syncFromProcessor()
{
    if (p.page != shownPage) setPage (p.page);
    const int c = p.choiceIndex ("color");
    if (c != lastColor) { lastColor = c; env.pal.s = &scheme (c); static_cast<SpecView*> (views[0].get())->clearHistory(); repaint(); for (auto* k : { kGain, kScGain }) k->repaint(); }
    if (juce::String (p.choiceRaw ("sync")) != lastSync) relayoutScope();
    if (p.muted.load() != lastMuted) { lastMuted = p.muted.load(); bPanic->setLit (lastMuted); repaint (0, 75, getWidth(), 117); }
    if (labelTick++ % 8 == 0)
    {
        const bool dirty = juce::JSON::toString (p.currentState(), true) != p.presetBaseJson; const juce::String t = p.presetName + (dirty ? " *" : ""); if (t != presetBox.text) { presetBox.text = t; presetBox.repaint(); }
        repaint (44, 75, 1272, 117);
    }
}

void Content::tick()
{
    const double now = nowMs(); const float dt = (float) juce::jlimit (0.005, 0.2, (now - (lastTick > 0 ? lastTick : now - 33)) / 1000.0); lastTick = now;
    env.update (dt); syncFromProcessor();
    const int pg = juce::jlimit (0, 3, p.page); views[pg]->update (dt); views[pg]->repaint(); meters->repaint();
    if (pg == 3) updateShapesStatus();
}

//------------------------------------------------------------------------------
void Content::showSettings()
{
    struct RowLabel : juce::Component, juce::SettableTooltipClient { explicit RowLabel (const juce::String& t) : text (t) {} void paint (juce::Graphics& g) override { txt (g, text, 0, 20, juce::Colour (0xffd6d8de), juce::Justification::left, label (15.0f, 500, 0.02f)); } juce::String text; };
    struct Body : juce::Component
    {
        explicit Body (Content& c) : owner (c)
        {
            auto addRow = [this] (const juce::String& lab, juce::Component* ctl, const juce::String& tip)
            { auto* l = new RowLabel (lab); l->setTooltip (lab + "\n" + tip); labels.add (l); addAndMakeVisible (l); if (auto* tc = dynamic_cast<juce::SettableTooltipClient*> (ctl)) tc->setTooltip (lab + "\n" + tip); addAndMakeVisible (ctl); ctls.add (ctl); };
            auto& S = owner.settings; auto* b1 = new MBtn (S.bubbles ? "On" : "Off", MBtn::Plain, owner.env.pal); b1->setSize (64, 30); b1->setLit (S.bubbles);
            b1->onClick = [this, b1] { owner.settings.bubbles = ! owner.settings.bubbles; b1->setButtonText (owner.settings.bubbles ? "On" : "Off"); b1->setLit (owner.settings.bubbles); owner.applySettings(); WhiskerScopeProcessor::saveSettings (owner.settings); };
            addRow ("Info bubbles", b1, "The small help notes that appear after hovering a control for a second."); own.add (b1);
            auto* b2 = new MBtn (S.eco ? "On" : "Off", MBtn::Plain, owner.env.pal); b2->setSize (64, 30); b2->setLit (S.eco);
            b2->onClick = [this, b2] { owner.settings.eco = ! owner.settings.eco; b2->setButtonText (owner.settings.eco ? "On" : "Off"); b2->setLit (owner.settings.eco); owner.startFrames(); WhiskerScopeProcessor::saveSettings (owner.settings); };
            addRow ("Power saving", b2, "Draws about 12 pictures a second instead of 30. Lighter on the computer. Sound and measuring are not affected."); own.add (b2);
            auto* cb = new juce::ComboBox(); for (auto* s : wsp::choices()[0].labels) cb->addItem (s, cb->getNumItems() + 1); cb->setSize (160, 30); cb->setSelectedItemIndex (owner.p.choiceIndex ("color"), juce::dontSendNotification);
            cb->onChange = [this, cb] { owner.p.setParam ("color", (float) cb->getSelectedItemIndex()); }; addRow ("Colours", cb, "Colour scheme for all displays."); own.add (cb);
            auto* wr = new juce::Component(); wr->setSize (190, 30); winVal = new juce::Label(); winVal->setFont (monoCached (14.0f)); winVal->setJustificationType (juce::Justification::centred); winVal->setColour (juce::Label::textColourId, col::ink); wr->addAndMakeVisible (winVal); winVal->setBounds (34, 0, 62, 30);
            auto mk = [&] (const juce::String& t, int x, int w, std::function<void()> fn) { auto* b = new MBtn (t, MBtn::Plain, owner.env.pal); b->setBounds (x, 0, w, 30); b->onClick = std::move (fn); wr->addAndMakeVisible (b); wrBtns.add (b); };
            mk ("-", 0, 30, [this] { owner.editor.applyWidth ((int) std::lround (owner.editor.getWidth() / 1.1)); owner.editor.saveScale(); refreshWin(); });
            mk ("+", 98, 30, [this] { owner.editor.applyWidth ((int) std::lround (owner.editor.getWidth() * 1.1)); owner.editor.saveScale(); refreshWin(); });
            mk ("100 %", 132, 58, [this] { owner.editor.resetSize (true); owner.editor.saveScale(); refreshWin(); });
            addRow ("Window size", wr, "Same as dragging any edge or corner of the plugin. The shape of the window never changes."); own.add (wr); refreshWin();
            auto* wl = new juce::ComboBox(); for (int v : { -18, -12, -9, -6 }) wl->addItem (juce::String (v) + " dB", -v); wl->setSize (160, 30); wl->setSelectedId (-S.warnDb, juce::dontSendNotification);
            wl->onChange = [this, wl] { owner.settings.warnDb = -wl->getSelectedId(); owner.env.st = owner.settings; WhiskerScopeProcessor::saveSettings (owner.settings); };
            addRow ("Meter warning from", wl, "Level bars are blue below this, purple above it (warning) and pink near 0 dB (clipping)."); own.add (wl);
            setSize (500, ctls.size() * 40 + 4);
        }
        void resized() override { int y = 0; for (int i = 0; i < ctls.size(); ++i) { labels[i]->setBounds (0, y, 220, 30); ctls[i]->setTopLeftPosition (getWidth() - ctls[i]->getWidth(), y); y += 40; } }
        void refreshWin() { winVal->setText (juce::String ((int) std::lround (owner.editor.scaleNow() * 100.0)) + " %", juce::dontSendNotification); }
        Content& owner; juce::OwnedArray<RowLabel> labels; juce::Array<juce::Component*> ctls; juce::OwnedArray<juce::Component> own; juce::OwnedArray<MBtn> wrBtns; juce::Label* winVal = nullptr;
    };
    modal.begin ("Settings"); modal.custom (new Body (*this)); modal.para ("Settings and presets are kept in your user data folder (Studio 87 / Whisker Scope). Nothing is sent anywhere."); modal.show ({ { "Close" } });
}

void Content::showAbout()
{
    modal.begin ("About"); modal.head ("Whisker Scope");
    modal.para ("A scope, spectrum analyser, stereo field and spectrum-shape tool for music makers. By **Studio 87**. It reads the audio of your track, shows it, and can apply a matching EQ on the Shapes page.");
    modal.head ("Free, open and private"); modal.bullets ({ "Free and open source (AGPL-3.0-or-later).", "Collects nothing: no telemetry, no account, no network access.", "Your presets and shapes stay on your computer." });
    modal.head ("Installing and removing"); modal.bullets ({ "Windows: one .exe installer. macOS: one .pkg plus an Uninstall app. Linux: Flatpak or a plain .vst3 folder.", "The uninstaller removes everything it installed and keeps your presets and shapes, unless you tick the box to delete them too.", "Presets live in the Studio 87 / Whisker Scope folder of your user data." });
    modal.head ("Fonts"); modal.para ("Barlow Condensed (Jeremy Tribby) and JetBrains Mono (JetBrains), both under the SIL Open Font License 1.1.");
    modal.head ("Support the project"); modal.para ("If it helps your music, a coffee keeps it free for everyone.");
    static const juce::String kofi;     // the Ko-fi page name goes here once the page exists (same as the mockup)
    if (kofi.isEmpty()) modal.para ("The Ko-fi page is not set up yet. This button goes live when it is.");
    modal.show ({ { "Support on Ko-fi", false, [] { if (kofi.isNotEmpty()) juce::URL ("https://ko-fi.com/" + kofi).launchInDefaultBrowser(); return false; } }, { "Close" } });
}

void Content::showGuide()
{
    modal.begin ("Quick guide");
    modal.head ("Scope"); modal.bullets ({ "Shows the waveform. Sync keeps repeating waves still: Trigger, a fixed Hz, the detected pitch, or the host beat.", "Channels: L / R, mono, or Mid / Side." });
    modal.head ("Spectrum"); modal.bullets ({ "Hover for frequency, note and dB. The top-left line names the highest peak.", "Waterfall shows history. Bands groups the curve like an equaliser. Freeze holds the picture." });
    modal.head ("Stereo field"); modal.bullets ({ "Vertical line is mono, sideways is wide. A line leaning left or right means one channel is louder.", "Correlation and Mono loss tell you if it survives being summed to mono." });
    modal.head ("Shapes, in three steps"); modal.bullets ({ "**1 Reference:** play the song or lead you want to sound like, press Capture reference, press again to keep it. Or pick a saved shape.", "**2 Mine:** play your own sound and Capture mine. Skip it and the live sound is used.", "**3 Match:** the bars at the bottom of the graph are the EQ that moves mine toward the reference. EQ on applies it to the sound.",
                                                           "Input and Output set the level in and out. Auto level keeps the loudness the same with the EQ on or off (Keep my loudness), or follows the loudness captured with the reference (Match reference).", "Meters on the right show IN and OUT. Save shapes to your library; Manage exports and imports them as text." });
    modal.head ("Meters"); modal.bullets ({ "Blue is normal, purple is a warning, pink is clipping. The pink cap on top stays lit until Peak reset.", "Metering: Peak, RMS (average level), both, or Loudness (LUFS): momentary M, short-term S and integrated I, the way streaming and broadcast meters read it." });
    modal.head ("Window"); modal.para ("Drag any edge or corner to resize. The shape stays the same. Double-click a wooden side for the default size.");
    modal.head ("Sidechain (layer B)"); modal.para ("Route a second track to the sidechain input of the plugin in your DAW. It is never played; it is drawn in yellow on Spectrum, Scope and Stereo field. On Spectrum, pink bands mark where A and B may mask each other (rule of thumb).");
    modal.head ("Keys"); modal.para ("P panic (mutes the output), 1 to 4 pages, F freeze, R peak reset. Double-click a knob to reset it.");
    modal.show ({ { "Close" } });
}
}   // namespace wsui

//==============================================================================
struct WhiskerScopeEditor::Handle : juce::Component
{
    Handle (WhiskerScopeEditor& e, int sx_, int sy_) : ed (e), sx (sx_), sy (sy_)
    {
        setMouseCursor (sx != 0 && sy != 0 ? ((sx * sy) > 0 ? juce::MouseCursor::TopLeftCornerResizeCursor : juce::MouseCursor::TopRightCornerResizeCursor) : sx != 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor);
    }
    void paint (juce::Graphics& g) override { if (isMouseOverOrDragging()) { g.setColour (juce::Colour (0x5502EBBA)); g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f); } }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDown (const juce::MouseEvent& e) override { start = e.getScreenPosition(); w0 = ed.getWidth(); h0 = ed.getHeight(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto d = e.getScreenPosition() - start; const double aspect = (double) kDesignW / kDesignH; double w = w0;
        const double cw = sx != 0 ? w0 + sx * d.x : -1.0, ch = sy != 0 ? h0 + sy * d.y : -1.0;
        if (cw > 0 && ch > 0) w = 0.5 * (cw + ch * aspect); else if (cw > 0) w = cw; else if (ch > 0) w = ch * aspect;
        ed.applyWidth ((int) std::lround (w));
    }
    void mouseUp (const juce::MouseEvent&) override { ed.saveScale(); }
    WhiskerScopeEditor& ed; int sx, sy; juce::Point<int> start; int w0 = 0, h0 = 0;
};

WhiskerScopeEditor::WhiskerScopeEditor (WhiskerScopeProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    content = std::make_unique<wsui::Content> (*this, p); addAndMakeVisible (*content);
    static const int dirs[8][2] = { { 0, -1 }, { 0, 1 }, { 1, 0 }, { -1, 0 }, { 1, -1 }, { -1, -1 }, { 1, 1 }, { -1, 1 } };
    for (auto& d : dirs) { handles.push_back (std::make_unique<Handle> (*this, d[0], d[1])); addAndMakeVisible (*handles.back()); }
    setResizable (true, false); getConstrainer()->setFixedAspectRatio ((double) kDesignW / kDesignH); getConstrainer()->setSizeLimits (kDesignW / 4, kDesignH / 4, (int) (kDesignW * 2.5), (int) (kDesignH * 2.5));
    const int w = defaultWidth(); setSize (w, (int) std::lround (w * (double) kDesignH / kDesignW));
}

WhiskerScopeEditor::~WhiskerScopeEditor() { saveScale(); }

int WhiskerScopeEditor::defaultWidth() const
{
    if (proc.editorWidth >= kDesignW / 4) return juce::jlimit (kDesignW / 4, (int) (kDesignW * 2.5), proc.editorWidth);
    const auto s = WhiskerScopeProcessor::loadSettings(); if (s.winScale >= 0.25) return (int) std::lround (juce::jlimit (0.25, 2.5, s.winScale) * kDesignW);
    double sc = 1.0; if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) sc = std::min ({ 1.0, (d->userArea.getWidth() - 40.0) / kDesignW, (d->userArea.getHeight() - 120.0) / kDesignH });
    return (int) std::lround (juce::jlimit (0.25, 1.0, sc) * kDesignW);
}

void WhiskerScopeEditor::applyWidth (int w)
{
    w = juce::jlimit (kDesignW / 4, (int) (kDesignW * 2.5), w); const int h = (int) std::lround ((double) w * kDesignH / kDesignW);
    if (w != getWidth() || h != getHeight()) setSize (w, h);
}

void WhiskerScopeEditor::resetSize (bool exactly100)
{
    double sc = 1.0;
    if (! exactly100) if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) sc = std::min ({ 1.0, (d->userArea.getWidth() - 40.0) / kDesignW, (d->userArea.getHeight() - 120.0) / kDesignH });
    applyWidth ((int) std::lround (juce::jlimit (0.25, 2.5, sc) * kDesignW)); saveScale();
}

void WhiskerScopeEditor::tickForScreenshot() { content->tickNow(); }

void WhiskerScopeEditor::saveScale()
{
    proc.editorWidth = getWidth(); auto s = WhiskerScopeProcessor::loadSettings(); const double sc = scaleNow();
    if (std::abs (s.winScale - sc) > 0.0005) { s.winScale = sc; WhiskerScopeProcessor::saveSettings (s); }
}

void WhiskerScopeEditor::resized()
{
    if (content == nullptr) return;
    const double s = scaleNow(); content->setBounds (0, 0, kDesignW, kDesignH); content->setTransform (juce::AffineTransform::scale ((float) s));
    const int w = getWidth(), h = getHeight(), t = 8, c = 18;
    handles[0]->setBounds (c, 0, w - 2 * c, t); handles[1]->setBounds (c, h - t, w - 2 * c, t); handles[2]->setBounds (w - t, c, t, h - 2 * c); handles[3]->setBounds (0, c, t, h - 2 * c);
    handles[4]->setBounds (w - c, 0, c, c); handles[5]->setBounds (0, 0, c, c); handles[6]->setBounds (w - c, h - c, c, c); handles[7]->setBounds (0, h - c, c, c);
    proc.editorWidth = w;
}

