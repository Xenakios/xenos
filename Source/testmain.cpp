#include <complex>
// #include "Xenos.h"
#include <JuceHeader.h>
#include "Tunings.h"
#include <random>
#include "sst/basic-blocks/dsp/PanLaws.h"

static std::unique_ptr<juce::AudioFormatWriter> makeWavWriter(juce::File outfile, int chans,
                                                              double sr)
{
    outfile.deleteFile();
    auto os = outfile.createOutputStream();
    juce::WavAudioFormat wavf;
    auto writer = wavf.createWriterFor(os.release(), sr, chans, 32, {}, 0);
    return std::unique_ptr<juce::AudioFormatWriter>(writer);
}
/*
void test_xenoscore()
{
    XenosCore core;
    double sr = 44100.0;
    core.initialize(sr);
    XenosSynth synth;
    juce::MidiKeyboardState state;
    XenosSynthHolder source(state);

    core.setPitchWidth(12.0f);
    core.setPitchCenter(67.0f);
    core.pitchWalk.setStepRatio(0.8);
    core.quantizer.setActive(true);
    core.quantizer.setScale(0);

    int nsamples = sr * 20;
    juce::AudioBuffer<float> buffer(1, nsamples);
    auto writer = makeWavWriter(juce::File(R"(C:\develop\xenos\out.wav)"), 1, sr);
    for (int i = 0; i < nsamples; ++i)
    {
        buffer.setSample(0, i, core() * 0.5);
    }
    writer->writeFromAudioSampleBuffer(buffer, 0, nsamples);
}


inline void test_sst_tuning()
{
    try
    {
        auto scale = Tunings::readSCLFile(R"(C:\develop\AdditiveSynth\scala\05-19.scl)");
        auto kbm = Tunings::startScaleOnAndTuneNoteTo(0, 69, 440.0);
        auto tuning = Tunings::Tuning(scale, kbm);
        std::vector<double> freqs{220.0, 440.0, 441.0, 439.0, 572.3, 16.7};
        for (auto &hz : freqs)
        {
            double quantizedhz = findClosestFrequency(tuning,hz, nullptr);
            std::cout << hz << "\t" << quantizedhz << "\t" << (quantizedhz/hz) << "\n";
        }
        // for (int i = 60; i < 72; ++i)
        //     std::cout << i << "\t" << tuning.frequencyForMidiNote(i) << "\n";
    }
    catch (std::exception &excep)
    {
        std::cout << excep.what() << "\n";
    }
}
*/
class ShadowTest
{
  public:
    ShadowTest(int foo) { foo = foo; }
    int foo = 0;
};

void test_shadowing()
{
    ShadowTest s(42);
    std::cout << s.foo << "\n";
}

void test_dropped_samples()
{
    float sr = 44100.0;
    int outlen = 20 * sr;
    juce::AudioBuffer<float> outbuf(1, outlen);
    int sinephase = 0;
    for (int i = 0; i < outlen; ++i)
    {
        float outsamp = std::sin(M_PI * 2 / sr * 1000.13 * sinephase);
        outbuf.setSample(0, i, outsamp);
        if (i % (1 * 44100) != 0)
            ++sinephase;
    }
    auto writer = makeWavWriter(juce::File("C:\\MusicAudio\\dropsamples1.wav"), 1, sr);
    writer->writeFromAudioSampleBuffer(outbuf, 0, outlen);
}

class GrainVoice
{
  public:
    GrainVoice() {}
    void reset(float samplerate, float lenseconds, float frequency, float gain)
    {
        m_phase = 0;
        m_sr = samplerate;
        m_hz = frequency;
        m_gn = gain;
        m_end_sample = samplerate * lenseconds;
        m_available = false;
    }
    float getNextSample()
    {
        float outsample = std::sin(2 * M_PI / m_sr * m_hz * m_phase);
        outsample *= m_gn;
        float fadeoutbegin = m_end_sample * 0.95;
        if (m_phase < m_end_sample * 0.05)
            outsample *= juce::jmap<float>(m_phase, 0, m_end_sample * 0.05, 0.0f, 1.0f);
        if (m_phase >= fadeoutbegin)
            outsample *= juce::jmap<float>(m_phase, fadeoutbegin, m_end_sample, 1.0f, 0.0f);
        ++m_phase;
        if (m_phase == m_end_sample)
        {
            m_available = true;
            return 0.0f;
        }
        return outsample;
    }
    bool isAvailable() const { return m_available; }

  private:
    int m_phase = 0;
    int m_end_sample = 0;
    bool m_available = true;
    float m_sr = 44100.0f;
    float m_hz = 440.0f;
    float m_gn = 1.0f;
};

const char *grainScreenA[4] = {"BAAAAAAAAAAAAAAA", "AAAAAAAEAAAAAAAA", "AAAAAACAAAAAAAAA",
                               "AAAAAAAAAAAAAAAD"};

const char *grainScreenB[4] = {"ACAAAAAAAAAAAAAA", "AAAAAAAEAAAAAAAA", "AAAAAACAAAAAAAAA",
                               "AAEAAAAAAFAAAFAA"};

const char *grainScreenC[4] = {"DAAAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAB",
                               "AAADAAAAAAAAAAAA"};

const char *grainScreenD[4] = {"AABAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAA", "AAAAAAAEAEAEAAAA",
                               "AAAAAAAAAAAAAAEE"};

