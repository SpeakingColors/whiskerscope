#include "whiskerscope/Engine.h"
#include <cstdio>
using namespace ws;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)
#define NEAR(a, b, t) do { double _a = (a), _b = (b); if (!(std::fabs(_a - _b) <= (t))) { std::printf("FAIL line %d: %s = %g, expected %g +- %g\n", __LINE__, #a, _a, _b, (double) (t)); ++fails; } } while (0)

static std::vector<float> sine(double hz, double amp, double sr, int n, double ph = 0) { std::vector<float> v(n); for (int i = 0; i < n; ++i) v[i] = (float) (amp * std::sin(2 * kPi * hz * i / sr + ph)); return v; }
static int peakBin(const std::vector<float>& d) { int b = 0; for (int k = 1; k < (int) d.size(); ++k) if (d[k] > d[b]) b = k; return b; }

int main()
{
    const double sr = 48000;
    // Spectrum: peak bin at the right place, 0 dB for amplitude 1, -6 dB for 0.5, tone between bins still close to 0 dB (Blackman scalloping under 1.1 dB)
    for (int n : { 2048, 8192, 32768 })
    {
        Spectrum sp; sp.prepare(sr, n); const double hz = 1000.0; auto x = sine(hz, 1.0, sr, n); sp.analyse(x.data(), 0.0f);
        const int b = peakBin(sp.spectrum()); NEAR(b * sr / n, hz, 1.5 * sr / n); NEAR(sp.spectrum()[b], 0.0, 1.2);
        x = sine(hz, 0.5, sr, n); sp.reset(); sp.analyse(x.data(), 0.0f); NEAR(sp.spectrum()[peakBin(sp.spectrum())], -6.0, 1.2);
        auto y = sine(1234.5, 1.0, sr, n); sp.reset(); sp.analyse(y.data(), 0.0f); NEAR(sp.spectrum()[peakBin(sp.spectrum())], 0.0, 1.2);
    }
    {   // smoothing: with keep 0.8 the level falls slowly after the tone stops; reset clears it
        Spectrum sp; sp.prepare(sr, 2048); auto x = sine(1000, 1.0, sr, 2048); std::vector<float> z(2048, 0.0f);
        sp.analyse(x.data(), 0.8f); const int k = peakBin(sp.spectrum()); const float first = sp.spectrum()[k]; for (int i = 0; i < 20; ++i) sp.analyse(x.data(), 0.8f);
        const float full = sp.spectrum()[k]; NEAR(full, 0.0, 1.2); CHECK(first < full - 6.0f);
        sp.analyse(z.data(), 0.8f); CHECK(sp.spectrum()[k] < full && sp.spectrum()[k] > full - 6.0f); sp.reset(); CHECK(sp.spectrum()[k] < -190.0f);
    }
    // note names
    CHECK(noteName(440.0) == "A4"); CHECK(noteName(261.63) == "C4"); CHECK(noteName(55.0) == "A1"); CHECK(noteName(466.16) == "A#4"); CHECK(noteName(0) == "");
    {   // loudness: stereo 1 kHz sine at -20 dBFS reads -20 LUFS, same at 44.1 kHz; 20 Hz is cut hard; silence reads very low
        for (double fs : { 48000.0, 44100.0 })
        {
            LoudnessMeter m; m.prepare(fs); auto x = sine(1000, 0.1, fs, (int) (fs * 5.0)); m.process(x.data(), x.data(), (int) x.size());
            NEAR(m.momentary(), -20.0, 0.15); NEAR(m.shortTerm(), -20.0, 0.15); NEAR(m.integrated(), -20.0, 0.15); NEAR(m.maxMomentary(), -20.0, 0.15);
        }
        LoudnessMeter lo; lo.prepare(sr); auto z = sine(20, 0.1, sr, (int) (sr * 2)); lo.process(z.data(), z.data(), (int) z.size()); CHECK(lo.momentary() < -26.0f);
        LoudnessMeter si; si.prepare(sr); std::vector<float> zero(48000, 0.0f); si.process(zero.data(), zero.data(), 48000); CHECK(si.momentary() < -100.0f || si.momentary() == -120.0f);
        // gating: 10 s of -20 dB then 10 s of silence: integrated stays at -20 (silence is gated out)
        LoudnessMeter g; g.prepare(sr); auto t = sine(1000, 0.1, sr, (int) (sr * 10)); g.process(t.data(), t.data(), (int) t.size()); std::vector<float> sil((int) (sr * 10), 0.0f); g.process(sil.data(), sil.data(), (int) sil.size());
        NEAR(g.integrated(), -20.0, 0.2); CHECK(g.momentary() < -100.0f); NEAR(g.maxMomentary(), -20.0, 0.2);
        g.requestReset(); g.process(sil.data(), sil.data(), 480); CHECK(g.maxMomentary() < -100.0f && g.integrated() < -100.0f);
    }
    {   // stereo: mono, anti phase, one sided, independent
        auto x = sine(300, 0.5, sr, 4096), y = sine(450, 0.5, sr, 4096); std::vector<float> nx(4096); for (int i = 0; i < 4096; ++i) nx[i] = -x[i];
        auto s = stereoNow(x.data(), x.data(), 4096); CHECK(!s.silent); NEAR(s.correlation, 1.0, 0.01); NEAR(s.widthPct, 0.0, 0.5); NEAR(s.balanceDb, 0.0, 0.1); NEAR(s.monoLossDb, 0.0, 0.1); CHECK(stereoVerdict(s.silent, s.correlation, s.monoLossDb) == 0);
        s = stereoNow(x.data(), nx.data(), 4096); NEAR(s.correlation, -1.0, 0.01); CHECK(s.monoLossDb < -30.0f); CHECK(stereoVerdict(s.silent, s.correlation, s.monoLossDb) == 2); CHECK(s.widthPct == 200.0f);
        std::vector<float> zero(4096, 0.0f); s = stereoNow(x.data(), zero.data(), 4096); NEAR(s.balanceDb, 24.0, 1e-3); NEAR(s.monoLossDb, -3.0, 0.2);
        s = stereoNow(x.data(), y.data(), 4096); NEAR(s.correlation, 0.0, 0.1); CHECK(stereoVerdict(s.silent, s.correlation, s.monoLossDb) == 1);
        s = stereoNow(zero.data(), zero.data(), 4096); CHECK(s.silent); CHECK(stereoVerdict(true, 0, 0) == -1);
    }
    {   // pitch
        auto y = sine(220, 0.5, sr, 8192); NEAR(detectPitch(y.data(), 8192, sr), 220.0, 6.0); std::vector<float> zero(8192, 0.0f); CHECK(detectPitch(zero.data(), 8192, sr) == 0.0);
    }
    {   // bands: grid, a 1 kHz sine lands in the right band, bands above 98 percent of Nyquist are -140
        NEAR(bandEdgeHz(0), 20.0, 1e-9); NEAR(bandEdgeHz(6), 40.0, 1e-6); NEAR(bandCentreHz(0), 20.0 * std::pow(2.0, 1.0 / 12.0), 1e-6); CHECK(bandEdgeHz(60) > 20400 && bandEdgeHz(60) < 20500);
        Spectrum sp; sp.prepare(sr, 16384); auto x = sine(1000, 1.0, sr, 16384); sp.analyse(x.data(), 0.0f);
        float bands[kBands]; spectrumToBands(sp.spectrum().data(), sp.numBins(), sr, bands); int top = 0; for (int i = 0; i < kBands; ++i) if (bands[i] > bands[top]) top = i;
        CHECK(bandEdgeHz(top) <= 1000.0 && bandEdgeHz(top + 1) >= 1000.0); CHECK(bands[59] == -140.0f);
    }
    {   // matching: level offset ignored, strength scales, limit holds, identical shapes give zero
        float ref[kBands], mine[kBands], eq[kBands], eq2[kBands];
        for (int i = 0; i < kBands; ++i) { ref[i] = -40.0f; mine[i] = -40.0f; } ref[30] = -34.0f;
        matchTargets(ref, mine, 1, 1.0f, 12.0f, eq); CHECK(eq[30] > 5.0f && eq[30] < 12.0f); CHECK(eq[0] < 1.0f); NEAR(eq[30] + 0.0, eq[30], 0);
        matchTargets(ref, mine, 1, 0.5f, 12.0f, eq2); NEAR(eq2[30], eq[30] * 0.5, 0.01);
        matchTargets(ref, mine, 1, 1.0f, 2.0f, eq2); CHECK(eq2[30] <= 2.0f + 1e-4f);
        matchTargets(ref, ref, 2, 1.0f, 12.0f, eq2); for (float v : eq2) NEAR(v, 0.0, 1e-4);
        float loud[kBands]; for (int i = 0; i < kBands; ++i) loud[i] = mine[i] + 10.0f; matchTargets(ref, loud, 1, 1.0f, 12.0f, eq2); NEAR(eq2[30], eq[30], 0.01);
        matchTargets(ref, mine, 6, 1.0f, 18.0f, eq2); CHECK(eq2[30] < eq[30] && eq2[30] > 0);     // wider smoothing, gentler move
        float t[kBands], u[kBands]; for (int i = 0; i < kBands; ++i) t[i] = 0; tiltBands(t, 3.0f, u); NEAR(u[35], 3.0 * std::log2(bandCentreHz(35) / 1000.0), 1e-4);
        NEAR(interpBands(eq, bandCentreHz(30)), eq[30], 1e-3);
        std::vector<double> v(kBands, -50.0); CHECK(shapeValuesOk(v)); v[3] = std::nan(""); CHECK(!shapeValuesOk(v)); v[3] = 99; CHECK(!shapeValuesOk(v)); v.pop_back(); CHECK(!shapeValuesOk(v));
    }
    {   // bells: the cascade follows a smooth target within 1.2 dB at the band centres; silence is skipped; zero gains are bit exact
        for (double fs : { 44100.0, 48000.0, 96000.0 })
        {
            float target[kBands], gains[kBells]; for (int k = 0; k < kBands; ++k) target[k] = 6.0f * (float) std::exp(-std::pow((k - 30) / 8.0, 2.0)) - 3.0f * (float) std::exp(-std::pow((k - 12) / 5.0, 2.0));
            designBells(target, fs, gains); double worst = 0;
            for (int k = 0; k < kBands; ++k) if (bandCentreHz(k) < 0.4 * fs) worst = std::max(worst, std::fabs(bellsResponseDb(gains, fs, bandCentreHz(k)) - target[k]));
            CHECK(worst < 1.2); if (worst >= 1.2) std::printf("  worst fit error at %g Hz: %g dB\n", fs, worst);
        }
        float flat[kBands] = {}, gains[kBells]; designBells(flat, 48000.0, gains); for (float g : gains) NEAR(g, 0.0, 1e-6);
        MidSideEq eq; eq.design(gains, gains, 48000.0); CHECK(!eq.isActive()); float l = 0.3f, r = -0.2f; eq.process(l, r); NEAR(l, 0.3, 1e-6); NEAR(r, -0.2, 1e-6);
        // a +6 dB bell on Mid at 1 kHz raises a centred 1 kHz sine by about 6 dB and leaves a Side only signal untouched
        float tg[kBands]; for (int k = 0; k < kBands; ++k) tg[k] = 0; for (int k = 31; k <= 36; ++k) tg[k] = 6.0f; float gm[kBells], gs[kBells] = {}; designBells(tg, 48000.0, gm);
        MidSideEq e2; e2.design(gm, gs, 48000.0); CHECK(e2.isActive()); auto x = sine(1000, 0.2, 48000, 48000); double pin = 0, pout = 0, sin_ = 0, sout = 0;
        for (int i = 0; i < 48000; ++i) { float a = x[i], b = x[i]; e2.process(a, b); if (i > 24000) { pin += x[i] * x[i]; pout += a * a; } }
        const double g1 = 10 * std::log10(pout / pin); NEAR(g1, bellsResponseDb(gm, 48000.0, 1000.0), 0.15); CHECK(g1 > 4.5);
        e2.reset(); for (int i = 0; i < 48000; ++i) { float a = x[i], b = -x[i]; e2.process(a, b); if (i > 24000) { sin_ += x[i] * x[i]; sout += a * a; } } NEAR(10 * std::log10(sout / sin_), 0.0, 0.01);
        // NaN input does not poison the filter
        e2.reset(); float n1 = std::nanf(""), n2 = 0; e2.process(n1, n2); float k1 = 0.1f, k2 = 0.1f; e2.process(k1, k2); CHECK(std::isfinite(k1) && std::isfinite(k2));
    }
    {   // auto level: lifts a quiet match, lowers a loud one, never past -0.3 dBFS, mode 0 returns to 0, no signal leaves it alone
        AutoLevel a; for (int i = 0; i < 400; ++i) a.update(0.05f, 1, -26.0f, -20.0f, -999.0f, 0.0f, -30.0f); NEAR(a.db, 6.0, 0.1); CHECK(!a.held);
        AutoLevel b; for (int i = 0; i < 400; ++i) b.update(0.05f, 1, -14.0f, -20.0f, -999.0f, 0.0f, -30.0f); NEAR(b.db, -6.0, 0.1);
        AutoLevel c; for (int i = 0; i < 400; ++i) c.update(0.05f, 1, -26.0f, -20.0f, -999.0f, 0.0f, -3.0f); NEAR(c.db, 2.7, 0.1); CHECK(c.held);
        AutoLevel d; for (int i = 0; i < 400; ++i) d.update(0.05f, 2, -26.0f, -20.0f, -14.0f, 0.0f, -30.0f); NEAR(d.db, 12.0, 0.1);
        AutoLevel e; for (int i = 0; i < 400; ++i) e.update(0.05f, 2, -26.0f, -20.0f, -999.0f, 0.0f, -30.0f); NEAR(e.db, 6.0, 0.1);   // no reference loudness: keeps mine
        for (int i = 0; i < 400; ++i) { a.update(0.05f, 0, -26.0f, -20.0f, -999.0f, 0.0f, -30.0f); }
        NEAR(a.db, 0.0, 0.01);
        AutoLevel f; for (int i = 0; i < 100; ++i) f.update(0.05f, 1, -80.0f, -80.0f, -999.0f, 0.0f, -90.0f); NEAR(f.db, 0.0, 1e-6);
    }
    {   // limiter: never above the ceiling, transparent below it
        SafetyLimiter lim; lim.prepare(48000); double worst = 0; auto x = sine(100, 1.8, 48000, 48000);
        for (float v : x) { float a = v, b = v; lim.process(a, b); worst = std::max(worst, (double) std::fabs(a)); } CHECK(worst <= SafetyLimiter::ceiling + 1e-6);
        SafetyLimiter lim2; lim2.prepare(48000); auto y = sine(100, 0.5, 48000, 4800); double d = 0; for (float v : y) { float a = v, b = v; lim2.process(a, b); d = std::max(d, (double) std::fabs(a - v)); } NEAR(d, 0.0, 1e-9);
    }
    {   // ring
        SampleRing r; for (int i = 0; i < 100; ++i) r.push((float) i); float o[10]; r.latest(o, 10); NEAR(o[0], 90, 0); NEAR(o[9], 99, 0);
        for (int i = 100; i < 300000; ++i) { r.push((float) (i % 1000)); }
        r.latest(o, 10); NEAR(o[9], 299999 % 1000, 0); CHECK(r.written() == 300000);
    }
    {   // key detector on synthetic chord progressions (a lone triad is ambiguous with its relative key, so progressions are used)
        auto midiHz = [](int m) { return 440.0 * std::pow(2.0, (m - 69) / 12.0); };
        auto chordSig = [&](const std::vector<std::vector<int>>& chords, double secPer, double fs, int loops) {
            std::vector<float> v; const int per = (int) (secPer * fs);
            for (int l = 0; l < loops; ++l) for (auto& ch : chords) for (int i = 0; i < per; ++i) {
                double a = 0; for (int m : ch) for (int h = 1; h <= 3; ++h) a += std::sin(2 * kPi * midiHz(m) * h * i / fs) / h;
                v.push_back((float) (0.08 * a)); }
            return v; };
        auto detect = [&](const std::vector<float>& x, double fs, double* secondsToName = nullptr) {
            KeyDetector kd; const int hop = (int) (fs * 0.1); double t = 0, named = -1;
            for (int p = 0; p + KeyDetector::kFftSize <= (int) x.size(); p += hop) { kd.process(x.data() + p, fs, 0.1); t += 0.1; if (named < 0 && kd.result().tonic >= 0) named = t; }
            if (secondsToName) *secondsToName = named;
            return kd.result(); };
        for (double fs : { 48000.0, 44100.0 })
        {
            // C major: C F G C chords (C4 = 60)
            auto cmaj = chordSig({ { 48, 60, 64, 67 }, { 53, 65, 69, 72 }, { 55, 67, 71, 74 }, { 48, 60, 64, 67 } }, 2.0, fs, 3);
            double named = 0; KeyResult r = detect(cmaj, fs, &named);
            CHECK(r.tonic == 0 && !r.minor); CHECK(keyName(r) == "C major"); CHECK(camelotOf(r) == "8B"); CHECK(r.confidence > 0.6f); CHECK(!r.listening); CHECK(named >= KeyDetector::kNeedSeconds - 0.01);
            // A minor with the raised seventh: Am Dm E Am
            auto amin = chordSig({ { 45, 57, 60, 64 }, { 50, 62, 65, 69 }, { 52, 64, 68, 71 }, { 45, 57, 60, 64 } }, 2.0, fs, 3);
            r = detect(amin, fs); CHECK(r.tonic == 9 && r.minor); CHECK(keyName(r) == "A minor"); CHECK(camelotOf(r) == "8A");
            // D major: D G A D, F# minor: F#m Bm C#(major) F#m
            auto dmaj = chordSig({ { 50, 62, 66, 69 }, { 55, 67, 71, 74 }, { 57, 69, 73, 76 }, { 50, 62, 66, 69 } }, 2.0, fs, 3);
            r = detect(dmaj, fs); CHECK(r.tonic == 2 && !r.minor); CHECK(camelotOf(r) == "10B");
            auto fsm = chordSig({ { 54, 66, 69, 73 }, { 59, 71, 74, 78 }, { 61, 73, 77, 80 }, { 54, 66, 69, 73 } }, 2.0, fs, 3);
            r = detect(fsm, fs); CHECK(r.tonic == 6 && r.minor); CHECK(camelotOf(r) == "11A");
        }
        {   // a lone C major triad: the answer is C major or its relative A minor, never anything else
            auto tri = chordSig({ { 48, 60, 64, 67 } }, 12.0, 48000, 1); KeyResult r = detect(tri, 48000);
            CHECK((r.tonic == 0 && !r.minor) || (r.tonic == 9 && r.minor));
            CHECK(!r.ambiguous || r.altTonic != r.tonic);
        }
        {   // silence and noise: no key. Loud drums only (noise bursts) too.
            std::vector<float> z(48000 * 12, 0.0f); KeyResult r = detect(z, 48000); CHECK(r.tonic < 0); CHECK(r.listening);
            std::vector<float> nz(48000 * 12); uint32_t seed = 12345; for (auto& v : nz) { seed = seed * 1664525u + 1013904223u; v = 0.2f * ((seed >> 8) / 8388608.0f - 1.0f); }
            r = detect(nz, 48000); CHECK(r.tonic < 0);
            std::vector<float> sw(48000 * 12); { double ph = 0; for (size_t i = 0; i < sw.size(); ++i) { const double f = 20.0 * std::pow(1000.0, (double) i / sw.size()); ph += 2 * kPi * f / 48000; sw[i] = (float) (0.3 * std::sin(ph)); } }
            r = detect(sw, 48000); CHECK(r.tonic < 0);
        }
        {   // hold semantics live in the caller; reset clears the memory
            KeyDetector kd; auto cmaj = chordSig({ { 48, 60, 64, 67 }, { 53, 65, 69, 72 }, { 55, 67, 71, 74 }, { 48, 60, 64, 67 } }, 2.0, 48000, 2);
            for (int p = 0; p + KeyDetector::kFftSize <= (int) cmaj.size(); p += 4800) kd.process(cmaj.data() + p, 48000, 0.1);
            CHECK(kd.result().tonic == 0); kd.reset(); CHECK(kd.result().tonic < 0); CHECK(kd.result().listening);
        }
        // names
        KeyResult k; k.tonic = 3; k.minor = true; CHECK(keyName(k) == "Eb minor"); CHECK(camelotOf(k) == "2A"); k.minor = false; CHECK(camelotOf(k) == "5B"); CHECK(camelotOf(KeyResult()) == "");
    }
    {   // reference file analysis: tone level, Mid/Side split, silence skipped, short input refused, level steps of 6 dB
        const double fs = 48000; const int n = (int) (fs * 6);
        auto run = [&](const std::vector<float>& l, const std::vector<float>* r, Shape& out) { ShapeAccumulator a; a.prepare(fs);
            for (size_t p = 0; p < l.size(); p += 1000) { int c = (int) std::min<size_t>(1000, l.size() - p); a.push(l.data() + p, r ? r->data() + p : nullptr, c); }
            bool ok = a.finish(out); return std::make_pair(ok, a.usedFrames()); };
        auto bandOf = [](double hz) { return (int) std::floor(6.0 * std::log2(hz / 20.0)); };
        Shape a, b, c, d; auto x = sine(1000, 0.5, fs, n); auto hx = sine(1000, 0.25, fs, n);
        auto ra = run(x, nullptr, a); auto rb = run(hx, nullptr, b);
        CHECK(ra.first && rb.first); const int kb = bandOf(1000); CHECK(a.m[kb] > -30.0f); NEAR(a.m[kb] - b.m[kb], 6.02, 0.3);
        CHECK(a.s[kb] < -100.0f);                                   // identical L and R: no Side
        std::vector<float> zero(n, 0.0f); run(x, &zero, c);          // left only: Mid and Side are equal
        NEAR(c.m[kb], c.s[kb], 0.3); NEAR(c.m[kb], a.m[kb] - 6.02, 0.3);
        std::vector<float> ph(n); for (int i = 0; i < n; ++i) ph[(size_t) i] = -x[(size_t) i]; run(x, &ph, d);   // opposite phase: all Side
        CHECK(d.m[kb] < -100.0f); NEAR(d.s[kb], a.m[kb], 0.3);
        // 6 s of silence before the tone changes nothing
        std::vector<float> sil((size_t) n, 0.0f), xs = sil; xs.insert(xs.end(), x.begin(), x.end()); Shape e; auto re2 = run(xs, nullptr, e);
        NEAR(e.m[kb], a.m[kb], 0.3); CHECK(re2.second <= ra.second + 1);
        // pure silence and a too short file are refused
        Shape f; auto rf = run(sil, nullptr, f); CHECK(!rf.first);
        std::string why; CHECK(!audioInfoOk(48000, 2, 48000.0 * 1.5, why) && !why.empty()); CHECK(audioInfoOk(44100, 2, 44100.0 * 30, why) && why.empty());
        CHECK(!audioInfoOk(100, 2, 1e6, why)); CHECK(!audioInfoOk(48000, 0, 1e6, why)); CHECK(!audioInfoOk(48000, 2, 48000.0 * 3700, why)); CHECK(!audioInfoOk(NAN, 2, 1e6, why));
    }
    std::printf(fails ? "%d checks FAILED\n" : "all engine checks passed\n", fails); return fails ? 1 : 0;
}
