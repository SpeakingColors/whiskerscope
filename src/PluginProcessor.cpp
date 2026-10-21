#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

using APF = juce::AudioParameterFloat; using APC = juce::AudioParameterChoice; using APB = juce::AudioParameterBool;

//==============================================================================
// Parameters: one table (Params.h, from the approved mockup) drives the layout, the presets and the editor.
juce::AudioProcessorValueTreeState::ParameterLayout WhiskerScopeProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (auto& c : wsp::choices())
    {
        juce::StringArray items; for (auto* s : c.labels) items.add (s);
        l.add (std::make_unique<APC> (juce::ParameterID { c.id, 1 }, c.name, items, c.def));
    }
    for (auto& n : wsp::nums())
    {
        juce::NormalisableRange<float> r (n.lo, n.hi, n.step > 0 ? n.step : 0.0f);
        if (n.logScale)
            r = juce::NormalisableRange<float> (n.lo, n.hi, [] (float s, float e, float p) { return s * std::pow (e / s, p); },
                                                [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
                                                [] (float s, float e, float v) { return juce::jlimit (s, e, v); });
        const char* fmt = n.fmt;
        l.add (std::make_unique<APF> (juce::ParameterID { n.id, 1 }, n.name, r, n.def,
                                      juce::AudioParameterFloatAttributes().withStringFromValueFunction ([fmt] (float v, int) { return juce::String (wsp::formatNum (fmt, v)); })));
    }
    for (auto& f : wsp::flags()) l.add (std::make_unique<APB> (juce::ParameterID { f.id, 1 }, f.name, f.def));
    return l;
}

WhiskerScopeProcessor::WhiskerScopeProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", layout())
{
    pGain = apvts.getRawParameterValue ("gain"); pScGain = apvts.getRawParameterValue ("scGain"); pOut = apvts.getRawParameterValue ("shOut"); pHear = apvts.getRawParameterValue ("shHear");
    for (int c = 0; c < 2; ++c) { tmpIn[c].assign (kChunk, 0.0f); tmpPre[c].assign (kChunk, 0.0f); tmpOut[c].assign (kChunk, 0.0f); }
    lufsIn.prepare (48000); lufsPre.prepare (48000); lufsOut.prepare (48000);
    presetBaseJson = juce::JSON::toString (currentState(), true);
    startTimerHz (20);
}


const char* WhiskerScopeProcessor::choiceRaw (const char* id) const
{
    for (auto& c : wsp::choices()) if (std::strcmp (c.id, id) == 0) return c.raws[(size_t) juce::jlimit (0, (int) c.raws.size() - 1, choiceIndex (id))];
    return "";
}

void WhiskerScopeProcessor::setParam (const char* id, float v)
{
    if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); }
}

//==============================================================================
void WhiskerScopeProcessor::prepareToPlay (double rate, int)
{
    sr = rate; sampleRateNow.store (rate);
    lufsIn.prepare (rate); lufsPre.prepare (rate); lufsOut.prepare (rate);
    const float ig = juce::Decibels::decibelsToGain (pGain->load()), og = juce::Decibels::decibelsToGain (pOut->load());
    inGainSm.reset (rate, 0.015); inGainSm.setCurrentAndTargetValue (ig);
    outGainSm.reset (rate, 0.015); outGainSm.setCurrentAndTargetValue (og);
    autoGainSm.reset (rate, 0.05); autoGainSm.setCurrentAndTargetValue (1.0f);
    mixSm.reset (rate, 0.03); mixSm.setCurrentAndTargetValue (pHear->load() > 0.5f ? 1.0f : 0.0f);
    muteSm.reset (rate, 0.01); muteSm.setCurrentAndTargetValue (muted.load() ? 0.0f : 1.0f);
    limiter.prepare (rate); autoLevel.reset(); autoDb.store (0.0f); eq.reset(); bellSeen = 0; eqWasOn = false;
    bellVersion.fetch_add (1);      // design the bells again for the new rate
}

