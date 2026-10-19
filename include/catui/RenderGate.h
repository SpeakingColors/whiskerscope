#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace catui
{
// RenderGate: drives meters, scopes, lights and animations, but only while
// the plugin window is visible and in front. Audio, automation and MIDI are
// never touched: they run on the audio thread, this only gates painting.
//
//   gate.onFrame = [this] { scope.repaint(); meters.repaint(); };
//   gate.start(30);   // frames per second while active
//
// Paused when: the component is not showing (closed, hidden tab, minimised)
// or, if pauseInBackground is true, the host/app is not the foreground app.
class RenderGate : private juce::Timer
{
public:
    explicit RenderGate(juce::Component& ownerToWatch) : owner(ownerToWatch) {}
    ~RenderGate() override { stopTimer(); }

    std::function<void()> onFrame;              // called once per frame while active
    std::function<void(bool active)> onChange;  // optional: active state changed

    void start(int framesPerSecond = 30) { fps = juce::jlimit(1, 120, framesPerSecond); startTimerHz(fps); }
    void stop() { stopTimer(); }
    void setPauseInBackground(bool b) { pauseInBackground = b; }
    bool isActive() const { return active; }

    // True when drawing is worth doing right now.
    bool shouldDraw() const
    {
        if (! owner.isShowing()) return false;
        if (auto* peer = owner.getPeer())
            if (peer->isMinimised()) return false;
        if (pauseInBackground && ! juce::Process::isForegroundProcess()) return false;
        return true;
    }

private:
    void timerCallback() override
    {
        const bool now = shouldDraw();
        if (now != active)
        {
            active = now;
            // While paused, poll slowly (4 Hz) so waking up is quick but cheap.
            startTimerHz(active ? fps : 4);
            if (onChange) onChange(active);
        }
        if (active && onFrame) onFrame();
    }

    juce::Component& owner;
    int fps = 30;
    bool active = false;
    bool pauseInBackground = true;
};
}
