#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "whiskerscope/Engine.h"
#include "Params.h"

// Whisker Scope. The audio goes through a short chain that is bit exact when nothing is switched on:
//   input gain -> [Mid/Side match EQ, 30 bells per channel, crossfaded] -> auto level + output gain -> safety limiter (only while the
//   match stage or the output gain is in use) -> out.
// Samples are copied to lock free rings for the displays, loudness and peaks are measured on the audio thread, and a message thread
// timer does the slow Shapes work (spectrum bands, capture, matching targets, EQ design).
class WhiskerScopeProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    WhiskerScopeProcessor();
    ~WhiskerScopeProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Whisker Scope"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    juce::AudioProcessorValueTreeState apvts;

    //==========================================================================
    // Read by the editor (message thread), written by the audio thread.
    ws::SampleRing outL, outR, inL, inR;           // after everything / after input gain, before the match EQ
    ws::SampleRingSmall scL, scR;                  // sidechain (layer B), after SC gain, never played
    std::atomic<float> peakOut[2] { { 0.0f }, { 0.0f } }, peakIn[2] { { 0.0f }, { 0.0f } };   // linear peak since the editor last took it
    ws::LoudnessMeter lufsIn, lufsPre, lufsOut;
    std::atomic<float> autoDb { 0.0f };
    std::atomic<bool> autoHeld { false };
    std::atomic<bool> scEnabled { false }, scSeen { false };
    std::atomic<float> hostBpm { 0.0f };
    std::atomic<double> hostPpq { -1.0 };           // quarter note position at the end of the last block, negative when the host does not say
    std::atomic<double> sampleRateNow { 48000.0 };
    std::atomic<bool> muted { false };            // Panic: silences the output until pressed again
    std::atomic<float> refLufsA { -999.0f };      // loudness stored with the reference, for auto level
    float takePeak (std::atomic<float>& a) { return a.exchange (0.0f); }

    //==========================================================================
    // Parameters by id.
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    int choiceIndex (const char* id) const { return (int) std::lround (param (id)); }
    const char* choiceRaw (const char* id) const;                    // raw option text, for example "lr" or "8192"
    double choiceNumber (const char* id) const { return juce::String (choiceRaw (id)).getDoubleValue(); }
    void setParam (const char* id, float plainValue);                // with a change gesture, for the host

    //==========================================================================
    // UI state kept with the plugin state
    int page = 0;                                                    // 0 spectrum, 1 scope, 2 stereo, 3 shapes
    juce::String presetName { "Init" };
    int editorWidth = 0;                                             // 0 = default

    //==========================================================================
    // Shapes (message thread only; the editor and the timer both run there)
    struct ShapeView
    {
        bool hasRef = false, hasMine = false, hasLive = false, hasRes = false, capturing = false, captureIsRef = false, hasRefShown = false, refFromFile = false;
        ws::Shape ref, mine;
        float refShown[2][ws::kBands] {};                            // reference as compared (tilt applied); valid when hasRefShown
        float live[2][ws::kBands] {}, res[2][ws::kBands] {}, eq[2][ws::kBands] {};   // [0] Mid, [1] Side
        int captureFrames = 0; double captureSeconds = 0;
    };
    const ShapeView& shapes() const { return view; }
    void setShapesWatched (bool on) { shapesWatched = on; }
    void startCapture (bool toReference);
    void stopCapture();                                              // keeps the result when there was enough sound (see takeNote)
    void cancelCapture();
    void useAsReference (const ws::Shape& s);
    void clearReference();
    void clearMine();
    void renameReference (const juce::String& n);
    void shapeParamsChanged() { matchDirty = true; }
    juce::String takeNote() { auto n = lastNote; lastNote.clear(); return n; }

    // Reference file: the average Mid/Side shape and the loudness of an audio file, analysed on a background thread, offline, never
    // uploaded. The result becomes the reference. Errors come back through takeNote().
    struct FileStatus { bool running = false; float progress = 0.0f; juce::String name; };
    bool startReferenceFile (const juce::File& f);                  // false while a capture or another file analysis is running
    void cancelReferenceFile();
    FileStatus fileStatus() const;
    static juce::String fileWildcard();                              // every format this build can decode, for the file chooser

    // Key detector on the Spectrum page (message thread). It listens to the input (Mid) while the Spectrum page is open and Key hold is off.
    const ws::KeyResult& keyResult() const { return keyRes; }
    void resetKey();
    void setKeyWatched (bool on) { keyWatched = on; }

    // shape library = reference presets (starting points made by us plus the user's files) and text exchange
    std::vector<ws::Shape> factoryShapes() const;
    std::vector<ws::Shape> userShapes() const;
    bool saveUserShape (const ws::Shape& s);
    bool deleteUserShape (const juce::String& name);
    bool renameUserShape (const juce::String& from, const juce::String& to, juce::String& whyNot);
    bool importShapeText (const juce::String& text, ws::Shape& saved, juce::String& whyNot);   // validated; saved under a free name
    bool isFactoryShapeName (const juce::String& name) const;
    static juce::String shapeToText (const ws::Shape& s);
    static bool shapeFromText (const juce::String& text, ws::Shape& out);
    static juce::String cleanName (const juce::String& n);          // letters, digits, space . ( ) + - only, 40 characters at most

    //==========================================================================
    // Presets: parameter values as JSON, factory list plus user files
    struct Preset { juce::String name; juce::var state; bool factory = false; };
    std::vector<Preset> allPresets() const;
    juce::var currentState() const;                                  // all parameters except colours, and the page
    void applyState (const juce::var& st);
    void loadPreset (const juce::String& name);
    bool saveUserPreset (const juce::String& name);                  // overwrites a user preset of the same name
    bool userPresetExists (const juce::String& name) const;
    bool deleteUserPreset (const juce::String& name);
    bool renameUserPreset (const juce::String& from, const juce::String& to);
    juce::String presetToText (const juce::String& name) const;
    bool importPresetText (const juce::String& text, juce::String& newName);
    juce::String presetBaseJson;                                     // state when the preset was loaded or saved, to show the star

    // folders and global settings (not part of the project)
    static juce::File dataDir();
    struct Settings { bool bubbles = true, eco = false; int warnDb = -12; double winScale = 0.0; };
    static Settings loadSettings();
    static void saveSettings (const Settings&);

    // for the offline tests and the screenshot tool: run the slow work once, now
    void runShapesTickNow (double dtSeconds) { shapesTick (dtSeconds); keyTick (dtSeconds); fileTick(); }