bool WhiskerScopeProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != out || (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono())) return false;
    if (l.inputBuses.size() > 1)
    {
        const auto sc = l.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void WhiskerScopeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int total = buffer.getNumSamples();
    auto mainBuf = getBusBuffer (buffer, false, 0);
    const int nch = mainBuf.getNumChannels();
    if (nch < 1 || total < 1) return;
    const bool scOn = getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled() && getBus (true, 1)->getNumberOfChannels() > 0;
    scEnabled.store (scOn, std::memory_order_relaxed);
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) hostBpm.store ((float) *bpm, std::memory_order_relaxed);
            if (auto ppq = pos->getPpqPosition()) hostPpq.store (*ppq + (double) total / sr * (pos->getBpm().hasValue() ? *pos->getBpm() / 60.0 : 2.0), std::memory_order_relaxed); else hostPpq.store (-1.0, std::memory_order_relaxed);
        }
    }

    // new bells from the message thread (try lock: the audio thread never waits)
    const unsigned v = bellVersion.load (std::memory_order_acquire);
    if (v != bellSeen && bellLock.tryEnter())
    {
        std::memcpy (audioBells, pendBells, sizeof audioBells); bellLock.exit();
        eq.design (audioBells[0], audioBells[1], sr); bellSeen = v;
    }

    inGainSm.setTargetValue (juce::Decibels::decibelsToGain (pGain->load())); outGainSm.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    mixSm.setTargetValue (pHear->load() > 0.5f ? 1.0f : 0.0f); muteSm.setTargetValue (muted.load (std::memory_order_relaxed) ? 0.0f : 1.0f);
    const float scGainLin = juce::Decibels::decibelsToGain (pScGain->load());
    float prePeak = 0;

    for (int start = 0; start < total; start += kChunk)
    {
        const int len = std::min (kChunk, total - start);
        float* c0 = mainBuf.getWritePointer (0) + start; float* c1 = nch > 1 ? mainBuf.getWritePointer (1) + start : nullptr;
        const bool inUnity = ! inGainSm.isSmoothing() && inGainSm.getTargetValue() == 1.0f;
        const bool outUnity = ! outGainSm.isSmoothing() && ! autoGainSm.isSmoothing() && outGainSm.getTargetValue() == 1.0f && autoGainSm.getTargetValue() == 1.0f;
        const bool muteUnity = ! muteSm.isSmoothing() && muteSm.getTargetValue() == 1.0f;
        const bool eqRun = mixSm.isSmoothing() || mixSm.getTargetValue() > 0.0f;
        const bool limOn = eqRun || ! outUnity;
        if (! eqRun && eqWasOn) eq.reset();
        eqWasOn = eqRun;
        float* iL = tmpIn[0].data(); float* iR = tmpIn[1].data(); float* pL = tmpPre[0].data(); float* pR = tmpPre[1].data(); float* oL = tmpOut[0].data(); float* oR = tmpOut[1].data();
        float pkI0 = 0, pkI1 = 0, pkO0 = 0, pkO1 = 0;
        for (int i = 0; i < len; ++i)
        {
            float l = c0[i], r = c1 ? c1[i] : l;
            if (! inUnity) { const float g = inGainSm.getNextValue(); l *= g; r *= g; }
            iL[i] = l; iR[i] = r; inL.push (l); inR.push (r); pkI0 = std::max (pkI0, std::fabs (l)); pkI1 = std::max (pkI1, std::fabs (r));
            if (eqRun)
            {
                float wl = l, wr = r; if (c1) eq.process (wl, wr); else { eq.processMono (wl); wr = wl; }
                const float m = mixSm.getNextValue(); l += m * (wl - l); r += m * (wr - r);
            }
            pL[i] = l; pR[i] = r; prePeak = std::max (prePeak, std::max (std::fabs (l), std::fabs (r)));
            if (! outUnity) { const float g = outGainSm.getNextValue() * autoGainSm.getNextValue(); l *= g; r *= g; }
            if (limOn) { if (c1) limiter.process (l, r); else { limiter.processMono (l); r = l; } } else limiter.gr = 1.0f;
            if (! muteUnity) { const float g = muteSm.getNextValue(); l *= g; r *= g; }
            c0[i] = l; if (c1) c1[i] = r;
            oL[i] = l; oR[i] = r; outL.push (l); outR.push (r); pkO0 = std::max (pkO0, std::fabs (l)); pkO1 = std::max (pkO1, std::fabs (r));
        }
        lufsIn.process (iL, iR, len); lufsPre.process (pL, pR, len); lufsOut.process (oL, oR, len);
        auto up = [] (std::atomic<float>& a, float x) { if (x > a.load (std::memory_order_relaxed)) a.store (x, std::memory_order_relaxed); };
        up (peakIn[0], pkI0); up (peakIn[1], pkI1); up (peakOut[0], pkO0); up (peakOut[1], pkO1);

        if (scOn)
        {
            auto sc = getBusBuffer (buffer, true, 1); const int nsc = sc.getNumChannels(); float spk = 0;
            if (nsc > 0)
                for (int i = 0; i < len; ++i)
                {
                    const float a = sc.getSample (0, start + i) * scGainLin, b = nsc > 1 ? sc.getSample (1, start + i) * scGainLin : a;
                    scL.push (a); scR.push (b); spk = std::max (spk, std::max (std::fabs (a), std::fabs (b)));
                }
            if (spk > 1.0e-5f) scQuiet = 0; else scQuiet = std::min (scQuiet + len, 1 << 30);
        }
    }
    if (! scOn) scQuiet = 1 << 30;
    scSeen.store (scQuiet < (int) (0.5 * sr), std::memory_order_relaxed);

    // auto level for the next block
    const float dt = (float) (total / sr);
    autoLevel.update (dt, choiceIndex ("shAuto"), lufsPre.shortTerm(), lufsIn.shortTerm(), refLufsA.load (std::memory_order_relaxed), pOut->load(), ws::dbOf (prePeak));
    autoGainSm.setTargetValue (juce::Decibels::decibelsToGain (autoLevel.db)); autoDb.store (autoLevel.db, std::memory_order_relaxed); autoHeld.store (autoLevel.held, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* WhiskerScopeProcessor::createEditor() { return new WhiskerScopeEditor (*this); }

//==============================================================================
// Shapes: slow work on the message thread
void WhiskerScopeProcessor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    const double dt = lastTickSeconds > 0 ? juce::jlimit (0.01, 0.5, now - lastTickSeconds) : 0.05; lastTickSeconds = now; shapesTick (dt);
    keyTick (dt); fileTick();
}

