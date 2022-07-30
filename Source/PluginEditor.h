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

struct ValueHolder : juce::Timer
{
    ValueHolder();
    ~ValueHolder() override;
    void timerCallback() override;
    void setThreshold(float th);
    void updateHeldValue(float v);
    void setHoldDuration(int ms) { durationToHoldForMs = ms; }
    float getCurrentValue() const { return currentValue; }
    float getHeldValue() const { return heldValue; }
    bool getIsOverThreshold() const { return isOverThreshold; }
private:
    float threshold = 0;
    float currentValue = NEGATIVE_INFINITY;
    float heldValue = NEGATIVE_INFINITY;
    juce::int64 timeOfPeak;
    int durationToHoldForMs { 500 };
    bool isOverThreshold { false };
};

struct TextMeter : juce::Component
{
    TextMeter();
    void paint(juce::Graphics& g) override;
    void update(float valueDb);
private:
    float cachedValueDb;
    ValueHolder valueHolder;
};

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
    TextMeter textMeter;
    Meter meter;
    DbScale dbScale;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PFM10AudioProcessorEditor)
};
