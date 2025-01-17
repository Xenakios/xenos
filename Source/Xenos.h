/*
  ==============================================================================

    Xenos.h

    Xenos: Xenharmonic Stochastic Synthesizer
    Raphael Radna
    This code is licensed under the GPLv3

  ==============================================================================
*/

#pragma once

#include <iostream>
#include <random>
#include "Quantizer.h"
#include "RandomWalk.h"
#include "RandomSource.h"
#include "Utility.h"
#include "sst/basic-blocks/dsp/PanLaws.h"
#include "sst/basic-blocks/modulators/SimpleLFO.h"
#include "SSTQuantizer.h"
#include "somedsp.h"

#define MAX_POINTS (128)
#define NUM_VOICES (128)

struct XenosCore
{
    void initialize(double sr)
    {
        sampleRate = sr;
        pitchWalk.initialize(MAX_POINTS);
        ampWalk.initialize(MAX_POINTS);
        ampWalk.setParams(1.0);
        reset();
        hzSmoothingFilter.setParameters(BiquadFilter::LOWPASS_1POLE, 16.0 / sr, 1.0, 1.0);
    }
    int tuningUpdateRate = 256;
    int tuningUpdateCounter = 0;
    void reset()
    {
        for (unsigned i = 0; i < MAX_POINTS; ++i)
        {
            pitchWalk.reset(i, mtos(pitchCenter, sampleRate) / nPoints);
            // ampWalk.reset(i, uniform(generator));
            // ampWalk.reset(i, 0.0f);
            float ph = juce::jmap<float>(i, 0, nPoints, 0.0f, M_PI * 2);
            ampWalk.reset(i, std::sin(ph));
        }
        calcMetaParams();
    }

    void calcMetaParams()
    {
        calcPeriodRange(pitchCenter, pitchWidthKeys);
        pitchWalk.setParams(periodRange, nPoints);
        quantizer.calcSteps();
    }

    void calcPeriodRange(double pC, double pW)
    {
        double min = pC - pW / 2;
        double max = pC + pW / 2;
        periodRange[0] = mtos(min, sampleRate);
        periodRange[1] = mtos(max, sampleRate);
        quantizer.setRange(max, min);
    }
    double curHz = 440.0;
    double curQuantizedHz = 440.0;
    BiquadFilter hzSmoothingFilter;
    double operator()()
    {
        if (tuningUpdateCounter == 0)
        {
            if (quan2->use_oddsound && quan2->mts_client && MTS_HasMaster(quan2->mts_client))
                setPitchCenter(pitchCenterAsKey);
        }
        ++tuningUpdateCounter;
        if (tuningUpdateCounter == tuningUpdateRate)
            tuningUpdateCounter = 0;

        if (index < 0.0)
            index = 0.0;
        int intdex = floor(index);
        double quanfactor = curHz / curQuantizedHz;
        quanfactor = hzSmoothingFilter.process(quanfactor);
        quanfactor = juce::jlimit(0.25, 4.0, quanfactor);
        // double segmentSamps = pitchWalk(intdex, quantizer.getFactor()) * bend;
        double segmentSamps = pitchWalk(intdex, quanfactor) * bend;
        double increment = 1 / segmentSamps;

        if (intdex != _index)
        {
            step(_index);
            _index = intdex;
        }

        index += increment;

        if (index >= nPoints)
        {
            // quantizer.setFactor(pitchWalk.getSumPeriod());

            index -= nPoints;
            if (nPoints_ > 0)
                setNPoints();
            curHz = sampleRate / pitchWalk.getSumPeriod();
            curQuantizedHz = quan2->quantizeHz(curHz);
        }
        return ampWalk(index, nPoints);
    }

    void step(int n)
    {
        pitchWalk.step(n, pitchSource());
        ampWalk.step(n, ampSource());
    }

    void setNPoints()
    {
        nPoints = nPoints_;
        calcMetaParams();
        nPoints_ = 0;
    }
    double pitchCenterAsKey = 0.0;

    void setPitchCenter(float pC)
    {
        pitchCenterAsKey = pC;
        if (quan2->use_oddsound && quan2->mts_client && MTS_HasMaster(quan2->mts_client))
        {
            double diff = MTS_RetuningInSemitones(quan2->mts_client, pC, -1);
            pitchCenter = pC + diff;
        }
        else
        {
            pitchCenter = quan2->tuning.logScaledFrequencyForMidiNote(pC) * 12.0;
        }
        curHz = quan2->getHzForMidiNote(pC);
        calcMetaParams();
    }