//==============================================================================
// Key detector: about ten updates a second from the newest 16384 input samples (Mid), only while the Spectrum page is open.
void WhiskerScopeProcessor::resetKey() { keyDet.reset(); keyRes = ws::KeyResult(); keyClock = 0; keyLastWritten = inL.written(); }

void WhiskerScopeProcessor::keyTick (double dt)
{
    if (! keyWatched || param ("keyHold") > 0.5f) { keyLastWritten = inL.written(); keyClock = 0; return; }
    keyClock += dt; if (keyClock < 0.1) return;
    const double fs = sampleRateNow.load(); const uint64_t w = inL.written();
    if (w < keyLastWritten + (uint64_t) (0.05 * fs)) { if (keyClock > 1.0) keyClock = 0; return; }     // no new audio (transport stopped): learn nothing
    const int N = ws::KeyDetector::kFftSize; keyBufA.resize ((size_t) N); keyBufB.resize ((size_t) N);
    inL.latest (keyBufA.data(), N); inR.latest (keyBufB.data(), N);
    for (int i = 0; i < N; ++i) keyBufA[(size_t) i] = 0.5f * (keyBufA[(size_t) i] + keyBufB[(size_t) i]);
    keyDet.process (keyBufA.data(), fs, keyClock); keyRes = keyDet.result(); keyClock = 0; keyLastWritten = w;
}

//==============================================================================
// Reference file analysis on a background thread
struct WhiskerScopeProcessor::RefJob : public juce::Thread
{
    explicit RefJob (const juce::File& f) : juce::Thread ("Whisker Scope reference file"), file (f) {}
    juce::File file;
    std::atomic<float> progress { 0.0f }; std::atomic<bool> finished { false }, cancelled { false };
    bool ok = false; juce::String error, info; ws::Shape shape;           // written before finished is set

    void fail (const juce::String& e) { error = e; finished.store (true); }
    void run() override
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (file));
        if (rd == nullptr) { fail ("That file could not be read. It may be damaged, or in a format this system cannot decode. wav, aiff, flac and ogg always work."); return; }
        std::string why; if (! ws::audioInfoOk (rd->sampleRate, (int) rd->numChannels, (double) rd->lengthInSamples, why)) { fail (juce::String (why)); return; }
        const int nch = (int) rd->numChannels; const int use = juce::jmin (2, nch); const juce::int64 total = rd->lengthInSamples; const int block = 65536;
        ws::ShapeAccumulator acc; acc.prepare (rd->sampleRate); ws::LoudnessMeter lm; lm.prepare (rd->sampleRate);
        juce::AudioBuffer<float> buf (use, block); juce::int64 pos = 0;
        while (pos < total)
        {
            if (threadShouldExit() || cancelled.load()) { finished.store (true); return; }
            const int n = (int) juce::jmin ((juce::int64) block, total - pos);
            if (! rd->read (&buf, 0, n, pos, true, use > 1)) { fail ("That file could not be read to the end. It may be damaged."); return; }
            const float* l = buf.getReadPointer (0); const float* r = use > 1 ? buf.getReadPointer (1) : nullptr;
            acc.push (l, r, n); lm.process (l, r, n); pos += n; progress.store ((float) ((double) pos / (double) total));
        }
        if (! acc.finish (shape)) { fail ("Not enough sound in that file to build a shape. It looks silent or very quiet."); return; }
        const float lu = lm.integrated(); if (lu > -90.0f) shape.lufs = lu;
        shape.name = cleanName (file.getFileNameWithoutExtension()).isEmpty() ? std::string ("Reference file") : cleanName (file.getFileNameWithoutExtension()).toStdString();
        if (nch == 1) info = "That file is mono, so its Side shape is empty. Matching still works on Mid.";
        ok = true; finished.store (true);
    }
};

WhiskerScopeProcessor::~WhiskerScopeProcessor()
{
    stopTimer();
    if (refJob != nullptr) { refJob->cancelled.store (true); refJob->signalThreadShouldExit(); refJob->stopThread (5000); }
}

juce::String WhiskerScopeProcessor::fileWildcard()
{
    juce::AudioFormatManager fm; fm.registerBasicFormats(); return fm.getWildcardForAllFormats();
}

bool WhiskerScopeProcessor::startReferenceFile (const juce::File& f)
{
    if (refJob != nullptr || view.capturing) return false;
    if (! f.existsAsFile()) { lastNote = "That file does not exist any more."; return true; }
    refJob = std::make_unique<RefJob> (f); refJob->startThread (juce::Thread::Priority::background); return true;
}

void WhiskerScopeProcessor::cancelReferenceFile() { if (refJob != nullptr) { refJob->cancelled.store (true); refJob->signalThreadShouldExit(); } }

WhiskerScopeProcessor::FileStatus WhiskerScopeProcessor::fileStatus() const
{
    FileStatus s; if (refJob == nullptr) return s;
    s.running = true; s.progress = refJob->progress.load(); s.name = refJob->file.getFileNameWithoutExtension(); return s;
}

void WhiskerScopeProcessor::fileTick()
{
    if (refJob == nullptr || ! refJob->finished.load()) return;
    refJob->stopThread (2000);
    if (! refJob->cancelled.load())
    {
        if (refJob->ok) { useAsReference (refJob->shape); view.refFromFile = true; lastNote = refJob->info; }
        else lastNote = refJob->error;
    }
    refJob.reset();
}

