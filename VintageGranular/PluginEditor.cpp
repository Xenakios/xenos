#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(VintageGranularAudioProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p), m_grain_comp(p)
{
    juce::ignoreUnused(processorRef);
    addAndMakeVisible(m_grain_comp);
    setSize(1200, 600);
    startTimerHz(20);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {}

void AudioPluginAudioProcessorEditor::mouseDown(const juce::MouseEvent &ev) {}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint(juce::Graphics &g)
{
    auto &geng = processorRef.m_eng;
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.drawText(juce::String(geng.m_cur_screen) + " " +
                   juce::String((int)geng.m_grains_to_gui_fifo.getUsedSlots()),
               0, 0, 150, 20, juce::Justification::centredLeft);
    g.setColour(juce::Colours::green);
    double cpuload = processorRef.m_cpu_load.getLoadAsProportion();
    cpuload = juce::jmap<double>(cpuload, 0.0, 1.0, 0, 500);
    g.drawRect(50, 1, 500, 15);
    g.fillRect(50, 1, cpuload, 15);
}

void AudioPluginAudioProcessorEditor::timerCallback() { repaint(0, 0, getWidth(), 20); }

void AudioPluginAudioProcessorEditor::resized()
{
    m_grain_comp.setBounds(3, 21, getWidth() - 6, getHeight() - 23);
}
