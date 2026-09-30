#pragma once

#include "PluginProcessor.h"

//==============================================================================
class TLLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TLLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    static inline const juce::Colour accent { 0xff4fc3f7 };
    static inline const juce::Colour hot    { 0xffff8a3d };
    static inline const juce::Colour bg     { 0xff14171d };
    static inline const juce::Colour panel  { 0xff1b1f27 };
};

//==============================================================================
class MeterPanel : public juce::Component, private juce::Timer
{
public:
    explicit MeterPanel (TransientLockAudioProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    TransientLockAudioProcessor& proc;
    float body = 0.0f, applied = 0.0f, trans = 0.0f;
};

//==============================================================================
class TransientLockAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit TransientLockAudioProcessorEditor (TransientLockAudioProcessor&);
    ~TransientLockAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAtt> att;
    };

    void setupKnob (Knob&, const juce::String& id, const juce::String& name, bool hero = false);
    static void place (Knob&, juce::Rectangle<int>);

    TransientLockAudioProcessor& proc;
    TLLookAndFeel laf;

    Knob kInput, kThreshold, kRatio, kAttack, kRelease, kKnee,
         kMakeup, kMix, kOutput, kProtection, kBody;

    juce::ToggleButton bypass { "BYPASS" };
    std::unique_ptr<ButtonAtt> bypassAtt;
    MeterPanel meters;
    juce::Rectangle<int> heroBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransientLockAudioProcessorEditor)
};
