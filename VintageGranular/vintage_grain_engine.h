#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Tunings.h"
#include <random>
#include "sst/basic-blocks/dsp/PanLaws.h"
#include "sst/waveshapers.h"
#include <algorithm>
#include "sst/basic-blocks/modulators/SimpleLFO.h"
#include "choc_SingleReaderSingleWriterFIFO.h"

inline float softClip(float x)
{
    if (x <= -1.0f)
        return -2.0f / 3.0f;
    if (x >= 1.0f)
        return 2.0f / 3.0f;
    return x - std::pow(x, 3.0f) / 3.0f;
}

struct SRProvider
{
    static constexpr int BLOCK_SIZE = 32;
    static constexpr int BLOCK_SIZE_OS = BLOCK_SIZE * 2;
    SRProvider() { initTables(); }
    alignas(32) float table_envrate_linear[512];
    double samplerate = 44100.0;
    void initTables()
    {
        double dsamplerate_os = samplerate * 2;
        for (int i = 0; i < 512; ++i)
        {
            double k =
                dsamplerate_os * pow(2.0, (((double)i - 256.0) / 16.0)) / (double)BLOCK_SIZE_OS;
            table_envrate_linear[i] = (float)(1.f / k);
        }
    }
    float envelope_rate_linear_nowrap(float x)
    {
        x *= 16.f;
        x += 256.f;
        int e = std::clamp<int>((int)x, 0, 0x1ff - 1);

        float a = x - (float)e;

        return (1 - a) * table_envrate_linear[e & 0x1ff] +
               a * table_envrate_linear[(e + 1) & 0x1ff];
    }
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
    void startGrain(float samplerate, float hz, float gain, float duration, float pan,
                    float envpercent)
    {
        if (m_active)
            return;
        m_env_percent = juce::jlimit(0.01f, 0.499f, envpercent * 0.5f);
        m_grain_phase = 0.0;
        m_osc_phase = 0.0;
        m_sr = samplerate;
        m_dur = duration;
        m_hz = hz;
        m_osc_phase_inc = 2 * M_PI / m_sr * m_hz;
        m_gain = gain;
        m_active = true;
        sst::basic_blocks::dsp::pan_laws::monoEqualPower(pan, panmatrix);
    }
    double m_osc_phase_inc = 0.0;
    void processFrame(float *frame)
    {
        float out = std::sin(m_osc_phase);
        out *= m_gain;
        double dursamples = m_dur * m_sr;
        double fadelen = dursamples * m_env_percent;
        if (m_grain_phase < fadelen)
            out *= juce::jmap<double>(m_grain_phase, 0.0, fadelen, 0.0, 1.0);
        if (m_grain_phase >= dursamples - fadelen)
            out *= juce::jmap<double>(m_grain_phase, dursamples - fadelen, dursamples, 1.0, 0.0);
        m_grain_phase += 1.0;
        if (m_grain_phase > dursamples)
            m_active = false;
        m_osc_phase += m_osc_phase_inc;
        frame[0] = panmatrix[0] * out;
        frame[1] = panmatrix[3] * out;
    }
    bool m_active = false;
    float m_hz = 440.0f;
    float m_gain = 0.5f;
    float m_dur = 0.1;
    float m_sr = 44100.0f;
    double m_osc_phase = 0.0;
    double m_grain_phase = 0.0;
    float m_env_percent = 0.1f;
};

class GrainVisualizationInfo
{
  public:
    GrainVisualizationInfo() {}
    GrainVisualizationInfo(double pitch_, double volume_) : pitch(pitch_), volume(volume_) {}
    double pitch = 0.0;
    double volume = 0.0;
};

enum class GuiToAudioActionType
{
    None,
    ClearAllCells,
    ChangeCellDensity,
    Last
};

