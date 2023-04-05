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

#ifdef  INV_SQRT_OF_2
#undef  INV_SQRT_OF_2
#endif
#define INV_SQRT_OF_2 0.7071f

//==============================================================================
// Look And Feel classes
//==============================================================================
struct LAF_ThresholdSlider : juce::LookAndFeel_V4
{
    LAF_ThresholdSlider()
    {
        setColour(juce::Slider::thumbColourId, juce::Colours::red);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentWhite);
    }
private:
    float thumbWidth { 2.0f };
    
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        // This Look-And-Feel class is designed specifically for linear-bar style sliders!
        //
        // If you intend to expand this to handle other slider styles, then change this jassert
        // to an if(slider.isBar()) condition and add an else block (see default LAF_V4 implementation),
        // or create a different custom LAF class.
        jassert (slider.isBar());

        g.setColour (slider.findColour (juce::Slider::thumbColourId));
        g.fillRect (slider.isHorizontal() ? juce::Rectangle<float> (sliderPos - thumbWidth,
                                                                    (float) y + 0.5f,
                                                                    thumbWidth,
                                                                    (float) height - 1.0f)
                                          : juce::Rectangle<float> ((float) x + 0.5f,
                                                                    sliderPos,
                                                                    (float) width - 1.0f,
                                                                    thumbWidth));
    }
};
    
//==============================================================================
// JUCE Components and custom classes
//==============================================================================

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
    void resized() override;
    void updateLevel(float level);
    int getTextHeight() const { return textHeight; }
    int getTextMeterHeight() const { return peakTextMeter.getHeight(); }
    int getMeterHeight() const { return peakMeter.getHeight(); }
private:
    int textHeight { 12 };
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

struct StereoMeter : juce::Component
{
    StereoMeter(juce::String meterName);
    ~StereoMeter() override;
    void resized() override;
    void update(float leftChannelDb, float rightChannelDb);
private:
    LAF_ThresholdSlider thresholdSliderLAF;

    MacroMeter leftMacroMeter;
    MacroMeter rightMacroMeter;
    DbScale dbScale;
    juce::Label label;
    juce::Slider thresholdSlider;
};

template<typename T>
struct ReadAllAfterWriteCircularBuffer
{
    ReadAllAfterWriteCircularBuffer(T fillValue);

    void resize(std::size_t s, T fillValue);
    void clear(T fillValue);
    void write(T t);

    std::vector<T>& getData();
    size_t getReadIndex() const;
    size_t getSize() const;
private:
    std::atomic<std::size_t> writeIndex {0};
    std::vector<T> data;

    void resetWriteIndex();
};

struct Histogram : juce::Component
{
    Histogram(const juce::String& title_);
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void update(float value);
private:
    ReadAllAfterWriteCircularBuffer<float> buffer {float(NEGATIVE_INFINITY)};
    juce::Path path;
    const juce::String title;
    
    void displayPath(juce::Graphics& g, juce::Rectangle<float> bounds);
    static juce::Path buildPath(juce::Path& p,
                          ReadAllAfterWriteCircularBuffer<float>& buffer,
                          juce::Rectangle<float> bounds);
};

struct Goniometer : juce::Component
{
    Goniometer(juce::AudioBuffer<float>& buffer);
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    juce::AudioBuffer<float>& buffer;
    juce::AudioBuffer<float> internalBuffer;
    juce::Image backgroundImage;
    juce::Path p;
    int w, h;
    float radius, diameter;
    juce::Point<int> center;

    void buildBackground(juce::Graphics& g);
};

struct CorrelationMeter : juce::Component
{
    CorrelationMeter(juce::AudioBuffer<float>& buffer, double sampleRate);
    void paint(juce::Graphics& g) override;
    void update();
private:
    juce::AudioBuffer<float>& buffer;
    
    std::array<juce::dsp::FIR::Filter<float>, 3> filters;

    Averager<float> slowAverager{1024*4, 0},
                    peakAverager{512, 0};
    
    void drawAverage(juce::Graphics& g,
                     juce::Rectangle<int> bounds,
                     float average,
                     bool drawBorder);
};

struct StereoImageMeter : juce::Component
{
    StereoImageMeter(juce::AudioBuffer<float>& buffer, double sampleRate);
    void resized() override;
    void update();
private:
    Goniometer goniometer;
    CorrelationMeter correlationMeter;
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
    
    StereoMeter peakStereoMeter;
    Histogram peakHistogram;
    StereoImageMeter stereoImageMeter;
    
    int pluginWidth { 700 };
    int pluginHeight { 600 };

    int refreshRateHz { 60 };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PFM10AudioProcessorEditor)
};
