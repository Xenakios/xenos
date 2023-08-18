#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (900, 500);
    startTimerHz(20);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto& geng = processorRef.m_geng;
    if (geng.spinlock.tryEnter())
    {
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::white);
        for (auto& grain : geng.grains_to_play)
        {
            double xcor = juce::jmap<double>(grain.pitch,24.0,115.0,0,getWidth());
            double ycor = juce::jmap<double>(grain.volume,-40,-6.0,getHeight(),20);
            g.fillEllipse(xcor,ycor,3,3);
        }
        geng.spinlock.exit();
        char screenchar = geng.m_cur_screen + 65;
        g.drawText(juce::String(screenchar),0,0,40,20,juce::Justification::centredLeft);
        g.setColour(juce::Colours::green);
        for (int i=0;i<16;++i)
        {
            g.drawLine(getWidth()/16.0*i,20.0, getWidth()/16.0*i,getHeight());
        }
        for (int i=0;i<4;++i)
        {
            double ycor = juce::jmap<double>(i,0,4,20.0,getHeight());
            g.drawLine(0,ycor,getWidth(),ycor);
        }
    }
    
}

void AudioPluginAudioProcessorEditor::timerCallback()
{
    repaint();
}

void AudioPluginAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}
