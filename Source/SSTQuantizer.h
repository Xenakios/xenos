#pragma once

#include <JuceHeader.h>
#include "libMTSClient.h"
#include "Tunings.h"

#define SCALE_PRESETS (14)

// obviously need a better solution for this...
// i guess we could precalculate all the possible frequencies into a sorted vector/array
// and do a binary search
inline double findClosestFrequency(const Tunings::Tuning &tuning, double sourceFrequency,
                                   MTSClient *mts)
{
    double lastdiff = 10000000.0;
    double found = 0.0;
    int notesToScan = 256;
    if (mts)
        notesToScan = 128;
    for (int i = 0; i < notesToScan; ++i)
    {
        double hz = 0.0;
        if (!mts)
            hz = tuning.frequencyForMidiNote(i);
        else
            hz = MTS_NoteToFrequency(mts, i, -1);
        double diff = std::abs(hz - sourceFrequency);
        if (diff < lastdiff)
        {
            lastdiff = diff;
            found = hz;
        }
    }

    return found;
}

struct Quantizer2
{
    MTSClient *mts_client = nullptr;
    Quantizer2()
    {
        mts_client = MTS_RegisterClient();
        initScalePresets();
    }
    ~Quantizer2() { MTS_DeregisterClient(mts_client); }

    double getHzForMidiNote(int note)
    {
        if (use_oddsound && mts_client && MTS_HasMaster(mts_client))
            return MTS_NoteToFrequency(mts_client, note, -1);
        return tuning.frequencyForMidiNote(note);
    }
    bool use_oddsound = true;
    static Tunings::Scale scaleFromRatios(std::vector<double> ratios)
    {
        try
        {
            std::stringstream ss;
            ss << "! Xenos preset\n";
            ss << "!\n";
            ss << "Xenos preset\n";
            ss << ratios.size() << "\n";
            ss << "!\n";
            char buf[100];
            for (auto &ratio : ratios)
            {
                double cents = 1200.0 * std::log2(ratio);
                // because of how the Scala format works we need floating point cents
                // always with the decimal point, so can't stream the float directly
                sprintf_s(buf, "%f", cents);
                ss << buf << "\n";
            }
            auto str = ss.str();
            return Tunings::parseSCLData(str);
        }
        catch (const std::exception &e)
        {
            DBG(e.what());
        }
        return Tunings::evenTemperament12NoteScale();
    }
    std::vector<Tunings::Scale> scalePresets;
    void setScale(int index, Tunings::KeyboardMapping &kbm)
    {
        jassert(index >= 0 && index < scalePresets.size());
        auto &scale = scalePresets[index];
        // auto kbm = Tunings::startScaleOnAndTuneNoteTo(0, 69, 440.0);
        tuning = Tunings::Tuning(scale, kbm);
    }
    juce::String loadScalaFile(juce::File fn, Tunings::KeyboardMapping &kbm)
    {
        auto str = fn.getFullPathName().toStdString();
        try
        {
            auto scale = Tunings::readSCLFile(str);
            scalePresets.back() = scale;
            tuning = Tunings::Tuning(scale, kbm);
            return "";
        }
        catch (std::exception &ex)
        {
            return ex.what();
        }
        return "";
    }
    void initScalePresets()
    {
        scalePresets.clear();
        scalePresets.push_back(
            scaleFromRatios({1.122462, 1.259921, 1.498307, 1.681793, 2.0})); // pentatonic
        scalePresets.push_back(
            scaleFromRatios({1.125, 1.265625, 1.5, 1.6875, 2.0})); // pentatonic (pythagorean)
        scalePresets.push_back(
            scaleFromRatios({1.189207, 1.33484, 1.414214, 1.498307, 1.781797, 2.0})); // blues

        scalePresets.push_back(
            scaleFromRatios({1.166667, 1.333333, 1.4, 1.5, 1.75, 2.0})); // blues (7-limit)
        scalePresets.push_back(
            scaleFromRatios({1.122462, 1.259921, 1.414214, 1.587401, 1.781797, 2.0})); // whole tone
        scalePresets.push_back(scaleFromRatios(
            {1.122462, 1.259921, 1.33484, 1.498307, 1.681793, 1.887749, 2.0})); // major
        scalePresets.push_back(
            scaleFromRatios({1.125, 1.25, 1.333333, 1.5, 1.666667, 1.875, 2.0})); // major 5limit
        scalePresets.push_back(scaleFromRatios(
            {1.122462, 1.189207, 1.33484, 1.498307, 1.587401, 1.781797, 2.0})); // minor
        scalePresets.push_back(
            scaleFromRatios({1.125, 1.2, 1.333333, 1.5, 1.6, 1.777778, 2.0})); // minor 5limit
        scalePresets.push_back(scaleFromRatios({1., 1.122462, 1.189207, 1.33484, 1.414214, 1.587401,
                                                1.681793, 1.887749, 2.0})); // octatonic
        scalePresets.push_back(
            scaleFromRatios({1.125, 1.25, 1.375, 1.5, 1.625, 1.75, 1.875, 2.0})); // overtone
        scalePresets.push_back(Tunings::evenTemperament12NoteScale());            // chromatic
        scalePresets.push_back(scaleFromRatios({1.088182, 1.18414, 1.288561, 1.402189, 1.525837,
                                                1.660388, 1.806806, 1.966134, 2.139512, 2.328178,
                                                2.533484, 2.756892, 3.0}));   // bohlen-pierce
        scalePresets.push_back(Tunings::evenDivisionOfCentsByM(1200.0f, 24)); // quarter-tone
        auto scale = Tunings::readSCLFile(R"(C:\develop\xenos\scala_scales\major_chord_ji.scl)");
        scalePresets.push_back(scale);
        jassert(scalePresets.size() == SCALE_PRESETS + 1);
    }
    double quantizeHz(double sourceHz)
    {
        if (!active)
            return sourceHz;
        MTSClient *temp = nullptr;
        if (use_oddsound && mts_client && MTS_HasMaster(mts_client))
            temp = mts_client;
        double hz = findClosestFrequency(tuning, sourceHz, temp);
        if (hz > 0.0)
            return hz;
        return sourceHz;
    }
    bool active = false;
    Tunings::Tuning tuning;
};