void WhiskerScopeProcessor::shapesTick (double dt)
{
    const double fs = sampleRateNow.load(); const int N = 16384;
    const float key[6] = { param ("shStrength"), param ("shLimit"), (float) choiceNumber ("shSmooth"), param ("shRefOn"), (float) choiceNumber ("shRefTilt"), 0.0f };
    for (int i = 0; i < 5; ++i) if (key[i] != matchKey[i]) { matchKey[i] = key[i]; matchDirty = true; }
    if (shapesWatched || capturing)
    {
        if (fs != analysedRate)
        {
            spM.prepare (fs, N); spS.prepare (fs, N); spRM.prepare (fs, N); spRS.prepare (fs, N);
            bufA.assign (N, 0.0f); bufB.assign (N, 0.0f); bufM.assign (N, 0.0f); bufS.assign (N, 0.0f); analysedRate = fs;
        }
        inL.latest (bufA.data(), N); inR.latest (bufB.data(), N);
        for (int i = 0; i < N; ++i) { bufM[(size_t) i] = 0.5f * (bufA[(size_t) i] + bufB[(size_t) i]); bufS[(size_t) i] = 0.5f * (bufA[(size_t) i] - bufB[(size_t) i]); }
        const float keep = std::pow (0.8f, (float) (dt * 60.0));
        spM.analyse (bufM.data(), keep); spS.analyse (bufS.data(), keep);
        float bm[ws::kBands], bs[ws::kBands]; ws::spectrumToBands (spM.spectrum().data(), spM.numBins(), fs, bm); ws::spectrumToBands (spS.spectrum().data(), spS.numBins(), fs, bs);
        float lvl = -999.0f; for (int k = 6; k < 58; ++k) lvl = std::max (lvl, bm[k]);
        const juce::ScopedLock sl (shapeLock);
        // thresholds: the mockup used -95 and -75 on the browser analyser scale; this analyser reads 13.5 dB higher (0 dB = full scale sine)
        if (lvl > -81.5f)
        {
            const float blend = 1.0f - std::pow (0.85f, (float) (dt * 60.0));
            for (int k = 0; k < ws::kBands; ++k) { view.live[0][k] = view.hasLive ? view.live[0][k] * (1 - blend) + bm[k] * blend : bm[k]; view.live[1][k] = view.hasLive ? view.live[1][k] * (1 - blend) + bs[k] * blend : bs[k]; }
            view.hasLive = true;
        }
        if (capturing)
        {
            if (lvl > -62.0f)
            {
                for (int k = 0; k < ws::kBands; ++k) { capAccM[k] += std::pow (10.0, bm[k] / 10.0); capAccS[k] += std::pow (10.0, bs[k] / 10.0); }
                ++capFrames; const float lm = lufsIn.momentary(); if (lm > -70.0f) { capLufsE += std::pow (10.0, (lm + 0.691) / 10.0); ++capLufsN; }
            }
            const double el = juce::Time::getMillisecondCounterHiRes() / 1000.0 - capStart; view.captureFrames = capFrames; view.captureSeconds = el;
            if (capLimit > 0 && el > capLimit) stopCapture();
        }
        view.hasRes = false;
        if (pHear->load() > 0.5f && lvl > -81.5f)
        {
            outL.latest (bufA.data(), N); outR.latest (bufB.data(), N);
            for (int i = 0; i < N; ++i) { bufM[(size_t) i] = 0.5f * (bufA[(size_t) i] + bufB[(size_t) i]); bufS[(size_t) i] = 0.5f * (bufA[(size_t) i] - bufB[(size_t) i]); }
            spRM.analyse (bufM.data(), keep); spRS.analyse (bufS.data(), keep);
            ws::spectrumToBands (spRM.spectrum().data(), spRM.numBins(), fs, view.res[0]); ws::spectrumToBands (spRS.spectrum().data(), spRS.numBins(), fs, view.res[1]); view.hasRes = true;
        }
    }
    matchClock += dt;
    if (matchDirty || (shapesWatched && ! view.hasMine && matchClock >= 0.25)) { matchClock = 0; recomputeMatch(); }
}

void WhiskerScopeProcessor::refreshRefShown()
{
    view.hasRefShown = view.hasRef && param ("shRefOn") > 0.5f;
    if (! view.hasRefShown) return;
    const float tilt = (float) choiceNumber ("shRefTilt");
    ws::tiltBands (view.ref.m.data(), tilt, view.refShown[0]); ws::tiltBands (view.ref.s.data(), tilt, view.refShown[1]);
}

void WhiskerScopeProcessor::recomputeMatch()
{
    matchDirty = false;
    const juce::ScopedLock sl (shapeLock);
    refreshRefShown();
    const int smooth = (int) choiceNumber ("shSmooth"); const float strength = param ("shStrength"), limit = param ("shLimit"); const double fs = sampleRateNow.load();
    float bells[2][ws::kBells];
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* mine = view.hasMine ? (ch == 0 ? view.mine.m.data() : view.mine.s.data()) : (view.hasLive ? view.live[ch] : nullptr);
        if (view.hasRefShown && mine) ws::matchTargets (view.refShown[ch], mine, smooth, strength, limit, view.eq[ch]);
        else std::fill (view.eq[ch], view.eq[ch] + ws::kBands, 0.0f);
        ws::designBells (view.eq[ch], fs, bells[ch]);
    }
    { const juce::SpinLock::ScopedLockType sp (bellLock); std::memcpy (pendBells, bells, sizeof bells); }
    bellVersion.fetch_add (1, std::memory_order_release);
    refLufsA.store (view.hasRef && view.ref.hasLufs() ? view.ref.lufs : -999.0f);
}