const char *grainScreenE[4] = {"BAAAAAAAAAAAAAAA", "AABAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAA",
                               "AAAAAAAAAAACBAB"};

const char *grainScreenF[4] = {"AAAAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAA", "BABABABABABABABA",
                               "ABABABABABABABAB"};

const char *grainScreenG[4] = {"DDDDAAAAAAAADDDD", "AAAAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAA",
                               "AAAAAAAFAAAAAAAA"};

const char *grainScreenH[4] = {"AAAAAAAAAAAEAAAA", "AAAAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAA",
                               "EEEEEEEEEEEAEEEE"};

class XenGranularEngine
{
  public:
    GrainVoice m_voices[2048];
    int m_phase = 0;
    double m_screen_dur = 0.5;
    double m_sr = 44100.0;
    float panpositions[4][16];
    std::vector<float> m_screen_buffer;
    XenGranularEngine()
    {
        std::uniform_real_distribution<float> pandist{0.0f, 1.0f};
        m_screen_buffer.resize(m_screen_dur * m_sr * 2);
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 16; ++j)
            {
                if (j > 4)
                    panpositions[i][j] = pandist(m_rng);
                else
                    panpositions[i][j] = 0.5;
            }
    }
    sst::basic_blocks::dsp::pan_laws::panmatrix_t panmatrix;
    void renderScreenAudio()
    {
        std::fill(m_screen_buffer.begin(), m_screen_buffer.end(), 0.0f);

        int screendursamples = m_screen_dur * m_sr;
        std::uniform_real_distribution<double> tposdist{0.0, 1.0};
        std::uniform_int_distribution<int> screendist{0, 7};
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
        for (int i = 0; i < 16; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                float density = screendata[j][i] - 65;
                if (density > 0.0f)
                {
                    density = 1.5 * std::pow(density, M_E);
                    int numgrains = std::round(density * m_screen_dur);
                    sst::basic_blocks::dsp::pan_laws::monoEqualPower(panpositions[j][i], panmatrix);
                    for (int k = 0; k < numgrains; ++k)
                    {

                        float pitch =
                            juce::jmap<float>(i + tposdist(m_rng), 0.0, 16.0, 24.0, 115.0);
                        float hz = 440.0 * std::pow(2.0, 1.0 / 12.0 * (pitch - 69.0));
                        float graindur = juce::jmap<float>(pitch, 24.0, 104.0, 0.2, 0.025);
                        int graindursamples = graindur * m_sr;
                        float tpos = tposdist(m_rng) * (m_screen_dur - graindur);
                        int tpossamples = tpos * m_sr;
                        float volume =
                            juce::jmap<float>(j + tposdist(m_rng), 0.0, 4.0, -40.0, -6.0);
                        float gain = juce::Decibels::decibelsToGain(volume);
                        gain *= 0.5;
                        for (int sample = tpossamples; sample < tpossamples + graindursamples;
                             ++sample)
                        {
                            if (sample >= screendursamples)
                                break;
                            int envpos = sample - tpossamples;
                            float outsample0 = std::sin(2 * M_PI / m_sr * envpos * hz) * gain;
                            float outsample1 =
                                std::sin(2 * M_PI / m_sr * envpos * hz * 2) * gain * 0.25;
                            float outsample = outsample0 + outsample1;
                            const float fadepercent = 0.1;
                            if (envpos < graindursamples * fadepercent)
                                outsample *= juce::jmap<float>(
                                    envpos, 0, graindursamples * fadepercent, 0.0, 1.0);
                            if (envpos > graindursamples * (1.0 - fadepercent))
                                outsample *=
                                    juce::jmap<float>(envpos, graindursamples * (1.0 - fadepercent),
                                                      graindursamples, 1.0, 0.0);
                            // outsample = std::tanh(outsample);
                            m_screen_buffer[sample * 2 + 0] += outsample * panmatrix[0];
                            m_screen_buffer[sample * 2 + 1] += outsample * panmatrix[3];
                        }
                    }
                }
            }
        }
    }
    std::mt19937 m_rng{39};
    void processSample(float *outframe)
    {
        if (m_phase == 0)
        {
            renderScreenAudio();
            std::cout << "rendered new screen...\n";
        }
        outframe[0] = m_screen_buffer[m_phase * 2 + 0];
        outframe[1] = m_screen_buffer[m_phase * 2 + 1];
        ++m_phase;
        if (m_phase >= m_sr * m_screen_dur)
            m_phase = 0;
    }
};

void test_xen_grains()
{
    auto writer = makeWavWriter(juce::File("C:\\MusicAudio\\xengrain01.wav"), 2, 44100);
    auto eng = std::make_unique<XenGranularEngine>();
    int outlen = 60 * 44100;
    juce::AudioBuffer<float> buf(2, outlen);
    for (int i = 0; i < outlen; ++i)
    {
        float samples[2];
        eng->processSample(samples);
        buf.setSample(0, i, (samples[0]));
        buf.setSample(1, i, (samples[1]));
    }
    writer->writeFromAudioSampleBuffer(buf, 0, outlen);
}

int main()
{
    // test_sst_tuning();
    // test_xenoscore();
    // test_shadowing();
    // test_dropped_samples();
    test_xen_grains();
    return 0;
}
