#pragma once
// The four displays (Spectrum, Scope, Stereo field, Shapes) and the meter drawing. Ported from the approved mockup, same maths and same pixels.
// Components are laid out in design pixels (1360 x 920 window); the editor scales everything.
#include "Look.h"
#include "PluginProcessor.h"

namespace wsui
{
inline float dbm (float x) { return 20.0f * std::log10 (std::max (x, 1.0e-6f)); }
inline double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

struct Lv { float pk[2] { -120, -120 }, rms[2] { -120, -120 }, disp[2] { -120, -120 }, hold[2] { -120, -120 }; bool clip[2] { false, false }; };

// Data every display and meter reads, refreshed once per frame by the editor.
struct Env
{
    static constexpr int N = 32768;
    explicit Env (WhiskerScopeProcessor& pr) : p (pr), tL (N), tR (N), sL (N), sR (N), tmpA (131072), tmpB (131072) {}
    WhiskerScopeProcessor& p; Palette pal; WhiskerScopeProcessor::Settings st;
    std::vector<float> tL, tR, sL, sR, tmpA, tmpB;      // newest N samples after everything / sidechain
    Lv out, in; bool silent = true, scOn = false; float corr = 0, monoLoss = 0, width = 0, bal = 0; int verdict = -1;
    double sr = 48000; float dt = 0.033f; bool hadSound = false;

    void update (float dtSec)
    {
        dt = dtSec; sr = p.sampleRateNow.load();
        p.outL.latest (tL.data(), N); p.outR.latest (tR.data(), N);
        scOn = p.scEnabled.load() && p.scSeen.load();
        if (scOn) { p.scL.latest (sL.data(), N); p.scR.latest (sR.data(), N); }
        const int rn = juce::jlimit (64, 131072, (int) std::lround (p.choiceNumber ("rms") * sr / 1000.0));
        p.outL.latest (tmpA.data(), rn); p.outR.latest (tmpB.data(), rn);
        rmsOf (out, tmpA.data(), tmpB.data(), rn); update (out, p.peakOut);
        p.inL.latest (tmpA.data(), rn); p.inR.latest (tmpB.data(), rn);
        rmsOf (in, tmpA.data(), tmpB.data(), rn); update (in, p.peakIn);
        const auto sn = ws::stereoNow (tL.data() + N - 4096, tR.data() + N - 4096, 4096);
        silent = sn.silent;
        if (! silent) { corr += (sn.correlation - corr) * (1.0f - std::pow (0.85f, dt * 60.0f)); monoLoss = sn.monoLossDb; width = sn.widthPct; bal = sn.balanceDb; hadSound = true; }
        verdict = ws::stereoVerdict (silent, corr, monoLoss);
    }
    void resetPeaks()
    {
        for (auto* l : { &out, &in }) { for (int c = 0; c < 2; ++c) { l->hold[c] = -120; l->clip[c] = false; } }
        p.lufsOut.requestReset(); p.lufsIn.requestReset(); p.lufsPre.requestReset();
    }
    bool anyClip() const { return out.clip[0] || out.clip[1] || in.clip[0] || in.clip[1]; }
private:
    static void rmsOf (Lv& l, const float* a, const float* b, int n)
    {
        double sa = 0, sb = 0; for (int i = 0; i < n; ++i) { sa += (double) a[i] * a[i]; sb += (double) b[i] * b[i]; }
        l.rms[0] = dbm ((float) std::sqrt (sa / n)); l.rms[1] = dbm ((float) std::sqrt (sb / n));
    }
    void update (Lv& l, std::atomic<float>* peaks)
    {
        for (int c = 0; c < 2; ++c)
        {
            l.pk[c] = dbm (p.takePeak (peaks[c]));
            l.disp[c] = std::max (l.pk[c], l.disp[c] - 24.0f * dt); if (l.pk[c] > l.hold[c]) l.hold[c] = l.pk[c]; if (l.pk[c] >= -0.1f) l.clip[c] = true;
        }
    }
};

// Common to all displays: hover position and a "no sound" hint.
class ViewBase : public juce::Component
{
public:
    explicit ViewBase (Env& e) : env (e) { setOpaque (true); }
    void mouseMove (const juce::MouseEvent& e) override { mx = (float) e.x; my = (float) e.y; }
    void mouseDrag (const juce::MouseEvent& e) override { mouseMove (e); }
    void mouseExit (const juce::MouseEvent&) override { mx = my = -1; }
    virtual void update (float) {}
    virtual void resetPeaks() {}
protected:
    Env& env; float mx = -1, my = -1;
    const Scheme& C() const { return *env.pal; }
    juce::Colour lc() const { return C().l; }
    void drawHint (juce::Graphics& g, const juce::String& line1, const juce::String& line2 = {})
    {
        txt (g, line1, (float) getWidth() * 0.5f, (float) getHeight() * 0.5f - (line2.isEmpty() ? -6.0f : 4.0f), col::dim, juce::Justification::centred, label (18.0f, 500, 0.1f));
        if (line2.isNotEmpty()) txt (g, line2, (float) getWidth() * 0.5f, (float) getHeight() * 0.5f + 24.0f, col::dim, juce::Justification::centred, label (18.0f, 500, 0.1f));
    }
    static void hline (juce::Graphics& g, float x0, float x1, float y) { g.fillRect (x0, std::floor (y), x1 - x0, 1.0f); }
    static void vline (juce::Graphics& g, float x, float y0, float y1) { g.fillRect (std::floor (x), y0, 1.0f, y1 - y0); }
    void tag (juce::Graphics& g, const juce::String& s, float bx, float by, float tw, float h = 18.0f, float px = 11.0f)
    {
        g.setColour (juce::Colour (0xee0a0a0c)); g.fillRect (bx, by, tw, h); mtxt (g, s, bx + 6, by + (h > 17 ? 13.0f : 12.0f), juce::Colours::white, juce::Justification::left, px);
    }
};

//==============================================================================
class SpecView : public ViewBase
{
public:
    explicit SpecView (Env& e) : ViewBase (e) {}
    void resetPeaks() override { std::fill (pkA.begin(), pkA.end(), -200.0f); std::fill (pkB.begin(), pkB.end(), -200.0f); }
    void clearHistory() { wfClear = true; }

