#include <complex>
// #include "Xenos.h"
#include <JuceHeader.h>
#include "Tunings.h"
#include <random>
#include "sst/basic-blocks/dsp/PanLaws.h"
#include "sst/waveshapers.h"
#include <algorithm>

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
                               "AAAAAAAAAAACBABA"};

const char *grainScreenF[4] = {"AAAAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAA", "BABABABABABABABA",
                               "ABABABABABABABAB"};

const char *grainScreenG[4] = {"DDDDAAAAAAAADDDD", "AAAAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAA",
                               "AAAAAAAFAAAAAAAA"};

const char *grainScreenH[4] = {"AAAAAAAAAAAEAAAA", "AAAGAAAAAAAAAAAA", "AAAAAAAAAAAAAAAA",
                               "EEEAEEEEEEEAEEEE"};

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
        float out = std::sin(2 * M_PI / sr * hz * phase);
        out *= gain;
        double fadelen = dursamples * 0.1;
        if (phase < fadelen)
            out *= jmap<double>(phase, 0.0, fadelen, 0.0, 1.0);
        if (phase >= dursamples - fadelen)
            out *= jmap<double>(phase, dursamples - fadelen, dursamples, 1.0, 0.0);
        phase += 1.0;
        return out;
    }
    double phase = 0.0;
    double tpos = 0.0;
    double dur = 0.05;
    double hz = 440.0;
    double gain = 0.0;
    double sr = 44100.0;
    int outputchan = 0;
    bool active = false;
    double pan = 0.5;
};

class XenGrainVoice
{
  public:
    XenGrainVoice()
    {
        for (int i = 0; i < 4; ++i)
            panmatrix[i] = 0.0f;
    }
    sst::basic_blocks::dsp::pan_laws::panmatrix_t panmatrix;
    void startGrain(float samplerate, float hz, float gain, float duration, float pan)
    {
        if (m_active)
            return;
        m_phase = 0.0;
        m_sr = samplerate;
        m_dur = duration;
        m_hz = hz;
        m_gain = gain;
        m_active = true;
        sst::basic_blocks::dsp::pan_laws::monoEqualPower(pan, panmatrix);
    }
    void processFrame(float *frame)
    {
        float out = std::sin(2 * M_PI / m_sr * m_hz * m_phase);
        out *= m_gain;
        double dursamples = m_dur * m_sr;
        double fadelen = dursamples * 0.1;
        if (m_phase < fadelen)
            out *= jmap<double>(m_phase, 0.0, fadelen, 0.0, 1.0);
        if (m_phase >= dursamples - fadelen)
            out *= jmap<double>(m_phase, dursamples - fadelen, dursamples, 1.0, 0.0);
        m_phase += 1.0;
        if (m_phase > dursamples)
            m_active = false;
        frame[0] = panmatrix[0] * out;
        frame[1] = panmatrix[3] * out;
    }
    bool m_active = false;
    float m_hz = 440.0f;
    float m_gain = 0.5f;
    float m_dur = 0.1;
    float m_sr = 44100.0f;
    double m_phase = 0.0;
};

class XenGrainStream
{
    double m_grain_rate = 0.0;
    double m_min_pitch = 24.0;
    double m_max_pitch = 100.0;
    double m_min_volume = -40.0;
    double m_max_volume = 0.0;
    double m_grain_dur = 0.05;
    bool m_is_playing = false;

  public:
    std::minstd_rand0 m_rng;
    std::array<XenGrainVoice, 16> m_voices;

    double m_sr = 44100.0;

    XenGrainStream()
    {
        m_rng = std::minstd_rand0((unsigned int)this);
        m_adsr.setSampleRate(m_sr);
        m_adsr.setParameters({0.001, 0.01, 1.0, 0.5});
    }
    bool isAvailable() const { return !m_is_playing; }

