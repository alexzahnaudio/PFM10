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
//MARK: - LAF_ThresholdSlider

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
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;
};
    
//==============================================================================
// JUCE Components and custom classes
//==============================================================================
//MARK: - Averager

template<typename T>
struct Averager
{
    Averager(size_t _numElements, T _initialValue);
    
    void resize(size_t numElements, T initialValue);
    
    void clear(T initialValue);
    
    size_t getSize() const { return elements.size(); }
    
    void add(T t);
    
    float getAvg() const { return avg; }
private:
    std::vector<T> elements;
    std::atomic<float> avg { NEGATIVE_INFINITY };
    std::atomic<size_t> writeIndex = 0;
    std::atomic<T> sum { NEGATIVE_INFINITY };
};

//MARK: - DecayingValueHolder

struct DecayingValueHolder : juce::Timer, juce::ValueTree::Listener
{
    DecayingValueHolder(juce::ValueTree _vt);
    ~DecayingValueHolder() override;
    void updateHeldValue(float input);
    void resetHeldValue();
    float getHeldValue() const { return heldValue; }
    bool isOverThreshold() const;
    void setHoldTime(int ms);
    void setDecayRate(int dbPerSec);
    void setHoldForInf(bool b);
    void timerCallback() override;
private:
    // Value Tree
    juce::ValueTree vt;
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    
    bool holdForInf;
    float heldValue { NEGATIVE_INFINITY };
    juce::int64 holdTimeMs;
    juce::int64 peakTime = getNow();
    float threshold { NEGATIVE_INFINITY };
    float decayRatePerFrame;
    static juce::int64 getNow();
    
    std::mutex heldValueMutex;
    std::mutex peakTimeMutex;
};

//MARK: - ValueHolder

struct ValueHolder : juce::Timer, juce::ValueTree::Listener
{
    ValueHolder(juce::ValueTree _vt);
    ~ValueHolder() override;
    void setThreshold(float th);
    bool updateHeldValue(float v);
    void resetHeldValue();
    void setHoldDuration(int ms) { durationToHoldForMs = ms; }
    void setHoldEnabled(bool b);
    void setHoldForInf(bool b) { holdForInf = b; }
    float getCurrentValue() const { return currentValue.load(); }
    float getHeldValue() const { return heldValue.load(); }
    bool getIsOverThreshold() const { return isOverThreshold.load(); }
    void timerCallback() override;
private:
    // Value Tree
    juce::ValueTree vt;
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    
    bool holdEnabled;
    bool holdForInf;
    int durationToHoldForMs;
    std::atomic<float> threshold { NEGATIVE_INFINITY };
    std::atomic<float> currentValue { NEGATIVE_INFINITY };
    std::atomic<float> heldValue { NEGATIVE_INFINITY };
    juce::int64 timeOfPeak;
    std::atomic<bool> isOverThreshold { false };
    
    std::mutex timeOfPeakMutex;
};

//MARK: - TextMeter

struct TextMeter : juce::Component
{
    TextMeter(juce::ValueTree _vt);
    void paint(juce::Graphics& g) override;
    void update(float valueDb);
    void setThreshold(float dbLevel);
    void resetHold();
private:
    ValueHolder valueHolder;
    float dbThreshold { 0 };
    juce::Colour textColorDefault { juce::Colours::white };
    juce::Colour textColorOverThreshold { juce::Colours::red };
    juce::String textToDisplay { "-inf" };
    
    std::mutex textToDisplayMutex;
};

//MARK: - Meter

struct Meter : juce::Component
{
    Meter(juce::ValueTree _vt);
    void paint (juce::Graphics&) override;
    void update(float dbLevel);
    void setThreshold(float dbLevel) { dbThreshold = dbLevel; }
    void setPeakHoldEnabled(bool isEnabled) { peakHoldEnabled = isEnabled; }
    void resetHold();
private:
    bool peakHoldEnabled { true };
    float dbPeak { NEGATIVE_INFINITY };
    float dbThreshold { 0 };
    DecayingValueHolder decayingValueHolder;
    
    juce::ColourGradient meterColourGradient;
    juce::Colour bottomColour         { juce::Colours::green.withAlpha(0.9f) };
    juce::Colour belowThresholdColour { juce::Colours::gold.withAlpha(0.9f) };
    juce::Colour aboveThresholdColour { juce::Colours::red.withAlpha(0.9f) };
    
    std::mutex dbPeakMutex;
};

//MARK: - MacroMeter

