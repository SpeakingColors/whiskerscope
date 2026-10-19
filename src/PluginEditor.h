#pragma once
#include "PluginProcessor.h"

class WhiskerScopeEditor : public juce::AudioProcessorEditor
{
public:
    explicit WhiskerScopeEditor (WhiskerScopeProcessor&);
    ~WhiskerScopeEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override {}
    static constexpr int W = 1200, H = 780;
    class Content;
private:
    WhiskerScopeProcessor& proc;
    std::unique_ptr<Content> content;
    std::unique_ptr<juce::TooltipWindow> tooltip;
    std::unique_ptr<juce::ResizableCornerComponent> corner;
    juce::ComponentBoundsConstrainer constrainer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WhiskerScopeEditor)
};
