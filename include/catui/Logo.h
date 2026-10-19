#pragma once
#include <juce_graphics/juce_graphics.h>

namespace catui
{
// The cat head logo (same shape as assets/icon.svg), drawn as vector art.
// Fits inside `area`, keeps its proportions.
inline void drawCatHead(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour fill, juce::Colour features = juce::Colour(0xff111111))
{
    juce::Path head;
    head.startNewSubPath(30, 52); head.lineTo(38, 28); head.lineTo(56, 44); head.lineTo(72, 44);
    head.lineTo(90, 28); head.lineTo(98, 52);
    head.quadraticTo(104, 68, 92, 86); head.quadraticTo(80, 100, 64, 100);
    head.quadraticTo(48, 100, 36, 86); head.quadraticTo(24, 68, 30, 52); head.closeSubPath();
    const auto box = juce::Rectangle<float>(26, 26, 80, 76);
    auto t = juce::RectanglePlacement(juce::RectanglePlacement::centred).getTransformToFit(box, area);
    g.setColour(fill); g.fillPath(head, t);
    g.setColour(features);
    for (float x : { 52.f, 76.f }) { juce::Path e; e.addEllipse(x - 6, 60, 12, 12); g.fillPath(e, t); }
    juce::Path m; m.startNewSubPath(58, 80); m.quadraticTo(64, 86, 70, 80);
    g.strokePath(m, juce::PathStrokeType(3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
}
}