    void update (float dt) override
    {
        auto& p = env.p; sr = env.sr;
        const int fft = (int) p.choiceNumber ("fft"); const juce::String mode (p.choiceRaw ("ana"));
        const float k0 = std::string (p.choiceRaw ("speed")) == "slow" ? 0.93f : std::string (p.choiceRaw ("speed")) == "fast" ? 0.5f : 0.8f, k = std::pow (k0, dt * 60.0f);
        if (fft != curFft || sr != curSr) { curFft = fft; curSr = sr; spA.prepare (sr, fft); spB.prepare (sr, fft); spC.prepare (sr, fft); wfClear = true; fr.valid = false; }
        specBoth = mode == "lr" || mode == "ms"; specMode = mode;
        float t[4] = { 0.5f, 0.5f, 0, 0 };
        if (mode == "l") { t[0] = 1; t[1] = 0; } else if (mode == "r") { t[0] = 0; t[1] = 1; } else if (mode == "side") { t[0] = 0.5f; t[1] = -0.5f; } else if (mode == "lr") { t[0] = 1; t[1] = 0; t[2] = 0; t[3] = 1; } else if (mode == "ms") { t[2] = 0.5f; t[3] = -0.5f; }
        const bool frozen = p.param ("fzSpec") > 0.5f;
        if (frozen && ! wasFrozen) { fr.a = spA.spectrum(); fr.b = spB.spectrum(); fr.c = spC.spectrum(); fr.valid = true; fr.fft = fft; fr.bothAtFreeze = specBoth; }
        wasFrozen = frozen;
        if (! frozen)
        {
            bufA.resize ((size_t) fft); bufB.resize ((size_t) fft); bufC.resize ((size_t) fft);
            const float* L = env.tL.data() + Env::N - fft; const float* R = env.tR.data() + Env::N - fft;
            for (int i = 0; i < fft; ++i) { bufA[(size_t) i] = t[0] * L[i] + t[1] * R[i]; bufB[(size_t) i] = t[2] * L[i] + t[3] * R[i]; }
            spA.analyse (bufA.data(), k); if (specBoth) spB.analyse (bufB.data(), k);
            if (env.scOn) { const float* a = env.sL.data() + Env::N - fft; const float* b = env.sR.data() + Env::N - fft; for (int i = 0; i < fft; ++i) bufC[(size_t) i] = 0.5f * (a[i] + b[i]); spC.analyse (bufC.data(), k); }
            else spC.reset();
        }
        if (--maskTick <= 0 && env.scOn && p.choiceNumber ("maskDb") > 0)
        {
            maskTick = 3; if (spMA.fftSize() != 8192 || spMA.sampleRate() != sr) { spMA.prepare (sr, 8192); spMB.prepare (sr, 8192); }
            std::vector<float> a (8192), b (8192);
            for (int i = 0; i < 8192; ++i) { a[(size_t) i] = 0.5f * (env.tL[(size_t) (Env::N - 8192 + i)] + env.tR[(size_t) (Env::N - 8192 + i)]); b[(size_t) i] = 0.5f * (env.sL[(size_t) (Env::N - 8192 + i)] + env.sR[(size_t) (Env::N - 8192 + i)]); }
            spMA.analyse (a.data(), 0.8f); spMB.analyse (b.data(), 0.8f);
            const float md = (float) p.choiceNumber ("maskDb"); maskRanges.clear(); Run* cur = nullptr;
            for (int kk = -17; kk <= 13; ++kk)
            {
                const double fc = 1000.0 * std::pow (2.0, kk / 3.0), fa = fc * std::pow (2.0, -1.0 / 6.0), fb = fc * std::pow (2.0, 1.0 / 6.0);
                const float la = ws::bandDb (spMA.spectrum().data(), 4096, sr, fa, fb), lb = ws::bandDb (spMB.spectrum().data(), 4096, sr, fa, fb);
                const float hit = (la > -70 && lb > -70 && std::abs (la - lb) <= md) ? 1.0f : 0.0f; float& sc = maskSc[kk + 17]; sc = sc * 0.88f + hit * 0.12f;
                if (sc > 0.55f) { if (cur && cur->k == kk - 1) { cur->f1 = fb; cur->k = kk; } else { maskRanges.push_back ({ fa, fb, kk }); cur = &maskRanges.back(); } }
            }
        }
        draw (dt);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::disp); const auto& P = env.p;
        if (pw <= 0) return;
        const int rng = (int) P.choiceNumber ("range"); const float fl = (float) -rng; const bool lin = std::string (P.choiceRaw ("scale")) == "lin";
        const float W = (float) getWidth(), H = (float) getHeight(), ml = 44, mr = 8, mt = 8, mb = 20, ph = H - mt - mb;
        auto Y = [&] (float v) { return mt + ph * (1.0f - (juce::jlimit (fl, 0.0f, v) - fl) / (-fl)); };
        if (P.param ("wf") > 0.5f && wf.isValid()) { g.setOpacity (0.95f); g.drawImageAt (wf, (int) ml, (int) mt); g.setOpacity (1.0f); }
        g.setColour (col::grid);
        for (int d = 0; d >= (int) fl; d -= 10) hline (g, ml, W - mr, Y ((float) d) + 0.5f);
        for (int d = 0; d >= (int) fl; d -= 10) mtxt (g, juce::String (d), ml - 5, Y ((float) d) + 3, C().acc, juce::Justification::right, 10);
        static const double logF[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 }, linF[] = { 0, 2000, 4000, 6000, 8000, 10000, 12000, 14000, 16000, 18000, 20000 };
        auto lab = [&] (double f) { return f >= 1000 ? juce::String (f / 1000.0) + "k" : juce::String ((int) f); };
        auto drawF = [&] (double f) { const float x = xOfF (std::max (f, 20.0), ml, (float) pw, lin); g.setColour (col::grid); vline (g, x + 0.5f, mt, H - mb); mtxt (g, lab (f), x, H - 6, C().acc, juce::Justification::horizontallyCentred, 10); };
        if (lin) for (double f : linF) drawF (f); else for (double f : logF) drawF (f);
        mtxt (g, "dBFS", 4, 14, C().acc, juce::Justification::left, 10);
        const bool fill = P.param ("style") > 0.5f && P.param ("wf") < 0.5f;
        auto plot = [&] (const std::vector<float>& v, juce::Colour c, const std::vector<Band>& bands, float alpha)
        {
            if (! bands.empty()) { g.setColour (c.withAlpha (alpha)); for (auto& b : bands) g.fillRect (ml + (float) b.xa + 1, Y (b.v), std::max (1.0f, (float) (b.xb - b.xa - 1)), mt + ph - Y (b.v)); return; }
            juce::Path pth; bool st = false;
            for (int x = 0; x < pw; ++x) { if (v[(size_t) x] <= -150) continue; const float y = Y (v[(size_t) x]); if (! st) { pth.startNewSubPath (ml + x, y); st = true; } else pth.lineTo (ml + x, y); }
            if (fill && st)
            {
                juce::Path f (pth); f.lineTo (ml + (float) pw, mt + ph); f.lineTo (ml, mt + ph); f.closeSubPath();
                g.setGradientFill (juce::ColourGradient (c.withAlpha (0.8f), 0, mt, c.withAlpha (0.067f), 0, mt + ph, false)); g.fillPath (f);
            }
            g.setColour (c); g.strokePath (pth, juce::PathStrokeType (1.4f));
        };
        const juce::Colour colA = specMode == "side" ? C().acc : specMode == "r" ? C().r : specMode == "ms" ? C().m : C().l;
        { g.saveState(); g.setOpacity (1.0f); plot (vA, colA, bandsA, 0.75f); g.restoreState(); }
        if (specBoth) { g.beginTransparencyLayer (0.9f); plot (vB, specMode == "ms" ? C().acc : C().r, bandsB, 0.55f); g.endTransparencyLayer(); }
        const bool scOn = env.scOn;
        if (scOn) { g.beginTransparencyLayer (0.95f); plot (vC, col::cb, bandsC, 0.45f); g.endTransparencyLayer(); }
        const int maskDb = (int) P.choiceNumber ("maskDb");
        if (scOn && maskDb > 0)
        {
            for (auto& q : maskRanges)
            {
                const float x0 = ml + xOfF (std::max (20.0, q.f0), 0, (float) pw, lin), x1 = ml + xOfF (std::min (20000.0, q.f1), 0, (float) pw, lin);
                g.setColour (col::mask.withAlpha (0.2f)); g.fillRect (x0, mt, x1 - x0, ph); g.setColour (col::mask.withAlpha (0.9f)); g.fillRect (x0, mt, x1 - x0, 3.0f);
            }
            auto fm = [] (double f) { return f >= 1000 ? juce::String (std::round (f / 100.0) / 10.0, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + "k" : juce::String ((int) std::lround (f)); };
            juce::String s; if (maskRanges.empty()) s = "No masking found at this threshold"; else { juce::StringArray a; for (size_t i = 0; i < std::min<size_t> (5, maskRanges.size()); ++i) a.add (fm (maskRanges[i].f0) + "-" + fm (maskRanges[i].f1) + " Hz"); s = "Possible masking (A and B within " + juce::String (maskDb) + " dB): " + a.joinIntoString (", "); }
            mtxt (g, s, ml + 8, mt + 26, maskRanges.empty() ? juce::Colour (0xff9aa0ae) : juce::Colour (0xffff6fa5), juce::Justification::left, 11);
        }
        if (P.param ("hold") > 0.5f)
        {
            for (int qi = 0; qi < (specBoth ? 2 : 1); ++qi)
            {
                auto& pk = qi ? pkB : pkA; g.setColour ((qi ? juce::Colour (0xffe1cbf4) : juce::Colours::white).withAlpha (0.7f));
                juce::Path pth; bool st = false;
                for (int x = 0; x < pw; ++x) { if (pk[(size_t) x] <= -150) continue; const float y = Y (pk[(size_t) x]); if (! st) { pth.startNewSubPath (ml + x, y); st = true; } else pth.lineTo (ml + x, y); }
                g.strokePath (pth, juce::PathStrokeType (1.0f));
            }
        }
        const float hi = hiV; const int hx = hiX;
        if (hi > fl)
        {
            const double hf = fOfX ((float) hx + 0.5f, (float) pw, lin);
            juce::Path tri; tri.addTriangle (ml + (float) hx, Y (hi) - 3, ml + (float) hx - 4, Y (hi) - 10, ml + (float) hx + 4, Y (hi) - 10); g.setColour (juce::Colours::white); g.fillPath (tri);
            mtxt (g, "Highest: " + juce::String (hi, 1) + " dB @ " + fmtHz (hf).replace (" kHz", " kHz") + " (" + noteOf (hf) + ")", ml + 8, mt + 12, juce::Colours::white, juce::Justification::left, 11);
        }
        if (P.choiceNumber ("pkN") > 0)
        {
            for (auto& q : picks)
            {
                const double pf = fOfX ((float) q.x + 0.5f, (float) pw, lin);
                const juce::String l2 = juce::String (q.v, 1) + " dB  " + fmtHz (pf) + "  " + noteOf (pf); const float tw2 = (float) l2.length() * 6.4f + 10.0f;
                const float bx2 = std::min (W - mr - tw2, std::max (ml + 2, ml + (float) q.x - tw2 / 2)), by2 = std::max (mt + 34, Y (q.v) - 24);
                g.setColour (juce::Colours::white.withAlpha (0.67f)); vline (g, ml + (float) q.x + 0.5f, Y (q.v) - 2, by2 + 16);
                tag (g, l2, bx2, by2, tw2, 16.0f, 10.0f);
            }
        }
        if (scOn) mtxt (g, "B sidechain", W - mr - 4, mt + (P.param ("fzSpec") > 0.5f ? 26 : 12), col::cb, juce::Justification::right, 11);
        if (P.param ("fzSpec") > 0.5f) mtxt (g, "[FROZEN]", W - mr - 4, mt + 12, C().r, juce::Justification::right, 11);
        if (mx >= ml && mx <= ml + (float) pw && my >= mt && my <= mt + ph)
        {
            const int xi = juce::jlimit (0, pw - 1, (int) std::floor (mx - ml)); const double hf2 = fOfX ((float) xi + 0.5f, (float) pw, lin); const float hv = vA[(size_t) xi];
            g.setColour (juce::Colours::white.withAlpha (0.33f)); vline (g, mx + 0.5f, mt, mt + ph); hline (g, ml, ml + (float) pw, my + 0.5f);
            const juce::String l = fmtHz (hf2) + " " + noteOf (hf2) + "  Mag: " + (hv > -150 ? juce::String (hv, 1) : juce::String ("-inf")) + " dB"; const float tw = (float) l.length() * 6.6f + 12, bx = std::min (W - tw - 6, std::max (ml, mx + 10));
            tag (g, l, bx, std::max (mt, my - 24), tw);
        }
        if (env.silent && ! env.hadSound) drawHint (g, "No sound yet. Play something in your DAW.");
    }
    void resized() override { pw = (int) std::floor ((float) getWidth() - 44 - 8); vA.assign ((size_t) std::max (pw, 1), -200.0f); vB = vA; vC = vA; pkA = vA; pkB = vA; wfClear = true; }

private:
    struct Band { int xa, xb; float v; };
    struct Run { double f0, f1; int k; };
    struct Pick { int x; float v; };
    struct Frozen { std::vector<float> a, b, c; bool valid = false, bothAtFreeze = false; int fft = 0; } fr;
    ws::Spectrum spA, spB, spC, spMA, spMB; int curFft = 0; double curSr = 0, sr = 48000; bool specBoth = false, wasFrozen = false; juce::String specMode { "lr" };
    std::vector<float> bufA, bufB, bufC, vA, vB, vC, pkA, pkB; std::vector<Band> bandsA, bandsB, bandsC; int pw = 0;
    std::vector<Run> maskRanges; float maskSc[40] {}; int maskTick = 0; std::vector<Pick> picks; float hiV = -999; int hiX = 0; double labT[2] { 0, 0 }; juce::String labKey;
    juce::Image wf; bool wfClear = true;