void WhiskerScopeProcessor::startCapture (bool toRef)
{
    const juce::ScopedLock sl (shapeLock);
    capturing = true; capToRef = toRef; capFrames = 0; capLufsE = 0; capLufsN = 0; capStart = juce::Time::getMillisecondCounterHiRes() / 1000.0; capLimit = choiceNumber ("shLen");
    std::fill (capAccM, capAccM + ws::kBands, 0.0); std::fill (capAccS, capAccS + ws::kBands, 0.0);
    view.capturing = true; view.captureIsRef = toRef; view.captureFrames = 0; view.captureSeconds = 0;
}

void WhiskerScopeProcessor::cancelCapture()
{
    const juce::ScopedLock sl (shapeLock); capturing = false; view.capturing = false;
}

void WhiskerScopeProcessor::stopCapture()
{
    const juce::ScopedLock sl (shapeLock);
    if (! capturing) return;
    capturing = false; view.capturing = false;
    if (capFrames < 10) { lastNote = "Not enough sound to capture. Let the song or lead play for a few seconds, then stop."; return; }
    ws::Shape s; s.name = capToRef ? "Captured reference" : "Mine";
    for (int k = 0; k < ws::kBands; ++k) { s.m[(size_t) k] = (float) (10.0 * std::log10 (capAccM[k] / capFrames + 1e-20)); s.s[(size_t) k] = (float) (10.0 * std::log10 (capAccS[k] / capFrames + 1e-20)); }
    if (capLufsN > 0) { const double lu = -0.691 + 10.0 * std::log10 (capLufsE / capLufsN); if (lu > -90.0) s.lufs = (float) lu; }
    if (capToRef) { view.ref = s; view.hasRef = true; view.refFromFile = false; } else { view.mine = s; view.hasMine = true; }
    matchDirty = true;
}

void WhiskerScopeProcessor::useAsReference (const ws::Shape& s) { { const juce::ScopedLock sl (shapeLock); view.ref = s; view.hasRef = true; view.refFromFile = false; } matchDirty = true; }
void WhiskerScopeProcessor::clearReference() { { const juce::ScopedLock sl (shapeLock); view.hasRef = false; } matchDirty = true; }
void WhiskerScopeProcessor::clearMine() { { const juce::ScopedLock sl (shapeLock); view.hasMine = false; } matchDirty = true; }
void WhiskerScopeProcessor::renameReference (const juce::String& n) { const juce::ScopedLock sl (shapeLock); if (view.hasRef) view.ref.name = cleanName (n).toStdString(); }

//==============================================================================
// Shape library and text
juce::String WhiskerScopeProcessor::cleanName (const juce::String& n)
{
    juce::String o; for (auto c : n) if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '_' || c == ' ' || c == '.' || c == '(' || c == ')' || c == '+' || c == '-') o << juce::String::charToString (c);
    o = o.trim().substring (0, 40).trim();
    return o.containsOnly (".") ? juce::String() : o;
}

juce::String WhiskerScopeProcessor::shapeToText (const ws::Shape& s)
{
    auto* o = new juce::DynamicObject(); auto rd = [] (const std::array<float, ws::kBands>& a) { juce::Array<juce::var> v; for (float x : a) v.add (std::round (x * 10.0f) / 10.0f); return juce::var (v); };
    o->setProperty ("v", 1); o->setProperty ("app", "Whisker Scope"); o->setProperty ("name", juce::String (s.name)); o->setProperty ("n", ws::kBands); o->setProperty ("m", rd (s.m)); o->setProperty ("s", rd (s.s));
    if (s.hasLufs()) o->setProperty ("lufs", std::round (s.lufs * 10.0f) / 10.0f);
    return juce::JSON::toString (juce::var (o), true);
}

bool WhiskerScopeProcessor::shapeFromText (const juce::String& text, ws::Shape& out)
{
    if (text.length() > 20000) return false;
    const auto v = juce::JSON::parse (text); auto* o = v.getDynamicObject(); if (o == nullptr) return false;
    if ((int) o->getProperty ("n") != ws::kBands) return false;
    auto read = [] (const juce::var& a, std::vector<double>& d) { auto* arr = a.getArray(); if (arr == nullptr) return false; for (auto& x : *arr) { if (! (x.isDouble() || x.isInt() || x.isInt64())) return false; d.push_back ((double) x); } return ws::shapeValuesOk (d); };
    std::vector<double> m, s; if (! read (o->getProperty ("m"), m) || ! read (o->getProperty ("s"), s)) return false;
    ws::Shape sh; for (int k = 0; k < ws::kBands; ++k) { sh.m[(size_t) k] = (float) m[(size_t) k]; sh.s[(size_t) k] = (float) s[(size_t) k]; }
    auto nm = cleanName (o->getProperty ("name").toString()); sh.name = (nm.isEmpty() ? "Imported shape" : nm).toStdString();
    const auto lv = o->getProperty ("lufs"); if ((lv.isDouble() || lv.isInt()) && (double) lv > -90.0 && (double) lv < 5.0) sh.lufs = (float) (double) lv;
    out = sh; return true;
}

