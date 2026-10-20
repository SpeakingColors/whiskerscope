// Whisker Scope engine. Pure C++17, no JUCE, so it can be unit tested (tests/engine_test.cpp).
// Contains: FFT and spectrum (Blackman window, 0 dB for a full scale sine), band maths, loudness (BS.1770 K weighting:
// momentary, short term, integrated), stereo numbers, scope helpers, the Shapes maths (60 bands of 1/6 octave, matching
// targets), and the Mid/Side match EQ (a cascade of peaking bells) with auto level and a safety limiter.
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace ws
{
constexpr double kPi = 3.14159265358979323846;
inline float dbOf(float x) { return 20.0f * std::log10(std::max(x, 1.0e-9f)); }
inline double clampd(double v, double a, double b) { return v < a ? a : (v > b ? b : v); }
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

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

// Note name for a frequency (A4 = 440 Hz), for example "A4" or "C#5". Empty for non finite or non positive input.
inline std::string noteName(double hz)
{
    if (!(hz > 8.0) || !std::isfinite(hz)) return "";
    static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const double m = 69.0 + 12.0 * std::log2(hz / 440.0); const int r = (int) std::lround(m);
    return std::string(n[((r % 12) + 12) % 12]) + std::to_string((int) std::floor(r / 12.0) - 1);
}

//==============================================================================
// Spectrum: Blackman window, FFT of the newest fftSize samples, magnitude smoothed in the linear domain, read as dB.
// Level reference: a full scale sine (amplitude 1) reads 0 dB at its peak bin.
class Spectrum
{
public:
    void prepare(double sampleRate, int fftSize)
    {
        sr = sampleRate; n = fftSize; fft.setSize(n);
        win.resize(n); double s = 0;
        for (int i = 0; i < n; ++i) { const double p = 2.0 * kPi * i / n; win[i] = (float) (0.42 - 0.5 * std::cos(p) + 0.08 * std::cos(2.0 * p)); s += win[i]; }
        norm = (float) (2.0 / s);
        re.assign(n, 0); im.assign(n, 0); bins = n / 2; mag.assign(bins, 0.0f); db.assign(bins, -200.0f);
    }
    int numBins() const { return bins; }      // fftSize / 2 bins, bin k is at k * sr / fftSize
    int fftSize() const { return n; }
    double sampleRate() const { return sr; }
    // x holds exactly fftSize samples. smoothing is the keep factor of the old magnitude (0 = no smoothing, 0.8 = calm).
    void analyse(const float* x, float smoothing)
    {
        for (int i = 0; i < n; ++i) { re[i] = x[i] * win[i]; im[i] = 0; }
        fft.forward(re.data(), im.data());
        const float k0 = clampf(smoothing, 0.0f, 0.999f);
        for (int k = 0; k < bins; ++k)
        {
            const float m = std::sqrt(re[k] * re[k] + im[k] * im[k]) * norm * (k == 0 ? 0.5f : 1.0f);
            mag[k] = k0 * mag[k] + (1.0f - k0) * m; db[k] = std::max(-200.0f, dbOf(mag[k]));
        }
    }
    void reset() { std::fill(mag.begin(), mag.end(), 0.0f); std::fill(db.begin(), db.end(), -200.0f); }
    const std::vector<float>& spectrum() const { return db; }
private:
    double sr = 48000; int n = 2048, bins = 1024; float norm = 1; Fft fft; std::vector<float> win, re, im, mag, db;
};

// dB read at a frequency, linear interpolation between bins. arr has n bins, bin k at k * sr / (2 n).
inline float binDb(const float* arr, int n, double sr, double f)
{
    const double df = sr / (n * 2.0), p = f / df; const int i = (int) std::floor(p); const float fr = (float) (p - i);
    if (i < 0) return arr[0];
    if (i >= n - 1) return arr[n - 1];
    return arr[i] * (1 - fr) + arr[i + 1] * fr;
}
// Mean power between f0 and f1 in dB (interpolated when the band is narrower than a bin). Bins below -139 count as -140.
inline float bandDb(const float* arr, int n, double sr, double f0, double f1)
{
    const double df = sr / (n * 2.0), b0 = f0 / df, b1 = f1 / df;
    if (b1 - b0 < 1) return binDb(arr, n, sr, 0.5 * (f0 + f1));
    const int i0 = std::max(0, (int) std::floor(b0)), i1 = std::min(n - 1, (int) std::ceil(b1)); double s = 0; int c = 0;
    for (int i = i0; i <= i1; ++i) { float v = arr[i]; if (v < -139.0f) v = -140.0f; s += std::pow(10.0, v / 10.0); ++c; }
    return (float) (10.0 * std::log10(s / std::max(1, c) + 1e-30));
}

//==============================================================================
// Loudness, ITU-R BS.1770 K weighting (shelf + high pass), 100 ms sub blocks. momentary = last 400 ms, short term = last 3 s,
// integrated = gated (absolute -70 LUFS, relative -10 LU) over the stored 400 ms blocks (up to two hours). Called from the audio
// thread (no allocation after prepare); the numbers are atomics for the display. This is a monitoring meter, not a certified one.
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
inline double luOf(double e) { return e > 1e-12 ? -0.691 + 10.0 * std::log10(e) : -120.0; }

class LoudnessMeter
{
public:
    static constexpr int kHist = 72000;      // two hours of 400 ms blocks, one per 100 ms
    void prepare(double sampleRate)
    {
        sr = sampleRate; sub = std::max(1, (int) std::lround(sr * 0.1)); hist.assign(kHist, 0.0f); resetState();
        mom.store(-120.0f); shortT.store(-120.0f); integ.store(-120.0f); maxM.store(-120.0f);
    }
    void requestReset() { resetFlag.store(true); }
    void process(const float* l, const float* r, int count)
    {
        if (resetFlag.exchange(false)) { resetState(); mom.store(-120.0f); shortT.store(-120.0f); integ.store(-120.0f); maxM.store(-120.0f); }
        for (int i = 0; i < count; ++i)
        {
            const double a = hp[0].tick(sh[0].tick(l[i])), b = hp[1].tick(sh[1].tick(r ? r[i] : l[i]));
            acc += a * a + b * b;
            if (++cnt >= sub) endSub();
        }
    }
    float momentary() const { return mom.load(); }
    float shortTerm() const { return shortT.load(); }
    float integrated() const { return integ.load(); }
    float maxMomentary() const { return maxM.load(); }
private:
    static constexpr int kSubs = 40;
    void resetState()
    {
        for (int c = 0; c < 2; ++c) { sh[c] = kShelf(sr); hp[c] = kHighpass(sr); }
        acc = 0; cnt = 0; nsub = 0; head = 0; histN = 0; maxMom = -120.0;
    }
    void endSub()
    {
        subs[head] = acc / sub; head = (head + 1) % kSubs; nsub = std::min(kSubs, nsub + 1); acc = 0; cnt = 0;
        if (nsub < 4) return;
        auto at = [&](int back) { return subs[(head - 1 - back + 2 * kSubs) % kSubs]; };
        const double b = (at(0) + at(1) + at(2) + at(3)) / 4.0, m = luOf(b);
        if (histN >= kHist) { std::copy(hist.begin() + kHist / 2, hist.end(), hist.begin()); histN = kHist / 2; }
        hist[histN++] = (float) b; if (m > maxMom) maxMom = m;
        mom.store((float) m); maxM.store((float) maxMom);
        const int n = std::min(30, nsub); double a = 0; for (int i = 0; i < n; ++i) a += at(i); shortT.store((float) luOf(a / n));
        const double absT = std::pow(10.0, (-70.0 + 0.691) / 10.0); double s = 0; int c = 0;
        for (int i = 0; i < histN; ++i) if (hist[i] > absT) { s += hist[i]; ++c; }
        if (!c) { integ.store(-120.0f); return; }
        const double rel = s / c * 0.1; s = 0; c = 0;
        for (int i = 0; i < histN; ++i) if (hist[i] > absT && hist[i] > rel) { s += hist[i]; ++c; }
        integ.store(c ? (float) luOf(s / c) : -120.0f);
    }
    double sr = 48000, acc = 0, maxMom = -120.0; int sub = 4800, cnt = 0, nsub = 0, head = 0, histN = 0;
    Biquad sh[2], hp[2]; double subs[kSubs] = {}; std::vector<float> hist;
    std::atomic<bool> resetFlag { false }; std::atomic<float> mom { -120.0f }, shortT { -120.0f }, integ { -120.0f }, maxM { -120.0f };
};

//==============================================================================
// Stereo field numbers from the newest samples (the display smooths them over time).
struct StereoNow { bool silent = true; float correlation = 0, monoLossDb = 0, widthPct = 0, balanceDb = 0; };
inline StereoNow stereoNow(const float* l, const float* r, int n)
{
    double ll = 0, rr = 0, lr = 0, mm = 0, ss = 0;
    for (int i = 0; i < n; ++i) { const double a = l[i], b = r[i], m = (a + b) * 0.5, s = (a - b) * 0.5; ll += a * a; rr += b * b; lr += a * b; mm += m * m; ss += s * s; }
    StereoNow o; o.silent = (ll + rr) / (2.0 * std::max(1, n)) < 1e-7; if (o.silent) return o;
    o.correlation = (float) (lr / std::sqrt(ll * rr + 1e-20));
    o.monoLossDb = (float) (10.0 * std::log10((mm + 1e-20) / ((ll + rr) / 2 + 1e-20)));
    o.widthPct = (float) clampd(std::sqrt(ss / (mm + 1e-12)) * 100.0, 0, 200);
    o.balanceDb = (float) clampd(10.0 * std::log10((ll + 1e-12) / (rr + 1e-12)), -24, 24); return o;
}
// 0 mono safe, 1 check mono, 2 phase problem, -1 no signal (rules of thumb of the mockup, ours, not a standard).
inline int stereoVerdict(bool silent, float corr, float monoLossDb)
{
    if (silent) return -1;
    if (corr < -0.2f || monoLossDb < -6.0f) return 2;
    if (corr < 0.2f || monoLossDb < -3.0f) return 1;
    return 0;
}

//==============================================================================
// Scope helper: autocorrelation pitch (Hz, 0 when no clear tone) on 2048 samples, every second one of the newest 4096.
inline double detectPitch(const float* x, int len, double sr)
{
    const int n = 2048; if (len < 4096) return 0;
    std::vector<float> d(n); const int off = len - 4096; for (int i = 0; i < n; ++i) d[i] = x[off + 2 * i];
    const double s2 = sr / 2; const int minL = std::max(2, (int) std::floor(s2 / 1500)), maxL = std::min(n / 2, (int) std::floor(s2 / 50));
    double r0 = 0; for (int i = 0; i < n; ++i) r0 += (double) d[i] * d[i];
    if (r0 < 1e-4) return 0;
    double best = 0, prev = 1; int bl = 0; bool rising = false;
    for (int l = minL; l < maxL; ++l)
    {
        double r = 0; for (int i = 0; i < n - l; ++i) r += (double) d[i] * d[i + l]; r /= r0;
        if (r > prev) rising = true;
        if (rising && r > best) { best = r; bl = l; }
        prev = r;
    }
    return (best > 0.6 && bl) ? s2 / bl : 0;
}

//==============================================================================
// Shapes: the average spectrum of a signal on 60 bands of 1/6 octave from 20 Hz (band k spans 20 * 2^(k/6) to 20 * 2^((k+1)/6)),
// kept apart for Mid and Side.
constexpr int kBands = 60;
inline double bandEdgeHz(int k) { return 20.0 * std::pow(2.0, k / 6.0); }
inline double bandCentreHz(int k) { return 20.0 * std::pow(2.0, (k + 0.5) / 6.0); }

struct Shape
{
    std::string name; std::array<float, kBands> m {}, s {}; float lufs = -999.0f;   // lufs below -900: not stored
    bool hasLufs() const { return lufs > -900.0f; }
};

// dB spectrum (n bins) to 60 band levels in dB. Bands above 98 percent of Nyquist are -140.
inline void spectrumToBands(const float* db, int n, double sr, float* out)
{
    for (int k = 0; k < kBands; ++k) out[k] = bandEdgeHz(k + 1) > sr / 2 * 0.98 ? -140.0f : bandDb(db, n, sr, bandEdgeHz(k), bandEdgeHz(k + 1));
}
// Each shape is compared against its own average (bands 6 to 57), so only the tone matters, not the level.
inline void relNorm(const float* a, float* o)
{
    double s = 0; int c = 0; for (int k = 6; k <= 57; ++k) if (a[k] > -130.0f) { s += a[k]; ++c; }
    const float mu = c ? (float) (s / c) : 0.0f; for (int k = 0; k < kBands; ++k) o[k] = a[k] > -130.0f ? a[k] - mu : -30.0f;
}
inline void smoothBands(const float* d, int w, float* o)
{
    const int h = w / 2;
    for (int k = 0; k < kBands; ++k) { double a = 0; int c = 0; for (int j = k - h; j <= k + h; ++j) if (j >= 0 && j < kBands) { a += d[j]; ++c; } o[k] = (float) (a / c); }
}
inline void tiltBands(const float* in, float dbPerOct, float* out)
{
    for (int k = 0; k < kBands; ++k) out[k] = in[k] + dbPerOct * (float) std::log2(bandCentreHz(k) / 1000.0);
}
// The EQ that moves `mine` toward `ref`: both normalised, difference, smoothed over smoothBandsN bands, times strength, limited to +-limitDb.
inline void matchTargets(const float* ref, const float* mine, int smoothBandsN, float strength, float limitDb, float* out)
{
    float a[kBands], b[kBands], d[kBands], sm[kBands]; relNorm(ref, a); relNorm(mine, b);
    for (int k = 0; k < kBands; ++k) d[k] = a[k] - b[k];
    smoothBands(d, std::max(1, smoothBandsN), sm);
    for (int k = 0; k < kBands; ++k) out[k] = clampf(sm[k] * strength, -limitDb, limitDb);
}
// Value at a frequency from 60 band values (interpolated between band centres).
inline float interpBands(const float* arr, double fc)
{
    const double kf = clampd(6.0 * std::log2(fc / 20.0) - 0.5, 0, kBands - 1); const int a = (int) std::floor(kf), b = std::min(kBands - 1, a + 1); const float f = (float) (kf - a);
    return arr[a] * (1 - f) + arr[b] * f;
}
inline bool shapeValuesOk(const std::vector<double>& v)
{
    if ((int) v.size() != kBands) return false;
    for (double x : v) if (!std::isfinite(x) || x <= -200 || x >= 80) return false;
    return true;
}

//==============================================================================
// Match EQ: 30 peaking bells (every second band centre, 1/3 octave apart) per channel, Mid and Side apart.
// designBells fits the bell gains so the cascade follows the 60 band target curve (a few correction passes, because bells overlap).
constexpr int kBells = 30;
constexpr double kBellQ = 2.5;
inline double bellHz(int i) { return bandCentreHz(2 * i + 1); }

// Exact magnitude in dB of an RBJ peaking biquad (centre fc, quality q, gain g dB) at frequency f, sample rate fs.
inline double peakingDb(double fc, double q, double g, double fs, double f)
{
    const double A = std::pow(10.0, g / 40.0), w0 = 2 * kPi * fc / fs, al = std::sin(w0) / (2 * q), c0 = std::cos(w0);
    const double b0 = 1 + al * A, b1 = -2 * c0, b2 = 1 - al * A, a0 = 1 + al / A, a1 = -2 * c0, a2 = 1 - al / A, w = 2 * kPi * f / fs;
    const std::complex<double> z1 = std::polar(1.0, -w), z2 = std::polar(1.0, -2 * w);
    const double num = std::abs(b0 + b1 * z1 + b2 * z2), den = std::abs(a0 + a1 * z1 + a2 * z2);
    return 20.0 * std::log10(std::max(1e-12, num / den));
}
inline void designBells(const float* target, double fs, float* gains)
{
    double g[kBells] = {}, tgt[kBells];
    for (int i = 0; i < kBells; ++i) tgt[i] = 0.5 * (target[2 * i] + target[2 * i + 1]);
    for (int pass = 0; pass < 14; ++pass)
        for (int i = 0; i < kBells; ++i)
        {
            if (bellHz(i) > 0.45 * fs) { g[i] = 0; continue; }
            double r = 0; for (int j = 0; j < kBells; ++j) if (g[j] != 0.0 && bellHz(j) <= 0.45 * fs) r += peakingDb(bellHz(j), kBellQ, g[j], fs, bellHz(i));
            g[i] = clampd(g[i] + 0.6 * (tgt[i] - r), -24, 24);
        }
    for (int i = 0; i < kBells; ++i) gains[i] = (float) g[i];
}
inline double bellsResponseDb(const float* gains, double fs, double f)
{
    double r = 0; for (int j = 0; j < kBells; ++j) if (gains[j] != 0.0f && bellHz(j) <= 0.45 * fs) r += peakingDb(bellHz(j), kBellQ, gains[j], fs, f);
    return r;
}

class BellCascade
{
public:
    // Sets the coefficients. Bells under 0.05 dB are skipped; a bell that was off starts from a clean state.
    void design(const float* gains, double fs)
    {
        for (int i = 0; i < kBells; ++i)
        {
            const double gdb = gains[i]; const bool on = std::fabs(gdb) >= 0.05 && bellHz(i) <= 0.45 * fs;
            if (!on) { active[i] = false; z1[i] = z2[i] = 0; continue; }
            const double A = std::pow(10.0, gdb / 40.0), w0 = 2 * kPi * bellHz(i) / fs, al = std::sin(w0) / (2 * kBellQ), c0 = std::cos(w0), a0 = 1 + al / A;
            b0[i] = (1 + al * A) / a0; b1[i] = -2 * c0 / a0; b2[i] = (1 - al * A) / a0; a1[i] = -2 * c0 / a0; a2[i] = (1 - al / A) / a0;
            if (!active[i]) z1[i] = z2[i] = 0;
            active[i] = true;
        }
        any = false; for (int i = 0; i < kBells; ++i) any = any || active[i];
    }
    bool isActive() const { return any; }
    double process(double x)
    {
        if (!std::isfinite(x)) { reset(); return 0; }
        for (int i = 0; i < kBells; ++i)
        {
            if (!active[i]) continue;
            const double y = b0[i] * x + z1[i]; z1[i] = b1[i] * x - a1[i] * y + z2[i]; z2[i] = b2[i] * x - a2[i] * y; x = y;
        }
        return x;
    }
    void reset() { for (int i = 0; i < kBells; ++i) z1[i] = z2[i] = 0; }
private:
    bool any = false; bool active[kBells] = {};
    double b0[kBells] = {}, b1[kBells] = {}, b2[kBells] = {}, a1[kBells] = {}, a2[kBells] = {}, z1[kBells] = {}, z2[kBells] = {};
};

// Mid/Side EQ stage. With one channel only Mid is used.
class MidSideEq
{
public:
    void design(const float* gainsM, const float* gainsS, double fs) { m.design(gainsM, fs); s.design(gainsS, fs); }
    bool isActive() const { return m.isActive() || s.isActive(); }
    void reset() { m.reset(); s.reset(); }
    void process(float& l, float& r)
    {
        const double mid = 0.5 * ((double) l + r), sid = 0.5 * ((double) l - r), M = m.process(mid), S = s.process(sid);
        l = (float) (M + S); r = (float) (M - S);
    }
    void processMono(float& l) { l = (float) m.process(l); }
private:
    BellCascade m, s;
};

// Auto level (mockup rules): keeps the loudness of the matched sound equal to the loudness that comes in (mode 1), or to the
// loudness stored with the reference (mode 2). Moves slowly (1 s), never beyond +-12 dB, never so far that peaks pass -0.3 dBFS.
struct AutoLevel
{
    float db = 0, prePeakDb = -120.0f; bool held = false;
    void reset() { db = 0; prePeakDb = -120.0f; held = false; }
    // mode 0 off, 1 keep mine, 2 match reference. refLufs: loudness stored with the reference, or below -900 for none.
    void update(float dt, int mode, float preShortLufs, float inShortLufs, float refLufs, float outDb, float newPeakDb)
    {
        held = false; prePeakDb = std::max(newPeakDb, prePeakDb - 6.0f * dt); bool ok = false; float want = 0;
        if (mode != 0)
        {
            const float tgt = (mode == 2 && refLufs > -900.0f) ? refLufs : inShortLufs;
            if (preShortLufs > -65.0f && tgt > -65.0f)
            {
                want = clampf(tgt - preShortLufs, -12.0f, 12.0f); const float cap = -0.3f - prePeakDb - outDb;
                if (want > cap) { want = cap; held = true; }
                ok = true;
            }
        }
        if (mode == 0) db += (0 - db) * std::min(1.0f, dt / 0.3f); else if (ok) db += (want - db) * std::min(1.0f, dt / 1.0f);
        if (std::fabs(db) < 0.002f && (mode == 0 || (ok && std::fabs(want) < 0.002f))) db = 0;     // snap to exactly 0 dB so the path is bit exact again
    }
};

// Safety limiter: instant attack, 60 ms release, ceiling -0.1 dBFS. Only meant to catch overs from the match stage.
struct SafetyLimiter
{
    static constexpr float ceiling = 0.98855f; float gr = 1.0f, rel = 0.999f;
    void prepare(double fs) { rel = (float) std::exp(-1.0 / (0.060 * fs)); gr = 1.0f; }
    void process(float& l, float& r)
    {
        const float pk = std::max(std::fabs(l), std::fabs(r)); const float need = pk > ceiling ? ceiling / pk : 1.0f;
        gr = std::min(need, 1.0f + (gr - 1.0f) * rel);
        l *= gr; r *= gr;
    }
    void processMono(float& l) { float r = l; process(l, r); l = r; }
};

//==============================================================================
// Lock free single writer ring for the audio thread to hand samples to the display thread. Size is a power of two.
template <int Log2Size>
class SampleRingT
{
public:
    static constexpr int kSize = 1 << Log2Size;
    SampleRingT() : buf(kSize, 0.0f) {}
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
using SampleRing = SampleRingT<18>;     // about 1.3 s at 192 kHz
using SampleRingSmall = SampleRingT<16>;
}
