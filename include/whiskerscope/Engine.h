// Whisker Scope analysis engine. Pure C++17, no JUCE, so it can be unit tested (tests/engine_test.cpp).
// Contains: FFT, spectrum analyser (smoothing, tilt, peak hold, peak finder), levels (peak, RMS, K weighted loudness),
// stereo field (correlation, width, balance, mono loss), scope trigger, and the Shapes bank (60 bands, capture,
// matching EQ, text export and import).
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace ws
{
constexpr double kPi = 3.14159265358979323846;
inline float dbOf(float x) { return 20.0f * std::log10(std::max(x, 1.0e-9f)); }
inline float powDb(float p) { return 10.0f * std::log10(std::max(p, 1.0e-18f)); }

//==============================================================================
// Radix 2 FFT, in place. n must be a power of two. Tables are built once per size.
class Fft
{
public:
    explicit Fft(int n = 2048) { setSize(n); }
    void setSize(int n)
    {
        size = n; cosT.resize(n / 2); sinT.resize(n / 2);
        for (int i = 0; i < n / 2; ++i) { cosT[i] = (float) std::cos(2.0 * kPi * i / n); sinT[i] = (float) -std::sin(2.0 * kPi * i / n); }
        rev.resize(n); int bits = 0; while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n; ++i) { int r = 0; for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b); rev[i] = r; }
    }
    int getSize() const { return size; }
    void forward(float* re, float* im) const
    {
        for (int i = 0; i < size; ++i) { int j = rev[i]; if (j > i) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); } }
        for (int len = 2; len <= size; len <<= 1)
        {
            const int half = len / 2, step = size / len;
            for (int i = 0; i < size; i += len)
                for (int k = 0; k < half; ++k)
                {
                    const float wr = cosT[k * step], wi = sinT[k * step];
                    const int a = i + k, b = a + half;
                    const float tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - tr; im[b] = im[a] - ti; re[a] += tr; im[a] += ti;
                }
        }
    }
private:
    int size = 0; std::vector<float> cosT, sinT; std::vector<int> rev;
};

//==============================================================================
// Note name for a frequency (A4 = 440 Hz), for example "A4" or "C#5". Empty for non finite or non positive input.
inline std::string noteName(double hz)
{
    if (!(hz > 8.0) || !std::isfinite(hz)) return "";
    static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const double m = 69.0 + 12.0 * std::log2(hz / 440.0); const int r = (int) std::lround(m);
    return std::string(n[((r % 12) + 12) % 12]) + std::to_string((int) std::floor(r / 12.0) - 1);
}

//==============================================================================
// Spectrum analyser. Feed it the newest `fftSize` samples (oldest first) with analyse(); read dB per bin.
// Level reference: a full scale sine reads 0 dB at its peak bin.
struct PeakInfo { float db = -200.0f; double hz = 0.0; };

class SpectrumAnalyzer
{
public:
    void prepare(double sampleRate, int fftSize)
    {
        sr = sampleRate; n = fftSize; fft.setSize(n);
        win.resize(n); double s = 0; for (int i = 0; i < n; ++i) { win[i] = (float) (0.5 - 0.5 * std::cos(2.0 * kPi * i / n)); s += win[i]; }
        norm = (float) (2.0 / s);     // sine of amplitude 1 gives a bin magnitude of 1
        re.assign(n, 0); im.assign(n, 0); bins = n / 2 + 1; db.assign(bins, -200.0f); hold.assign(bins, -200.0f); smooth.assign(bins, -200.0f);
    }
    int numBins() const { return bins; }
    int fftSize() const { return n; }
    double binHz(int k) const { return sr * k / n; }

    // x holds exactly fftSize samples. dt is the time since the last call, used by the smoothing.
    void analyse(const float* x, float tiltDbPerOct, float attackMs, float releaseMs, float dt)
    {
        for (int i = 0; i < n; ++i) { re[i] = x[i] * win[i]; im[i] = 0; }
        fft.forward(re.data(), im.data());
        const float ka = attackMs <= 0 ? 1.0f : 1.0f - std::exp(-dt * 1000.0f / attackMs), kr = releaseMs <= 0 ? 1.0f : 1.0f - std::exp(-dt * 1000.0f / releaseMs);
        for (int k = 0; k < bins; ++k)
        {
            const float mag = std::sqrt(re[k] * re[k] + im[k] * im[k]) * norm * (k == 0 || k == n / 2 ? 0.5f : 1.0f);
            float d = dbOf(mag);
            if (k > 0) d += tiltDbPerOct * std::log2((float) k / (float) n * (float) sr / 1000.0f);   // tilt is 0 dB at 1 kHz
            float& s = smooth[k]; s += (d > s ? ka : kr) * (d - s); if (!(s > -200.0f)) s = -200.0f;
            db[k] = s; if (s > hold[k]) hold[k] = s;
        }
    }
    void resetHold() { std::fill(hold.begin(), hold.end(), -200.0f); }
    void resetSmoothing() { std::fill(smooth.begin(), smooth.end(), -200.0f); }
    const std::vector<float>& spectrum() const { return db; }
    const std::vector<float>& peakHold() const { return hold; }

