#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat; using APC = juce::AudioParameterChoice; using APB = juce::AudioParameterBool;

juce::AudioProcessorValueTreeState::ParameterLayout WhiskerScopeProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto f = [&] (const char* id, const char* name, float lo, float hi, float def, bool logLike = false)
    { juce::NormalisableRange<float> r (lo, hi); if (logLike) r.setSkewForCentre (std::sqrt (lo * hi)); l.add (std::make_unique<APF> (juce::ParameterID { id, 1 }, name, r, def)); };
    auto c = [&] (const char* id, const char* name, juce::StringArray items, int def) { l.add (std::make_unique<APC> (juce::ParameterID { id, 1 }, name, items, def)); };
    auto b = [&] (const char* id, const char* name, bool def) { l.add (std::make_unique<APB> (juce::ParameterID { id, 1 }, name, def)); };
    f ("gain", "Display gain (dB)", -24.0f, 24.0f, 0.0f);
    c ("scopeChan", "Scope channels", { "L / R", "Left", "Right", "Mono", "Mid / Side" }, 0);
    c ("sync", "Scope sync", { "Free", "Trigger", "Pitch", "Beat (host tempo)" }, 1);
    f ("scopeTime", "Scope time (ms)", 1.0f, 400.0f, 20.0f, true);
    c ("specChan", "Spectrum channels", { "Mono", "Left", "Right", "Side", "L / R", "Mid / Side" }, 0);
    c ("fft", "FFT size", { "2048", "4096", "8192", "16384", "32768" }, 2);
    c ("speed", "Spectrum speed", { "Slow", "Medium", "Fast" }, 1);
    c ("range", "Spectrum range", { "60 dB", "90 dB", "120 dB" }, 1);
    f ("tilt", "Spectrum slope (dB/oct)", 0.0f, 6.0f, 0.0f);
    c ("smooth", "Spectrum smoothing", { "Off", "1/6 octave", "1/3 octave" }, 0);
    b ("linear", "Linear frequency scale", false); b ("fill", "Fill", true); b ("hold", "Peak hold", true); b ("freeze", "Freeze", false);
    f ("zoom", "Goniometer zoom", 0.5f, 4.0f, 1.0f);
    f ("strength", "Shape strength", 0.0f, 1.0f, 0.6f);
    f ("maxMove", "Shape max move (dB)", 3.0f, 18.0f, 9.0f);
    return l;
}

WhiskerScopeProcessor::WhiskerScopeProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", layout()) {}

juce::File WhiskerScopeProcessor::shapeDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Studio 87").getChildFile ("Whisker Scope").getChildFile ("shapes");
}

void WhiskerScopeProcessor::prepareToPlay (double sr, int)
{
    sampleRateNow.store (sr); stereo.prepare (sr, 300.0f); loud.prepare (sr);
}

bool WhiskerScopeProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void WhiskerScopeProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples(), ch = buf.getNumChannels();
    if (ch < 1 || n < 1) return;
    const float gain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("gain")->load());
    const float* l = buf.getReadPointer (0); const float* r = buf.getReadPointer (ch > 1 ? 1 : 0);
    if (resetPeakFlag.exchange (false)) pkL = pkR = pkM = pkS = 0;
    bool any = false;
    for (int i = 0; i < n; ++i)
    {
        const float a = l[i] * gain, b = r[i] * gain;
        ringL.push (a); ringR.push (b); stereo.tick (a, b); loud.tick (a, b);
        pkL = juce::jmax (pkL, std::fabs (a)); pkR = juce::jmax (pkR, std::fabs (b)); pkM = juce::jmax (pkM, std::fabs (0.5f * (a + b))); pkS = juce::jmax (pkS, std::fabs (0.5f * (a - b)));
        if (std::fabs (a) > 1.0e-5f || std::fabs (b) > 1.0e-5f) any = true;
    }
    const auto s = stereo.get();
    live.peakL = pkL; live.peakR = pkR; live.peakM = pkM; live.peakS = pkS; live.lufs = loud.lufs(); live.corr = s.correlation; live.width = s.width; live.balance = s.balance; live.monoLoss = s.monoLossDb;
    live.rmsL = s.rmsL; live.rmsR = s.rmsR; live.rmsM = s.rmsM; live.rmsS = s.rmsS; live.hasSignal = any;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto bpm = pos->getBpm()) live.bpm = (float) *bpm;
}

juce::AudioProcessorEditor* WhiskerScopeProcessor::createEditor() { return new WhiskerScopeEditor (*this); }

void WhiskerScopeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto xml = apvts.copyState().createXml(); if (xml == nullptr) return;
    xml->setAttribute ("page", page); xml->setAttribute ("width", editorWidth); xml->setAttribute ("tooltips", tooltips);
    if (hasRef) xml->setAttribute ("ref", juce::String (ws::shapeToText (refShape)));
    if (hasMine) xml->setAttribute ("mine", juce::String (ws::shapeToText (mineShape)));
    copyXmlToBinary (*xml, dest);
}

void WhiskerScopeProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size); if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    page = juce::jlimit (0, 3, xml->getIntAttribute ("page", 1)); editorWidth = juce::jlimit (600, 2600, xml->getIntAttribute ("width", 1200)); tooltips = xml->getBoolAttribute ("tooltips", true);
    hasRef = xml->hasAttribute ("ref") && ws::shapeFromText (xml->getStringAttribute ("ref").toStdString(), refShape);
    hasMine = xml->hasAttribute ("mine") && ws::shapeFromText (xml->getStringAttribute ("mine").toStdString(), mineShape);
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WhiskerScopeProcessor(); }
