/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

#ifdef  MAX_DECIBELS
#undef  MAX_DECIBELS
#endif
#define MAX_DECIBELS 12.f

#ifdef  NEGATIVE_INFINITY
#undef  NEGATIVE_INFINITY
#endif
#define NEGATIVE_INFINITY -66.f

struct Meter : juce::Component
{
    void paint (juce::Graphics&) override;
    void update(float dbLevel);
private:
    float dbPeak { NEGATIVE_INFINITY };
};

struct Tick
{
    float db { 0.f };
    int y { 0 };
};

struct DbScale : juce::Component
{
    ~DbScale() override = default;
    void paint (juce::Graphics& g) override;
    void buildBackgroundImage(int dbDivision, juce::Rectangle<int> meterBounds, int minDb, int maxDb);
    static std::vector<Tick> getTicks(int dbDivision, juce::Rectangle<int> meterBounds, int minDb, int maxDb);
private:
    juce::Image bkgd;
};

//==============================================================================
/**
*/
class PFM10AudioProcessorEditor  : public juce::AudioProcessorEditor, juce::Timer
{
public:
    PFM10AudioProcessorEditor (PFM10AudioProcessor&);
    ~PFM10AudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    //==============================================================================
    void timerCallback() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PFM10AudioProcessor& audioProcessor;
    
    juce::AudioBuffer<float> editorAudioBuffer;
    Meter meter;
    DbScale dbScale;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PFM10AudioProcessorEditor)
};