    void initVoice(XenGrainVoice &v)
    {
        std::uniform_real_distribution<float> dist{0.0f, 1.0f};
        float pitch = juce::jmap<float>(dist(m_rng), 0.0f, 1.0f, m_min_pitch, m_max_pitch);
        float hz = 440.0 * std::pow(2.0f, 1.0 / 12 * (pitch - 69.0));
        jassert(hz > 16.0 && hz < 20000.0);
        float vol = juce::jmap<float>(dist(m_rng), 0.0f, 1.0f, m_min_volume, m_max_volume);
        float gain = juce::Decibels::decibelsToGain(vol);
        float pan = dist(m_rng);
        v.startGrain(m_sr, hz, gain, m_grain_dur, pan);
    }
    juce::ADSR m_adsr;
    void startStream(float rate, float minpitch, float maxpitch, float minvolume, float maxvolume,
                     float mingraindur, float maxgraindur)
    {
        m_grain_rate = rate;
        m_min_pitch = minpitch;
        m_max_pitch = maxpitch;
        m_min_volume = minvolume;
        m_max_volume = maxvolume;
        m_grain_dur = mingraindur;
        m_stop_requested = false;
        m_is_playing = true;
        m_adsr.noteOn();
        m_next_grain_time = 0.0;
    }
    bool m_stop_requested = false;
    void stopStream()
    {
        m_stop_requested = true;
        m_stop_fade_counter = 0;
        m_adsr.noteOff();
    }
    int m_stop_fade_counter = 0;
    int m_stream_id = -1;
    void processFrame(float *outframe)
    {
        if (!m_is_playing)
        {
            outframe[0] = 0.0f;
            outframe[1] = 0.0f;
        }

        if (m_phase >= m_next_grain_time)
        {
            for (auto &v : m_voices)
            {
                if (v.m_active == false)
                {
                    initVoice(v);
                    break;
                }
            }
            std::exponential_distribution<float> expdist(m_grain_rate);
            m_next_grain_time = m_phase + expdist(m_rng) * m_sr;
            // m_next_grain_time = m_phase + ((1.0 / m_grain_rate) * m_sr);
        }
        float voicesums[2] = {0.0f, 0.0f};
        float voiceframe[2];
        for (auto &v : m_voices)
        {
            if (v.m_active)
            {
                v.processFrame(voiceframe);
                voicesums[0] += voiceframe[0];
                voicesums[1] += voiceframe[1];
            }
        }
        float envgain = m_adsr.getNextSample();
        voicesums[0] *= envgain;
        voicesums[1] *= envgain;
        if (!m_adsr.isActive())
        {
            m_is_playing = false;
            for (auto &v : m_voices)
                v.m_active = false;
        }

        m_phase += 1.0;
        outframe[0] = std::tanh(voicesums[0]);
        outframe[1] = std::tanh(voicesums[1]);
    }
    double m_phase = 0;
    double m_next_grain_time = 0;
};

class XenVintageGranular
{
  public:
    std::array<XenGrainStream, 20> m_streams;
    double m_sr = 44100.0;
    float m_screensdata[8][16][4];
    int m_maxscreen = 0;
    XenVintageGranular(std::mt19937 &rng) : m_rng(rng)
    {
        for (int i = 0; i < m_streams.size(); ++i)
            m_streams[i].m_stream_id = i;
        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < 16; ++j)
                for (int k = 0; k < 4; ++k)
                    m_screensdata[i][j][k] = 0.0;
        juce::File datafile(R"(C:\develop\xenos\VintageGranular\testscreens.json)");
        auto json = juce::JSON::parse(datafile);
        auto allscreenssarray = json["screens"];
        m_maxscreen = allscreenssarray.size();
        for (int i = 0; i < allscreenssarray.size(); ++i)
        {
            std::cout << "screen " << i << "\n";
            auto screenarray = allscreenssarray[i];
            for (int j = 0; j < 4; ++j)
            {
                auto screenrow = screenarray[j];
                auto rowstring = screenrow.toString();
                std::cout << "\t";
                for (int k = 0; k < 16; ++k)
                {
                    auto c = rowstring[k];
                    if (c != '.')
                        m_screensdata[i][k][3 - j] = c - 64;
                    std::cout << m_screensdata[i][k][3 - j] << " ";
                }
                std::cout << "\n";
            }
        }
    }
    std::mt19937 &m_rng;
    void updateStreams()
    {
        // cancel all previous active streams
        for (auto &stream : m_streams)
        {
            // if (stream.m_grain_rate > 0)
            stream.stopStream();
        }
        std::uniform_int_distribution<int> screendist{0, m_maxscreen - 1};
        int screentouse = screendist(m_rng);
        for (int i = 0; i < 16; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {

                float density = m_screensdata[screentouse][i][j];
                if (density > 0.0)
                {
                    density = std::pow(M_E, density);
                    // hopefully find available stream...
                    bool streamfound = false;
                    for (auto &stream : m_streams)
                    {
                        if (stream.isAvailable())
                        {
                            float pitchwidth = (115.0 - 24.0) / 16;
                            float minpitch = 24.0 + (i * pitchwidth);
                            float maxpitch = 24.0 + ((i + 1) * pitchwidth);
                            float volwidth = 40.0 / 4;
                            float minvol = -40.0 + volwidth * j;
                            float maxvol = -40.0 + volwidth * (j + 1);
                            float graindur = jmap<float>(minpitch, 24.0, 115.0, 0.15, 0.025);
                            stream.startStream(density, minpitch, maxpitch, minvol, maxvol,
                                               graindur, 0.05);
                            streamfound = true;
                            break;
                        }
                    }
                    if (!streamfound)
                        std::cout << "could not find grain stream to start\n";
                }
            }
        }
    }
    void process(float *outframe)
    {
        if (m_phase == 0.0)
        {
            // std::cout << "updating streams\n";
            updateStreams();
        }
        outframe[0] = 0.0f;
        outframe[1] = 0.0f;
        float streamframe[2] = {0.0f, 0.0f};
        for (auto &stream : m_streams)
        {
            if (!stream.isAvailable())
            {
                stream.processFrame(streamframe);
                outframe[0] += streamframe[0];
                outframe[1] += streamframe[1];
            }
        }
        m_phase += 1.0;
        if (m_phase >= m_sr * m_screendur)
            m_phase = 0.0;
    }
    double m_phase = 0.0;
    double m_screendur = 1.0;
};

