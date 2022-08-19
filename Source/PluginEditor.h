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

template<typename T>
struct Averager
{
    Averager(size_t numElements, T initialValue);
    
    void resize(size_t numElements, T initialValue);
    
    void clear(T initialValue);
    
    size_t getSize() const { return elements.size(); }
    
    void add(T t);
    
    float getAvg() const { return avg; }
private:
    std::vector<T> elements;
    std::atomic<float> avg { static_cast<float>(T()) };
    std::atomic<size_t> writeIndex = 0;
    std::atomic<T> sum { 0 };
};

struct DecayingValueHolder : juce::Timer
{
    DecayingValueHolder();
    void updateHeldValue(float input);
    float getHeldValue() const { return heldValue; }
    bool isOverThreshold() const;
    void setHoldTime(int ms);
    void setDecayRate(float dbPerSec);
    void timerCallback() override;
private:
    float heldValue { NEGATIVE_INFINITY };
    juce::int64 peakTime = getNow();
    float threshold = 0.f;
    juce::int64 holdTime = 2000; //2 seconds
    float decayRatePerFrame { 0 };
    float decayRateMultiplier { 1 };
    static juce::int64 getNow();
    void resetDecayRateMultiplier() { decayRateMultiplier = 1; }
};

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
    DecayingValueHolder decayingValueHolder;
};

struct MacroMeter : juce::Component
{
    MacroMeter();
    void paint(juce::Graphics&) override;
    void resized() override;
    void updateLevel(float level);
private:
    TextMeter peakTextMeter;
    Meter peakMeter;
    Meter averageMeter;
    Averager<float> averager;
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
    int getRefreshRateHz() const;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PFM10AudioProcessor& audioProcessor;
    
    juce::AudioBuffer<float> editorAudioBuffer;
    TextMeter textMeter;
    Meter meter;
    DbScale dbScale;
    
    MacroMeter macroMeter;

    int refreshRateHz { 60 };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PFM10AudioProcessorEditor)
};
