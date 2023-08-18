#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      )
{
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor() {}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const { return JucePlugin_Name; }

bool AudioPluginAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
              // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram() { return 0; }

void AudioPluginAudioProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }

const juce::String AudioPluginAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void AudioPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
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

void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                             juce::MidiBuffer &midiMessages)
{
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    auto bufs = buffer.getArrayOfWritePointers();
    float gainscaler = juce::Decibels::decibelsToGain(-96.0);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float outframe[2];
        m_geng.processSample(outframe);
        bufs[0][i] = outframe[0] * gainscaler;
        bufs[1][i] = outframe[1] * gainscaler;
    }
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor(*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused(destData);
}

void AudioPluginAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused(data, sizeInBytes);
}

void XenGranularEngine::generateScreen()
{
    double minpitch = 24.0;
    double maxpitch = 115.0;
    double pitchrange = maxpitch - minpitch;
    double pitchregionrange = pitchrange / 16.0;
    std::uniform_real_distribution<double> unidist{0.0, 1.0};
    std::uniform_int_distribution<int> screendist{0, 7};
    std::normal_distribution<double> pitchdist{0.0, 1.0};
    const char **screendata = nullptr;
    int screentouse = screendist(m_rng);
    if (screentouse == 0)
        screendata = grainScreenA;
    if (screentouse == 1)
        screendata = grainScreenB;
    if (screentouse == 2)
        screendata = grainScreenC;
    if (screentouse == 3)
        screendata = grainScreenD;
    if (screentouse == 4)
        screendata = grainScreenE;
    if (screentouse == 5)
        screendata = grainScreenF;
    if (screentouse == 6)
        screendata = grainScreenG;
    if (screentouse == 7)
        screendata = grainScreenH;
    m_cur_screen = screentouse;
    //spinlock.enter();
    grains_to_play.clear();
    for (int i = 0; i < 16; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            float density = screendata[3 - j][i] - 65;
            if (density > 0.0f)
            {
                density = std::pow(M_E, density);
                int numgrains = std::round(density * m_screen_dur);
                for (int k = 0; k < numgrains; ++k)
                {
                    float pitch = juce::jmap<float>(i + pitchdist(m_rng) * 0.5, 0.0, 16.0, minpitch,
                                                    maxpitch);
                    float hz = 440.0 * std::pow(2.0, 1.0 / 12.0 * (pitch - 69.0));
                    float graindur = juce::jmap<float>(pitch, minpitch, maxpitch, 0.2, 0.025);
                    int graindursamples = graindur * m_sr;
                    float tpos = unidist(m_rng) * (m_screen_dur - graindur);
                    int tpossamples = tpos * m_sr;
                    float volume = juce::jmap<float>(j + unidist(m_rng), 0.0, 4.0, -40.0, -6.0);
                    float gain = juce::Decibels::decibelsToGain(volume);
                    grains_to_play.emplace_back(tpos, graindur, hz, gain, 0);
                    double panpos = unidist(m_rng);
                    grains_to_play.back().sr = m_sr;
                    grains_to_play.back().pan = panpos;
                    grains_to_play.back().pitch = pitch;
                    grains_to_play.back().volume = volume;
                    //grains_to_gui_fifo.push(grains_to_play.back());
                }
            }
        }
    }
    //spinlock.exit();
    maxGrainsActive = std::max(maxGrainsActive, (int)grains_to_play.size());
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new AudioPluginAudioProcessor(); }
