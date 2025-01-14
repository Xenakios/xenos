#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <random>
#include "sst/basic-blocks/dsp/PanLaws.h"
#include "choc_SingleReaderSingleWriterFIFO.h"
#include "vintage_grain_engine.h"
#include "foleys_gui_magic/foleys_gui_magic.h"

namespace ParamIDs
{
static const juce::Identifier mainVolume{"MAINVOLUME"};
static const juce::Identifier mainDensity{"MAINDENSITY"};
static const juce::Identifier mainTranspose{"MAINTRANSPOSE"};
static const juce::Identifier mainGrainDur{"MAINDURATIONSCALE"};
static const juce::Identifier screenSelect{"SCREENSELECT"};
static const juce::Identifier screenChangeRate{"SCREENSELECTSPEED"};
static const juce::Identifier distortionAmount{"DISTORTIONAMOUNT"};
static const juce::Identifier grainPitchRandom0{"GRAINPITCHRANDOM0"};
static const juce::Identifier pitchLFO0Amount{"PITCHLFO0AMOUNT"};
static const juce::Identifier pitchLFO1Amount{"PITCHLFO1AMOUNT"};
static const juce::Identifier globalMinPitch{"MAINMINPITCH"};
static const juce::Identifier globalMaxPitch{"MAINMAXPITCH"};
static const juce::Identifier globalEnvelopeLen{"MAINENVELLEN"};
static const juce::Identifier dejavuSteps{"DEJAVUSTEPS"};
static const juce::Identifier dejavuPitch{"DEJAVUPITCH"};
static const juce::Identifier dejavuTime{"DEJAVUTIME"};
static const juce::Identifier dejavuPan{"DEJAVUPAN"};

} // namespace ParamIDs

using ParameterLayoutType = juce::AudioProcessorValueTreeState::ParameterLayout;

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
    void initialiseBuilder(foleys::MagicGUIBuilder &builder) override;

  private:
    ParameterLayoutType createParameters();
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VintageGranularAudioProcessor)
};