    static float xOfF (double f, float W0, float W, bool lin) { return lin ? W0 + W * (float) ((f - 20.0) / 19980.0) : W0 + W * (float) (std::log (f / 20.0) / std::log (1000.0)); }
    static double fOfX (float x, float W, bool lin) { return lin ? 20.0 + 19980.0 * x / W : 20.0 * std::pow (1000.0, x / W); }

    std::vector<Band> curve (const std::vector<float>& arr, int n, std::vector<float>& out, double nyq, bool lin)
    {
        std::vector<Band> res; const float tilt = (float) env.p.choiceNumber ("slope"); const juce::String bs (env.p.choiceRaw ("bands")); const int bnd = bs == "full" ? 0 : bs.getIntValue();
        const float* a = arr.data(); const float W = (float) pw;
        if (! bnd)
        {
            for (int i = 0; i < pw; ++i)
            {
                const double f0 = fOfX ((float) i, W, lin), f1 = fOfX ((float) i + 1, W, lin), f = std::sqrt (f0 * f1);
                out[(size_t) i] = f > nyq ? -200.0f : ws::bandDb (a, n, sr, f0, std::min (f1, nyq)) + (tilt != 0 ? tilt * (float) std::log2 (f / 1000.0) : 0.0f);
            }
        }
        else
        {
            std::fill (out.begin(), out.end(), -200.0f);
            for (int k = -60; k < 60; ++k)
            {
                const double fc = 1000.0 * std::pow (2.0, (double) k / bnd), fa = fc * std::pow (2.0, -0.5 / bnd), fb = fc * std::pow (2.0, 0.5 / bnd);
                if (fc < 20 || fc > 20000 || fb > nyq) continue;
                const float v = ws::bandDb (a, n, sr, fa, fb) + (tilt != 0 ? tilt * (float) std::log2 (fc / 1000.0) : 0.0f);
                const int xa = std::max (0, (int) std::floor (xOfF (fa, 0, W, lin))), xb = std::min (pw - 1, (int) std::floor (xOfF (fb, 0, W, lin)));
                res.push_back ({ xa, xb, v }); for (int i = xa; i <= xb; ++i) out[(size_t) i] = v;
            }
        }
        return res;
    }