    // Highest peak above minDb, with frequency refined by parabolic interpolation of the three bins around it.
    PeakInfo highest(float minDb = -90.0f, double loHz = 20.0, double hiHz = 20000.0) const
    {
        PeakInfo p; int best = -1; float bv = minDb;
        for (int k = std::max(2, (int) std::ceil(loHz * n / sr)); k < std::min(bins - 1, (int) (hiHz * n / sr)); ++k)
            if (db[k] > bv && db[k] >= db[k - 1] && db[k] >= db[k + 1]) { bv = db[k]; best = k; }
        if (best < 0) return p;
        const float a = db[best - 1], b = db[best], c = db[best + 1], den = a - 2 * b + c;
        const float off = std::fabs(den) < 1e-6f ? 0.0f : 0.5f * (a - c) / den;
        p.hz = binHz(best) + off * sr / n; p.db = b - 0.25f * (a - c) * off; return p;
    }
private:
    double sr = 48000; int n = 2048, bins = 1025; float norm = 1; Fft fft; std::vector<float> win, re, im, db, hold, smooth;
};

//==============================================================================
// Levels: peak, floating RMS, and momentary loudness with K weighting (ITU-R BS.1770: shelf + high-pass, 400 ms window).
// The result is labelled "LUFS" but this is a monitoring meter, not a certified loudness measurement.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    double tick(double x) { double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
};
inline Biquad kShelf(double sr)
{
    const double G = 3.999843853973347, f0 = 1681.974450955533, Q = 0.7071752369554196, K = std::tan(kPi * f0 / sr), Vh = std::pow(10.0, G / 20.0), Vb = std::pow(Vh, 0.4996667741545416);
    const double a0 = 1 + K / Q + K * K; Biquad b;
    b.b0 = (Vh + Vb * K / Q + K * K) / a0; b.b1 = 2 * (K * K - Vh) / a0; b.b2 = (Vh - Vb * K / Q + K * K) / a0; b.a1 = 2 * (K * K - 1) / a0; b.a2 = (1 - K / Q + K * K) / a0; return b;
}
inline Biquad kHighpass(double sr)
{
    const double f0 = 38.13547087602444, Q = 0.5003270373238773, K = std::tan(kPi * f0 / sr), a0 = 1 + K / Q + K * K; Biquad b;
    b.b0 = 1; b.b1 = -2; b.b2 = 1; b.a1 = 2 * (K * K - 1) / a0; b.a2 = (1 - K / Q + K * K) / a0; return b;
}

class Loudness
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate; for (int c = 0; c < 2; ++c) { shelf[c] = kShelf(sr); hp[c] = kHighpass(sr); }
        win = std::max(1, (int) std::lround(0.4 * sr)); ring.assign(win, 0.0f); pos = 0; sum = 0; filled = 0;
    }
    // One stereo frame in, momentary loudness available from lufs().
    void tick(float l, float r)
    {
        const double a = hp[0].tick(shelf[0].tick(l)), b = hp[1].tick(shelf[1].tick(r));
        const float p = (float) (a * a + b * b);
        sum += p - ring[pos]; ring[pos] = p; if (++pos >= win) pos = 0; if (filled < win) ++filled;
        if (sum < 0) sum = 0;
    }
    float lufs() const { return filled < win / 4 ? -200.0f : (float) (-0.691 + 10.0 * std::log10(std::max(1e-12, sum / win))); }
    void reset() { std::fill(ring.begin(), ring.end(), 0.0f); sum = 0; filled = 0; pos = 0; for (int c = 0; c < 2; ++c) { shelf[c].reset(); hp[c].reset(); } }
private:
    double sr = 48000, sum = 0; Biquad shelf[2], hp[2]; std::vector<float> ring; int win = 1, pos = 0, filled = 0;
};

//==============================================================================
// Stereo field numbers from running sums (exponential window, time constant tauMs).
struct StereoStats { float correlation = 0, width = 0, balance = 0, monoLossDb = 0, rmsL = 0, rmsR = 0, rmsM = 0, rmsS = 0; };

