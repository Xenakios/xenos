#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <random>
#include "sst/basic-blocks/dsp/PanLaws.h"
#include "choc_SingleReaderSingleWriterFIFO.h"

static const char *grainScreenA[4] = {"BAAAAAAAAAAAAAAA", "AAAAAAAEAAAAAAAA", "AAAAAACAAAAAAAAA",
                                      "AAAAAAAAAAAAAAAD"};

static const char *grainScreenB[4] = {"ACAAAAAAAAAAAAAA", "AAAAAAAEAAAAAAAA", "AAAAAACAAAAAAAAA",
                                      "AAEAAAAAAFAAAFAA"};

static const char *grainScreenC[4] = {"DAAAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAB",
                                      "AAADAAAAAAAAAAAA"};

static const char *grainScreenD[4] = {"AABAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAA", "AAAAAAAEAEAEAAAA",
                                      "AAAAAAAAAAAAAAEE"};

static const char *grainScreenE[4] = {"BAAAAAAAAAAAAAAA", "AABAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAA",
                                      "AAAAAAAAAAACBABA"};

static const char *grainScreenF[4] = {"AAAAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAA", "BABABABABABABABA",
                                      "ABABABABABABABAB"};

static const char *grainScreenG[4] = {"DDDDAAAAAAAADDDD", "AAAAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAA",
                                      "AAAAAAAFAAAAAAAA"};

static const char *grainScreenH[4] = {"AAAAAAAAAAAEAAAA", "AAAGAAAAAAAAAAAA", "AAAAAAAAAAAAAAAA",
                                      "EEEAEEEEEEEAEEEE"};

inline float softClip(float x)
{
    if (x <= -1.0f)
        return -2.0f / 3.0f;
    if (x >= 1.0f)
        return 2.0f / 3.0f;
    return x - std::pow(x, 3.0f) / 3.0f;
}

struct GrainInfo
{
    GrainInfo() {}
    GrainInfo(double tpos_, double dur_, double hz_, double gain_, int ochan_)
        : tpos(tpos_), dur(dur_), hz(hz_), gain(gain_), outputchan(ochan_)
    {
        phase = 0.0;
    }
    float getSample()
    {
        double dursamples = dur * sr;
        if (phase >= dursamples)
            return 0.0f;
        float out = std::sin(2 * M_PI / sr * hz * phase) * 1.0f;
        // out += std::sin(2 * M_PI / sr * hz * phase * 2) * 0.2f;
        out *= gain;
        double fadelen = dursamples * 0.1;
        if (phase < fadelen)
            out *= juce::jmap<double>(phase, 0.0, fadelen, 0.0, 1.0);
        if (phase >= dursamples - fadelen)
            out *= juce::jmap<double>(phase, dursamples - fadelen, dursamples, 1.0, 0.0);
        out = softClip(out);
        phase += 1.0;
        return out;
    }
    double phase = 0.0;
    double tpos = 0.0;
    double dur = 0.05;
    double hz = 440.0;
    double pitch = 60.0;
    double gain = 0.0;
    double sr = 44100.0;
    int outputchan = 0;
    bool active = false;
    double pan = 0.5;
    double volume = 0.0;
};

class XenGranularEngine
{
  public:
    std::vector<GrainInfo> grains_to_play;
    choc::fifo::SingleReaderSingleWriterFIFO<GrainInfo> grains_to_gui_fifo;

    int m_screen_phase = 0;
    int m_block_phase = 0;
    int m_block_len = 32;
    double m_screen_dur = 1.0;
    double m_sr = 44100.0;
    float panpositions[4][16];
    std::atomic<int> m_cur_screen{0};
    XenGranularEngine()
    {
        grains_to_play.reserve(16384);
        std::uniform_real_distribution<float> pandist{0.0f, 1.0f};
        grains_to_gui_fifo.reset(16384);
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 16; ++j)
            {
                if (j > 4)
                    panpositions[i][j] = pandist(m_rng);
                else
                    panpositions[i][j] = 0.5;
            }
    }
    int maxGrainsActive = 0;
    void generateScreen();

    sst::basic_blocks::dsp::pan_laws::panmatrix_t panmatrix;
    int m_pitch_distribution_mode = 0;
    std::mt19937 m_rng{39};
    float m_blockout_buf[64];
    std::atomic<bool> grainsReady{false};
    void processSample(float *outframe)
    {
        if (m_screen_phase == 0)
        {
            generateScreen();
        }
        if (m_block_phase == 0)
        {
            for (int i = 0; i < m_block_len * 2; ++i)
                m_blockout_buf[i] = 0.0f;
            juce::Range<int> blockregion(m_screen_phase, m_screen_phase + m_block_len);
            for (auto &grain : grains_to_play)
            {
                juce::Range<int> grainregion(grain.tpos * m_sr, (grain.tpos + grain.dur) * m_sr);
                if (blockregion.intersects(grainregion))
                {
                    if (!grain.active)
                    {
                        grain.active = true;
                        grains_to_gui_fifo.push(grain);
                    }
                    sst::basic_blocks::dsp::pan_laws::monoEqualPower(grain.pan, panmatrix);
                    for (int i = 0; i < m_block_len; ++i)
                    {
                        float os = grain.getSample();
                        m_blockout_buf[i * 2 + 0] += os * panmatrix[0];
                        m_blockout_buf[i * 2 + 1] += os * panmatrix[3];
                    }
                }
                else
                {
                    grain.active = false;
                }
            }
        }
        outframe[0] = m_blockout_buf[m_block_phase * 2 + 0];
        outframe[1] = m_blockout_buf[m_block_phase * 2 + 1];
        ++m_block_phase;
        if (m_block_phase == m_block_len)
            m_block_phase = 0;
        ++m_screen_phase;
        if (m_screen_phase >= m_sr * m_screen_dur)
            m_screen_phase = 0;
    }
};

//==============================================================================
class AudioPluginAudioProcessor : public juce::AudioProcessor
{
  public:
    //==============================================================================
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

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
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;
    XenGranularEngine m_geng;
    juce::AudioProcessLoadMeasurer m_cpu_load;

  private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