std::vector<ws::Shape> WhiskerScopeProcessor::factoryShapes() const
{
    struct Bump { double f, w, g; }; struct Q { const char* name; double tilt; std::vector<Bump> b; };
    static const std::vector<Q> list = {
        { "Gentle tilt (-3 dB/oct)", -3, {} }, { "Bright lead", -2, { { 3000, 1.2, 5 }, { 9000, 1, 4 }, { 60, 1, -6 } } }, { "Dark warm pad", -5, { { 250, 1, 3 }, { 8000, 1, -6 } } },
        { "Kick and bass focus", -4.5, { { 55, 0.7, 8 }, { 110, 0.8, 3 }, { 2500, 1.2, -4 } } }, { "Trance supersaw", -3.5, { { 1200, 1.2, 3 }, { 5000, 1.3, 3 }, { 40, 1, -8 } } },
        { "Vocal presence", -4, { { 200, 1, 2 }, { 3000, 1, 4 }, { 100, 0.8, -6 } } } };
    std::vector<ws::Shape> out;
    for (auto& q : list)
    {
        ws::Shape s; s.name = q.name;
        for (int k = 0; k < ws::kBands; ++k)
        {
            const double f = ws::bandCentreHz (k); double v = q.tilt * std::log2 (f / 1000.0);
            for (auto& b : q.b) { const double d = std::log2 (f / b.f) / b.w; v += b.g * std::exp (-d * d); }
            s.m[(size_t) k] = (float) v; s.s[(size_t) k] = (float) (v - (f < 150 ? 18 : f < 500 ? 8 : 3));
        }
        out.push_back (s);
    }
    return out;
}

std::vector<ws::Shape> WhiskerScopeProcessor::userShapes() const
{
    std::vector<ws::Shape> out; auto files = shapesDir().findChildFiles (juce::File::findFiles, false, "*.json"); files.sort();
    for (auto& f : files) { ws::Shape s; if (f.getSize() < 20000 && shapeFromText (f.loadFileAsString(), s)) out.push_back (s); }
    return out;
}

bool WhiskerScopeProcessor::saveUserShape (const ws::Shape& s)
{
    const auto n = cleanName (juce::String (s.name)); if (n.isEmpty()) return false;
    auto d = shapesDir(); if (! d.createDirectory().wasOk()) return false;
    ws::Shape c = s; c.name = n.toStdString(); return d.getChildFile (n + ".json").replaceWithText (shapeToText (c));
}

bool WhiskerScopeProcessor::deleteUserShape (const juce::String& name)
{
    const auto n = cleanName (name); if (n.isEmpty()) return false; return shapesDir().getChildFile (n + ".json").deleteFile();
}

bool WhiskerScopeProcessor::isFactoryShapeName (const juce::String& name) const
{
    for (auto& s : factoryShapes()) if (juce::String (s.name).equalsIgnoreCase (name)) return true;
    return false;
}

bool WhiskerScopeProcessor::renameUserShape (const juce::String& from, const juce::String& to, juce::String& whyNot)
{
    const auto f = cleanName (from), t = cleanName (to);
    if (f.isEmpty() || t.isEmpty()) { whyNot = "That name is not allowed. Use letters, digits, spaces and . ( ) + - only."; return false; }
    if (f == t) return true;
    const bool caseOnly = f.equalsIgnoreCase (t); const auto dst = shapesDir().getChildFile (t + ".json"), src = shapesDir().getChildFile (f + ".json");
    if (isFactoryShapeName (t) || (! caseOnly && dst.existsAsFile())) { whyNot = "A reference preset with that name exists already."; return false; }
    ws::Shape s; if (! src.existsAsFile() || src.getSize() > 20000 || ! shapeFromText (src.loadFileAsString(), s)) { whyNot = "That preset could not be read."; return false; }
    s.name = t.toStdString();
    if (caseOnly) src.deleteFile();
    if (! saveUserShape (s)) { whyNot = "The preset could not be saved. Check that your user folder is writable."; if (caseOnly) { s.name = f.toStdString(); saveUserShape (s); } return false; }
    if (! caseOnly) src.deleteFile();
    return true;
}

bool WhiskerScopeProcessor::importShapeText (const juce::String& text, ws::Shape& saved, juce::String& whyNot)
{
    ws::Shape s; if (! shapeFromText (text, s)) { whyNot = "That does not look like a reference preset. Nothing was imported."; return false; }
    const juce::String base = cleanName (juce::String (s.name)).isEmpty() ? juce::String ("Imported shape") : cleanName (juce::String (s.name)); juce::String n = base; int i = 2;
    while (isFactoryShapeName (n) || shapesDir().getChildFile (n + ".json").existsAsFile()) n = base.substring (0, 36).trim() + " " + juce::String (i++);
    s.name = n.toStdString(); if (! saveUserShape (s)) { whyNot = "The preset could not be saved. Check that your user folder is writable."; return false; }
    saved = s; return true;
}

//==============================================================================
// Presets
juce::File WhiskerScopeProcessor::dataDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Studio 87").getChildFile ("Whisker Scope");
}