class StereoMeter
{
public:
    void prepare(double sampleRate, float tauMs = 300.0f) { k = 1.0 - std::exp(-1000.0 / (tauMs * sampleRate)); reset(); }
    void reset() { ll = rr = lr = mm = ss = 0; }
    void tick(float l, float r)
    {
        const double m = 0.5 * (l + r), s = 0.5 * (l - r);
        ll += k * ((double) l * l - ll); rr += k * ((double) r * r - rr); lr += k * ((double) l * r - lr); mm += k * (m * m - mm); ss += k * (s * s - ss);
    }
    StereoStats get() const
    {
        StereoStats o; const double e = std::sqrt(ll * rr);
        o.correlation = e < 1e-12 ? 0.0f : (float) std::max(-1.0, std::min(1.0, lr / e));
        o.rmsL = (float) std::sqrt(ll); o.rmsR = (float) std::sqrt(rr); o.rmsM = (float) std::sqrt(mm); o.rmsS = (float) std::sqrt(ss);
        o.width = (mm + ss) < 1e-12 ? 0.0f : (float) (ss / (mm + ss));                      // share of side energy, 0 (mono) to 1 (all side)
        o.balance = (ll + rr) < 1e-12 ? 0.0f : (float) ((rr - ll) / (rr + ll));              // -1 left, +1 right
        const double avg = 0.5 * (ll + rr); o.monoLossDb = avg < 1e-12 ? 0.0f : (float) (10.0 * std::log10(std::max(1e-12, mm / avg)));   // 0 dB: nothing lost in mono
        return o;
    }
    // Rules of thumb of the mockup (ours, not a standard).
    static const char* verdict(const StereoStats& s)
    {
        if (s.correlation < -0.2f) return "Phase problem";
        if (s.correlation < 0.2f || s.monoLossDb < -3.0f) return "Check mono";
        return "Mono safe";
    }
private:
    double k = 0.001, ll = 0, rr = 0, lr = 0, mm = 0, ss = 0;
};

//==============================================================================
// Scope trigger: index of the first rising zero crossing (through `level`) after `from`, or -1. Used by the free-run
// and trigger sync modes.
inline int findRisingCrossing(const float* x, int n, int from, float level = 0.0f)
{
    for (int i = std::max(1, from); i < n; ++i) if (x[i - 1] < level && x[i] >= level) return i;
    return -1;
}
// Period in samples by autocorrelation, for the pitch sync mode. Returns 0 when no clear period (strength below 0.5).
inline int findPeriod(const float* x, int n, int minLag, int maxLag)
{
    maxLag = std::min(maxLag, n / 2); double e0 = 0; for (int i = 0; i < n - maxLag; ++i) e0 += (double) x[i] * x[i];
    if (e0 < 1e-9) return 0;
    int best = 0; double bv = 0.5;
    for (int lag = minLag; lag < maxLag; ++lag)
    {
        double s = 0, e1 = 0; for (int i = 0; i < n - maxLag; ++i) { s += (double) x[i] * x[i + lag]; e1 += (double) x[i + lag] * x[i + lag]; }
        const double c = s / std::sqrt(std::max(1e-12, e0 * e1)); if (c > bv) { bv = c; best = lag; }
    }
    return best;
}

//==============================================================================
// Shapes: the average spectrum of a signal on 60 bands of 1/6 octave from 20 Hz, kept apart for Mid and Side.
constexpr int kShapeBands = 60;
inline double shapeBandHz(int i) { return 20.0 * std::pow(2.0, i / 6.0); }

struct Shape { std::string name; std::vector<float> mid, side; Shape() : mid(kShapeBands, 0.0f), side(kShapeBands, 0.0f) {} };

// dB spectrum (analyser bins) to 60 band levels in dB: mean power inside each band, interpolated when a band is narrower than a bin.
inline void binsToBands(const std::vector<float>& db, double sr, int fftSize, float* out)
{
    const double bh = sr / fftSize; const int bins = (int) db.size();
    for (int i = 0; i < kShapeBands; ++i)
    {
        const double lo = shapeBandHz(i) * std::pow(2.0, -1.0 / 12.0), hi = shapeBandHz(i) * std::pow(2.0, 1.0 / 12.0);
        int k0 = (int) std::ceil(lo / bh), k1 = (int) std::floor(hi / bh);
        if (k1 < k0) { const double f = shapeBandHz(i) / bh; const int a = std::min(bins - 2, std::max(0, (int) f)); const float t = (float) (f - a); out[i] = db[a] * (1 - t) + db[a + 1] * t; continue; }
        k0 = std::max(1, k0); k1 = std::min(bins - 1, k1); double p = 0; int c = 0;
        for (int k = k0; k <= k1; ++k) { p += std::pow(10.0, db[k] / 10.0); ++c; }
        out[i] = c ? (float) (10.0 * std::log10(std::max(1e-20, p / c))) : -120.0f;
    }
}

