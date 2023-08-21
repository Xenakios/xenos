#include "PluginProcessor.h"
#include "PluginEditor.h"

using jnr = juce::NormalisableRange<float>;

inline void createAndAddFloatParameter(juce::AudioProcessorValueTreeState::ParameterLayout &layout,
                                       const juce::Identifier &paramid, juce::String displayname,
                                       float minval, float maxval, float step, float defaultval)
{
    auto fpar = std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(paramid.toString(), 1), displayname, jnr(minval, maxval, step),
        defaultval);
    layout.add(std::move(fpar));
}

juce::AudioProcessorValueTreeState::ParameterLayout
VintageGranularAudioProcessor::createParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    createAndAddFloatParameter(layout, ParamIDs::mainVolume, "Main volume", -48.0, 0.0, 0.5, -24.0);
    createAndAddFloatParameter(layout, ParamIDs::mainDensity, "Density scaling", 0.5, 2.0, 0.01,
                               1.0);
    createAndAddFloatParameter(layout, ParamIDs::mainTranspose, "Global transpose", -24.0, 24.0,
                               0.5, 0.0);
    createAndAddFloatParameter(layout, ParamIDs::mainGrainDur, "Grain duration scaling", 0.1, 2.0,
                               0.01, 1.0);
    return layout;
}

//==============================================================================
VintageGranularAudioProcessor::VintageGranularAudioProcessor()
    : foleys::MagicProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                                 .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                                 .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                                 ),
      m_apvts(*this, nullptr, "STATE", createParameters())
{
    FOLEYS_SET_SOURCE_PATH(__FILE__);
}

VintageGranularAudioProcessor::~VintageGranularAudioProcessor() {}

void VintageGranularAudioProcessor::initialiseBuilder(foleys::MagicGUIBuilder &builder)
{
    builder.registerJUCEFactories();
    builder.registerJUCELookAndFeels();

    builder.registerFactory("GrainVisualizer", &GrainVisualizerItem::factory);
}

//==============================================================================
const juce::String VintageGranularAudioProcessor::getName() const { return JucePlugin_Name; }

bool VintageGranularAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool VintageGranularAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool VintageGranularAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double VintageGranularAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int VintageGranularAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
              // so this should be at least 1, even if you're not really implementing programs.
}

int VintageGranularAudioProcessor::getCurrentProgram() { return 0; }

void VintageGranularAudioProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }

const juce::String VintageGranularAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void VintageGranularAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void VintageGranularAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    m_cpu_load.reset(sampleRate, samplesPerBlock);
    m_eng.setSampleRate(sampleRate);
}

void VintageGranularAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool VintageGranularAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

        // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}

void VintageGranularAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                                 juce::MidiBuffer &midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::AudioProcessLoadMeasurer::ScopedTimer measure(m_cpu_load, buffer.getNumSamples());
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    auto bufs = buffer.getArrayOfWritePointers();
    float mainvol = *m_apvts.getRawParameterValue(ParamIDs::mainVolume);
    float gainscaler = juce::Decibels::decibelsToGain(mainvol);
    float maindensity = *m_apvts.getRawParameterValue(ParamIDs::mainDensity);
    m_eng.setDensityScaling(maindensity);
    float gtranspose = *m_apvts.getRawParameterValue(ParamIDs::mainTranspose);
    m_eng.setGlobalTranspose(gtranspose);
    float gdur = *m_apvts.getRawParameterValue(ParamIDs::mainGrainDur);
    m_eng.setDurationScaling(gdur);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float outframe[2];
        m_eng.process(outframe);
        bufs[0][i] = outframe[0] * gainscaler;
        bufs[1][i] = outframe[1] * gainscaler;
    }
}

//==============================================================================
/*
bool VintageGranularAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}
*/
/*
juce::AudioProcessorEditor *VintageGranularAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor(*this);
}
*/
/*
void VintageGranularAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused(destData);
}
*/
/*
void VintageGranularAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused(data, sizeInBytes);
}
*/

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new VintageGranularAudioProcessor();
}