private:
    void timerCallback() override;
    void shapesTick (double dt);
    void keyTick (double dt);
    void fileTick();
    void recomputeMatch();
    void refreshRefShown();
    juce::File shapesDir() const { return dataDir().getChildFile ("shapes"); }
    juce::File presetsDir() const { return dataDir().getChildFile ("presets"); }

    // audio thread state
    static constexpr int kChunk = 8192;
    std::vector<float> tmpIn[2], tmpPre[2], tmpOut[2];
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inGainSm, outGainSm, autoGainSm, mixSm, muteSm;
    ws::MidSideEq eq; ws::AutoLevel autoLevel; ws::SafetyLimiter limiter;
    juce::SpinLock bellLock; float pendBells[2][ws::kBells] {}; std::atomic<unsigned> bellVersion { 1 }; unsigned bellSeen = 0; float audioBells[2][ws::kBells] {};
    int scQuiet = 1 << 30; double sr = 48000.0; bool eqWasOn = false;
    std::atomic<float>* pGain = nullptr; std::atomic<float>* pScGain = nullptr; std::atomic<float>* pOut = nullptr; std::atomic<float>* pHear = nullptr;

    // shapes (message thread; shapeLock protects what getStateInformation reads)
    mutable juce::CriticalSection shapeLock;
    ShapeView view; ws::Spectrum spM, spS, spRM, spRS; double analysedRate = 0;
    std::vector<float> bufA, bufB, bufM, bufS;
    double capAccM[ws::kBands] {}, capAccS[ws::kBands] {}, capLufsE = 0; int capFrames = 0, capLufsN = 0; double capStart = 0, capLimit = 0; bool capToRef = false, capturing = false;
    struct RefJob; std::unique_ptr<RefJob> refJob;
    ws::KeyDetector keyDet; ws::KeyResult keyRes; std::vector<float> keyBufA, keyBufB; double keyClock = 0; uint64_t keyLastWritten = 0; bool keyWatched = false;
    bool shapesWatched = false; double matchClock = 0, lastTickSeconds = 0; float matchKey[6] { -1, -1, -1, -1, -1, -1 }; bool matchDirty = true;
    juce::String lastNote;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WhiskerScopeProcessor)
};
