#include "PluginEditor.h"
#include "catui/Logo.h"
#include "catui/RenderGate.h"
#include "catui/SodaPop.h"

namespace sp = catui::SodaPop;
using APVTS = juce::AudioProcessorValueTreeState;
using namespace ws;

namespace
{
juce::Font F (float h, bool bold = false) { juce::Font f (juce::FontOptions { h }); return bold ? f.boldened() : f; }
const juce::Colour kPanel (0xff17181d), kPanel2 (0xff1e2027), kLine (0xff2c2e38), kDisp (0xff06080a), kInk (0xffe8eaf0), kDim (0xff8f93a3), kGrid (0xff1a2422), kAxis (0xff8f8bd6);
const juce::Colour kL = sp::mint(), kR = sp::hotPink(), kM = sp::lilac(), kS = sp::gold();

void paintWood (juce::Graphics& g, juce::Rectangle<int> r, bool left)
{
    const juce::Colour c[4] = { juce::Colour (0xff5B3A22), juce::Colour (0xff6E4729), juce::Colour (0xff4E3120), juce::Colour (0xff77502F) };
    const int w[4] = { 3, 4, 2, 5 }; int x = r.getX(), i = left ? 0 : 1;
    while (x < r.getRight()) { g.setColour (c[i & 3]); g.fillRect (x, r.getY(), juce::jmin (w[i & 3], r.getRight() - x), r.getHeight()); x += w[i & 3]; ++i; }
    const float x0 = left ? (float) r.getRight() - 8.0f : (float) r.getX(), x1 = left ? (float) r.getRight() : (float) r.getX() + 8.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (left ? 0.0f : 0.6f), x0, 0.0f, juce::Colours::black.withAlpha (left ? 0.6f : 0.0f), x1, 0.0f, false));
    g.fillRect (juce::Rectangle<float> (juce::jmin (x0, x1), (float) r.getY(), 8.0f, (float) r.getHeight()));
}

struct Look : juce::LookAndFeel_V4
{
    Look()
    {
        setColour (juce::ComboBox::backgroundColourId, kPanel2); setColour (juce::ComboBox::textColourId, kInk); setColour (juce::ComboBox::outlineColourId, kLine);
        setColour (juce::PopupMenu::backgroundColourId, kPanel2); setColour (juce::PopupMenu::textColourId, kInk); setColour (juce::PopupMenu::highlightedBackgroundColourId, sp::grape());
        setColour (juce::Slider::textBoxTextColourId, sp::gold()); setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextButton::buttonColourId, kPanel2); setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff241238)); setColour (juce::TextButton::textColourOffId, kInk); setColour (juce::TextButton::textColourOnId, sp::mistLilac());
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff23252d)); setColour (juce::TooltipWindow::textColourId, kInk); setColour (juce::TooltipWindow::outlineColourId, sp::grape());
    }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return F (14.0f, true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return F (14.0f); }
    juce::Font getPopupMenuFont() override { return F (15.0f); }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f); const bool on = b.getToggleState();
        g.setColour (on ? juce::Colour (0xff241238) : (hover || down ? juce::Colour (0xff272a33) : kPanel2)); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? sp::lilac() : kLine); g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override
    {
        const float d = (float) juce::jmin (w, h) - 6.0f, cx = x + w * 0.5f, cy = y + h * 0.5f, r = d * 0.5f;
        g.setColour (juce::Colour (0xff23252c)); g.fillEllipse (cx - r + 4, cy - r + 4, d - 8, d - 8);
        juce::Path t; t.addCentredArc (cx, cy, r, r, 0, a0, a1, true); g.setColour (juce::Colour (0xff33353d)); g.strokePath (t, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float a = a0 + pos * (a1 - a0); juce::Path v; v.addCentredArc (cx, cy, r, r, 0, a0, a, true);
        g.setColour (sp::lilac()); g.strokePath (v, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path tick; tick.addLineSegment ({ 0, -r + 7, 0, -r * 0.35f }, 1.0f); g.setColour (kInk); g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::rotation (a).translated (cx, cy));
    }
};

const int kFfts[5] = { 2048, 4096, 8192, 16384, 32768 };
float freqToX (double f, bool lin, float w) { return lin ? (float) (f / 20000.0 * w) : (float) (std::log (f / 20.0) / std::log (1000.0) * w); }
double xToFreq (float x, bool lin, float w) { return lin ? (double) x / w * 20000.0 : 20.0 * std::pow (1000.0, (double) x / w); }
}

//==============================================================================
class WhiskerScopeEditor::Content : public juce::Component
{
public:
    explicit Content (WhiskerScopeProcessor& p) : proc (p), gate (*this)
    {
        setOpaque (true); setWantsKeyboardFocus (false);
        for (int i = 0; i < 4; ++i)
        {
            auto* b = tabs.add (new juce::TextButton (names[i])); addAndMakeVisible (b); b->setClickingTogglesState (false);
            b->onClick = [this, i] { setPage (i); };
        }
        tabs[0]->setTooltip ("Scope: the waveform of the left and right channel, or mono, or mid and side. Sync keeps a steady picture.");
        tabs[1]->setTooltip ("Spectrum: what frequencies are in the sound, with peak labels (level, frequency and note).");
        tabs[2]->setTooltip ("Stereo field: goniometer, correlation and mono compatibility.");
        tabs[3]->setTooltip ("Shapes: capture a reference spectrum and your own, and see the EQ that moves yours toward it.");
        auto btn = [&] (juce::TextButton& b, const char* t, const char* tip) { b.setButtonText (t); b.setTooltip (tip); addChildComponent (b); };
        btn (settings, "Settings", "Info bubbles and the plugin version."); btn (about, "About & Support", "About Studio 87 and ways to support the project.");
        settings.setVisible (true); about.setVisible (true);
        settings.onClick = [this] { showSettings(); }; about.onClick = [this] { showAbout(); };
        btn (peakReset, "Peak reset", "Clears the held peaks and the clip light."); peakReset.setVisible (true);
        peakReset.onClick = [this] { proc.resetPeaks(); for (auto& a : an) a.resetHold(); };

        // sliders
        addSlider (gainS, "Display gain", "gain", "Boosts or cuts what you SEE and measure. The sound that goes to your speakers is never changed.", true);
        addCombo (scopeChanC, "Channels", "scopeChan", "Which channels the scope shows.", 0); addCombo (syncC, "Sync", "sync", "Free: always the newest samples. Trigger: starts each sweep at a rising zero crossing. Pitch: locks to the detected period. Beat: one beat of the host tempo.", 0);
        addSlider (timeS, "Visible time", "scopeTime", "How much time the scope shows, in milliseconds.", false, 0);
        addToggle (freezeT, "Freeze", "freeze", "Holds the picture so you can look at it.", { 0, 1, 3 });
        addCombo (specChanC, "Channels", "specChan", "Mono, left, right, side, or two traces at once.", 1); addCombo (fftC, "FFT size", "fft", "Bigger is sharper in frequency (good for bass), smaller is quicker.", 1);
        addCombo (speedC, "Speed", "speed", "How fast the picture falls back down.", 1); addCombo (rangeC, "Range", "range", "How many dB are shown below 0.", 1); addCombo (smoothC, "Smoothing", "smooth", "Averages neighbouring frequencies for a calmer picture.", 1);
        addSlider (tiltS, "Slope", "tilt", "Tilts the picture by dB per octave around 1 kHz. 3 dB makes pink noise look flat.", false, 1);
        addToggle (linearT, "Linear", "linear", "Linear frequency scale instead of logarithmic.", { 1 }); addToggle (fillT, "Fill", "fill", "Fill under the line.", { 1 }); addToggle (holdT, "Peak hold", "hold", "Keeps the highest value of every frequency until Peak reset.", { 1 });
        addToggle (freezeT2, "Freeze", "freeze", "Holds the picture so you can look at it.", { 1 });
        addSlider (zoomS, "Zoom", "zoom", "Magnifies the goniometer. Quiet material becomes easy to read.", false, 2);
        addSlider (strengthS, "Strength", "strength", "How much of the difference the EQ curve shows. 100 % is the full difference.", false, 3); addSlider (maxS, "Max move", "maxMove", "Largest boost or cut the EQ curve will suggest, in dB.", false, 3);

        auto sb = [&] (juce::TextButton& b, const char* t, const char* tip, std::function<void()> f) { b.setButtonText (t); b.setTooltip (tip); b.onClick = std::move (f); addChildComponent (b); };
        sb (capRef, "Capture reference", "Press, play the song or sound you want to sound like, press again. The average spectrum is kept (Mid and Side apart). Only moments with signal count.", [this] { toggleCapture (1); });
        sb (capMine, "Capture mine", "Same for your own sound. Without it the live spectrum is compared.", [this] { toggleCapture (2); });
        sb (clearMine, "Clear mine", "Forgets your captured shape and goes back to the live comparison.", [this] { proc.hasMine = false; if (capturing == 2) capturing = 0; });
        sb (saveShape, "Save shape", "Saves the reference shape to your shape library on this computer.", [this] { saveShapeDialog(); });
        sb (loadShape, "Load shape", "Loads a saved shape or one of our starting points as the reference.", [this] { loadShapeMenu(); });
        sb (copyShape, "Copy shape text", "Copies the reference shape as text so you can share it.", [this] { if (proc.hasRef) juce::SystemClipboard::copyTextToClipboard (juce::String (shapeToText (proc.refShape))); });
        sb (pasteShape, "Paste shape text", "Reads a shape from the clipboard. Bad text is refused.", [this] { pasteShapeText(); });
        sb (copyEq, "Copy EQ points", "Copies the EQ suggestion as a list of frequencies and gains.", [this] { juce::SystemClipboard::copyTextToClipboard (eqText()); });
        sb (sideT, "Show Side", "Switches the Shapes display between Mid and Side.", [this] { showSide = ! showSide; sideT.setToggleState (showSide, juce::dontSendNotification); });
        sideT.setClickingTogglesState (false);

        for (auto& a : an) a.prepare (48000.0, 8192);
        setPage (proc.page);
        gate.onFrame = [this] { update(); repaint(); }; gate.start (30);
    }
    ~Content() override { gate.stop(); }

