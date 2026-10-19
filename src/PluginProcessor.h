#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "whiskerscope/Engine.h"

// Whisker Scope: the audio passes through untouched. Samples are copied (after the display gain) to lock free rings for the
// editor, and the meters run on the audio thread.
class WhiskerScopeProcessor : public juce::AudioProcessor
{
public:
    WhiskerScopeProcessor();
    ~WhiskerScopeProcessor() override = default;

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

    // read by the editor
    ws::SampleRing ringL, ringR;
    struct Live
    {
        std::atomic<float> peakL { 0 }, peakR { 0 }, peakM { 0 }, peakS { 0 }, lufs { -200 }, corr { 0 }, width { 0 }, balance { 0 }, monoLoss { 0 }, rmsL { 0 }, rmsR { 0 }, rmsM { 0 }, rmsS { 0 }, bpm { 0 };
        std::atomic<bool> hasSignal { false };
    } live;
    std::atomic<double> sampleRateNow { 48000.0 };
    void resetPeaks() { resetPeakFlag.store (true); }

    // shapes, shared by the editor (message thread only)
    ws::Shape refShape, mineShape; bool hasRef = false, hasMine = false;
    int page = 1;               // 0 scope, 1 spectrum, 2 stereo, 3 shapes
    int editorWidth = 1200;
    bool tooltips = true;
    static juce::File shapeDir();

private:
    ws::StereoMeter stereo; ws::Loudness loud; std::atomic<bool> resetPeakFlag { false };
    float pkL = 0, pkR = 0, pkM = 0, pkS = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WhiskerScopeProcessor)
};