inline double calcMedian(std::vector<double> vec)
{
    std::sort(vec.begin(), vec.end());
    if (vec.size() % 2 == 0)
    {
        return (vec[vec.size() / 2 - 1] + vec[vec.size() / 2]) / 2.0;
    }
    return vec[vec.size() / 2];
}

inline double millisecondsToPercentage(double samplerate, int buffersize, double elapsed)
{
    double callbacklenmillis = 1000.0 / samplerate * buffersize;
    return (elapsed / callbacklenmillis) * 100.0;
}

void test_vintage_grains()
{
    std::mt19937 rng{7};
    auto writer = makeWavWriter(juce::File("C:\\MusicAudio\\xenvintagegrain01.wav"), 2, 44100);
    auto eng = std::make_unique<XenVintageGranular>(rng);
    double outlenseconds = 60.0;
    int outlen = outlenseconds * 44100;
    int procbufsize = 512;
    juce::AudioBuffer<float> buf(2, procbufsize);
    float gainscaler = juce::Decibels::decibelsToGain(-20.0);
    auto bufs = buf.getArrayOfWritePointers();

    int outcounter = 0;
    std::vector<double> benchmarks;
    benchmarks.reserve(65536);
    while (outcounter < outlen)
    {
        double t0 = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < procbufsize; ++i)
        {
            float samples[2];
            eng->process(samples);

            bufs[0][i] = gainscaler * samples[0];
            bufs[1][i] = gainscaler * samples[1];
        }
        double t1 = juce::Time::getMillisecondCounterHiRes();
        benchmarks.push_back(t1 - t0);
        writer->writeFromAudioSampleBuffer(buf, 0, procbufsize);
        outcounter += procbufsize;
    }
    auto it = std::max_element(benchmarks.begin(), benchmarks.end());
    std::cout << "maximum callback duration was " << *it << " milliseconds\n";
    std::cout << "maximum callback duration was "
              << millisecondsToPercentage(44100, procbufsize, *it) << " % of callbacklen\n";
    double median = calcMedian(benchmarks);
    std::cout << "median callback duration was " << median << " milliseconds\n";
    std::cout << "median callback duration was "
              << millisecondsToPercentage(44100, procbufsize, median) << " % of callbacklen\n";
    auto avg = std::accumulate(benchmarks.begin(), benchmarks.end(), 0.0) / benchmarks.size();
    std::cout << "average callback duration was " << avg << " milliseconds\n";
    std::cout << "average callback duration was "
              << millisecondsToPercentage(44100, procbufsize, avg) << " % of callbacklen\n";
}