    void setPage (int p)
    {
        page = proc.page = juce::jlimit (0, 3, p);
        for (int i = 0; i < 4; ++i) tabs[i]->setToggleState (i == page, juce::dontSendNotification);
        for (auto& c : pageCtl) c.comp->setVisible (c.page == page || c.page < 0);
        capRef.setVisible (page == 3); capMine.setVisible (page == 3); clearMine.setVisible (page == 3); saveShape.setVisible (page == 3); loadShape.setVisible (page == 3);
        copyShape.setVisible (page == 3); pasteShape.setVisible (page == 3); copyEq.setVisible (page == 3); sideT.setVisible (page == 3);
        resized(); repaint();
    }

    //==========================================================================
    void resized() override
    {
        auto b = getLocalBounds(); (void) b;
        int x = 244; for (int i = 0; i < 4; ++i) { tabs[i]->setBounds (x, 16, 118, 34); x += 124; }
        about.setBounds (1010, 16, 140, 34); settings.setBounds (912, 16, 92, 34);
        dispR = { 50, 70, 910, 480 }; metR = { 972, 70, 178, 480 };
        peakReset.setBounds (metR.getX() + 8, metR.getBottom() - 38, metR.getWidth() - 16, 30);
        layoutControls();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff0b0c0f));
        g.setColour (kPanel); g.fillRect (34, 0, W - 68, H);
        paintWood (g, { 0, 0, 34, H }, true); paintWood (g, { W - 34, 0, 34, H }, false);
        catui::drawCatHead (g, { 50.0f, 10.0f, 44.0f, 44.0f }, sp::lilac());
        g.setColour (kInk); g.setFont (F (28.0f, true)); g.drawText ("WHISKER SCOPE", 102, 8, 260, 32, juce::Justification::centredLeft);
        g.setColour (sp::gold()); g.setFont (F (12.0f)); g.drawText ("STUDIO 87", 104, 36, 200, 16, juce::Justification::centredLeft);
        g.setColour (kLine); g.fillRect (50, 60, 1100, 1);
        paintDisplay (g); paintMeters (g);
        g.setColour (kPanel2); g.fillRoundedRectangle (50.0f, 560.0f, 1100.0f, 178.0f, 4.0f); g.setColour (kLine); g.drawRoundedRectangle (50.0f, 560.0f, 1100.0f, 178.0f, 4.0f, 1.0f);
        paintControlLabels (g);
        g.setColour (kDim); g.setFont (F (12.0f)); g.drawText ("Studio 87 · Whisker Scope v" JucePlugin_VersionString, 50, 748, 500, 20, juce::Justification::centredLeft);
        g.drawText ("Free and open source. Collects nothing. The sound is never changed.", 600, 748, 550, 20, juce::Justification::centredRight);
    }

    void mouseMove (const juce::MouseEvent& e) override { hover = e.position; hasHover = dispR.contains (e.getPosition()); }
    void mouseExit (const juce::MouseEvent&) override { hasHover = false; }

