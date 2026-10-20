#pragma once
#include "PluginProcessor.h"

namespace wsui { class Content; }

// The window. All drawing happens in a fixed 1360 x 920 design space (the approved mockup) that is scaled to the window size, so the
// shape never changes. Resizable from all four sides and corners, double-click a wooden cheek for the default size.
class WhiskerScopeEditor : public juce::AudioProcessorEditor
{
public:
    explicit WhiskerScopeEditor (WhiskerScopeProcessor&);
    ~WhiskerScopeEditor() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }
    void resized() override;

    static constexpr int kDesignW = 1360, kDesignH = 920;
    void applyWidth (int w);                    // keeps the shape, limits 25 % to 250 %
    void resetSize (bool exactly100);           // default: 100 % when it fits on the screen, else the biggest that fits
    double scaleNow() const { return (double) getWidth() / kDesignW; }
    void saveScale();
    void tickForScreenshot();                   // runs one frame of the display timer (used by tools/shoot.cpp)

private:
    struct Handle;
    WhiskerScopeProcessor& proc;
    std::unique_ptr<wsui::Content> content;
    std::vector<std::unique_ptr<Handle>> handles;
    int defaultWidth() const;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WhiskerScopeEditor)
};