class GuiToAudioMessage
{
  public:
    GuiToAudioMessage() {}
    GuiToAudioMessage(GuiToAudioActionType act, int p0, int p1, int p2, float p3)
        : m_act(act), m_par0(p0), m_par1(p1), m_par2(p2), m_par3(p3)
    {
    }
    GuiToAudioActionType m_act = GuiToAudioActionType::None;
    int m_par0 = 0;
    int m_par1 = 0;
    int m_par2 = 0;
    float m_par3 = 0.0f;
};

using VisualizerFifoType = choc::fifo::SingleReaderSingleWriterFIFO<GrainVisualizationInfo>;
using GuiToAudioFifoType = choc::fifo::SingleReaderSingleWriterFIFO<GuiToAudioMessage>;

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
    VisualizerFifoType *m_grains_to_gui_fifo = nullptr;
    XenGrainStream()
    {

        m_rng = std::minstd_rand0((unsigned int)this);
        m_adsr.setSampleRate(m_sr);
        m_adsr.setParameters({0.01, 0.01, 1.0, 0.3});
    }
    bool isAvailable() const { return !m_is_playing; }
    bool m_use_tuning = false;
    void setPitchRange(float minpitch, float maxpitch)
    {
        m_min_pitch = minpitch;
        m_max_pitch = maxpitch;
    }

    void initVoice(XenGrainVoice &v)
    {
        std::uniform_real_distribution<float> dist{0.0f, 1.0f};
        float pitchrange = m_max_pitch - m_min_pitch;
        float regionpitchmin = m_min_pitch + (pitchrange / 16.0) * m_screen_x;
        float regionpitchmax = m_min_pitch + (pitchrange / 16.0) * (m_screen_x + 1);
        float regionpitchrange = regionpitchmax - regionpitchmin;
        float centerpitch = regionpitchmin + (regionpitchrange / 2.0f);
        float pitchmintouse =
            juce::jmap<float>(m_pitch_rand_par0, 0.0f, 1.0f, centerpitch, regionpitchmin);
        float pitchmaxtouse =
            juce::jmap<float>(m_pitch_rand_par0, 0.0f, 1.0f, centerpitch, regionpitchmax);
        double pitch = juce::jmap<float>(dist(m_rng), 0.0f, 1.0f, pitchmintouse, pitchmaxtouse);
        pitch += m_global_transpose;
        pitch += m_pitch_mod_amount;
        pitch = juce::jlimit(m_min_pitch, m_max_pitch, pitch);
        double hz = 440.0f;
        if (!m_use_tuning)
            hz = 440.0 * std::pow(2.0f, 1.0 / 12 * (pitch - 69.0));
        else
        {
            pitch = juce::jmap<float>(dist(m_rng), 0.0f, 1.0f, m_min_pitch, m_max_pitch);
            hz = m_tuning->frequencyForMidiNote(pitch);
            pitch = 12.0 * std::log(hz / 16.0);
        }

        jassert(hz >= 15.0 && hz < 20000.0);
        float vol = juce::jmap<float>(dist(m_rng), 0.0f, 1.0f, m_min_volume, m_max_volume);
        float gain = juce::Decibels::decibelsToGain(vol);
        float pan = dist(m_rng);
        v.startGrain(m_sr, hz, gain, m_grain_dur * m_dur_multiplier, pan, m_env_percent);
        if (m_visualization_enabled && m_grains_to_gui_fifo)
        {
            m_grains_to_gui_fifo->push({pitch, vol});
        }
    }
    juce::ADSR m_adsr;
    int m_screen_x = 0;
    int m_screen_y = 0;
    float m_density_multiplier = 1.0;
    float m_env_percent = 0.1f;
    void setDensityMultiplier(float x) { m_density_multiplier = x; }
    float m_dur_multiplier = 1.0f;
    void setDurationMultiplier(float x) { m_dur_multiplier = x; }
    float m_pitch_rand_par0 = 1.0;
    void setPitchRandomParameter(int index, float x) { m_pitch_rand_par0 = x; }
    void startStream(float rate, float minpitch, float maxpitch, float minvolume, float maxvolume,
                     float mingraindur, float maxgraindur, int screenX, int screenY)
    {
        m_screen_x = screenX;
        m_screen_y = screenY;
        m_grain_rate = rate;
        m_min_pitch = minpitch;
        m_max_pitch = maxpitch;
        m_min_volume = minvolume;
        m_max_volume = maxvolume;
        m_grain_dur = mingraindur;

        m_is_playing = true;
        m_adsr.noteOn();
        if (m_next_grain_time > 2.0 * m_sr)
            m_next_grain_time = 0.0;
        m_pitch_mod_amount = 0.0;
    }

    void stopStream() { m_adsr.noteOff(); }

    int m_stream_id = -1;
    float m_pitch_mod_amount = 0.0;
    float m_global_transpose = 0.0f;
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
            std::exponential_distribution<float> expdist(m_grain_rate * m_density_multiplier);
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
        float distorted0 = std::tanh(voicesums[0] * m_distortion_gain);
        float distorted1 = std::tanh(voicesums[1] * m_distortion_gain);
        outframe[0] = (1.0 - m_distortion_amount) * voicesums[0] + m_distortion_amount * distorted0;
        outframe[1] = (1.0 - m_distortion_amount) * voicesums[1] + m_distortion_amount * distorted1;
    }
    double m_phase = 0;
    double m_next_grain_time = 0;
    float m_density_scaling = 1.0f;
    float m_distortion_amount = 0.5f;
    float m_distortion_gain = 2.0f;
    void setDistortionAmount(float amt)
    {
        m_distortion_amount = amt;
        if (m_distortion_amount < 0.25)
            m_distortion_gain = 2.0f;
        else
        {
            float distortion_volume =
                juce::jmap<float>(m_distortion_amount, 0.25f, 1.0, 6.0f, 24.0f);
            m_distortion_gain = juce::Decibels::decibelsToGain(distortion_volume);
        }
    }
    Tunings::Tuning *m_tuning = nullptr;
    std::atomic<bool> m_visualization_enabled{false};
};