    void setPitchWidth(float pW)
    {
        pitchWidthKeys = pW;
        calcMetaParams();
    }

    void setBend(double b, double r = 2.0)
    {
        if (b > 8191)
        {
            bend = scale(b, 8192, 16383, 1.0, 1.0 / r);
        }
        else
        {
            bend = scale(b, 0, 8191, r, 1.0);
        }
    }

    double sampleRate = 44100.0;
    float pitchCenter = 48.0f;   // midi pitch
    float pitchWidthKeys = 1.0f; // keys
    double periodRange[2];
    double bend = 1.0;
    int nPoints = 12;
    int nPoints_ = 0;
    double index = 0.0;
    int _index = 0;
    RandomWalk pitchWalk, ampWalk;
    RandomSource pitchSource, ampSource;
    Quantizer quantizer;
    Quantizer2 *quan2 = nullptr;
    std::default_random_engine generator;
    std::uniform_real_distribution<double> uniform{-1.0, 1.0};
};

enum class VoicePanMode
{
    AlwaysCenter,
    Random,
    AltLeftRight,
    AltLeftCenterRight,
    Sine1Cycle,
    Sine2Cycles,
    AltLeftRight2,
    AltLeftCenterRight2,
    RandomPerVoice1,
    RandomPerVoice2,
    RandomPerVoice3,
    RandomPerVoice4,
    Last
};

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

//==============================================================================
struct XenosSound : public juce::SynthesiserSound
{
    XenosSound() { rng = std::minstd_rand((unsigned int)this); }

    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }

    std::minstd_rand rng;
    std::uniform_real_distribution<float> panDistribution{0.0f, 1.0f};
    // returns value in range 0.0 to 1.0, 0.5 center
    float getPanPositionFromMidiKey(int key, VoicePanMode vpm, int *notecounter)
    {
        const float positions[3] = {0.0f, 0.5f, 1.0f};
        if (vpm == VoicePanMode::AltLeftRight)
            return (float)(key % 2);
        else if (vpm == VoicePanMode::AltLeftCenterRight)
            return positions[key % 3];
        else if (vpm == VoicePanMode::Random)
            return panDistribution(rng);
        else if (vpm == VoicePanMode::Sine1Cycle)
            return 0.5 + 0.5 * std::sin(2 * juce::MathConstants<float>::pi / 128 * key * 5.0f);
        else if (vpm == VoicePanMode::Sine2Cycles)
            return 0.5 + 0.5 * std::sin(2 * juce::MathConstants<float>::pi / 128 * key * 10.0f);
        else if (vpm == VoicePanMode::AltLeftRight2 && notecounter != nullptr)
            return (*notecounter) % 2;
        else if (vpm == VoicePanMode::AltLeftCenterRight2 && notecounter != nullptr)
            return positions[(*notecounter) % 3];
        return 0.5f;
    }
};

//==============================================================================
struct XenosVoice : public juce::SynthesiserVoice
{
    SRProvider *srprovider = nullptr;
    XenosVoice(int *notecounter_, SRProvider *sp, Quantizer2 *qnt)
        : srprovider(sp), noteCounter(notecounter_)
    {
        xenos.quan2 = qnt;
        lfo1 = std::make_unique<LFOType>(sp);
    }

    bool canPlaySound(juce::SynthesiserSound *sound) override
    {
        return dynamic_cast<XenosSound *>(sound) != nullptr;
    }

    void setCurrentPlaybackSampleRate(double newRate) override
    {
        if (newRate > 0.0)
        {
            xenos.initialize(newRate);
            adsr.setSampleRate(newRate);
            updateADSR();
            srprovider->samplerate = newRate;
            srprovider->initTables();
        }
    }

    void updateADSR() { adsr.setParameters(juce::ADSR::Parameters(a, d, s, r)); }

    void startNote(int note, float velocity, juce::SynthesiserSound *snd,
                   int currentPitchWheelPosition) override
    {
        xenos.setPitchCenter(note);
        xenos.setBend(currentPitchWheelPosition);
        adsr.noteOn();
        xenos.reset();
        lfo_updatecounter = 0;
    }

    void stopNote(float /*velocity*/, bool allowTailOff) override
    {
        if (adsr.isActive())
        {
            adsr.noteOff();
            // clearCurrentNote();
        }
    }

    void pitchWheelMoved(int newPitchWheelValue) override { xenos.setBend(newPitchWheelValue); }

    void controllerMoved(int, int) override {}

