// Screenshot tool (not part of the plugin, not built in CI). Renders the real editor offscreen: feeds a synthetic stereo signal through the
// real processor, ticks the editor like the frame timer does, and writes one PNG per page. Used for the manual tour and for comparing
// the editor with the approved mockup.
//   usage: ws_shoot <outDir> [widthPx] [colourIndex]   (run under xvfb-run on a headless machine)
#include "../src/PluginEditor.h"
#include "../src/Views.h"

static double saw (double ph) { return 2.0 * (ph - std::floor (ph)) - 1.0; }

// A made-up loop: kick on the beat, detuned wide lead, a bass line and some hats. Stereo wide enough to look alive.
static void synth (std::vector<float>& L, std::vector<float>& R, double fs, double seconds, int variant, double bpm = 140.0)
{
    const int n = (int) (fs * seconds); L.assign ((size_t) n, 0.0f); R.assign ((size_t) n, 0.0f); juce::Random rnd (7 + variant); double hatE = 0; const double beat = 60.0 / bpm;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs, bt = std::fmod (t, beat), st = std::fmod (t, beat / 4);
        double kick = std::sin (2 * M_PI * (45 * bt + 100.0 * (1 - std::exp (-bt * 30)) / 30.0)) * std::exp (-bt * 9) * 0.8;
        double bass = 0.18 * std::sin (2 * M_PI * 55.0 * t) * (std::fmod (t, beat * 2) < beat * 1.5 ? 1 : 0.4);
        const double f0 = variant == 1 ? 220.0 : 330.0; double lead = 0, leadR = 0;
        for (int k = 0; k < 5; ++k) { lead += saw (f0 * (1 + 0.004 * (k - 2)) * t + k * 0.13) * 0.035; leadR += saw (f0 * (1 + 0.005 * (k - 2)) * t + k * 0.31) * 0.035; }
        const double env = 0.5 + 0.5 * std::sin (2 * M_PI * t / (beat * 4)); lead *= 0.5 + env; leadR *= 0.5 + env;
        hatE = (st < 0.0015 ? 1.0 : hatE * 0.9993); const double hat = (rnd.nextFloat() * 2 - 1) * 0.12 * hatE * ((int) (t / (beat / 4)) % 2 ? 1.0 : 0.3);
        const double pad = 0.03 * std::sin (2 * M_PI * 110.0 * t + 4 * std::sin (2 * M_PI * 0.3 * t));
        L[(size_t) i] = (float) (kick + bass + lead + hat + pad); R[(size_t) i] = (float) (kick + bass + leadR - hat * 0.6 - pad * 0.5);
    }
}

// Walks the editor and lists every control that has an info bubble: name, bubble text, kind, shown value and its box in design pixels.
static void collect (juce::Component* root, juce::Component* c, juce::Array<juce::var>& out)
{
    for (auto* ch : c->getChildren())
    {
        if (! ch->isVisible()) continue;
        auto toBox = [&] (juce::Component* k) { auto b = root->getLocalArea (k, k->getLocalBounds()); juce::Array<juce::var> a { b.getX(), b.getY(), b.getWidth(), b.getHeight() }; return juce::var (a); };
        auto add = [&] (const juce::String& kind, const juce::String& value)
        {
            auto* tc = dynamic_cast<juce::TooltipClient*> (ch); if (tc == nullptr) return false; auto tip = tc->getTooltip(); if (tip.isEmpty()) return false;
            auto* o = new juce::DynamicObject(); const auto nl = tip.indexOf ("\n"); o->setProperty ("name", nl > 0 ? tip.substring (0, nl) : tip); o->setProperty ("tip", nl > 0 ? tip.substring (nl + 1) : juce::String());
            o->setProperty ("kind", kind); o->setProperty ("value", value); o->setProperty ("box", toBox (ch)); out.add (juce::var (o)); return true;
        };
        if (auto* f = dynamic_cast<wsui::Field*> (ch)) { add ("menu", f->box.getText()); continue; }
        if (auto* k = dynamic_cast<juce::Slider*> (ch)) { add ("knob", juce::String (k->getValue())); continue; }
        if (auto* b = dynamic_cast<juce::Button*> (ch)) { add ("button", b->getToggleState() ? "on" : "off"); continue; }
        if (dynamic_cast<juce::ComboBox*> (ch) != nullptr) continue;
        if (auto* v = dynamic_cast<wsui::ViewBase*> (ch)) { auto* o = new juce::DynamicObject(); o->setProperty ("name", "display"); o->setProperty ("kind", "display"); o->setProperty ("tip", ""); o->setProperty ("value", ""); o->setProperty ("box", toBox (v)); out.add (juce::var (o)); continue; }
        collect (root, ch, out);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui; juce::File out (argc > 1 ? argv[1] : "shots"); out.createDirectory();
    const int wpx = argc > 2 ? juce::String (argv[2]).getIntValue() : 1360; const int colour = argc > 3 ? juce::String (argv[3]).getIntValue() : 0;
    auto settings = WhiskerScopeProcessor::loadSettings(); (void) settings;
    WhiskerScopeProcessor proc; const double fs = 48000.0; proc.prepareToPlay (fs, 512);
    if (auto* b = proc.getBus (true, 1)) b->enable (true);
    proc.setParam ("color", (float) colour);
    std::vector<float> L, R, A, B; synth (L, R, fs, 6.0, 0); synth (A, B, fs, 6.0, 1, 140.0);
    for (auto& s : A) s *= 0.7f; for (auto& s : B) s *= 0.7f;
    size_t pos = 0; const size_t total = L.size();
    auto feed = [&] (double seconds)
    {
        const int blocks = (int) (seconds * fs / 512);
        for (int k = 0; k < blocks; ++k)
        {
            juce::AudioBuffer<float> buf (4, 512); juce::MidiBuffer midi;
            for (int i = 0; i < 512; ++i) { const size_t j = (pos + (size_t) i) % total; buf.setSample (0, i, L[j]); buf.setSample (1, i, R[j]); buf.setSample (2, i, A[j]); buf.setSample (3, i, B[j]); }
            proc.processBlock (buf, midi); pos += 512;
            if (k % 20 == 0) proc.runShapesTickNow (0.05);
        }
    };
    const char* names[4] = { "spec", "scope", "stereo", "shapes" };
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor()); auto* editor = static_cast<WhiskerScopeEditor*> (ed.get());
    editor->applyWidth (wpx);
    // reference for the Shapes page
    { auto f = proc.factoryShapes(); if (f.size() > 2) proc.useAsReference (f[1]); }
    for (int pg = 0; pg < 4; ++pg)
    {
        if (pg == 3) { proc.setParam ("shHear", 1.0f); proc.setParam ("shAuto", 1.0f); }
        proc.page = pg; proc.setShapesWatched (pg == 3);
        for (int i = 0; i < 30; ++i) { feed (0.1); proc.runShapesTickNow (0.05); editor->tickForScreenshot(); juce::MessageManager::getInstance()->runDispatchLoopUntil (5); }
        const auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        juce::File f = out.getChildFile (juce::String ("real_") + names[pg] + ".png"); f.deleteFile(); juce::FileOutputStream fo (f); juce::PNGImageFormat png; png.writeImageToStream (img, fo);
        { juce::Array<juce::var> items; collect (editor, editor, items); out.getChildFile (juce::String ("real_") + names[pg] + ".json").replaceWithText (juce::JSON::toString (juce::var (items))); }
        std::printf ("wrote %s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    }
    ed.reset(); return 0;
}
