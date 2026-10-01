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
    juce::Font getLabelFont (juce::Label&) override;

    float uiScale = 1.0f;

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
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::String name;
        juce::Rectangle<int> nameBounds;
        bool hero = false;
        std::unique_ptr<SliderAtt> att;
    };

    void setupKnob (Knob&, const juce::String& id, const juce::String& name, bool hero = false);
    void place (Knob&, juce::Rectangle<int>, float scale);

    TransientLockAudioProcessor& proc;
    TLLookAndFeel laf;

    Knob kInput, kThreshold, kRatio, kAttack, kRelease, kKnee, kHpf, kSens,
         kMakeup, kMix, kOutput, kProtection, kBody;
    std::vector<Knob*> knobs;

    juce::ToggleButton bypass { "BYPASS" };
    std::unique_ptr<ButtonAtt> bypassAtt;

    juce::ComboBox presetBox, osBox;
    std::unique_ptr<ComboAtt> osAtt;

    MeterPanel meters;
    juce::ComponentBoundsConstrainer constrainer;

    juce::Rectangle<int> heroBounds, titleBounds, osCaptionBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransientLockAudioProcessorEditor)
};