    void aftertouchChanged(int newAftertouchValue) override
    {
        afterTouchAmount = newAftertouchValue / 127.0;
    }
    float cachedPanPosition = 0.5f;
    void renderNextBlock(juce::AudioSampleBuffer &outputBuffer, int startSample,
                         int numSamples) override
    {
        if (adsr.isActive())
        {

            float atVolume = juce::jmap(afterTouchAmount, 0.0f, 1.0f, 0.0f, 10.0f);
            atVolume = juce::Decibels::decibelsToGain(atVolume);

            const float lfo_pars0[4] = {1.0f, 3.0f, 0.25f, 5.0f};
            const float lfo_pars1[4] = {0.75f, 0.45f, 0.20f, 0.95f};

            int panlfomode = (int)vpm - (int)VoicePanMode::RandomPerVoice1;
            while (--numSamples >= 0)
            {
                if (lfo_updatecounter == 0)
                {
                    float panposition = 0.5f;
                    if (panlfomode >= 0 && panlfomode < 4)
                    {
                        lfo1->process_block(lfo_pars0[panlfomode], lfo_pars1[panlfomode],
                                            LFOType::Shape::SMOOTH_NOISE, false);
                        // we use only the first value from the LFO output block, which is a bit
                        // wasteful but we'd need to do the pan law calculation per sample if we
                        // used all the LFO output...
                        panposition = 0.5f + 0.5f * lfo1->outputBlock[0];
                    }
                    else
                    {
                        auto xsnd = dynamic_cast<XenosSound *>(getCurrentlyPlayingSound().get());
                        panposition = xsnd->getPanPositionFromMidiKey(getCurrentlyPlayingNote(),
                                                                      vpm, noteCounter);
                    }
                    sst::basic_blocks::dsp::pan_laws::monoEqualPower(panposition, panmatrix);
                    cachedPanPosition = panposition;
                }
                ++lfo_updatecounter;
                if (lfo_updatecounter == srprovider->BLOCK_SIZE)
                    lfo_updatecounter = 0;
                float gainLeft = panmatrix[0];
                float gainRight = panmatrix[3];
                auto currentSample = xenos() * adsr.getNextSample() * polyGainFactor * atVolume;
                // for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                outputBuffer.addSample(0, startSample, currentSample * gainLeft);
                outputBuffer.addSample(1, startSample, currentSample * gainRight);
                ++startSample;
            }
        }
        else
        {
            clearCurrentNote();
        }
    }

    XenosCore xenos;
    juce::ADSR adsr;
    using LFOType = sst::basic_blocks::modulators::SimpleLFO<SRProvider, 32>;
    std::unique_ptr<LFOType> lfo1;
    VoicePanMode vpm = VoicePanMode::AlwaysCenter;
    sst::basic_blocks::dsp::pan_laws::panmatrix_t panmatrix;
    int *noteCounter = nullptr;
    float afterTouchAmount = 0.0f;
    float a = 0.1f, d = 0.1f, s = 1.0f, r = 0.1f;
    const double polyGainFactor = 1 / sqrt(NUM_VOICES / 4);
    int lfo_updatecounter = 0;
};

//==============================================================================
class XenosSynth : public juce::Synthesiser
{
  public:
    void noteOn(const int midiChannel, const int midiNoteNumber, const float velocity)
    {
        const juce::ScopedLock sl(lock);

        for (auto *sound : sounds)
        {
            if (sound->appliesToNote(midiNoteNumber) && sound->appliesToChannel(midiChannel))
            {
                juce::SynthesiserVoice *voice =
                    findFreeVoice(sound, midiChannel, midiNoteNumber, isNoteStealingEnabled());
                startVoice(voice, sound, midiChannel, midiNoteNumber, velocity);
                ++noteCounter;
            }
        }
    }
    int noteCounter = 0;

  private:
};

//==============================================================================
class XenosSynthHolder
{
  public:
    SRProvider srProvider;
    Quantizer2 sharedquantizer;
    Tunings::KeyboardMapping sharedKBM;
    XenosSynthHolder(juce::MidiKeyboardState &keyState) : keyboardState(keyState)
    {
        sharedKBM = Tunings::startScaleOnAndTuneNoteTo(69, 69, 440.0);
        for (auto i = 0; i < NUM_VOICES; ++i)
            xenosSynth.addVoice(
                new XenosVoice(&xenosSynth.noteCounter, &srProvider, &sharedquantizer));

        xenosSynth.addSound(new XenosSound());
    }