static const char* const kViews[4] = { "spec", "scope", "stereo", "shapes" };

juce::var WhiskerScopeProcessor::currentState() const
{
    auto* o = new juce::DynamicObject();
    for (auto& c : wsp::choices())
    {
        if (std::strcmp (c.id, "color") == 0) continue;
        const juce::String raw (c.raws[(size_t) juce::jlimit (0, (int) c.raws.size() - 1, choiceIndex (c.id))]);
        o->setProperty (c.id, c.numeric ? juce::var (raw.getDoubleValue()) : juce::var (raw));
    }
    for (auto& n : wsp::nums()) o->setProperty (n.id, std::round (param (n.id) * 10000.0f) / 10000.0f);
    for (auto& f : wsp::flags()) o->setProperty (f.id, param (f.id) > 0.5f);
    o->setProperty ("view", kViews[juce::jlimit (0, 3, page)]);
    return juce::var (o);
}

void WhiskerScopeProcessor::applyState (const juce::var& st)
{
    auto* o = st.getDynamicObject();
    for (auto& c : wsp::choices())
    {
        if (std::strcmp (c.id, "color") == 0) continue;
        int idx = c.def;
        if (o != nullptr && o->hasProperty (c.id))
        {
            const auto v = o->getProperty (c.id); const bool okType = c.numeric ? (v.isDouble() || v.isInt() || v.isInt64()) : v.isString();
            if (okType) for (size_t i = 0; i < c.raws.size(); ++i)
                if (c.numeric ? std::fabs (juce::String (c.raws[i]).getDoubleValue() - (double) v) < 1e-6 : juce::String (c.raws[i]) == v.toString()) { idx = (int) i; break; }
        }
        setParam (c.id, (float) idx);
    }
    for (auto& n : wsp::nums())
    {
        float val = n.def; if (o != nullptr && o->hasProperty (n.id)) { const auto v = o->getProperty (n.id); if ((v.isDouble() || v.isInt() || v.isInt64()) && std::isfinite ((double) v)) val = juce::jlimit (n.lo, n.hi, (float) (double) v); }
        setParam (n.id, val);
    }
    for (auto& f : wsp::flags())
    {
        bool val = f.def; if (o != nullptr && o->hasProperty (f.id) && o->getProperty (f.id).isBool()) val = (bool) o->getProperty (f.id);
        setParam (f.id, val ? 1.0f : 0.0f);
    }
    int pg = 0; if (o != nullptr && o->hasProperty ("view")) for (int i = 0; i < 4; ++i) if (o->getProperty ("view").toString() == kViews[i]) pg = i;
    page = pg;
    matchDirty = true;
}

static const char* const kFactory[][2] = {
    { "Init", "{}" },
    { "Kick and bass low end", "{\"view\":\"spec\",\"ana\":\"mono\",\"fft\":16384,\"range\":90,\"scale\":\"log\"}" },
    { "Mix balance, mid and side", "{\"view\":\"spec\",\"ana\":\"ms\",\"slope\":4.5,\"bands\":\"3\",\"fft\":8192}" },
    { "Lead width check", "{\"view\":\"stereo\",\"pers\":0.7}" },
    { "Phase and mono check", "{\"view\":\"stereo\",\"pers\":0.5}" },
    { "Acid line scope", "{\"view\":\"scope\",\"sync\":\"trig\",\"dur\":40,\"chan\":\"mono\"}" },
    { "Waterfall sweep", "{\"view\":\"spec\",\"wf\":true,\"fft\":4096,\"speed\":\"fast\"}" },
    { "Shape match", "{\"view\":\"shapes\"}" } };

std::vector<WhiskerScopeProcessor::Preset> WhiskerScopeProcessor::allPresets() const
{
    std::vector<Preset> out;
    for (auto& f : kFactory) out.push_back ({ f[0], juce::JSON::parse (juce::String (f[1])), true });
    auto files = presetsDir().findChildFiles (juce::File::findFiles, false, "*.json"); files.sort();
    for (auto& f : files)
    {
        if (f.getSize() > 20000) continue; const auto v = juce::JSON::parse (f.loadFileAsString()); auto* o = v.getDynamicObject(); if (o == nullptr) continue;
        auto nm = cleanName (o->getProperty ("name").toString()); if (nm.isEmpty() || ! o->getProperty ("state").getDynamicObject()) continue;
        out.push_back ({ nm, o->getProperty ("state"), false });
    }
    return out;
}

void WhiskerScopeProcessor::loadPreset (const juce::String& name)
{
    for (auto& p : allPresets()) if (p.name == name) { applyState (p.state); presetName = name; presetBaseJson = juce::JSON::toString (currentState(), true); return; }
}

static juce::String presetFileText (const juce::String& name, const juce::var& state)
{
    auto* o = new juce::DynamicObject(); o->setProperty ("v", 1); o->setProperty ("app", "Whisker Scope"); o->setProperty ("name", name); o->setProperty ("state", state);
    return juce::JSON::toString (juce::var (o), true);
}

bool WhiskerScopeProcessor::saveUserPreset (const juce::String& nameIn)
{
    auto name = cleanName (nameIn); if (name.isEmpty()) return false;
    for (auto& f : kFactory) if (name == f[0]) name << " (mine)";
    auto d = presetsDir(); if (! d.createDirectory().wasOk()) return false;
    if (! d.getChildFile (name + ".json").replaceWithText (presetFileText (name, currentState()))) return false;
    presetName = name; presetBaseJson = juce::JSON::toString (currentState(), true); return true;
}