struct MacroMeter : juce::Component
{
    MacroMeter(juce::ValueTree _vt);
    void resized() override;
    void updateLevel(float level);
    void updateThreshold(float dbLevel);
    void setAveragerIntervals(int numElements);
    void setPeakHoldEnabled(bool isEnabled);
    void resetHold();
    //==============================================================================
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

//MARK: - Tick

struct Tick
{
    float db { 0.f };
    int y { 0 };
};

//MARK: - DbScale

struct DbScale : juce::Component
{
    ~DbScale() override = default;
    void paint (juce::Graphics& g) override;
    void buildBackgroundImage(int dbDivision, juce::Rectangle<int> meterBounds, int minDb, int maxDb);
    static std::vector<Tick> getTicks(int dbDivision, juce::Rectangle<int> meterBounds, int minDb, int maxDb);
    float yToDb(float y, float meterHeight, float minDb, float maxDb);
private:
    juce::Image bkgd;
};

//MARK: - StereoMeter

struct StereoMeter : juce::Component, juce::ValueTree::Listener
{
    StereoMeter(juce::ValueTree _vt, juce::String _meterName);
    ~StereoMeter() override;
    void resetHold();
    void resized() override;
    void update(float leftChannelDb, float rightChannelDb);
private:
    // Value Tree
    juce::ValueTree vt;
    juce::Identifier ID_thresholdValue    = juce::Identifier("thresholdValue");
    juce::Identifier ID_averagerIntervals = juce::Identifier("averagerIntervals");
    juce::Identifier ID_peakHoldEnabled   = juce::Identifier("peakHoldEnabled");
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;

    // Look and Feel
    LAF_ThresholdSlider thresholdSliderLAF;

    MacroMeter leftMacroMeter;
    MacroMeter rightMacroMeter;
    DbScale dbScale;
    juce::Label label;
    juce::Slider thresholdSlider;
};

//MARK: - ReadAllAfterWriteCircularBuffer

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

//MARK: - Histogram

struct Histogram : juce::Component, juce::ValueTree::Listener
{
    Histogram(juce::ValueTree _vt, const juce::String& _title);
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void update(float value);
private:
    // Value Tree
    juce::ValueTree vt;
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    
    ReadAllAfterWriteCircularBuffer<float> buffer {float(NEGATIVE_INFINITY)};
    juce::Rectangle<int> pathArea;
    int pathAreaTopBottomTrim { 10 };
    juce::Path path;
    float dbThreshold { 0 };
    juce::ColourGradient histogramColourGradient;
    juce::Colour bottomColour         { juce::Colours::green.withAlpha(0.9f) };
    juce::Colour belowThresholdColour { juce::Colours::gold.withAlpha(0.9f) };
    juce::Colour aboveThresholdColour { juce::Colours::red.withAlpha(0.9f) };
    
    const juce::String title;
    juce::Image titleImage;
    juce::Point<int> titleImagePosition;
    const int titleWidth { 64 };
    const int titleHeight { 16 };
    
    DbScale dbScale;
    int dbScaleWidth { 30 };
    int dbScaleDivision { 6 };
    std::vector<int> dbScaleTicksY;
    
    juce::Point<int> mousePos;
    bool isMouseHovered { false };
    juce::String dbValueHovered;
    int dbValueTextAreaWidth { 40 };
    int dbValueTextAreaHeight { 18 };
    juce::Rectangle<int> dbValueTextArea { 0, 0, dbValueTextAreaWidth, dbValueTextAreaHeight };
    
    void displayPath(juce::Graphics& g, juce::Rectangle<float> bounds);
    static juce::Path buildPath(juce::Path& p,
                                ReadAllAfterWriteCircularBuffer<float>& buffer,
                                juce::Rectangle<float> bounds);
    void buildTitleImage(juce::Graphics& g);
};

//MARK: - Goniometer

struct Goniometer : juce::Component
{
    Goniometer(juce::AudioBuffer<float>& buffer);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setScale(float newScale) { scale = newScale; }
    void update();
    float getDiameter() const { return diameter; }
private:
    juce::AudioBuffer<float>& buffer;
    juce::AudioBuffer<float> internalBuffer;
    juce::Image backgroundImage;
    juce::Rectangle<int> areaToRepaint;
    juce::Image pointCloud;
    int w, h;
    float radius, diameter;
    juce::Point<int> center;
    std::atomic<float> scale;
    
    std::mutex pointCloudMutex;

    void buildBackground(juce::Graphics& g);
};

//MARK: - CorrelationMeter

struct CorrelationMeter : juce::Component
{
    CorrelationMeter(juce::AudioBuffer<float>& buffer, double sampleRate);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void update();
    
    int getMeterAreaTrimBottom() const { return meterAreaTrimBottom; }
    int getMeterAreaTrimSide() const { return meterAreaTrimSide; }
private:
    juce::AudioBuffer<float>& buffer;
    std::array<juce::dsp::FIR::Filter<float>, 3> filters;
    Averager<float> slowAverager{1024*4, 0},
                    peakAverager{512, 0};
    