private:
    static constexpr int W = WhiskerScopeEditor::W, H = WhiskerScopeEditor::H;
    const char* names[4] = { "Scope", "Spectrum", "Stereo field", "Shapes" };

    struct PC { juce::Component* comp; int page; };
    juce::Array<PC> pageCtl;
    std::vector<std::unique_ptr<APVTS::SliderAttachment>> sAtt; std::vector<std::unique_ptr<APVTS::ComboBoxAttachment>> cAtt; std::vector<std::unique_ptr<APVTS::ButtonAttachment>> bAtt;
    juce::Slider gainS, timeS, tiltS, zoomS, strengthS, maxS; juce::ComboBox scopeChanC, syncC, specChanC, fftC, speedC, rangeC, smoothC;
    juce::TextButton freezeT, freezeT2, linearT, fillT, holdT, settings, about, peakReset, capRef, capMine, clearMine, saveShape, loadShape, copyShape, pasteShape, copyEq, sideT;
    juce::OwnedArray<juce::TextButton> tabs;
    struct Cap { juce::Component* comp; juce::String text; int page; };
    std::vector<Cap> caps;

    void addCaption (juce::Component& c, const juce::String& t, int page) { caps.push_back ({ &c, t, page }); }
    void addSlider (juce::Slider& s, const char* label, const char* id, const char* tip, bool gainKnob, int page = -1)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); s.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 18); s.setTooltip (tip); s.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (proc.apvts.getParameter (id)->getDefaultValue()));
        s.setNumDecimalPlacesToDisplay (id == std::string ("scopeTime") ? 0 : 1);
        addAndMakeVisible (s); sAtt.push_back (std::make_unique<APVTS::SliderAttachment> (proc.apvts, id, s)); addCaption (s, label, gainKnob ? -1 : page); pageCtl.add ({ &s, gainKnob ? -1 : page });
        if (std::string (id) == "gain") s.setTextValueSuffix (" dB"); else if (std::string (id) == "scopeTime") s.setTextValueSuffix (" ms"); else if (std::string (id) == "tilt") s.setTextValueSuffix (" dB/oct");
        else if (std::string (id) == "strength") { s.setTextValueSuffix (" %"); s.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)); }; s.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue() / 100.0; }; s.updateText(); }
        else if (std::string (id) == "maxMove") s.setTextValueSuffix (" dB"); else if (std::string (id) == "zoom") s.setTextValueSuffix (" x");
    }
    void addCombo (juce::ComboBox& c, const char* label, const char* id, const char* tip, int page)
    {
        const auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)); c.addItemList (p->choices, 1); c.setTooltip (tip); addAndMakeVisible (c);
        cAtt.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, c)); addCaption (c, label, page); pageCtl.add ({ &c, page });
    }
    void addToggle (juce::TextButton& b, const char* label, const char* id, const char* tip, juce::Array<int> pages)
    {
        b.setButtonText (label); b.setTooltip (tip); b.setClickingTogglesState (true); addAndMakeVisible (b); bAtt.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, b)); pageCtl.add ({ &b, pages[0] });
    }
    void layoutControls()
    {
        const int y0 = 596;
        auto knob = [&] (juce::Component& c, int x) { c.setBounds (x, y0 - 8, 96, 100); };
        // gain sits in the controls strip on the right for every page
        knob (gainS, 1040);
        scopeChanC.setBounds (70, y0 + 8, 170, 28); syncC.setBounds (260, y0 + 8, 190, 28); knob (timeS, 480); freezeT.setBounds (620, y0 + 8, 100, 28);
        specChanC.setBounds (70, y0 + 8, 130, 28); fftC.setBounds (210, y0 + 8, 90, 28); speedC.setBounds (310, y0 + 8, 100, 28); rangeC.setBounds (420, y0 + 8, 90, 28); smoothC.setBounds (520, y0 + 8, 130, 28);
        knob (tiltS, 665);
        linearT.setBounds (775, y0 + 8, 80, 28); fillT.setBounds (865, y0 + 8, 60, 28); holdT.setBounds (935, y0 + 8, 90, 28); freezeT2.setBounds (775, y0 + 48, 100, 28);
        knob (zoomS, 70);
        capRef.setBounds (70, y0 + 4, 150, 30); capMine.setBounds (230, y0 + 4, 130, 30); clearMine.setBounds (370, y0 + 4, 100, 30); saveShape.setBounds (480, y0 + 4, 100, 30); loadShape.setBounds (590, y0 + 4, 100, 30);
        copyShape.setBounds (70, y0 + 44, 150, 30); pasteShape.setBounds (230, y0 + 44, 150, 30); copyEq.setBounds (390, y0 + 44, 130, 30); sideT.setBounds (530, y0 + 44, 110, 30);
        knob (strengthS, 740); knob (maxS, 850);
        gainS.setBounds (1040, y0 - 8, 96, 100);
    }
    void paintControlLabels (juce::Graphics& g)
    {
        g.setFont (F (12.0f)); g.setColour (kDim);
        for (auto& c : caps)
        {
            if (! c.comp->isVisible()) continue;
            auto b = c.comp->getBounds();
            if (dynamic_cast<juce::Slider*> (c.comp)) g.drawText (c.text.toUpperCase(), b.getX(), b.getY() - 2, b.getWidth(), 14, juce::Justification::centred);
            else g.drawText (c.text.toUpperCase(), b.getX(), b.getY() - 16, b.getWidth(), 14, juce::Justification::centredLeft);
        }
        g.setColour (kDim); g.setFont (F (12.0f));
        const char* hint[4] = { "Scope: pick the channels and how the picture is locked.", "Spectrum: drag nothing, hover the picture to read Hz, note and dB.", "Stereo field: the dots show where the sound sits between left and right.", "Shapes: 1 capture the reference, 2 capture yours (or watch live), 3 read the EQ curve." };
        g.drawText (hint[page], 70, 714, 860, 18, juce::Justification::centredLeft);
    }

    //==========================================================================
    WhiskerScopeProcessor& proc; catui::RenderGate gate; int page = 1; juce::Rectangle<int> dispR, metR;
    juce::Point<float> hover; bool hasHover = false;
    SpectrumAnalyzer an[2]; int anSize = 0; std::vector<float> bl, br, bm, bs, tmpA, tmpB, scopeA, scopeB; float lastDt = 0.033f; double sr = 48000;
    PeakInfo peak; int scopeLen = 0; int periodFrame = 0, period = 0;
    juce::Image gonio { juce::Image::ARGB, 484, 480, true };
    int capturing = 0; ShapeCapture capture; bool showSide = false; float liveM[kShapeBands] = {}, liveS[kShapeBands] = {}; bool liveValid = false; juce::uint32 lastMs = 0;

    float prm (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    //==========================================================================
    void update()
    {
        const juce::uint32 now = juce::Time::getMillisecondCounter(); lastDt = lastMs ? juce::jlimit (0.005f, 0.2f, (now - lastMs) / 1000.0f) : 0.033f; lastMs = now;
        sr = proc.sampleRateNow.load();
        const bool freeze = prm ("freeze") > 0.5f;
        if (page == 0 && ! freeze) updateScope();
        if ((page == 1 && ! freeze) || page == 3) updateSpectrum();
        if (page == 2) updateGonio();
        if (page == 3 && capturing != 0) feedCapture();
    }

    void updateSpectrum()
    {
        const int fft = page == 3 ? 16384 : kFfts[juce::jlimit (0, 4, (int) prm ("fft"))];
        if (fft != anSize) { anSize = fft; for (auto& a : an) a.prepare (sr, fft); bl.resize ((size_t) fft); br.resize ((size_t) fft); bm.resize ((size_t) fft); bs.resize ((size_t) fft); }
        if (sr != lastSr) { lastSr = sr; for (auto& a : an) a.prepare (sr, fft); }
        proc.ringL.latest (bl.data(), fft); proc.ringR.latest (br.data(), fft);
        for (int i = 0; i < fft; ++i) { bm[(size_t) i] = 0.5f * (bl[(size_t) i] + br[(size_t) i]); bs[(size_t) i] = 0.5f * (bl[(size_t) i] - br[(size_t) i]); }
        const int ch = page == 3 ? 5 : (int) prm ("specChan"); const float rel[3] = { 600.0f, 250.0f, 80.0f }; const float rl = rel[juce::jlimit (0, 2, (int) prm ("speed"))], tilt = page == 3 ? 0.0f : prm ("tilt");
        const std::vector<float>* a = &bm; const std::vector<float>* b = nullptr;
        switch (ch) { case 0: a = &bm; break; case 1: a = &bl; break; case 2: a = &br; break; case 3: a = &bs; break; case 4: a = &bl; b = &br; break; default: a = &bm; b = &bs; break; }
        an[0].analyse (a->data(), tilt, 0.0f, rl, lastDt); if (b) an[1].analyse (b->data(), tilt, 0.0f, rl, lastDt);
        peak = an[0].highest (-80.0f);
        if (page == 3) { binsToBands (an[0].spectrum(), sr, anSize, liveM); binsToBands (an[1].spectrum(), sr, anSize, liveS); liveValid = true; }
    }
    double lastSr = 0;

    void updateScope()
    {
        const float ms = prm ("scopeTime"); const int sync = (int) prm ("sync");
        int n = juce::jlimit (16, 30000, (int) std::lround (ms * sr / 1000.0));
        if (sync == 3) { const float bpm = proc.live.bpm.load(); n = juce::jlimit (16, 30000, (int) std::lround (60.0 / (bpm > 20 ? bpm : 120.0f) * sr)); }
        const int total = juce::jmin (SampleRing::kSize, n * 2 + 64); std::vector<float>& L = tmpA; std::vector<float>& R = tmpB; L.resize ((size_t) total); R.resize ((size_t) total);
        proc.ringL.latest (L.data(), total); proc.ringR.latest (R.data(), total);
        int start = total - n;
        if (sync == 1 || sync == 2)
        {
            std::vector<float> mono ((size_t) total); for (int i = 0; i < total; ++i) mono[(size_t) i] = 0.5f * (L[(size_t) i] + R[(size_t) i]);
            if (sync == 2)
            {
                if ((periodFrame++ & 3) == 0) period = findPeriod (mono.data() + juce::jmax (0, total - 4096), juce::jmin (4096, total), (int) (sr / 2000.0), (int) (sr / 30.0));
                if (period > 8) { n = juce::jlimit (16, 30000, period * 2); }
            }
            const int s = findRisingCrossing (mono.data(), total - n, 0, 0.0f); start = s >= 0 ? s : total - n; if (start + n > total) start = total - n;
        }
        scopeLen = n; scopeA.assign (L.begin() + start, L.begin() + start + n); scopeB.assign (R.begin() + start, R.begin() + start + n);
    }

    void updateGonio()
    {
        const int n = 1600; std::vector<float>& L = tmpA; std::vector<float>& R = tmpB; L.resize (n); R.resize (n); proc.ringL.latest (L.data(), n); proc.ringR.latest (R.data(), n);
        juce::Graphics g (gonio); const auto r = gonio.getBounds().toFloat();
        g.fillAll (juce::Colours::transparentBlack); g.setColour (juce::Colour (0xff06080a).withAlpha (0.22f)); g.fillRect (r);
        const float cx = r.getCentreX(), cy = r.getCentreY(), rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.46f, zoom = prm ("zoom");
        for (int i = 0; i < n; ++i)
        {
            const float x = (R[(size_t) i] - L[(size_t) i]) * 0.70710678f * zoom, y = (L[(size_t) i] + R[(size_t) i]) * 0.70710678f * zoom;
            const float px = cx + juce::jlimit (-1.2f, 1.2f, x) * rad, py = cy - juce::jlimit (-1.2f, 1.2f, y) * rad;
            g.setColour (sp::mint().withAlpha (0.55f)); g.fillRect (px, py, 1.6f, 1.6f);
        }
    }

    void feedCapture()
    {
        if (! liveValid) return; float frameDb = -200.0f; { double e = 0; for (int i = 0; i < anSize; i += 8) e += (double) bm[(size_t) i] * bm[(size_t) i]; frameDb = powDb ((float) (e / (anSize / 8))); }
        capture.add (liveM, liveS, frameDb, -60.0f);
    }
    void toggleCapture (int which)
    {
        if (capturing == which)
        {
            Shape s; if (capture.result (s)) { s.name = which == 1 ? "Reference" : "Mine"; if (which == 1) { proc.refShape = s; proc.hasRef = true; } else { proc.mineShape = s; proc.hasMine = true; } }
            capturing = 0;
        }
        else { capturing = which; capture.reset(); }
    }

    //==========================================================================
    void paintDisplay (juce::Graphics& g)
    {
        g.setColour (kDisp); g.fillRoundedRectangle (dispR.toFloat(), 4.0f); g.setColour (kLine); g.drawRoundedRectangle (dispR.toFloat(), 4.0f, 1.0f);
        g.saveState(); g.reduceClipRegion (dispR);
        if (page == 0) paintScope (g); else if (page == 1) paintSpectrum (g); else if (page == 2) paintStereo (g); else paintShapes (g);
        g.restoreState();
    }

    void paintScope (juce::Graphics& g)
    {
        auto r = dispR.toFloat().reduced (8.0f); r.removeFromLeft (34.0f); r.removeFromBottom (18.0f);
        g.setFont (F (11.0f)); const float amps[7] = { 1.0f, 0.5f, 0.25f, 0.0f, -0.25f, -0.5f, -1.0f }; const char* lab[7] = { "0 dB", "-6", "-12", "-inf", "-12", "-6", "0 dB" };
        for (int i = 0; i < 7; ++i) { const float y = r.getCentreY() - amps[i] * r.getHeight() * 0.5f; g.setColour (i == 3 ? juce::Colour (0xff2c3a36) : kGrid); g.drawHorizontalLine ((int) y, r.getX(), r.getRight()); g.setColour (kAxis); g.drawText (lab[i], (int) dispR.getX() + 2, (int) y - 7, 36, 14, juce::Justification::centredLeft); }
        if (scopeLen < 2 || scopeA.empty()) return;
        const double totalMs = scopeLen * 1000.0 / sr; const double step = totalMs > 200 ? 50 : totalMs > 80 ? 20 : totalMs > 30 ? 10 : totalMs > 12 ? 5 : totalMs > 5 ? 2 : 1;
        for (double t = 0; t <= totalMs; t += step) { const float x = r.getX() + (float) (t / totalMs) * r.getWidth(); g.setColour (kGrid); g.drawVerticalLine ((int) x, r.getY(), r.getBottom()); g.setColour (kAxis); g.drawText (juce::String (t, step < 1 ? 1 : 0) + " ms", (int) x - 20, (int) r.getBottom() + 3, 40, 14, juce::Justification::centred); }
        const int ch = (int) prm ("scopeChan"); auto trace = [&] (auto fn, juce::Colour c)
        {
            juce::Path p; const int w = (int) r.getWidth(); const int n = (int) scopeA.size();
            for (int px = 0; px < w; ++px)
            {
                const int i0 = (int) ((double) px / w * n), i1 = juce::jmax (i0 + 1, (int) ((double) (px + 1) / w * n)); float lo = 1e9f, hi = -1e9f;
                for (int i = i0; i < juce::jmin (n, i1); ++i) { const float v = fn (i); lo = juce::jmin (lo, v); hi = juce::jmax (hi, v); }
                const float x = r.getX() + (float) px, y0 = r.getCentreY() - juce::jlimit (-1.0f, 1.0f, hi) * r.getHeight() * 0.5f, y1 = r.getCentreY() - juce::jlimit (-1.0f, 1.0f, lo) * r.getHeight() * 0.5f;
                if (n > w * 2) { p.startNewSubPath (x, y0); p.lineTo (x, y1 + 0.5f); } else { if (px == 0) p.startNewSubPath (x, y0); else p.lineTo (x, y0); }
            }
            g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.5f));
        };
        auto fl = [this] (int i) { return scopeA[(size_t) i]; }; auto fr = [this] (int i) { return scopeB[(size_t) i]; };
        auto fm = [this] (int i) { return 0.5f * (scopeA[(size_t) i] + scopeB[(size_t) i]); }; auto fs = [this] (int i) { return 0.5f * (scopeA[(size_t) i] - scopeB[(size_t) i]); };
        if (ch == 0) { trace (fl, kL); trace (fr, kR.withAlpha (0.85f)); } else if (ch == 1) trace (fl, kL); else if (ch == 2) trace (fr, kR); else if (ch == 3) trace (fm, kM); else { trace (fm, kM); trace (fs, kS); }
        g.setColour (kDim); g.setFont (F (12.0f)); g.drawText (juce::String (totalMs, 1) + " ms visible" + (period > 8 && prm ("sync") == 2 ? "   period " + juce::String (sr / period, 1) + " Hz (" + juce::String (noteName (sr / period)) + ")" : ""), dispR.getX() + 40, dispR.getY() + 6, 500, 16, juce::Justification::centredLeft);
    }

    // value of the analyser at each pixel column, with optional smoothing across frequency
    std::vector<float> columns (const SpectrumAnalyzer& a, const std::vector<float>& db, int w, bool lin, int smooth) const
    {
        std::vector<float> out ((size_t) w, -200.0f); const double bh = sr / a.fftSize(); const int bins = (int) db.size();
        for (int px = 0; px < w; ++px)
        {
            const double f0 = xToFreq ((float) px - 0.5f, lin, (float) w), f1 = xToFreq ((float) px + 0.5f, lin, (float) w); double lo = juce::jmax (20.0, f0), hi = juce::jmax (lo, f1);
            if (smooth > 0) { const double half = std::pow (2.0, (smooth == 1 ? 1.0 / 6.0 : 1.0 / 3.0) * 0.5); const double c = std::sqrt (lo * hi); lo = c / half; hi = c * half; }
            int k0 = juce::jlimit (1, bins - 2, (int) std::floor (lo / bh)), k1 = juce::jlimit (k0, bins - 2, (int) std::ceil (hi / bh));
            if (smooth > 0) { double p = 0; for (int k = k0; k <= k1; ++k) p += std::pow (10.0, db[(size_t) k] / 10.0); out[(size_t) px] = (float) (10.0 * std::log10 (juce::jmax (1e-20, p / (k1 - k0 + 1)))); }
            else { float m = -200.0f; for (int k = k0; k <= k1; ++k) m = juce::jmax (m, db[(size_t) k]); if (k1 - k0 < 1) { const double fk = 0.5 * (lo + hi) / bh; const int kk = juce::jlimit (1, bins - 2, (int) fk); const float t = (float) (fk - kk); m = db[(size_t) kk] * (1 - t) + db[(size_t) kk + 1] * t; } out[(size_t) px] = m; }
        }
        return out;
    }

    void paintSpectrum (juce::Graphics& g)
    {
        auto r = dispR.toFloat().reduced (8.0f); r.removeFromLeft (40.0f); r.removeFromBottom (20.0f); r.removeFromTop (6.0f);
        const float floors[3] = { -60.0f, -90.0f, -120.0f }; const float fl = floors[juce::jlimit (0, 2, (int) prm ("range"))]; const bool lin = prm ("linear") > 0.5f;
        auto Y = [&] (float db) { return r.getY() + juce::jlimit (0.0f, 1.0f, -db / -fl) * r.getHeight(); };
        g.setFont (F (11.0f));
        for (float d = 0; d >= fl; d -= (fl == -120.0f ? 20.0f : 10.0f)) { g.setColour (kGrid); g.drawHorizontalLine ((int) Y (d), r.getX(), r.getRight()); g.setColour (kAxis); g.drawText (juce::String ((int) d), (int) dispR.getX() + 2, (int) Y (d) - 7, 38, 14, juce::Justification::centredRight); }
        const double fr[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 }; const double frl[] = { 0, 2000, 4000, 6000, 8000, 10000, 12000, 14000, 16000, 18000, 20000 };
        auto vline = [&] (double f) { const float x = r.getX() + freqToX (f, lin, r.getWidth()); g.setColour (kGrid); g.drawVerticalLine ((int) x, r.getY(), r.getBottom()); g.setColour (kAxis); g.drawText (f >= 1000 ? juce::String (f / 1000) + "k" : juce::String ((int) f), (int) x - 20, (int) r.getBottom() + 4, 40, 14, juce::Justification::centred); };
        if (lin) for (double f : frl) vline (f); else for (double f : fr) vline (f);
        if (anSize == 0) return;
        const int w = (int) r.getWidth(), ch = (int) prm ("specChan"), sm = (int) prm ("smooth"); const bool two = ch >= 4, fill = prm ("fill") > 0.5f, hold = prm ("hold") > 0.5f;
        auto draw = [&] (const SpectrumAnalyzer& a, juce::Colour c)
        {
            auto col = columns (a, a.spectrum(), w, lin, sm); juce::Path line, area; area.startNewSubPath (r.getX(), r.getBottom());
            for (int px = 0; px < w; ++px) { const float y = Y (col[(size_t) px]); if (px == 0) line.startNewSubPath (r.getX(), y); else line.lineTo (r.getX() + (float) px, y); area.lineTo (r.getX() + (float) px, y); }
            area.lineTo (r.getRight(), r.getBottom()); area.closeSubPath();
            if (fill) { g.setGradientFill (juce::ColourGradient (c.withAlpha (0.55f), 0, r.getY(), c.withAlpha (0.04f), 0, r.getBottom(), false)); g.fillPath (area); }
            g.setColour (c); g.strokePath (line, juce::PathStrokeType (1.5f));
            if (hold) { auto hc = columns (a, a.peakHold(), w, lin, 0); juce::Path hp; for (int px = 0; px < w; ++px) { const float y = Y (hc[(size_t) px]); if (px == 0) hp.startNewSubPath (r.getX(), y); else hp.lineTo (r.getX() + (float) px, y); } g.setColour (c.brighter (0.4f).withAlpha (0.55f)); g.strokePath (hp, juce::PathStrokeType (1.0f)); }
            return col;
        };
        const juce::Colour c0 = ch == 1 ? kL : ch == 2 ? kR : ch == 3 ? kS : ch == 4 ? kL : kM, c1 = ch == 4 ? kR : kS;
        auto col0 = draw (an[0], c0); if (two) draw (an[1], c1);
        if (peak.db > -80.0f && peak.hz > 20.0)
        {
            const float x = r.getX() + freqToX (peak.hz, lin, r.getWidth()), y = Y (peak.db); g.setColour (sp::gold()); g.fillEllipse (x - 3, y - 3, 6, 6);
            g.setFont (F (13.0f, true)); g.drawText (juce::String (peak.db, 1) + " dB  " + juce::String (peak.hz, peak.hz < 1000 ? 1 : 0) + " Hz  " + juce::String (noteName (peak.hz)), juce::Rectangle<float> (juce::jmin (x + 8, r.getRight() - 230), juce::jmax (r.getY(), y - 22), 230, 16), juce::Justification::centredLeft);
        }
        if (hasHover && r.contains (hover))
        {
            const double f = xToFreq (hover.x - r.getX(), lin, r.getWidth()); const int px = juce::jlimit (0, w - 1, (int) (hover.x - r.getX()));
            g.setColour (juce::Colours::white.withAlpha (0.4f)); g.drawVerticalLine ((int) hover.x, r.getY(), r.getBottom());
            g.setColour (kInk); g.setFont (F (13.0f)); g.drawText (juce::String (f, f < 1000 ? 1 : 0) + " Hz  " + juce::String (noteName (f)) + "  " + juce::String (col0[(size_t) px], 1) + " dB", juce::Rectangle<float> (juce::jmin (hover.x + 10, r.getRight() - 230), r.getY() + 4, 230, 16), juce::Justification::centredLeft);
        }
    }

    void paintStereo (juce::Graphics& g)
    {
        const auto gr = juce::Rectangle<float> ((float) dispR.getX(), (float) dispR.getY(), 484.0f, 480.0f);
        g.drawImageAt (gonio, dispR.getX(), dispR.getY());
        const float cx = gr.getCentreX(), cy = gr.getCentreY(), rad = 480 * 0.46f, zoom = prm ("zoom");
        g.setFont (F (11.0f)); const float rings[4] = { 0.0f, -6.0f, -12.0f, -24.0f };
        for (float d : rings) { const float rr = rad * zoom * std::pow (10.0f, d / 20.0f); if (rr < rad * 1.3f && rr > 4) { g.setColour (juce::Colour (0xff2c3a36)); g.drawEllipse (cx - rr, cy - rr, rr * 2, rr * 2, 1.0f); g.setColour (kAxis); g.drawText (juce::String ((int) d) + " dB", (int) (cx + rr * 0.707f + 2), (int) (cy - rr * 0.707f - 12), 44, 12, juce::Justification::centredLeft); } }
        g.setColour (juce::Colour (0xff2c3a36)); g.drawLine (cx, gr.getY() + 6, cx, gr.getBottom() - 6); g.drawLine (gr.getX() + 6, cy, gr.getRight() - 6, cy); g.drawLine (cx - rad * 0.7f, cy + rad * 0.7f, cx + rad * 0.7f, cy - rad * 0.7f); g.drawLine (cx - rad * 0.7f, cy - rad * 0.7f, cx + rad * 0.7f, cy + rad * 0.7f);
        g.setColour (kAxis); g.setFont (F (13.0f, true)); g.drawText ("M", (int) cx - 10, (int) gr.getY() + 4, 20, 14, juce::Justification::centred); g.drawText ("L", (int) (cx - rad * 0.7f) - 18, (int) (cy - rad * 0.7f) - 10, 20, 14, juce::Justification::centred); g.drawText ("R", (int) (cx + rad * 0.7f) + 2, (int) (cy - rad * 0.7f) - 10, 20, 14, juce::Justification::centred);
        // right side: correlation meter and numbers
        const float corr = proc.live.corr.load(), width = proc.live.width.load(), bal = proc.live.balance.load(), loss = proc.live.monoLoss.load(); StereoStats st; st.correlation = corr; st.monoLossDb = loss;
        auto pr = juce::Rectangle<float> ((float) dispR.getX() + 504, (float) dispR.getY() + 24, 386.0f, 440.0f);
        g.setColour (kInk); g.setFont (F (15.0f, true)); g.drawText ("CORRELATION", pr.getX(), pr.getY(), 300, 18, juce::Justification::centredLeft);
        auto bar = pr.withTrimmedTop (28).withHeight (30); g.setColour (juce::Colour (0xff0e1012)); g.fillRoundedRectangle (bar, 3.0f); g.setColour (kLine); g.drawRoundedRectangle (bar, 3.0f, 1.0f);
        const float bx = bar.getX() + (corr + 1.0f) * 0.5f * bar.getWidth(); g.setColour (corr < 0 ? kR : sp::mint()); g.fillRect (juce::Rectangle<float> (juce::jmin (bx, bar.getCentreX()), bar.getY() + 3, std::fabs (bx - bar.getCentreX()), bar.getHeight() - 6));
        g.setColour (kInk); g.fillRect (bx - 1.0f, bar.getY(), 3.0f, bar.getHeight());
        g.setColour (kAxis); g.setFont (F (11.0f)); g.drawText ("-1 Antiphase", (int) bar.getX(), (int) bar.getBottom() + 3, 110, 14, juce::Justification::centredLeft); g.drawText ("0 Stereo", (int) bar.getCentreX() - 40, (int) bar.getBottom() + 3, 80, 14, juce::Justification::centred); g.drawText ("+1 Mono", (int) bar.getRight() - 90, (int) bar.getBottom() + 3, 90, 14, juce::Justification::centredRight);
        float y = bar.getBottom() + 40.0f; auto row = [&] (const juce::String& a, const juce::String& b2, juce::Colour c = kInk) { g.setColour (kDim); g.setFont (F (14.0f)); g.drawText (a, pr.getX(), y, 200, 20, juce::Justification::centredLeft); g.setColour (c); g.setFont (F (16.0f, true)); g.drawText (b2, pr.getX() + 180, y, 200, 20, juce::Justification::centredRight); y += 30.0f; };
        row ("Correlation", juce::String (corr, 2)); row ("Width (side share)", juce::String (juce::roundToInt (width * 100.0f)) + " %"); row ("Balance", bal < -0.02f ? "L " + juce::String (juce::roundToInt (-bal * 100.0f)) + " %" : bal > 0.02f ? "R " + juce::String (juce::roundToInt (bal * 100.0f)) + " %" : "centre");
        row ("Mono loss", juce::String (loss, 1) + " dB", loss < -3.0f ? sp::cherry() : kInk);
        const char* v = StereoMeter::verdict (st); const juce::Colour vc = std::string (v) == "Mono safe" ? sp::mint() : std::string (v) == "Check mono" ? sp::gold() : sp::cherry();
        y += 8; g.setColour (vc); g.setFont (F (26.0f, true)); g.drawText (proc.live.hasSignal.load() ? v : "No signal", pr.getX(), y, 380, 34, juce::Justification::centredLeft);
        g.setColour (kDim); g.setFont (F (12.0f)); juce::Rectangle<float> note (pr.getX(), y + 40, 380, 70); g.drawFittedText ("Mono loss is how much quieter the sound gets when left and right are added. Thresholds are rules of thumb: below 0.2 correlation or more than 3 dB lost means check it in mono.", note.toNearestInt(), juce::Justification::topLeft, 4);
    }

    std::vector<float> curveEq (bool side) const
    {
        const auto& ref = side ? proc.refShape.side : proc.refShape.mid; std::vector<float> mine (kShapeBands);
        for (int i = 0; i < kShapeBands; ++i) mine[(size_t) i] = proc.hasMine ? (side ? proc.mineShape.side[(size_t) i] : proc.mineShape.mid[(size_t) i]) : (side ? liveS[i] : liveM[i]);
        return matchingEq (ref, mine, prm ("strength"), prm ("maxMove"));
    }
    juce::String eqText() const
    {
        if (! proc.hasRef) return {}; juce::String s; for (int sd = 0; sd < 2; ++sd) { auto eq = curveEq (sd == 1); s << (sd ? "Side:\n" : "Mid:\n"); for (int i = 0; i < kShapeBands; i += 2) if (std::fabs (eq[(size_t) i]) >= 0.5f) s << juce::String (shapeBandHz (i), 0) << " Hz: " << (eq[(size_t) i] > 0 ? "+" : "") << juce::String (eq[(size_t) i], 1) << " dB\n"; }
        return s;
    }
    void paintShapes (juce::Graphics& g)
    {
        auto r = dispR.toFloat().reduced (8.0f); r.removeFromLeft (40.0f); r.removeFromBottom (20.0f); r.removeFromTop (6.0f); auto top = r.removeFromTop (r.getHeight() * 0.58f); r.removeFromTop (16.0f);
        const bool sd = showSide; const char* nm = sd ? "SIDE" : "MID";
        auto X = [&] (int i) { return top.getX() + (i + 0.5f) / kShapeBands * top.getWidth(); };
        const float lo = -90.0f, hi = -10.0f; auto Yd = [&] (float d) { return top.getY() + (1.0f - juce::jlimit (0.0f, 1.0f, (d - lo) / (hi - lo))) * top.getHeight(); };
        g.setFont (F (11.0f)); for (float d = hi; d >= lo; d -= 20) { g.setColour (kGrid); g.drawHorizontalLine ((int) Yd (d), top.getX(), top.getRight()); g.setColour (kAxis); g.drawText (juce::String ((int) d), (int) dispR.getX() + 2, (int) Yd (d) - 7, 38, 14, juce::Justification::centredRight); }
        const int ticks[] = { 0, 6, 12, 18, 24, 30, 36, 42, 48, 54 }; for (int t : ticks) { const float x = X (t); g.setColour (kGrid); g.drawVerticalLine ((int) x, top.getY(), top.getBottom()); g.setColour (kAxis); const double f = shapeBandHz (t); g.drawText (f >= 1000 ? juce::String (f / 1000, 1) + "k" : juce::String ((int) f), (int) x - 20, (int) top.getBottom() + 3, 40, 14, juce::Justification::centred); }
        auto plot = [&] (const float* v, juce::Colour c, float th, bool dashed = false)
        {
            juce::Path p; for (int i = 0; i < kShapeBands; ++i) { if (i == 0) p.startNewSubPath (X (i), Yd (v[i])); else p.lineTo (X (i), Yd (v[i])); } g.setColour (c);
            if (dashed) { juce::PathStrokeType (th).createDashedStroke (p, p, std::array<float, 2> { 5.0f, 4.0f }.data(), 2); g.fillPath (p); } else g.strokePath (p, juce::PathStrokeType (th));
        };
        if (proc.hasRef) plot ((sd ? proc.refShape.side : proc.refShape.mid).data(), kM, 2.0f);
        if (proc.hasMine) plot ((sd ? proc.mineShape.side : proc.mineShape.mid).data(), sp::mint(), 2.0f); else if (liveValid) plot (sd ? liveS : liveM, sp::mint(), 1.5f, true);
        g.setFont (F (13.0f, true)); g.setColour (kM); g.drawText ("Reference " + juce::String (nm) + (proc.hasRef ? "" : " (none yet)"), (int) top.getX() + 6, (int) top.getY() + 2, 260, 16, juce::Justification::centredLeft);
        g.setColour (sp::mint()); g.drawText (proc.hasMine ? "Mine (captured)" : "Mine (live, dashed)", (int) top.getX() + 6, (int) top.getY() + 18, 260, 16, juce::Justification::centredLeft);
        if (capturing) { g.setColour (kR); g.setFont (F (15.0f, true)); g.drawText ("CAPTURING " + juce::String (capturing == 1 ? "REFERENCE" : "MINE") + "  " + juce::String (capture.count()) + " frames. Press the button again to finish.", (int) top.getX() + 270, (int) top.getY() + 4, 560, 18, juce::Justification::centredLeft); }
        // EQ curve
        auto eqr = r; g.setColour (kInk); g.setFont (F (13.0f, true)); g.drawText ("EQ THAT MOVES MINE TOWARD THE REFERENCE (" + juce::String (nm) + ")", (int) eqr.getX(), (int) dispR.getBottom() - (int) eqr.getHeight() - 26, 600, 16, juce::Justification::centredLeft);
        eqr.removeFromTop (4.0f); const float mm = prm ("maxMove"); auto Ye = [&] (float d) { return eqr.getCentreY() - d / mm * eqr.getHeight() * 0.5f; };
        g.setColour (juce::Colour (0xff2c3a36)); g.drawHorizontalLine ((int) Ye (0), eqr.getX(), eqr.getRight()); g.setColour (kAxis); g.setFont (F (11.0f)); g.drawText ("+" + juce::String ((int) mm), (int) dispR.getX() + 2, (int) Ye (mm) - 6, 38, 12, juce::Justification::centredRight); g.drawText ("-" + juce::String ((int) mm), (int) dispR.getX() + 2, (int) Ye (-mm) - 6, 38, 12, juce::Justification::centredRight); g.drawText ("0", (int) dispR.getX() + 2, (int) Ye (0) - 6, 38, 12, juce::Justification::centredRight);
        if (! proc.hasRef) { g.setColour (kDim); g.setFont (F (15.0f)); g.drawText ("Capture or load a reference shape to see the EQ curve.", eqr.toNearestInt(), juce::Justification::centred); return; }
        auto eq = curveEq (sd); juce::Path p; for (int i = 0; i < kShapeBands; ++i) { const float x = eqr.getX() + (i + 0.5f) / kShapeBands * eqr.getWidth(), y = Ye (eq[(size_t) i]); if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y); }
        g.setColour (sp::orange()); g.strokePath (p, juce::PathStrokeType (2.5f));
        int worst = 0; for (int i = 1; i < kShapeBands; ++i) if (std::fabs (eq[(size_t) i]) > std::fabs (eq[(size_t) worst])) worst = i;
        g.setColour (kInk); g.setFont (F (13.0f)); g.drawText (std::fabs (eq[(size_t) worst]) < 0.5f ? "Already close to the reference." : juce::String (eq[(size_t) worst] > 0 ? "Biggest move: boost " : "Biggest move: cut ") + juce::String (std::fabs (eq[(size_t) worst]), 1) + " dB around " + juce::String (shapeBandHz (worst), 0) + " Hz (" + juce::String (noteName (shapeBandHz (worst))) + ")", (int) eqr.getX() + 10, (int) eqr.getY() + 2, 560, 16, juce::Justification::centredLeft);
    }

    //==========================================================================
    void paintMeters (juce::Graphics& g)
    {
        g.setColour (kPanel2); g.fillRoundedRectangle (metR.toFloat(), 4.0f); g.setColour (kLine); g.drawRoundedRectangle (metR.toFloat(), 4.0f, 1.0f);
        g.setColour (kDim); g.setFont (F (13.0f, true)); g.drawText ("LEVEL", metR.getX() + 8, metR.getY() + 6, 80, 16, juce::Justification::centredLeft);
        const float pk[4] = { proc.live.peakL.load(), proc.live.peakR.load(), proc.live.peakM.load(), proc.live.peakS.load() }, rm[4] = { proc.live.rmsL.load(), proc.live.rmsR.load(), proc.live.rmsM.load(), proc.live.rmsS.load() };
        const char* nn[4] = { "L", "R", "M", "S" }; const juce::Colour cc[4] = { kL, kR, kM, kS };
        const float top = (float) metR.getY() + 34.0f, bot = (float) metR.getY() + 270.0f; auto Y = [&] (float d) { return top + (juce::jlimit (-60.0f, 0.0f, d) / -60.0f) * (bot - top); };
        g.setFont (F (10.0f)); for (int d = 0; d >= -60; d -= 6) { g.setColour (kGrid); g.drawHorizontalLine ((int) Y ((float) d), (float) metR.getX() + 30, (float) metR.getRight() - 6); g.setColour (d >= -6 ? sp::amber() : kAxis); g.drawText (juce::String (d), metR.getX() + 2, (int) Y ((float) d) - 6, 26, 12, juce::Justification::centredRight); }
        for (int i = 0; i < 4; ++i)
        {
            const float x = (float) metR.getX() + 36.0f + i * 34.0f, db = dbOf (rm[i]); g.setColour (cc[i].withAlpha (0.85f)); g.fillRect (x, Y (db), 22.0f, bot - Y (db));
            const float pd = dbOf (pk[i]); g.setColour (pd > -0.1f ? kR : juce::Colours::white); g.fillRect (x, Y (pd), 22.0f, 2.0f);
            g.setColour (cc[i]); g.setFont (F (12.0f, true)); g.drawText (nn[i], (int) x - 4, (int) bot + 2, 30, 14, juce::Justification::centred);
            g.setColour (kInk); g.setFont (F (10.0f)); g.drawText (pd > -90 ? juce::String (pd, 1) : "-inf", (int) x - 6, (int) bot + 16, 34, 12, juce::Justification::centred);
        }
        g.setColour (kDim); g.setFont (F (10.0f)); g.drawText ("bars: RMS  line: peak", metR.getX() + 6, (int) bot + 30, 166, 12, juce::Justification::centredLeft);
        float y = bot + 52.0f; const float lufs = proc.live.lufs.load(); auto row = [&] (const char* a, const juce::String& b, juce::Colour c = kInk) { g.setColour (kDim); g.setFont (F (12.0f)); g.drawText (a, metR.getX() + 8, (int) y, 80, 16, juce::Justification::centredLeft); g.setColour (c); g.setFont (F (14.0f, true)); g.drawText (b, metR.getX() + 80, (int) y, 90, 16, juce::Justification::centredRight); y += 20; };
        row ("Momentary", lufs > -150 ? juce::String (lufs, 1) + " LUFS" : "-inf"); row ("Correlation", juce::String (proc.live.corr.load(), 2)); row ("Clip", (pk[0] > 0.999f || pk[1] > 0.999f) ? "yes" : "no", (pk[0] > 0.999f || pk[1] > 0.999f) ? kR : kInk);
        g.setColour (kDim); g.setFont (F (10.0f)); g.drawText ("Loudness: K weighted, 400 ms. A monitor, not a certified meter.", metR.getX() + 6, (int) y, 166, 28, juce::Justification::topLeft);
    }

    //==========================================================================
    void saveShapeDialog()
    {
        if (! proc.hasRef) { juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::InfoIcon).withTitle ("Save shape").withMessage ("Capture or load a reference shape first.").withButton ("OK"), nullptr); return; }
        auto* w = new juce::AlertWindow ("Save shape", "Name of the shape:", juce::MessageBoxIconType::NoIcon); w->addTextEditor ("n", proc.refShape.name.empty() ? "My shape" : juce::String (proc.refShape.name)); w->addButton ("Save", 1); w->addButton ("Cancel", 0);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
        {
            if (r != 1)
                return;
            auto name = w->getTextEditorContents ("n").trim().retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-").substring (0, 40); if (name.isEmpty()) name = "My shape";
            proc.refShape.name = name.toStdString(); auto dir = WhiskerScopeProcessor::shapeDir(); dir.createDirectory(); dir.getChildFile (name + ".wshape").replaceWithText (juce::String (shapeToText (proc.refShape)));
        }), true);
    }
    static Shape builtin (int kind)
    {
        Shape s; const char* nm[3] = { "Start: pink (flat to the ear)", "Start: bright lead", "Start: heavy bass" }; s.name = nm[kind];
        for (int i = 0; i < kShapeBands; ++i)
        {
            const double f = shapeBandHz (i), oct = std::log2 (f / 1000.0); double d = -3.0 * oct;                 // pink: -3 dB per octave
            if (kind == 1) d = -1.5 * oct + (f > 5000 ? -6.0 : 0.0); else if (kind == 2) d = -4.5 * oct + 8.0 * std::exp (-std::pow (std::log2 (f / 60.0), 2.0));
            s.mid[(size_t) i] = (float) (-40.0 + d); s.side[(size_t) i] = (float) (-52.0 + d - (kind == 2 ? 6.0 : 0.0));
        }
        return s;
    }
    void loadShapeMenu()
    {
        juce::PopupMenu m; m.addSectionHeader ("Starting points (made by us, not measured from any song)"); for (int i = 0; i < 3; ++i) m.addItem (100 + i, builtin (i).name);
        auto files = WhiskerScopeProcessor::shapeDir().findChildFiles (juce::File::findFiles, false, "*.wshape"); if (files.size() > 0) { m.addSeparator(); m.addSectionHeader ("Your shapes"); for (int i = 0; i < juce::jmin (50, files.size()); ++i) m.addItem (200 + i, files[i].getFileNameWithoutExtension()); }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&loadShape), [this, files] (int id)
        {
            if (id >= 100 && id < 103) { proc.refShape = builtin (id - 100); proc.hasRef = true; }
            else if (id >= 200 && id - 200 < files.size()) { Shape s; if (files[id - 200].getSize() < 5000 && shapeFromText (files[id - 200].loadFileAsString().trim().toStdString(), s)) { proc.refShape = s; proc.hasRef = true; } }
        });
    }
    void pasteShapeText()
    {
        Shape s; if (shapeFromText (juce::SystemClipboard::getTextFromClipboard().trim().toStdString(), s)) { proc.refShape = s; proc.hasRef = true; }
        else juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon).withTitle ("Paste shape").withMessage ("The clipboard does not hold a valid shape text.").withButton ("OK"), nullptr);
    }
    void showSettings()
    {
        auto* w = new juce::AlertWindow ("Settings", "Whisker Scope v" JucePlugin_VersionString, juce::MessageBoxIconType::NoIcon);
        w->addButton (proc.tooltips ? "Info bubbles: ON (click to turn off)" : "Info bubbles: OFF (click to turn on)", 1); w->addButton ("Close", 0);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r) { if (r == 1) { proc.tooltips = ! proc.tooltips; if (onTooltipChange) onTooltipChange(); } }), true);
    }
    void showAbout()
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::NoIcon).withTitle ("About Whisker Scope")
            .withMessage ("Whisker Scope v" JucePlugin_VersionString "\nFree and open source (AGPL-3.0-or-later), made by Studio 87.\nIt collects nothing, needs no account and never changes your sound.\n\nIf it helps your music, a coffee keeps new plugins coming.")
            .withButton ("Support on Ko-fi").withButton ("Close"), [] (int r) { if (r == 1) juce::URL ("https://ko-fi.com/studio87").launchInDefaultBrowser(); });
    }
