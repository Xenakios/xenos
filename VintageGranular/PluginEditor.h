#pragma once

#include "PluginProcessor.h"

class GrainScreenComponent : public juce::Component, public juce::Timer
{
  public:
    GrainScreenComponent(VintageGranularAudioProcessor &p) : m_proc(p) { startTimerHz(10); }
    void timerCallback() override { repaint(); }
    void paint(juce::Graphics &g) override
    {
        auto &geng = m_proc.m_eng;
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::white);

        GrainVisualizationInfo grain;
        while (geng.m_grains_to_gui_fifo.pop(grain))
        {
            double xcor = juce::jmap<double>(grain.pitch, 24.0, 115.0, 0, getWidth());
            double ycor = juce::jmap<double>(grain.volume, -40.0, 0.0, getHeight(), 0);
            g.fillEllipse(xcor, ycor, 5, 5);
        }

        for (int i = 0; i < 17; ++i)
        {
            g.drawLine(getWidth() / 16.0 * i, 0.0, getWidth() / 16.0 * i, getHeight());
        }
        for (int i = 0; i < 5; ++i)
        {
            double ycor = juce::jmap<double>(i, 0, 4, 0.0, getHeight());
            g.drawLine(0, ycor, getWidth(), ycor);
        }
    }
    void mouseDown(const juce::MouseEvent &ev) override
    {
        auto &geng = m_proc.m_eng;
        int screentouse = geng.m_cur_screen;
        screentouse = (screentouse + 1) % 8;
        geng.m_cur_screen = screentouse;
    }

  private:
    VintageGranularAudioProcessor &m_proc;
};

//==============================================================================
class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer
{
  public:
    explicit AudioPluginAudioProcessorEditor(VintageGranularAudioProcessor &);
    ~AudioPluginAudioProcessorEditor() override;
    void timerCallback() override;
    //==============================================================================
    void paint(juce::Graphics &) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent &ev) override;

  private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    VintageGranularAudioProcessor &processorRef;
    GrainScreenComponent m_grain_comp;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