    void draw (float dt)
    {
        if (pw <= 0) return;
        const bool lin = std::string (env.p.choiceRaw ("scale")) == "lin"; const double nyq = sr / 2 * 0.98; const bool frozen = env.p.param ("fzSpec") > 0.5f && fr.valid;
        const auto& A = frozen ? fr.a : spA.spectrum(); const auto& B = frozen ? fr.b : spB.spectrum(); const int n = (int) A.size();
        if (n < 2) return;
        bandsA = curve (A, n, vA, nyq, lin); bandsB.clear(); if (specBoth) bandsB = curve (B, n, vB, nyq, lin);
        bandsC.clear(); if (env.scOn) bandsC = curve (frozen ? fr.c : spC.spectrum(), n, vC, nyq, lin);
        const float fl = (float) -env.p.choiceNumber ("range");
        if (env.p.param ("hold") > 0.5f && ! frozen)
            for (int x = 0; x < pw; ++x) { pkA[(size_t) x] = std::max (vA[(size_t) x], pkA[(size_t) x] - 6.0f * 0.033f * 0.25f * (dt * 30.0f)); if (specBoth) pkB[(size_t) x] = std::max (vB[(size_t) x], pkB[(size_t) x] - 6.0f * 0.033f * 0.25f * (dt * 30.0f)); }
        // labels refresh at the chosen speed
        const double now = nowMs(), iv = env.p.choiceNumber ("pkSpd"); const juce::String key = juce::String (pw) + (lin ? "l" : "g");
        bool due = false; if (labKey != key) { labKey = key; labT[0] = 0; }
        if (iv == 0 || labT[0] == 0 || now - labT[0] >= iv) { labT[0] = now; due = true; }
        if (due)
        {
            float hi0 = -999; int hx0 = 0; for (int x = 0; x < pw; ++x) { if (fOfX ((float) x + 0.5f, (float) pw, lin) < 25) continue; if (vA[(size_t) x] > hi0) { hi0 = vA[(size_t) x]; hx0 = x; } }
            hiV = hi0; hiX = hx0; picks.clear();
            const int want = (int) env.p.choiceNumber ("pkN");
            if (want > 0)
            {
                std::vector<Pick> cand;
                for (int xp = 4; xp < pw - 4; ++xp)
                {
                    const float vv = vA[(size_t) xp]; if (vv <= fl + 3 || fOfX ((float) xp + 0.5f, (float) pw, lin) < 25) continue;
                    bool isMax = true; float mn = vv;
                    for (int w = -4; w <= 4; ++w) if (vA[(size_t) (xp + w)] > vv || (w < 0 && vA[(size_t) (xp + w)] == vv)) { isMax = false; break; }
                    if (! isMax) continue;
                    for (int w = -16; w <= 16; ++w) { const int ix = xp + w; if (ix >= 0 && ix < pw && vA[(size_t) ix] < mn) mn = vA[(size_t) ix]; }
                    if (vv - mn >= 1) cand.push_back ({ xp, vv });
                }
                std::sort (cand.begin(), cand.end(), [] (const Pick& a, const Pick& b) { return a.v > b.v; });
                std::vector<int> used; for (auto& q : cand) { if ((int) picks.size() >= want) break; bool near = false; for (int u : used) if (std::abs (u - q.x) < 70) near = true; if (near) continue; used.push_back (q.x); picks.push_back (q); }
            }
        }
        if (env.p.param ("wf") > 0.5f && ! frozen) pushWaterfall (fl, lin, dt);
    }