public:
    std::function<void()> onTooltipChange;
};

//==============================================================================
WhiskerScopeEditor::WhiskerScopeEditor (WhiskerScopeProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    static Look look; setLookAndFeel (&look);
    content = std::make_unique<Content> (p); addAndMakeVisible (*content); content->setSize (W, H);
    tooltip = std::make_unique<juce::TooltipWindow> (this, p.tooltips ? 1000 : 100000000);
    content->onTooltipChange = [this] { tooltip->setMillisecondsBeforeTipAppears (proc.tooltips ? 1000 : 100000000); };
    constrainer.setFixedAspectRatio ((double) W / H); constrainer.setSizeLimits (700, (int) (700.0 * H / W), 2400, (int) (2400.0 * H / W)); setConstrainer (&constrainer);
    corner = std::make_unique<juce::ResizableCornerComponent> (this, &constrainer); addAndMakeVisible (*corner);
    setResizable (true, false);
    const int w = juce::jlimit (700, 2400, proc.editorWidth); setSize (w, (int) std::lround (w * (double) H / W));
}

WhiskerScopeEditor::~WhiskerScopeEditor() { setLookAndFeel (nullptr); }

void WhiskerScopeEditor::resized()
{
    const float s = (float) getWidth() / (float) W;
    content->setTransform (juce::AffineTransform::scale (s)); content->setBounds (0, 0, W, H);
    if (corner) corner->setBounds (getWidth() - 26, getHeight() - 26, 26, 26);
    proc.editorWidth = getWidth();
}
