#include "PluginProcessor.h"
#include "PluginEditor.h"

#ifdef USING_PLUGINMAGIC
//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(VintageGranularAudioProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p), m_grain_comp(p), m_generic_editor(p)
{
    juce::ignoreUnused(processorRef);
    addAndMakeVisible(m_grain_comp);
    addAndMakeVisible(m_generic_editor);
    setSize(1200, 700);
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
    m_grain_comp.setBounds(3, 21, getWidth() - 6, getHeight() - 123);
    m_generic_editor.setBounds(3, m_grain_comp.getBottom(), getWidth() - 6, 99);
}
#endif

void GrainScreenComponent::mouseDown(const juce::MouseEvent &ev)
{
    if (ev.mods.isLeftButtonDown())
    {
        m_sel_cell_x = juce::jmap<float>(ev.x, 0, getWidth(), 0, 16);
        m_sel_cell_y = juce::jmap<float>(ev.y, 0, getHeight(), 0, 4);
        return;
    }
    auto &geng = m_proc.m_eng;
    if (ev.mods.isRightButtonDown() && !geng.isAutoScreenSelectActive())
    {
        juce::PopupMenu menu;
        menu.addItem("Randomize cells", [&geng]() {
            geng.m_gui_to_audio_fifo.push({GuiToAudioActionType::RandomizeCells,
                                           geng.getCurrentlyPlayingScreen(), 0, 0, 0.0f});
        });
        menu.addItem("Clear cells", [&geng]() {
            geng.m_gui_to_audio_fifo.push({GuiToAudioActionType::ClearAllCells,
                                           geng.getCurrentlyPlayingScreen(), 0, 0, 0.0f});
        });
        menu.showMenuAsync(juce::PopupMenu::Options{});
    }
}