    void pushWaterfall (float fl, bool, float dt)
    {
        const int w2 = pw, h2 = std::max (1, (int) std::round ((float) getHeight() - 8 - 20)); const bool rot = env.p.param ("rot") > 0.5f;
        if (! wf.isValid() || wf.getWidth() != w2 || wf.getHeight() != h2 || wfClear) { wf = juce::Image (juce::Image::ARGB, w2, h2, true, juce::SoftwareImageType()); juce::Graphics gg (wf); gg.fillAll (col::disp); wfClear = false; }
        const auto& S = C(); juce::Colour lut[256];
        for (int i = 0; i < 256; ++i) { const float pp = (float) i / 255.0f * 6.0f; const int a = (int) std::floor (pp), b = std::min (6, a + 1); lut[i] = S.wf[a].interpolatedWith (S.wf[b], pp - (float) a); }
        const int rows = juce::jlimit (1, 4, (int) std::lround (dt * 60.0f));
        juce::Image::BitmapData bd (wf, juce::Image::BitmapData::readWrite);
        for (int r = 0; r < rows; ++r)
        {
            if (! rot)
            {
                std::memmove (bd.getLinePointer (1), bd.getLinePointer (0), (size_t) (h2 - 1) * (size_t) bd.lineStride);
                for (int i = 0; i < w2; ++i) { const int q = (int) std::lround (juce::jlimit (0.0f, 1.0f, (vA[(size_t) i] - fl) / (-fl)) * 255.0f); bd.setPixelColour (i, 0, lut[q]); }
            }
            else
            {
                for (int y = 0; y < h2; ++y) std::memmove (bd.getLinePointer (y), bd.getLinePointer (y) + bd.pixelStride, (size_t) (w2 - 1) * (size_t) bd.pixelStride);
                for (int y = 0; y < h2; ++y) { const int fx = std::min (w2 - 1, (int) std::floor ((1.0f - (float) y / (float) h2) * (float) w2)); const int q = (int) std::lround (juce::jlimit (0.0f, 1.0f, (vA[(size_t) fx] - fl) / (-fl)) * 255.0f); bd.setPixelColour (w2 - 1, y, lut[q]); }
            }
        }
    }
};

//==============================================================================
class ScopeView : public ViewBase
{
public:
    explicit ScopeView (Env& e) : ViewBase (e), mono (Env::N) {}
    void update (float) override
    {
        auto& p = env.p; const bool fz = p.param ("fzScope") > 0.5f;
        if (fz && ! wasFz) { fL = env.tL; fR = env.tR; }
        wasFz = fz; const auto& L = fz && ! fL.empty() ? fL : env.tL; const auto& R = fz && ! fR.empty() ? fR : env.tR;
        for (int i = 0; i < Env::N; ++i) mono[(size_t) i] = (L[(size_t) i] + R[(size_t) i]) * 0.5f;
        useL = &L; useR = &R;
        if (--pitchTick <= 0) { pitchTick = 6; if (std::string (p.choiceRaw ("sync")) == "pitch") { const double f = ws::detectPitch (mono.data() + Env::N - 4096, 4096, env.sr); if (f > 0) detF = detF > 0 ? detF * 0.6 + f * 0.4 : f; } }
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::disp); if (useL == nullptr) return; auto& P = env.p; const auto& S = C();
        const std::string sync = P.choiceRaw ("sync"), chan = P.choiceRaw ("chan");
        const auto& L = *useL; const auto& R = *useR; const float W = (float) getWidth(), H = (float) getHeight(), ml = 40, mr = 8, mt = 8, mb = 20, pw = W - ml - mr, ph = H - mt - mb, mid = mt + ph / 2; const double sr = env.sr;
        const bool fz = P.param ("fzScope") > 0.5f; const float bpm = P.hostBpm.load() > 1 ? P.hostBpm.load() : 120.0f; const double ppq = P.hostPpq.load();
        double Nd;
        const float dur = P.param ("dur"), freq = P.param ("freq"), periods = std::round (P.param ("periods"));
        if (sync == "freq") Nd = periods * sr / freq;
        else if (sync == "pitch") Nd = detF > 0 ? periods * sr / detF : 0.05 * sr;
        else if (sync == "beat") { static const char* const dv[] = { "1/32", "1/16", "1/8", "1/4" }; static const double bw[] = { 0.125, 0.25, 0.5, 1.0 }; Nd = bw[juce::jlimit (0, 3, P.choiceIndex ("div"))] * 60.0 / bpm * sr; (void) dv; }
        else Nd = dur * sr / 1000.0;
        const int N = juce::jlimit (64, 16384, (int) std::lround (Nd)); const int len = Env::N, last = len - N; int s = last;
        if (sync == "trig" || sync == "freq" || sync == "pitch") { const float lv = P.param ("trig"); const int lim = std::max (1, last - 12000); for (int i = last; i > lim; --i) if (mono[(size_t) i - 1] < lv && mono[(size_t) i] >= lv) { s = i; break; } }
        bool beatOk = true;
        if (sync == "beat") { if (ppq >= 0) { static const double bw[] = { 0.125, 0.25, 0.5, 1.0 }; const double w = bw[juce::jlimit (0, 3, P.choiceIndex ("div"))]; const double off = std::fmod (ppq, w) * 60.0 / bpm * sr; s = std::max (0, (int) std::lround (len - off - N)); } else beatOk = false; }
        if (P.param ("trans") > 0.5f) { int best = -1; float prevE = 0; for (int i = 0; i < last; i += 32) { float e = 0; for (int j = 0; j < 32; ++j) e += std::abs (mono[(size_t) (i + j)]); e /= 32; if (e > 0.06f && e > 3 * prevE + 0.01f) best = i; prevE = e * 0.5f + prevE * 0.5f; } if (best >= 0) s = best; }
        s = juce::jlimit (0, len - N, s);
        auto A = [&] (float v) { return mid - juce::jlimit (-1.05f, 1.05f, v) * ph / 2 / 1.05f; };
        for (float a : { 1.0f, 0.5f, 0.25f, 0.125f }) { for (float sg : { -1.0f, 1.0f }) { g.setColour (col::grid); hline (g, ml, W - mr, A (sg * a) + 0.5f); } mtxt (g, juce::String (std::lround (20 * std::log10 (a))) + (a == 1.0f ? " dB" : ""), ml - 4, A (a) + 3, S.acc, juce::Justification::right, 10); }
        g.setColour (col::zero); hline (g, ml, W - mr, mid + 0.5f);
        const double ms = (double) N / sr * 1000.0;
        for (int i = 0; i <= 4; ++i) { const float xx = ml + pw * (float) i / 4; g.setColour (col::grid); vline (g, xx + 0.5f, mt, H - mb); mtxt (g, juce::String (ms * i / 4, ms < 20 ? 1 : 0) + " ms", xx, H - 6, S.acc, i == 4 ? juce::Justification::right : i == 0 ? juce::Justification::left : juce::Justification::horizontallyCentred, 10); }
        auto trace = [&] (const std::function<float (int)>& f, juce::Colour c, float alpha)
        {
            juce::Path pth; const int cols = (int) std::floor (pw);
            for (int x = 0; x < cols; ++x)
            {
                const int a = s + (int) std::floor ((double) x / cols * N), b = std::max (a + 1, s + (int) std::floor ((double) (x + 1) / cols * N)); float mn = 9, mx2 = -9;
                for (int j = a; j < b && j < len; ++j) { const float v = f (j); if (v < mn) mn = v; if (v > mx2) mx2 = v; }
                pth.startNewSubPath (ml + x + 0.5f, A (mx2)); pth.lineTo (ml + x + 0.5f, A (mn) + (b - a > 1 ? 0.0f : 1.0f));
            }
            g.setColour (c.withAlpha (alpha)); g.strokePath (pth, juce::PathStrokeType (1.4f));
        };
        auto fl = [&] (int j) { return L[(size_t) j]; }; auto fr = [&] (int j) { return R[(size_t) j]; }; auto fm = [&] (int j) { return mono[(size_t) j]; };
        auto fmid = fm; auto fside = [&] (int j) { return (L[(size_t) j] - R[(size_t) j]) * 0.5f; };
        if (chan == "lr") { trace (fl, S.l, 1.0f); trace (fr, S.r, 0.85f); } else if (chan == "l") trace (fl, S.l, 1.0f); else if (chan == "r") trace (fr, S.r, 1.0f); else if (chan == "mono") trace (fm, S.m, 1.0f); else { trace (fmid, S.m, 1.0f); trace (fside, S.acc, 0.85f); }
        if (env.scOn && chan != "ms") trace ([&] (int j) { return (env.sL[(size_t) j] + env.sR[(size_t) j]) * 0.5f; }, col::cb, 0.9f);
        if (P.choiceNumber ("pkN") > 0)
        {
            const double now = nowMs(), iv = P.choiceNumber ("pkSpd"); const juce::String key = juce::String (chan) + sync; bool due = false;
            if (key != labKey) { labKey = key; labT = 0; } if (iv == 0 || labT == 0 || now - labT >= iv) { labT = now; due = true; }
            if (due)
            {
                float pkv = 0; int pki = s; for (int i = s; i < s + N; ++i) { const float av = std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i])); if (av > pkv) { pkv = av; pki = i; } }
                lSv = pkv; lSf = (float) (pki - s) / (float) N; lSs = std::max (L[(size_t) pki], R[(size_t) pki]) >= -std::min (L[(size_t) pki], R[(size_t) pki]) ? 1.0f : -1.0f;
                if (sync != "pitch") { const double f3 = ws::detectPitch (mono.data() + len - 4096, 4096, sr); scPf = f3 > 0 ? (scPf > 0 ? scPf * 0.6 + f3 * 0.4 : f3) : 0; }
            }
            const float pkv = lSv; const int pki = juce::jlimit (s, s + N - 1, (int) std::lround (s + lSf * N));
            if (pkv > 0.001f)
            {
                const float px2 = ml + (float) (pki - s) / (float) N * pw, py2 = A (pkv * lSs); g.setColour (juce::Colours::white); g.fillEllipse (px2 - 3, py2 - 3, 6, 6);
                const bool rightSide = px2 > W - 90; mtxt (g, juce::String (20 * std::log10 (pkv), 1) + " dB", rightSide ? px2 - 8 : px2 + 8, std::max (mt + 24, py2 + (py2 < mid ? 14.0f : -6.0f)), juce::Colours::white, rightSide ? juce::Justification::right : juce::Justification::left, 11);
            }
        }
        juce::String info = sync == "pitch" ? (detF > 0 ? "Pitch " + juce::String (detF, 1) + " Hz (" + noteOf (detF) + ")" : juce::String ("Pitch: no clear tone")) : sync == "freq" ? "Locked " + juce::String (std::lround (freq)) + " Hz" : sync == "beat" ? "Beat sync " + juce::String (P.choiceRaw ("div")) + " at " + juce::String (std::lround (bpm)) + " BPM" + (beatOk ? "" : " (host gives no position, free run)") : sync == "trig" ? "Trigger" : "Free";
        mtxt (g, info + (fz ? "  [FROZEN]" : ""), ml + 6, mt + 12, col::key, juce::Justification::left, 11);
        if (P.choiceNumber ("pkN") > 0 && sync != "pitch") mtxt (g, scPf > 0 ? "Pitch " + juce::String (scPf, 1) + " Hz (" + noteOf (scPf) + ")" : juce::String ("Pitch: no clear tone"), ml + 6, mt + 26, col::key, juce::Justification::left, 11);
        std::vector<std::pair<juce::Colour, juce::String>> lg; if (chan == "lr") lg = { { S.l, "L" }, { S.r, "R" } }; else if (chan == "ms") lg = { { S.m, "Mid" }, { S.acc, "Side" } };
        if (env.scOn) lg.push_back ({ col::cb, "B sidechain" });
        float lx = W - mr - 8; for (auto& q : lg) { mtxt (g, q.second, lx, mt + 12, q.first, juce::Justification::right, 11); lx -= (float) q.second.length() * 7 + 14; }
        if (env.silent && ! env.hadSound) drawHint (g, "No sound yet. Play something in your DAW.");
    }