    void setUsingSineWaveSound() { xenosSynth.clearSounds(); }

    void prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate)
    {
        srProvider.samplerate = sampleRate;
        srProvider.initTables();
        xenosSynth.setCurrentPlaybackSampleRate(sampleRate);
    }

    void processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
    {
        buffer.clear();
        keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
        xenosSynth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
    }

    void setParam(const juce::String &parameterID, float newValue)
    {
        for (int i = 0; i < xenosSynth.getNumVoices(); ++i)
        {
            auto voice = dynamic_cast<XenosVoice *>(xenosSynth.getVoice(i));
            XenosCore &xenos = voice->xenos;
            if (parameterID == "voicePanningMode")
            {
                voice->vpm = (VoicePanMode)(int)newValue;
                // DBG("voice pan mode " << newValue);
            }

            if (parameterID == "segments")
            {
                xenos.nPoints_ = newValue;
            }
            if (parameterID == "pitchWidth")
            {
                xenos.pitchWidthKeys = newValue;
                if (voice->isVoiceActive())
                    xenos.calcMetaParams();
            }
            if (parameterID == "pitchBarrier")
            {
                xenos.pitchWalk.setBarrierRatio(newValue);
            }
            if (parameterID == "pitchStep")
            {
                xenos.pitchWalk.setStepRatio(newValue);
            }
            if (parameterID == "ampGain")
            {
                auto linear = juce::Decibels::decibelsToGain(newValue, -96.0f);
                xenos.ampWalk.setSecBarriers(linear);
                xenos.ampWalk.calcPriBarriers();
                xenos.ampWalk.calcPriStepSize();
            }
            if (parameterID == "ampBarrier")
            {
                xenos.ampWalk.setBarrierRatio(newValue);
            }
            if (parameterID == "ampStep")
            {
                xenos.ampWalk.setStepRatio(newValue);
            }

            if (parameterID == "pitchDistribution")
            {
                xenos.pitchSource.setMode(newValue);
            }
            if (parameterID == "pitchWalk")
            {
                xenos.pitchWalk.setWalk(newValue);
            }
            if (parameterID == "pitchAlpha")
            {
                xenos.pitchSource.setAlpha(newValue);
            }
            if (parameterID == "pitchBeta")
            {
                xenos.pitchSource.setBeta(newValue);
            }

            if (parameterID == "ampDistribution")
            {
                xenos.ampSource.setMode(newValue);
            }
            if (parameterID == "ampWalk")
            {
                xenos.ampWalk.setWalk(newValue);
            }
            if (parameterID == "ampAlpha")
            {
                xenos.ampSource.setAlpha(newValue);
            }
            if (parameterID == "ampBeta")
            {
                xenos.ampSource.setBeta(newValue);
            }

            if (parameterID == "attack")
            {
                voice->a = newValue;
                voice->updateADSR();
            }
            if (parameterID == "decay")
            {
                voice->d = newValue;
                voice->updateADSR();
            }
            if (parameterID == "sustain")
            {
                auto linear = juce::Decibels::decibelsToGain(newValue, -96.0f);
                voice->s = linear;
                voice->updateADSR();
            }
            if (parameterID == "release")
            {
                voice->r = newValue;
                voice->updateADSR();
            }

            if (parameterID == "scale")
            {
                if (newValue == 0)
                {
                    xenos.quantizer.setActive(0);
                    sharedquantizer.active = false;
                }
                else
                {
                    sharedquantizer.setScale(newValue - 1, sharedKBM);
                    sharedquantizer.active = true;
                    xenos.quantizer.setScale(newValue - 1);
                    xenos.quantizer.setActive(1);
                }
            }
            if (parameterID == "root")
            {
                xenos.quantizer.setRoot(newValue);
            }
        }
    }

    bool loadScala(juce::File fn)
    {
        auto err = sharedquantizer.loadScalaFile(fn, sharedKBM);
        return err.isEmpty();
    }
    bool loadScl(juce::StringArray &s, bool load)
    {
        bool result = false;
        if (!s.isEmpty())
        {
            for (int i = 0; i < xenosSynth.getNumVoices(); ++i)
            {
                auto voice = dynamic_cast<XenosVoice *>(xenosSynth.getVoice(i));
                XenosCore &xenos = voice->xenos;
                xenos.quantizer.loadScl(s, load);
            }
            result = true;
        }
        return result;
    }
    XenosSynth xenosSynth;

  private:
    juce::MidiKeyboardState &keyboardState;
};
