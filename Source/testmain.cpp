#include "Xenos.h"

std::unique_ptr<juce::AudioFormatWriter> makeWavWriter(juce::File outfile, int chans, double sr)
{
    outfile.deleteFile();
    auto os = outfile.createOutputStream();
    juce::WavAudioFormat wavf;
    auto writer = wavf.createWriterFor(os.release(),sr,chans,32,{},0);
    return std::unique_ptr<juce::AudioFormatWriter>(writer);
}

void test_xenoscore()
{
    XenosCore core;
    double sr = 44100.0;
    core.initialize(sr);
    int nsamples = sr * 5;
    juce::AudioBuffer<float> buffer(1,nsamples);
    auto writer = makeWavWriter(juce::File(R"(C:\develop\xenos\out.wav)"),1,sr);
    for (int i=0;i<nsamples;++i)
    {
        buffer.setSample(0,i,core() * 0.5);
    }
    writer->writeFromAudioSampleBuffer(buffer,0,nsamples);
}

int main()
{
    test_xenoscore();
    return 0;
}