class XenVintageGranular
{
    double m_sr = 44100.0;
    float m_density_scaling = 1.0f;
    float m_global_transpose = 0.0f;
    float m_global_durations = 1.0f;
    int m_cur_active_screen = -1;
    int m_screen_select_mode = 0;
    int m_switch_to_screen = -1;
    double m_screendur = 0.5;
    float m_screensdata[8][16][4];

  public:
    std::array<XenGrainStream, 20> m_streams;
    VisualizerFifoType m_grains_to_gui_fifo;
    GuiToAudioFifoType m_gui_to_audio_fifo;

    int m_maxscreen = 0;
    Tunings::Tuning m_tuning;

    float m_min_pitch = 24.0;
    float m_max_pitch = 115.0;
    bool m_use_tuning = false;
    void setVisualizationEnabled(bool b)
    {
        for (auto &s : m_streams)
        {
            s.m_visualization_enabled = b;
        }
    }
    void setEnvelopeLenth(float percent)
    {
        for (auto &s : m_streams)
        {
            s.m_env_percent = percent;
        }
    }
    void setDistortionAmount(float amt)
    {
        for (auto &s : m_streams)
        {
            s.setDistortionAmount(amt);
        }
    }
    void setDensityScaling(float x)
    {
        m_density_scaling = x;
        for (auto &s : m_streams)
        {
            s.setDensityMultiplier(x);
        }
    }
    void setDurationScaling(float x)
    {
        m_global_durations = x;
        for (auto &s : m_streams)
        {
            s.setDurationMultiplier(x);
        }
    }
    void setPitchRandomParameter(int index, float x)
    {
        for (auto &s : m_streams)
        {
            s.setPitchRandomParameter(index, x);
        }
    }
    void setGlobalTranspose(float x) { m_global_transpose = x; }
    void setAutoScreenSelectRate(float seconds) { m_screendur = seconds; }
    int getCurrentlyPlayingScreen() const
    {
        if (m_cur_active_screen >= 0 && m_cur_active_screen < 8)
            return m_cur_active_screen;
        return 0;
    }
    float getDensity(int screen, int x, int y)
    {
        if (screen >= 0 && screen < 8 && x >= 0 && x < 16 && y >= 0 && y < 4)
            return m_screensdata[screen][x][y];
        return 0.0f;
    }
    bool isAutoScreenSelectActive() const { return m_screen_select_mode != 0; }
    void setScreenOrSelectMode(int mode)
    {
        if (mode >= 0 && mode < 8)
        {
            m_switch_to_screen = mode;
            m_screen_select_mode = 0;
        }
        if (mode == -2)
            m_screen_select_mode = 1;
        if (mode == -1)
            m_screen_select_mode = 2;
    }
    void updatePitchLimits()
    {
        if (!m_use_tuning)
        {
            m_min_pitch = 24.0;
            m_max_pitch = 115.0;
        }
        else
        {
            // ok this is a bit tricky, we need to find a suitable pitch/key range out of the tuning
            double minhz = 20.0;
            double maxhz = 7500.0;
            std::vector<int> foo;
            foo.reserve(128);
            for (int i = 0; i < 128; ++i)
            {
                double hz = m_tuning.frequencyForMidiNote(i);
                if (hz >= minhz && hz <= maxhz)
                    foo.push_back(i);
            }
            std::cout << "key range is " << foo.front() << " - " << foo.back() << "\n";
            m_min_pitch = foo.front();
            m_max_pitch = foo.back();
        }
    }
    XenVintageGranular(int seed)
    {
        m_lfodepths = {0.0f, 0.0f};
        m_grains_to_gui_fifo.reset(16384);
        m_gui_to_audio_fifo.reset(1024);
        m_rng = std::mt19937(seed);
        m_tuning = Tunings::evenDivisionOfCentsByM(1200.0, 7);
        updatePitchLimits();
        for (int i = 0; i < m_streams.size(); ++i)
        {
            m_streams[i].m_stream_id = i;
            m_streams[i].m_tuning = &m_tuning;
            m_streams[i].m_use_tuning = m_use_tuning;
            m_streams[i].m_grains_to_gui_fifo = &m_grains_to_gui_fifo;
        }

        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < 16; ++j)
                for (int k = 0; k < 4; ++k)
                    m_screensdata[i][j][k] = 0.0;
        juce::File datafile(R"(C:\develop\xenos\VintageGranular\testscreens.json)");
        auto json = juce::JSON::parse(datafile);
        auto allscreenssarray = json["screens"];
        m_maxscreen = allscreenssarray.size();
        std::cout << std::fixed;
        std::cout << std::setprecision(2);
        for (int i = 0; i < allscreenssarray.size(); ++i)
        {
            // std::cout << "screen " << i << "\n";
            auto screenarray = allscreenssarray[i];
            for (int j = 0; j < 4; ++j)
            {
                auto screenrow = screenarray[j];
                auto rowstring = screenrow.toString();
                // std::cout << "\t";
                for (int k = 0; k < 16; ++k)
                {
                    auto c = rowstring[k];
                    if (c != '.')
                    {
                        m_screensdata[i][k][3 - j] = c - 64;
                        // std::cout << std::pow(M_E, m_screensdata[i][k][3 - j]) << " ";
                    }
                    else
                    {
                        // std::cout << 0.0 << " ";
                    }
                }
                // std::cout << "\n";
            }
        }
        setScreenOrSelectMode(0);
    }
    std::mt19937 m_rng;
    void setSampleRate(double sr)
    {
        m_sr = sr;
        m_sr_provider.samplerate = sr;
        m_sr_provider.initTables();
    }
    void updateStreams()
    {

        std::uniform_int_distribution<int> screendist{0, m_maxscreen - 1};
        int screentouse = m_cur_active_screen;
        if (m_screen_select_mode == 0)
        {
            if (m_switch_to_screen >= 0)
                screentouse = m_switch_to_screen;
        }

        if (m_screen_select_mode == 1)
            screentouse = screendist(m_rng);
        if (m_screen_select_mode == 2)
            screentouse = (screentouse + 1) % m_maxscreen;
        if (screentouse == m_cur_active_screen)
            return;
        // cancel all previous active streams if screen changed
        for (auto &stream : m_streams)
        {
            stream.stopStream();
        }
        m_cur_active_screen = screentouse;
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
                            float pitchwidth = (m_max_pitch - m_min_pitch) / 16;
                            float minpitch = m_min_pitch + (i * pitchwidth);
                            float maxpitch = m_min_pitch + ((i + 1) * pitchwidth);
                            float volwidth = 40.0 / 4;
                            float minvol = -40.0 + volwidth * j;
                            float maxvol = -40.0 + volwidth * (j + 1);
                            float graindur =
                                juce::jmap<float>(minpitch, m_min_pitch, m_max_pitch, 0.15, 0.025);
                            stream.startStream(density, minpitch, maxpitch, minvol, maxvol,
                                               graindur, 0.05, i, j);
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
    int m_num_pitch_regions = 16;
    void setPitchRange(float minpitch, float maxpitch)
    {
        m_min_pitch = minpitch;
        m_max_pitch = maxpitch;
        for (auto &s : m_streams)
        {
            s.setPitchRange(minpitch, maxpitch);
        }
    }
    juce::Range<float> getPitchRange() const { return juce::Range(m_min_pitch, m_max_pitch); }
    void handleGUIMessage(const GuiToAudioMessage &msg)
    {
        if (msg.m_act == GuiToAudioActionType::ClearAllCells)
        {
            for (int i = 0; i < 16; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {
                    m_screensdata[msg.m_par0][i][j] = 0.0f;
                }
            }
            for (auto &s : m_streams)
            {
                s.stopStream();
            }
        }
    }
    void process(float *outframe)
    {
        if (m_phase_resetted)
        {
            updateStreams();
            m_phase_resetted = false;
        }
        if (m_lfo_update_counter == 0)
        {
            // is this going to happen too often with the blocksize of 32?
            GuiToAudioMessage msg;
            while (m_gui_to_audio_fifo.pop(msg))
            {
                handleGUIMessage(msg);
            }
            m_lfo0.process_block(2.0, 0.5, LFOType::Shape::SMOOTH_NOISE);
            m_lfo1.process_block(3.0, 0.6, LFOType::Shape::SMOOTH_NOISE);
            for (auto &stream : m_streams)
            {
                stream.m_global_transpose = m_global_transpose;
                stream.m_pitch_mod_amount = 0.0;
                if (stream.m_screen_x % 2 == 0)
                    stream.m_pitch_mod_amount = m_lfodepths[0] * 6.0 * m_lfo0.outputBlock[0];
                if (stream.m_screen_x % 2 == 1)
                    stream.m_pitch_mod_amount = m_lfodepths[1] * 6.0 * m_lfo1.outputBlock[0];
            }
        }
        ++m_lfo_update_counter;
        if (m_lfo_update_counter == m_sr_provider.BLOCK_SIZE)
            m_lfo_update_counter = 0;
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
        double hz = 1.0 / m_screendur;
        m_phase += 1.0 / m_sr * hz;
        if (m_phase >= 1.0)
        {
            m_phase -= 1.0;
            m_phase_resetted = true;
        }
    }
    double m_phase = 0.0;
    bool m_phase_resetted = true;
    SRProvider m_sr_provider;
    using LFOType = sst::basic_blocks::modulators::SimpleLFO<SRProvider, 32>;
    LFOType m_lfo0{&m_sr_provider};
    LFOType m_lfo1{&m_sr_provider};
    int m_lfo_update_counter = 0;
    std::array<float, 2> m_lfodepths;

    void setLFODepth(int index, float val) { m_lfodepths[index] = val; }
};