// Averages frames that carry signal (frame loudness above gateDb), Mid and Side apart.
class ShapeCapture
{
public:
    void reset() { frames = 0; for (int i = 0; i < kShapeBands; ++i) accM[i] = accS[i] = 0.0; }
    // bandsM, bandsS in dB; frameDb is the loudness of the frame, used as the gate.
    bool add(const float* bandsM, const float* bandsS, float frameDb, float gateDb = -60.0f)
    {
        if (frameDb < gateDb) return false;
        for (int i = 0; i < kShapeBands; ++i) { accM[i] += std::pow(10.0, bandsM[i] / 10.0); accS[i] += std::pow(10.0, bandsS[i] / 10.0); }
        ++frames; return true;
    }
    int count() const { return frames; }
    bool result(Shape& s) const
    {
        if (frames < 1) return false;
        for (int i = 0; i < kShapeBands; ++i) { s.mid[i] = (float) (10.0 * std::log10(std::max(1e-20, accM[i] / frames))); s.side[i] = (float) (10.0 * std::log10(std::max(1e-20, accS[i] / frames))); }
        return true;
    }
private:
    double accM[kShapeBands] = {}, accS[kShapeBands] = {}; int frames = 0;
};

inline float meanOf(const std::vector<float>& v) { double s = 0; for (float x : v) s += x; return v.empty() ? 0.0f : (float) (s / v.size()); }

// EQ that moves `mine` toward `ref`: each shape is normalised to its own average, then difference, 3 tap smoothing,
// times strength (0..1), limited to +-maxMoveDb.
inline std::vector<float> matchingEq(const std::vector<float>& ref, const std::vector<float>& mine, float strength, float maxMoveDb)
{
    std::vector<float> d(kShapeBands, 0.0f); if ((int) ref.size() != kShapeBands || (int) mine.size() != kShapeBands) return d;
    const float mr = meanOf(ref), mm = meanOf(mine);
    for (int i = 0; i < kShapeBands; ++i) d[i] = (ref[i] - mr) - (mine[i] - mm);
    std::vector<float> s(kShapeBands);
    for (int i = 0; i < kShapeBands; ++i) { const float a = d[std::max(0, i - 1)], b = d[i], c = d[std::min(kShapeBands - 1, i + 1)]; s[i] = 0.25f * a + 0.5f * b + 0.25f * c; }
    for (auto& v : s) v = std::max(-maxMoveDb, std::min(maxMoveDb, v * strength));
    return s;
}

// Text form of a shape: "WSHAPE1|name|m0,m1,...|s0,s1,...". Import validates everything and refuses bad input.
inline std::string shapeToText(const Shape& s)
{
    std::ostringstream o; o.setf(std::ios::fixed); o.precision(1); std::string nm; for (char c : s.name) if (c != '|' && c != '\n' && c != '\r') nm += c;
    o << "WSHAPE1|" << nm << "|"; for (int i = 0; i < kShapeBands; ++i) o << (i ? "," : "") << s.mid[i];
    o << "|"; for (int i = 0; i < kShapeBands; ++i) o << (i ? "," : "") << s.side[i]; return o.str();
}
inline bool parseList(const std::string& t, std::vector<float>& out)
{
    out.clear(); std::stringstream ss(t); std::string tok;
    while (std::getline(ss, tok, ',')) { char* e = nullptr; const double v = std::strtod(tok.c_str(), &e); if (e == tok.c_str() || *e != 0 || !std::isfinite(v) || v < -200 || v > 40) return false; out.push_back((float) v); }
    return (int) out.size() == kShapeBands;
}
inline bool shapeFromText(const std::string& text, Shape& out)
{
    if (text.size() > 4000) return false;
    std::vector<std::string> p; std::stringstream ss(text); std::string tok; while (std::getline(ss, tok, '|')) p.push_back(tok);
    if (p.size() != 4 || p[0] != "WSHAPE1" || p[1].empty() || p[1].size() > 40) return false;
    Shape s; s.name = p[1]; if (!parseList(p[2], s.mid) || !parseList(p[3], s.side)) return false; out = s; return true;
}

//==============================================================================
// Lock free single writer ring for the audio thread to hand samples to the display thread.
class SampleRing
{
public:
    static constexpr int kSize = 1 << 16;
    SampleRing() : buf(kSize, 0.0f) {}
    void push(float x) { const uint64_t w = wr.load(std::memory_order_relaxed); buf[w & (kSize - 1)] = x; wr.store(w + 1, std::memory_order_release); }
    // Copies the newest n samples (oldest first). A tear at the very newest samples is possible and harmless for display.
    void latest(float* out, int n) const
    {
        const uint64_t w = wr.load(std::memory_order_acquire); n = std::min(n, kSize);
        for (int i = 0; i < n; ++i) out[i] = (w >= (uint64_t) (n - i)) ? buf[(w - (uint64_t) (n - i)) & (kSize - 1)] : 0.0f;
    }
    uint64_t written() const { return wr.load(std::memory_order_acquire); }
private:
    std::vector<float> buf; std::atomic<uint64_t> wr { 0 };
};
}
