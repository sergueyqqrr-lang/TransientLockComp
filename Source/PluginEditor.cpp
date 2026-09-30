#include "PluginEditor.h"

//==============================================================================
TLLookAndFeel::TLLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.85f));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
}

void TLLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                      float pos, float startAngle, float endAngle, juce::Slider& s)
{
    const bool hero = (bool) s.getProperties()["hero"];
    const auto col  = hero ? hot : accent;

    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre  = bounds.getCentre();
    const float angle  = startAngle + pos * (endAngle - startAngle);
    const float thick  = hero ? 6.0f : 4.0f;
    const float arcR   = radius - thick;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff2a2f3a));
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (col);
    g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float bodyR = arcR - thick - 2.0f;
    g.setColour (panel.brighter (0.15f));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -bodyR + 3.0f, 3.0f, bodyR * 0.45f, 1.5f);
    g.setColour (juce::Colours::white);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void TLLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? hot : panel.brighter (0.1f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? juce::Colours::black : juce::Colours::white.withAlpha (0.75f));
    g.setFont (13.0f);
    g.drawText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred);
}

//==============================================================================
MeterPanel::MeterPanel (TransientLockAudioProcessor& p) : proc (p)
{
    startTimerHz (30);
}

void MeterPanel::timerCallback()
{
    auto follow = [] (float& disp, float target, float fall)
    {
        disp = target > disp ? target : std::max (target, disp - fall);
    };
    follow (body,    proc.meterBody.load(),      0.8f);
    follow (applied, proc.meterApplied.load(),   0.8f);
    follow (trans,   proc.meterTransient.load(), 0.06f);
    repaint();
}

void MeterPanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (TLLookAndFeel::panel);
    g.fillRoundedRectangle (b, 8.0f);

    auto inner = b.reduced (10.0f);
    auto labelsArea = inner.removeFromBottom (18.0f);
    const float colW = inner.getWidth() / 3.0f;
    const char* names[] = { "BODY", "APPLIED", "TRANS" };
    const float values[] = { body, applied, trans };

    for (int i = 0; i < 3; ++i)
    {
        auto col = juce::Rectangle<float> (inner.getX() + (float) i * colW, inner.getY(),
                                           colW, inner.getHeight()).reduced (9.0f, 0.0f);
        g.setColour (juce::Colour (0xff0e1015));
        g.fillRoundedRectangle (col, 4.0f);

        if (i < 2)   // GR: crece desde arriba, escala 0..24 dB
        {
            const float frac = juce::jlimit (0.0f, 1.0f, values[i] / 24.0f);
            g.setColour (i == 0 ? TLLookAndFeel::accent : TLLookAndFeel::hot);
            g.fillRoundedRectangle (col.withHeight (juce::jmax (0.0f, col.getHeight() * frac)), 4.0f);
        }
        else         // actividad transiente: crece desde abajo
        {
            const float frac = juce::jlimit (0.0f, 1.0f, values[i]);
            g.setColour (juce::Colour (0xff7CFFB2));
            g.fillRoundedRectangle (col.withTrimmedTop (col.getHeight() * (1.0f - frac)), 4.0f);
        }

        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.setFont (11.0f);
        g.drawText (names[i],
                    juce::Rectangle<float> (inner.getX() + (float) i * colW, labelsArea.getY(), colW, labelsArea.getHeight()).toNearestInt(),
                    juce::Justification::centred);
    }
}

//==============================================================================
TransientLockAudioProcessorEditor::TransientLockAudioProcessorEditor (TransientLockAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), meters (p)
{
    setLookAndFeel (&laf);

    setupKnob (kInput,      ids::input,      "INPUT");
    setupKnob (kThreshold,  ids::threshold,  "THRESHOLD");
    setupKnob (kRatio,      ids::ratio,      "RATIO");
    setupKnob (kAttack,     ids::attack,     "ATTACK");
    setupKnob (kRelease,    ids::release,    "RELEASE");
    setupKnob (kKnee,       ids::knee,       "KNEE");
    setupKnob (kMakeup,     ids::makeup,     "MAKEUP");
    setupKnob (kMix,        ids::mix,        "MIX");
    setupKnob (kOutput,     ids::output,     "OUTPUT");
    setupKnob (kProtection, ids::protection, "TRANSIENT PROTECTION", true);
    setupKnob (kBody,       ids::body,       "BODY COMPRESSION");

    addAndMakeVisible (bypass);
    bypassAtt = std::make_unique<ButtonAtt> (proc.apvts, ids::bypass, bypass);
    addAndMakeVisible (meters);

    setSize (840, 470);
}

TransientLockAudioProcessorEditor::~TransientLockAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void TransientLockAudioProcessorEditor::setupKnob (Knob& k, const juce::String& id,
                                                   const juce::String& name, bool hero)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, hero ? 100 : 72, 20);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                  juce::MathConstants<float>::pi * 2.8f, true);
    k.slider.getProperties().set ("hero", hero);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
}

void TransientLockAudioProcessorEditor::place (Knob& k, juce::Rectangle<int> r)
{
    k.label.setBounds (r.removeFromBottom (18));
    k.slider.setBounds (r);
}

void TransientLockAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (TLLookAndFeel::bg);

    g.setColour (juce::Colours::white);
    g.setFont (22.0f);
    g.drawText ("TRANSIENTLOCK", 20, 14, 200, 30, juce::Justification::centredLeft);
    g.setColour (TLLookAndFeel::hot);
    g.drawText ("COMP", 178, 14, 80, 30, juce::Justification::centredLeft);

    g.setColour (TLLookAndFeel::panel);
    g.fillRoundedRectangle (heroBounds.toFloat(), 10.0f);
    g.setColour (TLLookAndFeel::hot.withAlpha (0.55f));
    g.drawRoundedRectangle (heroBounds.toFloat().reduced (0.5f), 10.0f, 1.5f);
}

void TransientLockAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto header = area.removeFromTop (40);
    bypass.setBounds (header.removeFromRight (90).reduced (0, 5));
    area.removeFromTop (8);

    meters.setBounds (area.removeFromRight (150));
    area.removeFromRight (12);

    auto top = area.removeFromTop ((int) (area.getHeight() * 0.55f));
    area.removeFromTop (8);
    auto bottom = area;

    const int unit = top.getWidth() / 5;
    place (kThreshold, top.removeFromLeft (unit));
    place (kRatio,     top.removeFromLeft (unit));
    heroBounds = top.removeFromLeft (unit * 2);
    place (kProtection, heroBounds.reduced (8));
    place (kBody, top);

    const int u = bottom.getWidth() / 7;
    Knob* row[] = { &kInput, &kAttack, &kRelease, &kKnee, &kMakeup, &kMix, &kOutput };
    for (auto* k : row)
        place (*k, bottom.removeFromLeft (u));
}