bool WhiskerScopeProcessor::userPresetExists (const juce::String& name) const { const auto n = cleanName (name); return n.isNotEmpty() && presetsDir().getChildFile (n + ".json").existsAsFile(); }

bool WhiskerScopeProcessor::deleteUserPreset (const juce::String& name)
{
    const auto n = cleanName (name); if (n.isEmpty() || ! presetsDir().getChildFile (n + ".json").deleteFile()) return false;
    if (presetName == n) presetName = "Init"; return true;
}

bool WhiskerScopeProcessor::renameUserPreset (const juce::String& from, const juce::String& to)
{
    const auto a = cleanName (from), b = cleanName (to); if (a.isEmpty() || b.isEmpty() || userPresetExists (b)) return false;
    for (auto& f : kFactory) if (b == f[0]) return false;
    auto src = presetsDir().getChildFile (a + ".json"); if (! src.existsAsFile()) return false;
    const auto v = juce::JSON::parse (src.loadFileAsString()); auto* o = v.getDynamicObject(); if (o == nullptr) return false;
    if (! presetsDir().getChildFile (b + ".json").replaceWithText (presetFileText (b, o->getProperty ("state")))) return false;
    src.deleteFile(); if (presetName == a) presetName = b; return true;
}

juce::String WhiskerScopeProcessor::presetToText (const juce::String& name) const
{
    for (auto& p : allPresets()) if (p.name == name) return presetFileText (p.name, p.state);
    return presetFileText (name, currentState());
}

bool WhiskerScopeProcessor::importPresetText (const juce::String& text, juce::String& newName)
{
    if (text.length() > 20000) return false;
    const auto v = juce::JSON::parse (text); auto* o = v.getDynamicObject(); if (o == nullptr || o->getProperty ("state").getDynamicObject() == nullptr) return false;
    auto n = cleanName (o->getProperty ("name").toString()); if (n.isEmpty()) n = "Imported";
    for (auto& f : kFactory) if (n == f[0]) n << " (mine)";
    auto d = presetsDir(); if (! d.createDirectory().wasOk()) return false;
    // stored through the same validation as loading: unknown keys and wrong types fall back to defaults
    if (! d.getChildFile (n + ".json").replaceWithText (presetFileText (n, o->getProperty ("state")))) return false;
    newName = n; return true;
}

//==============================================================================
// Global settings (info bubbles, power saving, meter warning level, window size). Not part of the project.
WhiskerScopeProcessor::Settings WhiskerScopeProcessor::loadSettings()
{
    Settings s; const auto v = juce::JSON::parse (dataDir().getChildFile ("settings.json").loadFileAsString()); auto* o = v.getDynamicObject(); if (o == nullptr) return s;
    if (o->getProperty ("bubbles").isBool()) s.bubbles = (bool) o->getProperty ("bubbles");
    if (o->getProperty ("eco").isBool()) s.eco = (bool) o->getProperty ("eco");
    const int w = (int) o->getProperty ("warn"); if (w == -18 || w == -12 || w == -9 || w == -6) s.warnDb = w;
    const double sc = (double) o->getProperty ("win"); if (std::isfinite (sc) && sc >= 0.25 && sc <= 2.5) s.winScale = sc;
    return s;
}

void WhiskerScopeProcessor::saveSettings (const Settings& s)
{
    auto* o = new juce::DynamicObject(); o->setProperty ("bubbles", s.bubbles); o->setProperty ("eco", s.eco); o->setProperty ("warn", s.warnDb); o->setProperty ("win", s.winScale);
    auto d = dataDir(); if (d.createDirectory().wasOk()) d.getChildFile ("settings.json").replaceWithText (juce::JSON::toString (juce::var (o), true));
}

//==============================================================================
void WhiskerScopeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto xml = apvts.copyState().createXml(); if (xml == nullptr) return;
    xml->setAttribute ("page", page); xml->setAttribute ("width", editorWidth); xml->setAttribute ("preset", presetName);
    { const juce::ScopedLock sl (shapeLock);
      if (view.hasRef) xml->setAttribute ("ref", shapeToText (view.ref));
      if (view.hasMine) xml->setAttribute ("mine", shapeToText (view.mine)); }
    copyXmlToBinary (*xml, dest);
}

void WhiskerScopeProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size); if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    page = juce::jlimit (0, 3, xml->getIntAttribute ("page", 0)); editorWidth = juce::jlimit (0, 4000, xml->getIntAttribute ("width", 0));
    presetName = cleanName (xml->getStringAttribute ("preset", "Init")); if (presetName.isEmpty()) presetName = "Init";
    { const juce::ScopedLock sl (shapeLock);
      ws::Shape s; view.hasRef = xml->hasAttribute ("ref") && shapeFromText (xml->getStringAttribute ("ref"), s); if (view.hasRef) view.ref = s;
      view.hasMine = xml->hasAttribute ("mine") && shapeFromText (xml->getStringAttribute ("mine"), s); if (view.hasMine) view.mine = s; }
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    presetBaseJson = juce::JSON::toString (currentState(), true); matchDirty = true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WhiskerScopeProcessor(); }
