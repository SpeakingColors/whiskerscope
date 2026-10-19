#include "whiskerscope/Engine.h"
#include <cstdio>
using namespace ws;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)
#define NEAR(a, b, t) do { double _a = (a), _b = (b); if (!(std::fabs(_a - _b) <= (t))) { std::printf("FAIL line %d: %s = %g, expected %g +- %g\n", __LINE__, #a, _a, _b, (double) (t)); ++fails; } } while (0)

static std::vector<float> sine(double hz, double amp, double sr, int n, double ph = 0) { std::vector<float> v(n); for (int i = 0; i < n; ++i) v[i] = (float) (amp * std::sin(2 * kPi * hz * i / sr + ph)); return v; }

int main()
{
    const double sr = 48000;
    // FFT of a sine: peak bin at the right place, level is 0 dB for amplitude 1, -6 dB for 0.5
    for (int n : { 2048, 8192, 32768 })
    {
        SpectrumAnalyzer sa; sa.prepare(sr, n);
        const double hz = 1000.0; auto x = sine(hz, 1.0, sr, n); sa.analyse(x.data(), 0, 0, 0, 0.03f);
        auto p = sa.highest(); NEAR(p.hz, hz, 3.0 * sr / n); NEAR(p.db, 0.0, 1.5);
        x = sine(hz, 0.5, sr, n); sa.resetSmoothing(); sa.analyse(x.data(), 0, 0, 0, 0.03f); NEAR(sa.highest().db, -6.0, 1.5);
    }
    {   // frequency refinement: a tone between bins
        SpectrumAnalyzer sa; sa.prepare(sr, 4096); auto x = sine(1234.5, 0.8, sr, 4096); sa.analyse(x.data(), 0, 0, 0, 0.03f);
        NEAR(sa.highest().hz, 1234.5, 3.0);
    }
    {   // tilt: pink slope of +3 dB/oct raises 2 kHz by 3 dB against 1 kHz
        SpectrumAnalyzer a, b; a.prepare(sr, 8192); b.prepare(sr, 8192); auto x = sine(2000, 1.0, sr, 8192);
        a.analyse(x.data(), 0, 0, 0, 0.03f); b.analyse(x.data(), 3.0f, 0, 0, 0.03f); NEAR(b.highest().db - a.highest().db, 3.0, 0.3);
    }
    {   // smoothing: release makes the level fall slowly, attack 0 jumps up
        SpectrumAnalyzer sa; sa.prepare(sr, 2048); auto x = sine(1000, 1.0, sr, 2048); std::vector<float> z(2048, 0.0f);
        sa.analyse(x.data(), 0, 0, 300, 0.03f); const float hi = sa.highest().db; sa.analyse(z.data(), 0, 0, 300, 0.03f);
        const int k = (int) std::lround(1000.0 * 2048 / sr); CHECK(sa.spectrum()[k] < hi && sa.spectrum()[k] > hi - 60.0f); CHECK(sa.peakHold()[k] >= sa.spectrum()[k]);
        sa.resetHold(); CHECK(sa.peakHold()[k] < -150.0f);
    }
    // note names
    CHECK(noteName(440.0) == "A4"); CHECK(noteName(261.63) == "C4"); CHECK(noteName(55.0) == "A1"); CHECK(noteName(466.16) == "A#4"); CHECK(noteName(0) == "");
    {   // loudness: stereo 1 kHz sine at -20 dBFS reads -20 LUFS (BS.1770: the -0.691 offset cancels the K filter gain of +0.69 dB at 1 kHz)
        Loudness l; l.prepare(sr); auto x = sine(1000, 0.1, sr, (int) (sr * 1.0)); for (float v : x) l.tick(v, v);
        NEAR(l.lufs(), -20.0, 0.15);
        Loudness l2; l2.prepare(44100); auto y = sine(1000, 0.1, 44100, 44100); for (float v : y) l2.tick(v, v); NEAR(l2.lufs(), -20.0, 0.15);
        // K weighting: 100 Hz is nearly flat, 20 Hz is cut hard
        Loudness lo; lo.prepare(sr); auto z = sine(20, 0.1, sr, (int) (sr * 2)); for (float v : z) lo.tick(v, v); CHECK(lo.lufs() < -26.0f);
        Loudness silent; silent.prepare(sr); for (int i = 0; i < 48000; ++i) silent.tick(0, 0); CHECK(silent.lufs() < -100.0f);
    }
    {   // stereo: mono, anti phase, one sided, independent
        StereoMeter m; m.prepare(sr, 100); auto x = sine(300, 0.5, sr, 48000);
        for (float v : x) { m.tick(v, v); }
        auto s = m.get(); NEAR(s.correlation, 1.0, 0.01); NEAR(s.width, 0.0, 0.01); NEAR(s.balance, 0.0, 0.01); NEAR(s.monoLossDb, 0.0, 0.1); CHECK(std::string(StereoMeter::verdict(s)) == "Mono safe");
        m.reset(); for (float v : x) m.tick(v, -v); s = m.get(); NEAR(s.correlation, -1.0, 0.01); NEAR(s.width, 1.0, 0.01); CHECK(s.monoLossDb < -30.0f); CHECK(std::string(StereoMeter::verdict(s)) == "Phase problem");
        m.reset(); for (float v : x) m.tick(v, 0); s = m.get(); NEAR(s.balance, -1.0, 0.01); NEAR(s.correlation, 0.0, 0.2); NEAR(s.monoLossDb, -3.0, 0.2);
        m.reset(); auto y = sine(450, 0.5, sr, 48000); for (int i = 0; i < 48000; ++i) m.tick(x[i], y[i]); s = m.get(); NEAR(s.correlation, 0.0, 0.1);
    }
    {   // trigger and period
        auto x = sine(100, 1.0, sr, 4800, 0.5); const int t = findRisingCrossing(x.data(), 4800, 0); CHECK(t > 0 && x[t - 1] < 0 && x[t] >= 0);
        auto y = sine(200, 1.0, sr, 4800); const int per = findPeriod(y.data(), 4800, 20, 1200); NEAR(per, 240, 2); auto nz = std::vector<float>(4800, 0.0f); CHECK(findPeriod(nz.data(), 4800, 20, 1200) == 0);
    }
    {   // shapes: band grid, capture gate, matching EQ, text round trip, bad input
        NEAR(shapeBandHz(0), 20.0, 1e-9); NEAR(shapeBandHz(6), 40.0, 1e-6); CHECK(shapeBandHz(59) > 15000 && shapeBandHz(59) < 20000);
        SpectrumAnalyzer sa; sa.prepare(sr, 16384); auto x = sine(1000, 1.0, sr, 16384); sa.analyse(x.data(), 0, 0, 0, 0.03f);
        float bands[kShapeBands]; binsToBands(sa.spectrum(), sr, 16384, bands); int top = 0; for (int i = 0; i < kShapeBands; ++i) if (bands[i] > bands[top]) top = i;
        CHECK(std::fabs(shapeBandHz(top) - 1000.0) < 100.0);
        ShapeCapture cap; float quiet[kShapeBands], loud[kShapeBands]; for (int i = 0; i < kShapeBands; ++i) { quiet[i] = -100; loud[i] = -30; }
        CHECK(!cap.add(quiet, quiet, -80.0f)); CHECK(cap.add(loud, quiet, -20.0f)); CHECK(cap.add(loud, quiet, -20.0f)); CHECK(cap.count() == 2);
        Shape s; CHECK(cap.result(s)); NEAR(s.mid[10], -30.0, 0.01); NEAR(s.side[10], -100.0, 0.01); ShapeCapture none; Shape t; CHECK(!none.result(t));
        std::vector<float> ref(kShapeBands), mine(kShapeBands); for (int i = 0; i < kShapeBands; ++i) { ref[i] = -40.0f; mine[i] = -40.0f; } ref[30] = -30.0f; mine[30] = -40.0f;
        auto eq = matchingEq(ref, mine, 1.0f, 12.0f); CHECK(eq[30] > 3.0f); CHECK(eq[0] < 1.0f); auto half = matchingEq(ref, mine, 0.5f, 12.0f); NEAR(half[30], eq[30] * 0.5, 0.01);
        auto lim = matchingEq(ref, mine, 1.0f, 2.0f); CHECK(lim[30] <= 2.0f + 1e-4f); auto same = matchingEq(ref, ref, 1.0f, 12.0f); for (float v : same) NEAR(v, 0.0, 1e-4);
        auto loudMine = mine; for (auto& v : loudMine) v += 10.0f; auto eq2 = matchingEq(ref, loudMine, 1.0f, 12.0f); NEAR(eq2[30], eq[30], 0.01);   // level offset is ignored
        Shape sh; sh.name = "Test lead"; for (int i = 0; i < kShapeBands; ++i) { sh.mid[i] = -50.0f + i * 0.3f; sh.side[i] = -70.0f; }
        Shape back; CHECK(shapeFromText(shapeToText(sh), back)); CHECK(back.name == "Test lead"); NEAR(back.mid[20], sh.mid[20], 0.06);
        CHECK(!shapeFromText("garbage", back)); CHECK(!shapeFromText("WSHAPE1||1,2|3,4", back)); CHECK(!shapeFromText(shapeToText(sh).substr(0, 100), back));
        std::string bad = shapeToText(sh); bad.replace(bad.find("-50.0"), 5, "nan"); CHECK(!shapeFromText(bad, back)); bad = shapeToText(sh); bad.replace(bad.find("-50.0"), 5, "999"); CHECK(!shapeFromText(bad, back));
        CHECK(!shapeFromText(std::string(5000, 'a'), back));
    }
    {   // ring
        SampleRing r; for (int i = 0; i < 100; ++i) r.push((float) i); float o[10]; r.latest(o, 10); NEAR(o[0], 90, 0); NEAR(o[9], 99, 0);
        for (int i = 100; i < 70000; ++i) { r.push((float) (i % 1000)); }
        r.latest(o, 10); NEAR(o[9], 69999 % 1000, 0);
    }
    std::printf(fails ? "%d checks FAILED\n" : "all engine checks passed\n", fails); return fails ? 1 : 0;
}
