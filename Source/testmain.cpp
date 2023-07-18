#include "Xenos.h"
#include "Tunings.h"

std::unique_ptr<juce::AudioFormatWriter> makeWavWriter(juce::File outfile, int chans, double sr)
{
    outfile.deleteFile();
    auto os = outfile.createOutputStream();
    juce::WavAudioFormat wavf;
    auto writer = wavf.createWriterFor(os.release(), sr, chans, 32, {}, 0);
    return std::unique_ptr<juce::AudioFormatWriter>(writer);
}

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
            double quantizedhz = findClosestFrequency(tuning,hz);
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

class ShadowTest
{
public:
    ShadowTest(int foo)
    {
        foo = foo;
    }
    int foo = 0;
};

void test_shadowing()
{
    ShadowTest s(42);
    std::cout << s.foo << "\n";
}

int main()
{
    //test_sst_tuning();
    //test_xenoscore();
    test_shadowing();
    return 0;
}
