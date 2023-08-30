#include <complex>
// #include "Xenos.h"
#include <JuceHeader.h>
#include "Tunings.h"
#include <random>
#include "sst/basic-blocks/dsp/PanLaws.h"
#include "sst/waveshapers.h"
#include <algorithm>
#include "sst/basic-blocks/modulators/SimpleLFO.h"
#include "../VintageGranular/vintage_grain_engine.h"
#include "choc_Assert.h"
#include "choc_UnitTest.h"

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

inline void test_vintage_grains()
{
    auto writer = makeWavWriter(juce::File("C:\\MusicAudio\\xenvintagegrain03.wav"), 2, 44100);
    auto eng = std::make_unique<XenVintageGranular>(9569);
    double outlenseconds = 30.0;
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
        // std::cout << outcounter << " ";
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
    // std::cout << "\n";
    auto it = std::max_element(benchmarks.begin(), benchmarks.end());
    std::cout << "maximum callback duration was " << *it << " milliseconds, "
              << millisecondsToPercentage(44100, procbufsize, *it) << "%\n";
    double median = calcMedian(benchmarks);
    std::cout << "median callback duration was " << median << " milliseconds, "
              << millisecondsToPercentage(44100, procbufsize, median) << "%\n";
    auto avg = std::accumulate(benchmarks.begin(), benchmarks.end(), 0.0) / benchmarks.size();
    std::cout << "average callback duration was " << avg << " milliseconds, "
              << millisecondsToPercentage(44100, procbufsize, avg) << "%\n";
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

template <typename Func> struct GraphingOptions
{
    Func thef;
    GraphingOptions() {}
    GraphingOptions withFunction(Func f)
    {
        GraphingOptions result;
        result.thef = f;
        return result;
    }
};

template <typename F>
inline void graphFunction(F &&func, juce::Graphics &g, int destwidth, int destheigth,
                          double funcrange_start, double funcrange_end, double func_y_range_start,
                          double func_y_range_end)
{
    juce::Path path;
    bool has_started = false;
    for (int i = 0; i < destwidth; ++i)
    {
        double xcor = i;
        double fx = juce::jmap<double>(i, 0, destwidth, funcrange_start, funcrange_end);
        double fy = func(fx);
        double ycor = juce::jmap<double>(fy, func_y_range_start, func_y_range_end, 0, destheigth);
        if (std::isfinite(ycor))
        {
            if (!has_started)
            {
                path.startNewSubPath(xcor, ycor);
                has_started = true;
            }

            else
                path.lineTo(xcor, ycor);
        }
    }
    g.strokePath(path, juce::PathStrokeType{1.0f});
}

inline void saveImageAsPNG(juce::Image &img, juce::File outfile, bool deleteExisting = true)
{
    juce::PNGImageFormat format;
    if (deleteExisting)
        outfile.deleteFile();
    else
        outfile = outfile.getNonexistentSibling();
    auto ostream = outfile.createOutputStream();
    format.writeImageToStream(img, *ostream);
}

inline void test_graphing()
{
    juce::ScopedJuceInitialiser_GUI ginit;
    juce::Image img{juce::Image::PixelFormat::RGB, 1500, 700, true};
    juce::Graphics g{img};
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    graphFunction([](double x) { return std::sin(4.2 * std::sin(x * 1.13)); }, g, img.getWidth(),
                  img.getHeight(), -6.2, 6.2, -1, 1);
    juce::File outfile{R"(C:\Users\teemu\Pictures\graphtest.png)"};
    saveImageAsPNG(img, outfile, false);
}

inline void test_uniform_distances()
{
    juce::ScopedJuceInitialiser_GUI ginit;
    int numbins = 1001;
    juce::Image img{juce::Image::PixelFormat::RGB, numbins, 700, true};
    juce::Graphics g{img};
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    std::mt19937 gen;
    std::uniform_real_distribution<double> dist{0.0, 1.0};
    int n = 10000000;

    std::vector<int> histo;
    histo.resize(numbins);
    int maxbinvalue = 0;
    for (int i = 0; i < n - 1; ++i)
    {
        double x0 = dist(gen);
        double x1 = dist(gen);
        double diff = x1 - x0;
        int index = jmap<double>(diff, -1.0, 1.0, 0, histo.size() - 1);
        jassert(index >= 0 && index < histo.size());
        ++histo[index];
        maxbinvalue = std::max(histo[index], maxbinvalue);
    }
    double scaler = (double)img.getHeight() / maxbinvalue;
    for (int i = 0; i < histo.size(); ++i)
    {
        g.drawLine(i, img.getHeight(), i, img.getHeight() - histo[i] * scaler);
    }
    juce::File outfile{R"(C:\Users\teemu\Pictures\histotest.png)"};
    saveImageAsPNG(img, outfile, true);
}

inline void vgBasicTests(choc::test::TestProgress &progress)
{
    {
        // just check the engine can be created and destroyed
        CHOC_TEST(Create and destroy);
        try
        {
            // heap allocation may be preferred as the object is kind of large
            auto vg = std::make_unique<XenVintageGranular>(7426);
        }
        catch (const std::exception &e)
        {
            CHOC_FAIL(e.what());
        }
    }
    {
        // We only test here that "some" sound is outputted with minimal setup,
        // when we run for 2 seconds, not that it is "right" as such!
        CHOC_TEST(Outputs audio with minimal setup);
        auto vg = std::make_unique<XenVintageGranular>(7426);
        float sr = 44100;
        float testlen = 2;
        vg->setSampleRate(sr);
        juce::AudioBuffer<float> buf(2, testlen * sr);
        buf.clear();
        float outframe[2] = {0.0f, 0.0f};
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            vg->process(outframe);
            buf.setSample(0, i, outframe[0]);
            buf.setSample(1, i, outframe[1]);
        }
        float rms = buf.getRMSLevel(0, 0, buf.getNumSamples());
        rms += buf.getRMSLevel(1, 0, buf.getNumSamples());
        CHOC_EXPECT_TRUE(rms > 0.01f);
    }
}

inline void runVintageGranularTests()
{
    choc::test::TestProgress progress;
    CHOC_CATEGORY(VintageGranular basic tests);
    vgBasicTests(progress);
}

struct myarrtestobject
{
    // myarrtestobject() {}
    myarrtestobject(int x) : m_x(x) {}
    int m_x = 0;
};

void test_array_init()
{
    std::vector<myarrtestobject> arr{16ULL, myarrtestobject{42}};

    for (auto &e : arr)
    {
        std::cout << e.m_x << "\n";
    }
}

int main()
{
    // runVintageGranularTests();
    // test_sst_tuning();
    // test_xenoscore();
    // test_shadowing();
    // test_dropped_samples();
    // test_xen_grains();
    // test_vintage_grains();
    // test_jsonparse();
    // test_graphing();
    // test_uniform_distances();
    test_array_init();
    return 0;
}
