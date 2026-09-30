#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TransientLockAudioProcessor::createLayout()
{
    using APF  = juce::AudioParameterFloat;
    using Attr = juce::AudioParameterFloatAttributes;
    using PID  = juce::ParameterID;
    using Range = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto add = [&] (const char* id, const char* name, Range r, float def, const char* unit)
    {
        layout.add (std::make_unique<APF> (PID { id, 1 }, name, r, def, Attr().withLabel (unit)));
    };

    add (ids::input,      "Input",               Range (-24.0f, 24.0f, 0.1f),        0.0f,   "dB");
    add (ids::threshold,  "Threshold",           Range (-60.0f, 0.0f, 0.1f),         -18.0f, "dB");
    layout.add (std::make_unique<APF> (PID { ids::ratio, 1 }, "Ratio",
                Range (1.0f, 20.0f, 0.1f, 0.5f), 4.0f,
                Attr().withStringFromValueFunction ([] (float v, int)
                { return juce::String (v, 1) + ":1"; })));
    add (ids::attack,     "Attack",              Range (0.1f, 100.0f, 0.01f, 0.35f), 10.0f,  "ms");
    add (ids::release,    "Release",             Range (10.0f, 1000.0f, 0.1f, 0.4f), 150.0f, "ms");
    add (ids::knee,       "Knee",                Range (0.0f, 24.0f, 0.1f),          6.0f,   "dB");
    add (ids::makeup,     "Makeup",              Range (0.0f, 24.0f, 0.1f),          0.0f,   "dB");
    add (ids::mix,        "Mix",                 Range (0.0f, 100.0f, 0.1f),         100.0f, "%");
    add (ids::output,     "Output",              Range (-24.0f, 24.0f, 0.1f),        0.0f,   "dB");
    add (ids::protection, "Transient Protection",Range (0.0f, 100.0f, 0.1f),         60.0f,  "%");
    add (ids::body,       "Body Compression",    Range (0.0f, 100.0f, 0.1f),         100.0f, "%");

    layout.add (std::make_unique<juce::AudioParameterBool> (PID { ids::bypass, 1 }, "Bypass", false));
    return layout;
}

//==============================================================================
TransientLockAudioProcessor::TransientLockAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

void TransientLockAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (0);   // separación en dominio de ganancia: latencia cero
}

bool TransientLockAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void TransientLockAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    tl::Params p;
    p.inputDb     = get (ids::input);
    p.thresholdDb = get (ids::threshold);
    p.ratio       = get (ids::ratio);
    p.attackMs    = get (ids::attack);
    p.releaseMs   = get (ids::release);
    p.kneeDb      = get (ids::knee);
    p.makeupDb    = get (ids::makeup);
    p.mix         = get (ids::mix) * 0.01f;
    p.outputDb    = get (ids::output);
    p.protection  = get (ids::protection) * 0.01f;
    p.bodyAmount  = get (ids::body) * 0.01f;
    p.bypass      = get (ids::bypass) > 0.5f;

    const auto m = engine.process (buffer, p);

    meterBody.store (m.bodyGrDb);
    meterApplied.store (m.appliedGrDb);
    meterTransient.store (m.transient);
}

//==============================================================================
juce::AudioProcessorEditor* TransientLockAudioProcessor::createEditor()
{
    return new TransientLockAudioProcessorEditor (*this);
}

void TransientLockAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TransientLockAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TransientLockAudioProcessor();
}
