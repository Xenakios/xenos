#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <random>
#include "sst/basic-blocks/dsp/PanLaws.h"
#include "choc_SingleReaderSingleWriterFIFO.h"
#include "vintage_grain_engine.h"
#include "foleys_gui_magic/foleys_gui_magic.h"

class VintageGranularAudioProcessor : public foleys::MagicProcessor
{
  public:
    //==============================================================================
    VintageGranularAudioProcessor();
    ~VintageGranularAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    // juce::AudioProcessorEditor *createEditor() override;
    // bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    //==============================================================================
    // void getStateInformation(juce::MemoryBlock &destData) override;
    // void setStateInformation(const void *data, int sizeInBytes) override;
    XenVintageGranular m_eng{9999};
    juce::AudioProcessLoadMeasurer m_cpu_load;
    juce::AudioProcessorValueTreeState m_apvts;
    void initialiseBuilder (foleys::MagicGUIBuilder& builder) override;
  private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VintageGranularAudioProcessor)
};
