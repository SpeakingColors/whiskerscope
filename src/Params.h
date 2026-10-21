// Generated once from the approved mockup (docs/mockup/whisker-scope-mockup.html); tips are the info bubble texts. No JUCE here.
#pragma once
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace wsp
{
struct ChoiceDef { const char* id; const char* name; const char* label; const char* tip; std::vector<const char*> labels, raws; int def; bool numeric; };
struct NumDef { const char* id; const char* name; const char* label; const char* tip; float lo, hi, def, step; bool logScale; const char* fmt; };
struct FlagDef { const char* id; const char* name; const char* label; const char* tip; bool def; };

inline const std::vector<ChoiceDef>& choices()
{
    static const std::vector<ChoiceDef> v = {
        { "color", "Colours", "Colours", "Colour scheme for all displays.", { "Soda aqua / pink", "Violet / orange", "Ice / rose", "Mono" }, { "soda", "violet", "ice", "mono" }, 0, false },
        { "chan", "Scope channels", "Channels", "Which channels are drawn. Mid is (L+R)/2, Side is (L-R)/2.", { "Stereo", "Left", "Right", "Mono", "Mid / Side" }, { "lr", "l", "r", "mono", "ms" }, 0, false },
        { "sync", "Scope sync", "Sync", "Free: always shows the latest audio. Trigger: locks on a rising edge so repeating waves hold still. Frequency: fixed Hz you set. Pitch: follows the detected pitch. Beat: window follows the host tempo.", { "Free", "Trigger", "Frequency", "Pitch", "Beat" }, { "free", "trig", "freq", "pitch", "beat" }, 0, false },
        { "div", "Scope beat length", "Beat length", "Length of the window in Beat mode. The window restarts on the beat grid of the host tempo.", { "1/32 note", "1/16 note", "1/8 note", "1/4 note" }, { "1/32", "1/16", "1/8", "1/4" }, 1, false },
        { "pkSpd", "Peak label speed", "Label speed", "How often the peak labels refresh. Slower is easier to read.", { "Live", "Medium", "Slow", "Hold 2 s" }, { "0", "400", "1000", "2000" }, 2, true },
        { "pkN", "Peak labels", "Peak label", "Shows the peak level and the pitch (Hz and note) right on the scope. 0 hides them.", { "Off", "1", "3", "5", "8" }, { "0", "1", "3", "5", "8" }, 2, true },
        { "ana", "Spectrum analyse", "Analyse", "Which signal the analyser looks at. Stereo (left and right) and Mid / Side draw two curves.", { "Mono", "Left", "Right", "Side", "Stereo", "Mid / Side" }, { "mono", "l", "r", "side", "lr", "ms" }, 4, false },
        { "fft", "FFT size", "FFT size", "Bigger sizes separate bass notes better but react slower.", { "2048", "4096", "8192", "16384", "32768" }, { "2048", "4096", "8192", "16384", "32768" }, 2, true },
        { "speed", "Spectrum speed", "Speed", "How quickly the curve follows the sound.", { "Slow", "Medium", "Fast" }, { "slow", "med", "fast" }, 1, false },
        { "range", "Spectrum range", "Range", "How many dB below 0 dBFS the display shows.", { "60 dB", "90 dB", "120 dB" }, { "60", "90", "120" }, 1, true },
        { "slope", "Spectrum slope", "Slope", "Tilts the curve up toward the highs so a typical mix looks flat. Pivot is 1 kHz. 4.5 dB/oct is the default, the same value other analysers use.", { "0 dB/oct", "3 dB/oct", "4.5 dB/oct" }, { "0", "3", "4.5" }, 2, true },
        { "maskDb", "Masking", "Masking", "With a sidechain (layer B) playing: marks the frequency ranges where A and B are both loud and within this many dB of each other, a sign that they may mask each other. Rule of thumb, check by ear.", { "Off", "Within 3 dB", "Within 6 dB", "Within 10 dB" }, { "0", "3", "6", "10" }, 2, true },
        { "bands", "Spectrum bands", "Bands", "Full shows every detail. Octave bands show how an equaliser would see it.", { "Full", "1/6 octave", "1/3 octave", "1 octave" }, { "full", "6", "3", "1" }, 0, false },
        { "scale", "Spectrum scale", "Scale", "Log spaces the octaves evenly. Linear spaces the Hz evenly.", { "Log", "Linear" }, { "log", "lin" }, 0, false },
        { "mmode", "Metering", "Metering", "Which level the bars show. Peak is the loudest moment, RMS is the average level, Loudness (LUFS) is how loud it sounds (momentary, short-term and integrated, K-weighted as in streaming and broadcast meters).", { "Peak", "RMS", "Peak + RMS", "Loudness (LUFS)" }, { "peak", "rms", "both", "lufs" }, 2, false },
        { "rms", "RMS length", "RMS length", "Averaging time of the RMS meter. Longer is calmer and closer to perceived loudness.", { "50 ms", "300 ms", "1 s" }, { "50", "300", "1000" }, 1, true },
        { "shChan", "Shapes show", "Show", "Which half of the sound the graph shows. Mid is the centre (kick, bass, lead), Side is what is wide. Both halves are always matched.", { "Mid", "Side" }, { "m", "s" }, 0, false },
        { "shSmooth", "EQ smoothing", "EQ smoothing", "Wider smoothing gives gentler, more musical EQ moves.", { "1/6 oct", "1/3 oct", "1/2 oct", "1 oct" }, { "1", "2", "3", "6" }, 1, true },
        { "shAuto", "Auto level", "Auto level", "Off: only the Output knob sets the level. Keep my loudness: the EQ never makes the sound louder or quieter than what comes in, so you judge tone, not volume. Match reference: output loudness follows the loudness captured with the reference (needs a captured or saved reference that has loudness).", { "Off", "Keep my loudness", "Match reference" }, { "off", "in", "ref" }, 1, false },
        { "shRefTilt", "Reference tilt", "Ref tilt", "Edits the reference: tilts its brightness around 1 kHz, for example to make a dark reference a bit brighter. 0 leaves it as it is.", { "-6 dB/oct", "-3 dB/oct", "-1.5", "0 (as is)", "+1.5", "+3 dB/oct", "+6 dB/oct" }, { "-6", "-3", "-1.5", "0", "1.5", "3", "6" }, 3, true },
        { "shLen", "Capture length", "Capture length", "How long a capture averages the sound.", { "Until stop", "10 s", "30 s" }, { "0", "10", "30" }, 0, true },
    };
    return v;
}

inline const std::vector<NumDef>& nums()
{
    static const std::vector<NumDef> v = {
        { "gain", "Input gain", "Input gain", "Boosts or cuts the level going into the plugin. The displays, the meters and the matching EQ all follow it.", -24.0f, 24.0f, 0.0f, 0.1f, false, "db" },
        { "scGain", "Sidechain gain", "SC gain", "Level of the sidechain (layer B) signal that the displays see. It is never played.", -24.0f, 24.0f, 0.0f, 0.1f, false, "db" },
        { "dur", "Scope time", "Time", "How much time fits on the screen (1 ms to 400 ms).", 1.0f, 400.0f, 60.0f, 0.0f, true, "ms" },
        { "freq", "Scope sync Hz", "Sync Hz", "Frequency the Frequency mode locks to.", 20.0f, 2000.0f, 110.0f, 0.0f, true, "hz" },
        { "periods", "Scope cycles", "Cycles", "How many wave cycles are shown in Frequency and Pitch modes.", 1.0f, 16.0f, 4.0f, 1.0f, false, "cyc" },
        { "trig", "Scope trigger level", "Trig level", "Level the Trigger mode waits for before starting the sweep.", -0.5f, 0.5f, 0.0f, 0.01f, false, "n2" },
        { "pers", "Persistence", "Persistence", "How long the dots leave a trail. More trail shows the shape of the sound over time.", 0.0f, 0.95f, 0.6f, 0.01f, false, "pct" },
        { "zoom", "Stereo zoom", "Zoom", "Magnifies the picture so quiet material becomes visible.", 0.25f, 8.0f, 1.0f, 0.0f, true, "x" },
        { "shStrength", "Match strength", "Strength", "How much of the difference to correct. 100 % tries to copy the reference exactly, 50 % goes halfway.", 0.0f, 1.0f, 0.7f, 0.01f, false, "pct" },
        { "shLimit", "Match max move", "Max move", "No EQ move is allowed to be bigger than this.", 3.0f, 18.0f, 9.0f, 0.5f, false, "lim" },
        { "shOut", "Output gain", "Output", "Level out of the plugin, after the EQ and the auto level.", -24.0f, 24.0f, 0.0f, 0.1f, false, "db" },
    };
    return v;
}

inline const std::vector<FlagDef>& flags()
{
    static const std::vector<FlagDef> v = {
        { "trans", "Transients", "Transients", "Starts the window on the latest sudden loud event, handy for kicks and plucks.", false },
        { "fzScope", "Freeze scope", "Freeze", "Holds the scope picture. Meters keep moving. Shortcut: F.", false },
        { "style", "Fill", "Fill", "Switches between a filled spectrum and a thin line.", true },
        { "wf", "Waterfall", "Waterfall", "Draws the history of the spectrum as colours scrolling away.", false },
        { "rot", "Rotate", "Rotate", "Turns the waterfall sideways: time runs left, frequency runs up.", false },
        { "hold", "Peak hold", "Peak hold", "Keeps a thin line at the loudest value each frequency reached.", true },
        { "fzSpec", "Freeze spectrum", "Freeze", "Holds the spectrum picture. Shortcut: F.", false },
        { "fzSt", "Freeze stereo", "Freeze", "Holds the picture. Shortcut: F.", false },
        { "keyHold", "Key hold", "Key hold", "Freezes the key readout in the top right corner of the display, so it stays while you play something else.", false },
        { "shHear", "EQ on", "EQ on", "Turns the matching EQ on or off so you can A/B. With Auto level on, the loudness stays the same, so you hear only the tone change. The EQ is 30 bells for Mid and 30 for Side, a third of an octave apart.", false },
        { "shRefOn", "Reference on", "Reference on", "Switches the reference off without losing it: the comparison and the matching EQ stop, the shape stays stored. Turn it back on any time.", true },
    };
    return v;
}

// Text of a numeric parameter value, as the knob and the host show it.
inline std::string formatNum(const char* fmt, float v)
{
    char b[48];
    if (!std::strcmp(fmt, "db")) std::snprintf(b, sizeof b, "%s%.1f dB", v > 0 ? "+" : "", (double) v);
    else if (!std::strcmp(fmt, "ms")) { if (v < 10) std::snprintf(b, sizeof b, "%.1f ms", (double) v); else std::snprintf(b, sizeof b, "%d ms", (int) std::lround(v)); }
    else if (!std::strcmp(fmt, "hz")) std::snprintf(b, sizeof b, "%d Hz", (int) std::lround(v));
    else if (!std::strcmp(fmt, "cyc")) std::snprintf(b, sizeof b, "%d %s", (int) std::lround(v), (int) std::lround(v) == 1 ? "cycle" : "cycles");
    else if (!std::strcmp(fmt, "pct")) std::snprintf(b, sizeof b, "%d %%", (int) std::lround(v * 100.0f));
    else if (!std::strcmp(fmt, "x")) std::snprintf(b, sizeof b, "%.2fx", (double) v);
    else if (!std::strcmp(fmt, "lim")) std::snprintf(b, sizeof b, "+/-%.1f dB", (double) v);
    else std::snprintf(b, sizeof b, "%.2f", (double) v);
    return b;
}
}