    juce::Rectangle<int> meterArea,
                         peakMeterArea,
                         slowMeterArea;
    const float slowMeterHeightPercentage { 0.75f };
    const int meterAreaTrimBottom { 20 };
    const int meterAreaTrimSide { 10 };
    
    juce::ColourGradient meterColorGradient;
    juce::Colour meterColorLeft   { juce::Colours::red };
    juce::Colour meterColorCenter { juce::Colours::gold };
    juce::Colour meterColorRight  { juce::Colours::green };
    
    juce::Image labelsImage;
    juce::Rectangle<int> labelsImageArea;
    
    void drawAverage(juce::Graphics& g,
                     juce::Rectangle<int> bounds,
                     float average,
                     bool drawBorder);
    void buildLabelsImage(juce::Graphics& g);
};

//MARK: - StereoImageMeter

struct StereoImageMeter : juce::Component, juce::ValueTree::Listener
{
    StereoImageMeter(juce::ValueTree _vt, juce::AudioBuffer<float>& _buffer, double _sampleRate);
    void resized() override;
    void update();
private:
    // Value Tree
    juce::ValueTree vt;
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    
    Goniometer goniometer;
    CorrelationMeter correlationMeter;
};

//MARK: - UpdateThread
class UpdateThread : public juce::Thread
{
public:
    UpdateThread() : juce::Thread("PFM10 Update Thread") {}
    ~UpdateThread() override { stopThread(2000); }
    void run() override
    {
        wait(20); //wait a few ms before running updates
        
        for (;;)
        {
            if (threadShouldExit()) break;
            fn();
            wait(-1); //wait until notified 
        }
    }
    
    std::function<void()> fn;
};

//==============================================================================
//MARK: - PFM10AudioProcessorEditor

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
    //==============================================================================
    void update();
    
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PFM10AudioProcessor& audioProcessor;
    
    juce::ValueTree valueTree;
    
    juce::AudioBuffer<float> editorAudioBuffer;
    
    juce::Image background;
    
    StereoMeter peakStereoMeter;
    Histogram peakHistogram;
    StereoImageMeter stereoImageMeter;
    
    UpdateThread updateThread;
    
    std::atomic<float> dbLeftChannel  { NEGATIVE_INFINITY };
    std::atomic<float> dbRightChannel { NEGATIVE_INFINITY };
    std::atomic<float> dbPeakMono     { NEGATIVE_INFINITY };
    
    std::mutex bufferMutex;
    
    //==============================================================================
    // Menus
    
    juce::Label decayRateMenuLabel { {}, "Decay Rate" };
    enum DecayRates
    {
        DB_PER_SEC_3 = 1,
        DB_PER_SEC_6,
        DB_PER_SEC_12,
        DB_PER_SEC_24,
        DB_PER_SEC_36
    };
    juce::ComboBox decayRateMenu;
    int decayRateMenuSelectByValue(int value);
    void onDecayRateMenuChanged();
    
    juce::Label averagerDurationMenuLabel { {}, "RMS Length" };
    enum AveragerDurations
    {
        AVERAGER_DURATION_MS_100 = 1,
        AVERAGER_DURATION_MS_250,
        AVERAGER_DURATION_MS_500,
        AVERAGER_DURATION_MS_1000,
        AVERAGER_DURATION_MS_2000
    };
    int durationMsToIntervals(int durationMs, int refreshRate) { return durationMs * refreshRate / 1000; }
    int intervalsToDurationMs(int intervals, int refreshRate) { return intervals * 1000 / refreshRate; }
    juce::ComboBox averagerDurationMenu;
    int averagerDurationMenuSelectByValue(int value);
    void onAveragerDurationMenuChanged();
    
    juce::Label peakHoldDurationMenuLabel { {}, "Hold Time" };
    enum PeakHoldDurations
    {
        PEAK_HOLD_DURATION_MS_0 = 1,
        PEAK_HOLD_DURATION_MS_500,
        PEAK_HOLD_DURATION_MS_2000,
        PEAK_HOLD_DURATION_MS_4000,
        PEAK_HOLD_DURATION_MS_6000,
        PEAK_HOLD_DURATION_MS_INF
    };
    juce::ComboBox peakHoldDurationMenu;
    int peakHoldDurationMenuSelectByValueTree(juce::ValueTree& tree);
    void onPeakHoldDurationMenuChanged();
    
    juce::TextButton peakHoldResetButton;
    void onPeakHoldResetButtonClicked();
    
    juce::Label goniometerScaleRotarySliderLabel { {}, "Gonio Scale" };
    juce::Slider goniometerScaleRotarySlider;
    
    void initMenus();
    
    //==============================================================================
    
    int pluginWidth { 720 };
    int pluginHeight { 620 };

    int refreshRateHz { 60 };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PFM10AudioProcessorEditor)
};
