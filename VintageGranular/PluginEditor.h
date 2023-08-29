#pragma once

#include "PluginProcessor.h"

class GrainScreenComponent : public juce::Component, public juce::Timer
{
  public:
    enum ColourIDs
    {
        // we are safe from collissions, because we set the colours on every component directly from
        // the stylesheet
        backgroundColourId,
        drawColourId,
        fillColourId
    };

    GrainScreenComponent(VintageGranularAudioProcessor &p) : m_proc(p) { startTimerHz(10); }
    ~GrainScreenComponent() override { m_proc.m_eng.setVisualizationEnabled(false); }
    void timerCallback() override { repaint(); }
    void paint(juce::Graphics &g) override
    {
        // m_proc.m_eng.setVisualizationEnabled(true);
        auto &geng = m_proc.m_eng;
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::yellow);
        g.setFont(60.0);
        g.drawText(juce::String(geng.getCurrentlyPlayingScreen()), 0, 0, getWidth(), getHeight(),
                   juce::Justification::centred);
        g.setColour(juce::Colours::white);

        float grainRadius = 5.0f;
        GrainVisualizationInfo grain;
        auto prange = geng.getPitchRange();
        while (geng.m_grains_to_gui_fifo.pop(grain))
        {
            double xcor =
                juce::jmap<double>(grain.pitch, prange.getStart(), prange.getEnd(), 0, getWidth());
            double ycor = juce::jmap<double>(grain.volume, -40.0, 0.0, getHeight(), 0);
            g.fillEllipse(xcor - grainRadius / 2, ycor - grainRadius / 2, grainRadius, grainRadius);
        }
        for (int i = 0; i < 16; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                float xcor = getWidth() / 16.0 * i;
                float ycor = getHeight() / 4.0 * j;
                float w = getWidth() / 16.0;
                float h = getHeight() / 4.0;
                bool active = geng.m_screensdata[geng.getCurrentlyPlayingScreen()][i][3 - j] > 0.0f;
                if (m_sel_cell_x == i && m_sel_cell_y == j)
                {
                    g.setColour(juce::Colours::cyan.withAlpha(0.5f));
                    active = true;
                }
                else if (active)
                    g.setColour(juce::Colours::cyan.withAlpha(0.2f));
                if (active)
                    g.fillRect(xcor, ycor, w, h);
            }
        }

        g.setColour(juce::Colours::white);
        for (int i = 0; i < 17; ++i)
        {
            g.drawLine(getWidth() / 16.0 * i, 0.0, getWidth() / 16.0 * i, getHeight());
        }
        for (int i = 0; i < 5; ++i)
        {
            double ycor = juce::jmap<double>(i, 0, 4, 0.0, getHeight());
            g.drawLine(0, ycor, getWidth(), ycor);
        }
        g.setColour(juce::Colours::green.withAlpha(0.5f));
        double cpuload = m_proc.m_cpu_load.getLoadAsProportion();
        cpuload = juce::jmap<double>(cpuload, 0.0, 1.0, 0, getWidth() / 2.0);
        g.drawRect(1, 1, 500, 15);
        g.fillRect(1, 1, cpuload, 15);
    }
    void visibilityChanged() override { m_proc.m_eng.setVisualizationEnabled(isVisible()); }
    void mouseDown(const juce::MouseEvent &ev) override
    {
        m_sel_cell_x = juce::jmap<float>(ev.x, 0, getWidth(), 0, 16);
        m_sel_cell_y = juce::jmap<float>(ev.y, 0, getHeight(), 0, 4);
        return;
        auto &geng = m_proc.m_eng;
        if (!geng.isAutoScreenSelectActive())
        {
            geng.m_gui_to_audio_fifo.push({GuiToAudioActionType::ClearAllCells,
                                           geng.getCurrentlyPlayingScreen(), 0, 0, 0.0f});
        }
    }

  private:
    VintageGranularAudioProcessor &m_proc;
    int m_sel_cell_x = -1;
    int m_sel_cell_y = -1;
};

// This class is creating and configuring your custom component
class GrainVisualizerItem : public foleys::GuiItem
{
  public:
    FOLEYS_DECLARE_GUI_FACTORY(GrainVisualizerItem)

    GrainVisualizerItem(foleys::MagicGUIBuilder &builder, const juce::ValueTree &node)
        : foleys::GuiItem(builder, node)
    {
        // Create the colour names to have them configurable
        setColourTranslation({{"grainscreen-background", GrainScreenComponent::backgroundColourId},
                              {"grainscreen-draw", GrainScreenComponent::drawColourId},
                              {"grainscreen-fill", GrainScreenComponent::fillColourId}});
        grainvis = std::make_unique<GrainScreenComponent>(
            *static_cast<VintageGranularAudioProcessor *>(builder.getMagicState().getProcessor()));
        addAndMakeVisible(*grainvis);
    }

    std::vector<foleys::SettableProperty> getSettableProperties() const override
    {
        std::vector<foleys::SettableProperty> newProperties;

        // newProperties.push_back ({ configNode, "factor", foleys::SettableProperty::Number, 1.0f,
        // {} });

        return newProperties;
    }

    // Override update() to set the values to your custom component
    void update() override
    {
        // auto factor = getProperty ("factor");
        // lissajour.setFactor (factor.isVoid() ? 3.0f : float (factor));
    }

    juce::Component *getWrappedComponent() override { return grainvis.get(); }

  private:
    std::unique_ptr<GrainScreenComponent> grainvis;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GrainVisualizerItem)
};

#ifdef USING_PLUGINMAGIC
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
    juce::GenericAudioProcessorEditor m_generic_editor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
#endif