void test_jsonparse()
{
    juce::File datafile(R"(C:\develop\xenos\VintageGranular\testscreens.json)");
    auto json = juce::JSON::parse(datafile);
    auto allscreenssarray = json["screens"];
    for (int i = 0; i < allscreenssarray.size(); ++i)
    {
        std::cout << "screen " << i << "\n";
        auto screenarray = allscreenssarray[i];
        for (int j = 0; j < 4; ++j)
        {
            auto screenrow = screenarray[j];
            std::cout << "\t" << screenrow.toString() << "\n";
        }
    }
}

class XenGranularEngine
{
  public:
    std::vector<GrainInfo> grains_to_play;

    int m_screen_phase = 0;
    int m_block_phase = 0;
    int m_block_len = 32;
    double m_screen_dur = 0.5;
    double m_sr = 44100.0;
    float panpositions[4][16];

    XenGranularEngine()
    {
        grains_to_play.reserve(16384);
        std::uniform_real_distribution<float> pandist{0.0f, 1.0f};

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
    void generateScreen()
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
        grains_to_play.clear();
        for (int i = 0; i < 16; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                float density = screendata[j][i] - 65;
                if (density > 0.0f)
                {
                    density = std::pow(M_E, density);
                    int numgrains = std::round(density * m_screen_dur);
                    for (int k = 0; k < numgrains; ++k)
                    {
                        float pitch = juce::jmap<float>(i + pitchdist(m_rng) * 0.5, 0.0, 16.0,
                                                        minpitch, maxpitch);
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
                    }
                }
            }
        }
        maxGrainsActive = std::max(maxGrainsActive, (int)grains_to_play.size());
    }
    sst::basic_blocks::dsp::pan_laws::panmatrix_t panmatrix;

    std::mt19937 m_rng{39};
    float m_blockout_buf[64];
    void processSample(float *outframe)
    {
        if (m_screen_phase == 0)
        {
            generateScreen();
            std::cout << "generated new screen...\n";
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
                    sst::basic_blocks::dsp::pan_laws::monoEqualPower(grain.pan, panmatrix);
                    for (int i = 0; i < m_block_len; ++i)
                    {
                        float os = grain.getSample();
                        m_blockout_buf[i * 2 + 0] += os * panmatrix[0];
                        m_blockout_buf[i * 2 + 1] += os * panmatrix[3];
                    }
                }
            }
        }
        outframe[0] = m_blockout_buf[m_block_phase * 2 + 0];
        outframe[1] = m_blockout_buf[m_block_phase * 2 + 1];
        ++m_block_phase;
        if (m_block_phase == m_block_len)
            m_block_phase = 0;
        // outframe[0] = m_screen_buffer[m_screen_phase * 2 + 0];
        // outframe[1] = m_screen_buffer[m_screen_phase * 2 + 1];
        ++m_screen_phase;
        if (m_screen_phase >= m_sr * m_screen_dur)
            m_screen_phase = 0;
    }
};

void test_xen_grains()
{
    auto writer = makeWavWriter(juce::File("C:\\MusicAudio\\xengrain02.wav"), 2, 44100);
    auto eng = std::make_unique<XenGranularEngine>();
    int outlen = 60 * 44100;
    juce::AudioBuffer<float> buf(2, outlen);
    float gainscaler = juce::Decibels::decibelsToGain(-20.0);
    auto shaper =
        sst::waveshapers::GetQuadWaveshaper(sst::waveshapers::WaveshaperType::wst_singlefold);
    sst::waveshapers::QuadWaveshaperState shaperstateL;
    shaperstateL.init = _mm_cmpneq_ps(_mm_setzero_ps(), _mm_setzero_ps());
    sst::waveshapers::QuadWaveshaperState shaperstateR;
    shaperstateR.init = _mm_cmpneq_ps(_mm_setzero_ps(), _mm_setzero_ps());
    for (int i = 0; i < outlen; ++i)
    {
        float samples[2];
        eng->processSample(samples);

        buf.setSample(0, i, std::tanh(gainscaler * samples[0]));
        buf.setSample(1, i, std::tanh(gainscaler * samples[1]));
    }
    writer->writeFromAudioSampleBuffer(buf, 0, outlen);
    std::cout << eng->maxGrainsActive << " grains at most were generated\n";
}

int main()
{
    // test_sst_tuning();
    // test_xenoscore();
    // test_shadowing();
    // test_dropped_samples();
    // test_xen_grains();
    test_vintage_grains();
    // test_jsonparse();
    return 0;
}
