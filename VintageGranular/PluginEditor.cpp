#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    juce::ignoreUnused(processorRef);
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize(1200, 600);
    startTimerHz(10);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {}

void AudioPluginAudioProcessorEditor::mouseDown(const juce::MouseEvent &ev)
{
    auto &geng = processorRef.m_geng;
    int screentouse = geng.m_cur_screen;
    screentouse = (screentouse + 1) % 8;
    geng.m_cur_screen = screentouse;
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint(juce::Graphics &g)
{
    auto &geng = processorRef.m_geng;
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    GrainInfo grain;
    while (geng.grains_to_gui_fifo.pop(grain))
    {
        double xcor = juce::jmap<double>(grain.pitch, 24.0, 115.0, 0, getWidth());
        double ycor = juce::jmap<double>(grain.volume, -36.0, 12.0, getHeight(), 20);
        g.fillEllipse(xcor, ycor, 5, 5);
    }

    g.drawText(juce::String(geng.m_cur_screen) + " " +
                   juce::String((int)geng.grains_to_gui_fifo.getUsedSlots()),
               0, 0, 150, 20, juce::Justification::centredLeft);
    g.setColour(juce::Colours::green);
    double cpuload = processorRef.m_cpu_load.getLoadAsProportion();
    cpuload = juce::jmap<double>(cpuload, 0.0, 1.0, 0, 500);
    g.drawRect(50,1,500,15);
    g.fillRect(50,1,cpuload,15);
    for (int i = 0; i < 16; ++i)
    {
        g.drawLine(getWidth() / 16.0 * i, 20.0, getWidth() / 16.0 * i, getHeight());
    }
    for (int i = 0; i < 4; ++i)
    {
        double ycor = juce::jmap<double>(i, 0, 4, 20.0, getHeight());
        g.drawLine(0, ycor, getWidth(), ycor);
    }
}

void AudioPluginAudioProcessorEditor::timerCallback() { repaint(); }

void AudioPluginAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}