private:
    std::vector<float> mono, fL, fR; const std::vector<float>* useL = nullptr; const std::vector<float>* useR = nullptr; bool wasFz = false; int pitchTick = 0; double detF = 0, scPf = 0, labT = 0; juce::String labKey; float lSv = 0, lSf = 0, lSs = 1;
};

//==============================================================================
class StereoView : public ViewBase
{
public:
    explicit StereoView (Env& e) : ViewBase (e) {}
    void update (float) override
    {
        auto& p = env.p; const float W = (float) getWidth(), H = (float) getHeight(); if (W < 4 || H < 4) return;
        if (! trail.isValid() || trail.getWidth() != getWidth() || trail.getHeight() != getHeight()) { trail = juce::Image (juce::Image::ARGB, getWidth(), getHeight(), true, juce::SoftwareImageType()); juce::Graphics gg (trail); gg.fillAll (col::disp); }
        const bool fz = p.param ("fzSt") > 0.5f; juce::Graphics g (trail); const float zoom = p.param ("zoom"), sc = std::min (W, H) * 0.42f * zoom, cx = W / 2, cy = H / 2;
        if (fz) return;
        g.setColour (col::disp.withAlpha (1.0f - p.param ("pers") * 0.96f)); g.fillRect (0.0f, 0.0f, W, H);
        const int n = 1536, off = Env::N - n; juce::Path pa; float px = 0, py = 0;
        for (int i = 0; i < n; ++i) { const float l = env.tL[(size_t) (off + i)], r = env.tR[(size_t) (off + i)], M = (l + r) * 0.7071f, Sd = (l - r) * 0.7071f, x = cx + Sd * sc, y = cy - M * sc; if (i) { pa.startNewSubPath (px, py); pa.lineTo (x, y); } px = x; py = y; }
        g.setColour (C().l.withAlpha (0.75f)); g.strokePath (pa, juce::PathStrokeType (1.1f));
        if (env.scOn)
        {
            juce::Path pb; float qx = 0, qy = 0;
            for (int i = 0; i < n; ++i) { const float l = env.sL[(size_t) (off + i)], r = env.sR[(size_t) (off + i)], M = (l + r) * 0.7071f, Sd = (l - r) * 0.7071f, x = cx + Sd * sc, y = cy - M * sc; if (i) { pb.startNewSubPath (qx, qy); pb.lineTo (x, y); } qx = x; qy = y; }
            g.setColour (col::cb.withAlpha (0.6f)); g.strokePath (pb, juce::PathStrokeType (1.0f));
        }
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::disp); if (trail.isValid()) g.drawImageAt (trail, 0, 0);
        const float W = (float) getWidth(), H = (float) getHeight(), cx = W / 2, cy = H / 2, sc = std::min (W, H) * 0.42f * env.p.param ("zoom"); const auto& S = C();
        g.setColour (juce::Colour (0xff2b2f3a));
        for (float a : { 1.0f, 0.5f, 0.25f }) g.drawEllipse (cx - a * sc, cy - a * sc, 2 * a * sc, 2 * a * sc, 1.0f);
        for (float a : { 1.0f, 0.5f, 0.25f }) mtxt (g, juce::String (std::lround (20 * std::log10 (a))) + " dB", cx + a * sc * 0.7071f + 3, cy - a * sc * 0.7071f - 3, S.acc, juce::Justification::left, 10);
        g.setColour (juce::Colour (0xff2b2f3a)); hline (g, cx - sc * 1.05f, cx + sc * 1.05f, cy); vline (g, cx, cy - sc * 1.05f, cy + sc * 1.05f);
        g.setColour (col::zero); g.drawLine (cx - sc * 0.75f, cy + sc * 0.75f, cx + sc * 0.75f, cy - sc * 0.75f); g.drawLine (cx - sc * 0.75f, cy - sc * 0.75f, cx + sc * 0.75f, cy + sc * 0.75f);
        mtxt (g, "Mono (M)", cx + 6, 14, col::key, juce::Justification::left, 11); mtxt (g, "Side", W - 8, cy - 6, col::key, juce::Justification::right, 11);
        mtxt (g, "L", cx - sc * 0.78f, cy - sc * 0.78f, S.l, juce::Justification::right, 13); mtxt (g, "R", cx + sc * 0.78f, cy - sc * 0.78f, S.r, juce::Justification::left, 13); mtxt (g, "Antiphase", cx + 6, H - 8, S.r, juce::Justification::left, 11);
        if (env.scOn) mtxt (g, "B sidechain", 8, 16, col::cb, juce::Justification::left, 11);
        if (env.p.param ("fzSt") > 0.5f) mtxt (g, "[FROZEN]", W - 8, 16, S.r, juce::Justification::right, 11);
        if (env.silent && ! env.hadSound) drawHint (g, "No sound yet. Play something in your DAW.");
    }
    void resized() override { trail = {}; }
    void clearTrail() { trail = {}; }
