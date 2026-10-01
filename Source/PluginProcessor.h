#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>
#include "TransientLockDSP.h"

namespace ids
{
    inline constexpr const char* input      = "input";
    inline constexpr const char* threshold  = "threshold";
    inline constexpr const char* ratio      = "ratio";
    inline constexpr const char* attack     = "attack";
    inline constexpr const char* release    = "release";
    inline constexpr const char* knee       = "knee";
    inline constexpr const char* makeup     = "makeup";
    inline constexpr const char* mix        = "mix";
    inline constexpr const char* output     = "output";
    inline constexpr const char* protection = "protection";
    inline constexpr const char* body       = "body";
    inline constexpr const char* hpf        = "hpf";
    inline constexpr const char* sens       = "sens";
    inline constexpr const char* os         = "os";
    inline constexpr const char* bypass     = "bypass";
}

class TransientLockAudioProcessor : public juce::AudioProcessor
{
public:
    TransientLockAudioProcessor();
    ~TransientLockAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Presets de fabrica (expuestos como "programas")
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    juce::AudioProcessorParameter* getBypassParameter() const override
    {
        return apvts.getParameter (ids::bypass);
    }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> meterBody { 0.0f }, meterApplied { 0.0f }, meterTransient { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void updateLatency();

    using Oversampler = juce::dsp::Oversampling<float>;

    std::array<tl::Engine, 3> engines;          // 0 = off, 1 = 2x, 2 = 4x
    std::unique_ptr<Oversampler> os2, os4;
    int currentOs = 0;
    int maxBlock  = 512;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransientLockAudioProcessor)
};