private:
    juce::Image trail;
};

//==============================================================================
class ShapesView : public ViewBase
{
public:
    explicit ShapesView (Env& e) : ViewBase (e) {}
    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::disp); auto& P = env.p; const auto& V = P.shapes(); const auto& S = C();
        const float W = (float) getWidth(), H = (float) getHeight(), ml = 44, mr = 8, mt = 22, mb = 20, pw = W - ml - mr, tot = H - mt - mb; const int h1 = (int) std::floor (tot * 0.58f); const float y2 = mt + (float) h1 + 26, h2 = H - mb - y2;
        auto xs = [&] (double f) { return ml + pw * (float) (std::log (f / 20.0) / std::log (1000.0)); }; auto X = [&] (int k) { return xs (ws::bandCentreHz (k)); };
        const float lim = P.param ("shLimit");
        auto Y1 = [&] (float v) { return mt + (float) h1 * (1.0f - (juce::jlimit (-36.0f, 36.0f, v) + 36.0f) / 72.0f); };
        auto Y2 = [&] (float v) { return y2 + h2 * (1.0f - (juce::jlimit (-lim, lim, v) + lim) / (2 * lim)); };
        for (int d : { -36, -18, 0, 18, 36 }) { g.setColour (d ? col::grid : col::zero); hline (g, ml, W - mr, Y1 ((float) d) + 0.5f); mtxt (g, (d > 0 ? "+" : "") + juce::String (d), ml - 5, Y1 ((float) d) + 3, S.acc, juce::Justification::right, 10); }
        for (float d : { -lim, -lim / 2, 0.0f, lim / 2, lim }) { g.setColour (d != 0 ? col::grid : col::zero); hline (g, ml, W - mr, Y2 (d) + 0.5f); mtxt (g, (d > 0 ? "+" : "") + juce::String (d, std::fmod (d, 1.0f) != 0 ? 1 : 0), ml - 5, Y2 (d) + 3, S.acc, juce::Justification::right, 10); }
        for (double f : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 }) { const float x = xs (f); g.setColour (col::grid); vline (g, x + 0.5f, mt, H - mb); mtxt (g, f >= 1000 ? juce::String (f / 1000.0) + "k" : juce::String ((int) f), x, H - 6, S.acc, juce::Justification::horizontallyCentred, 10); }
        mtxt (g, "Shape, dB relative to average", ml + 4, mt - 8, col::key, juce::Justification::left, 10); mtxt (g, "EQ to match, dB", ml + 4, y2 - 8, col::key, juce::Justification::left, 10);
        const int ch = std::string (P.choiceRaw ("shChan")) == "s" ? 1 : 0;
        auto line = [&] (const float* arr, juce::Colour c, float lw, bool dashed, const std::function<float (float)>& Y)
        {
            juce::Path pth; for (int i = 0; i < ws::kBands; ++i) { const float x = X (i), y = Y (arr[i]); if (! i) pth.startNewSubPath (x, y); else { const float px = X (i - 1), py = Y (arr[i - 1]); pth.quadraticTo (px, py, (px + x) / 2, (py + y) / 2); } }
            g.setColour (c); if (dashed) { juce::Path d; const float da[] = { 5, 4 }; juce::PathStrokeType (lw).createDashedStroke (d, pth, da, 2); g.fillPath (d); } else g.strokePath (pth, juce::PathStrokeType (lw));
        };
        float refN[ws::kBands], mineN[ws::kBands], resN[ws::kBands]; const bool hasRef = V.hasRefShown; if (hasRef) ws::relNorm (V.refShown[ch], refN);
        const float* mineSrc = V.hasMine ? (ch ? V.mine.s.data() : V.mine.m.data()) : (V.hasLive ? V.live[ch] : nullptr); const bool hasMineN = mineSrc != nullptr; if (hasMineN) ws::relNorm (mineSrc, mineN);
        const bool hasResN = P.param ("shHear") > 0.5f && V.hasRes; if (hasResN) ws::relNorm (V.res[ch], resN);
        if (hasMineN) line (mineN, S.l, 2.0f, ! V.hasMine, Y1);
        if (hasResN) { juce::Path pth; for (int i = 0; i < ws::kBands; ++i) { const float x = X (i), y = Y1 (resN[i]); if (! i) pth.startNewSubPath (x, y); else { const float px = X (i - 1), py = Y1 (resN[i - 1]); pth.quadraticTo (px, py, (px + x) / 2, (py + y) / 2); } } juce::Path d; const float da[] = { 2, 3 }; juce::PathStrokeType (1.4f).createDashedStroke (d, pth, da, 2); g.setColour (juce::Colours::white); g.fillPath (d); }
        if (hasRef) line (refN, S.acc, 2.4f, false, Y1);
        if (! hasRef)
        {
            g.setColour (juce::Colour (0xcc06080a)); g.fillRect (ml + pw * 0.12f, mt + (float) h1 * 0.3f, pw * 0.76f, 56.0f);
            mtxt (g, V.hasRef ? "Reference is switched off." : "No reference yet.", W / 2, mt + (float) h1 * 0.3f + 22, juce::Colours::white, juce::Justification::horizontallyCentred, 14);
            mtxt (g, V.hasRef ? "Turn Reference on in step 1 to compare and match again." : "Pick a starting-point shape below, or play a song and press Capture reference.", W / 2, mt + (float) h1 * 0.3f + 42, col::key, juce::Justification::horizontallyCentred, 11);
        }
        const float bw = pw / (float) ws::kBands * 0.8f;
        for (int k = 0; k < ws::kBands; ++k) { const float v = V.eq[ch][k]; if (std::abs (v) < 0.05f) continue; const float yy = Y2 (v), y0 = Y2 (0); g.setColour ((v > 0 ? S.l : S.r).withAlpha (0.85f)); g.fillRect (X (k) - bw / 2, std::min (yy, y0), bw, std::abs (yy - y0)); }
        const float tilt = (float) P.choiceNumber ("shRefTilt"); const bool on = P.param ("shRefOn") > 0.5f;
        mtxt (g, "Reference (yellow): " + (V.hasRef ? juce::String (V.ref.name) + (on ? (tilt != 0 ? " tilt " + juce::String (tilt > 0 ? "+" : "") + juce::String (tilt, 1) + " dB/oct" : juce::String()) : juce::String (" (off)")) : juce::String ("none"))
                   + "   Mine (aqua, " + (V.hasMine ? "captured" : V.hasLive ? "live, dashed" : "waiting") + ")" + (hasResN ? "   After EQ (white dots)" : ""), W - mr - 4, mt - 8, col::key, juce::Justification::right, 11);
        mtxt (g, ch == 0 ? "MID" : "SIDE", ml + pw - 4, mt + 14, juce::Colours::white, juce::Justification::right, 13);
        if (V.capturing) { g.setColour (juce::Colour (0xcc06080a)); g.fillRect (ml + 4, mt + 4, 300.0f, 20.0f); mtxt (g, juce::String ("Capturing ") + (V.captureIsRef ? "reference" : "mine") + "... " + juce::String (V.captureSeconds, 1) + " s, " + juce::String (V.captureFrames) + " frames", ml + 10, mt + 18, S.r, juce::Justification::left, 11); }
        if (mx >= ml && mx <= ml + pw && my >= mt && my <= H - mb)
        {
            const double f = 20.0 * std::pow (1000.0, (mx - ml) / pw); const int kk = juce::jlimit (0, ws::kBands - 1, (int) std::lround (6 * std::log2 (f / 20.0) - 0.5));
            g.setColour (juce::Colours::white.withAlpha (0.33f)); vline (g, mx + 0.5f, mt, H - mb);
            juce::String l = (f >= 1000 ? juce::String (f / 1000.0, 2) + " kHz " : juce::String (f, 0) + " Hz ") + noteOf (f);
            if (hasRef) l << "  ref " << juce::String (refN[kk], 1); if (hasMineN) l << "  mine " << juce::String (mineN[kk], 1); if (hasRef && hasMineN) l << "  EQ " << (V.eq[ch][kk] > 0 ? "+" : "") << juce::String (V.eq[ch][kk], 1);
            const float tw = (float) l.length() * 6.6f + 12, bx = std::min (W - tw - 6, std::max (ml, mx + 10)); tag (g, l, bx, std::max (mt, my - 24), tw);
        }
        if (env.silent && ! env.hadSound && ! V.hasLive) mtxt (g, "No sound yet. Play something in your DAW.", W / 2, H - mb - 14, col::dim, juce::Justification::horizontallyCentred, 11);
    }
};
}
