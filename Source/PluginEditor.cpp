/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Look And Feel classes
//==============================================================================
//MARK: - LAF_ThresholdSlider

void LAF_ThresholdSlider::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos,
                                           __attribute__((unused)) float minSliderPos,
                                           __attribute__((unused)) float maxSliderPos,
                                           __attribute__((unused)) const juce::Slider::SliderStyle style,
                                           juce::Slider& slider)
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

//==============================================================================
// JUCE Components and custom classes
//==============================================================================

//MARK: - Averager
template<typename T>
Averager<T>::Averager(size_t _numElements, T _initialValue)
{
    resize(_numElements, _initialValue);
}

template<typename T>
void Averager<T>::resize(size_t numElements, T initialValue)
{
    elements.resize(numElements);
    clear(initialValue);
}

template<typename T>
void Averager<T>::clear(T initialValue)
{
    size_t numElements = elements.size();
    for (size_t i = 0; i < numElements; i++)
    {
        elements[i] = initialValue;
    }
    
    writeIndex = 0;
    avg = initialValue;
    sum = static_cast<T>(initialValue * numElements);
}

template<typename T>
void Averager<T>::add(T t)
{
    // First, cache the atomics as local variables to work with
    auto writeIndexTemp = writeIndex.load();
    auto sumTemp = sum.load();
    
    sumTemp -= elements[writeIndexTemp];
    sumTemp += t;
    
    elements[writeIndexTemp] = t;
    
    ++writeIndexTemp;
    if (writeIndexTemp > (elements.size() - 1))
    {
        writeIndexTemp = 0;
    }
    
    writeIndex = writeIndexTemp;
    sum = sumTemp;
    avg = sumTemp / elements.size();
}

//==============================================================================
//MARK: - DecayingValueHolder

DecayingValueHolder::DecayingValueHolder(juce::ValueTree _vt)
: vt(_vt)
{
    vt.addListener(this);
    
    startTimerHz(60);
    
    setHoldForInf( vt.getProperty(IDs::peakHoldInf) );
    setHoldTime( vt.getProperty(IDs::peakHoldDuration) );
    setDecayRate( vt.getProperty(IDs::decayRate) );
}

DecayingValueHolder::~DecayingValueHolder()
{
    stopTimer();
}

void DecayingValueHolder::valueTreePropertyChanged(juce::ValueTree& _vt, const juce::Identifier& _ID)
{
    if (_ID == IDs::decayRate)
    {
        int decayRate = _vt.getProperty(IDs::decayRate);
        
        setDecayRate(decayRate);
        
        return;
    }
    else if (_ID == IDs::peakHoldDuration)
    {
        int newHoldDuration = _vt.getProperty(IDs::peakHoldDuration);
        
        setHoldTime(newHoldDuration);
        
        return;
    }
    else if (_ID == IDs::peakHoldInf)
    {
        bool isInfiniteHoldEnabled = _vt.getProperty(IDs::peakHoldInf);
        
        setHoldForInf(isInfiniteHoldEnabled);
        
        return;
    }
}

void DecayingValueHolder::updateHeldValue(float input)
{
    if (input > heldValue)
    {
        peakTime = getNow();
        heldValue = input;
    }
}

void DecayingValueHolder::resetHeldValue()
{
    heldValue = NEGATIVE_INFINITY;
}

bool DecayingValueHolder::isOverThreshold() const
{
    return (heldValue > threshold);
}

void DecayingValueHolder::setHoldTime(int ms)
{
    holdTimeMs = ms;
}

void DecayingValueHolder::setDecayRate(int dbPerSec)
{
    // note: getTimerInterval() returns milliseconds
    decayRatePerFrame = static_cast<float>(dbPerSec) * getTimerInterval() / 1000;
}

void DecayingValueHolder::setHoldForInf(bool b)
{
    holdForInf = b;
    
    if (! b) resetHeldValue();
}

void DecayingValueHolder::timerCallback()
{
    juce::int64 now = getNow();
    
    if (! holdForInf)
    {
        if ((now - peakTime) > holdTimeMs)
        {
            heldValue -= decayRatePerFrame;
            
            heldValue = juce::jlimit(NEGATIVE_INFINITY,
                                     MAX_DECIBELS,
                                     heldValue);
        }
    }
}

juce::int64 DecayingValueHolder::getNow()
{
    return juce::Time::currentTimeMillis();
}

//==============================================================================
//MARK: - ValueHolder

ValueHolder::ValueHolder(juce::ValueTree _vt)
: vt(_vt)
{
    vt.addListener(this);
    
    timeOfPeak = juce::Time::currentTimeMillis();
    startTimerHz(60);
    
    setHoldForInf( vt.getProperty(IDs::peakHoldInf) );
    setHoldDuration( vt.getProperty(IDs::peakHoldDuration) );
    setHoldEnabled( vt.getProperty(IDs::peakHoldEnabled) );
}

ValueHolder::~ValueHolder()
{
    stopTimer();
}

void ValueHolder::valueTreePropertyChanged(juce::ValueTree& _vt, const juce::Identifier& _ID)
{
    if (_ID == IDs::peakHoldDuration)
    {
        int newHoldDurationMs = _vt.getProperty(IDs::peakHoldDuration);
        
        setHoldDuration(newHoldDurationMs);
        
        return;
    }
    else if (_ID == IDs::peakHoldEnabled)
    {
        bool b = _vt.getProperty(IDs::peakHoldEnabled);
        
        setHoldEnabled(b);
        
        return;
    }
    else if (_ID == IDs::peakHoldInf)
    {
        bool b = _vt.getProperty(IDs::peakHoldInf);
        
        setHoldForInf(b);
        
        return;
    }
}

void ValueHolder::timerCallback()
{
    juce::int64 now = juce::Time::currentTimeMillis();
    juce::int64 elapsed = now - timeOfPeak;
    
    if (! holdForInf && elapsed > durationToHoldForMs)
    {
        heldValue = currentValue;
        isOverThreshold = (heldValue > threshold);
    }
}

void ValueHolder::setThreshold(float th)
{
    threshold = th;
    isOverThreshold = (heldValue > threshold);
}

void ValueHolder::setHoldEnabled(bool b)
{
    holdEnabled = b;
    
    if (! b) setHoldDuration(0);
}

void ValueHolder::updateHeldValue(float v)
{
    currentValue = v;
    
    if (v > heldValue)
    {
        timeOfPeak = juce::Time::currentTimeMillis();
        heldValue = v;
        isOverThreshold = (heldValue > threshold);
    }
}

void ValueHolder::resetHeldValue()
{
    heldValue = NEGATIVE_INFINITY;
}

//==============================================================================
//MARK: - TextMeter

TextMeter::TextMeter(juce::ValueTree _vt)
: valueHolder(_vt)
{
    valueHolder.setThreshold(0.f);
    valueHolder.updateHeldValue(NEGATIVE_INFINITY);
}

void TextMeter::paint(juce::Graphics &g)
{
    juce::Colour textColor = valueHolder.getIsOverThreshold() ? juce::Colours::red : juce::Colours::white;
    float valueToDisplay = valueHolder.getHeldValue();
    
    juce::String textToDisplay;
    if (valueToDisplay > NEGATIVE_INFINITY)
    {
        textToDisplay = juce::String(valueToDisplay, 1).trimEnd();
    }
    else
    {
        textToDisplay = juce::String("-inf");
    }
    
    g.fillAll(juce::Colours::black);
    g.setColour(textColor);
    g.setFont(12.f);
    g.drawFittedText(textToDisplay,
                     getLocalBounds(),
                     juce::Justification::centredBottom,
                     1);
}

void TextMeter::update(float valueDb)
{
    cachedValueDb = valueDb;
    valueHolder.updateHeldValue(valueDb);
    repaint();
}

void TextMeter::setThreshold(float dbLevel)
{
    dbThreshold = dbLevel;
    valueHolder.setThreshold(dbLevel);
}

void TextMeter::resetHold()
{
    valueHolder.resetHeldValue();
}

//==============================================================================
//MARK: - Meter

Meter::Meter(juce::ValueTree _vt)
: decayingValueHolder(_vt)
{
    setPeakHoldEnabled(_vt.getProperty(IDs::peakHoldEnabled));
}

void Meter::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    
    juce::Rectangle<float> meterBounds = getLocalBounds().toFloat();
    float yMin = meterBounds.getBottom();
    float yMax = meterBounds.getY();
            
    auto dbPeakMapped = juce::jmap(dbPeak, NEGATIVE_INFINITY, MAX_DECIBELS, yMin, yMax);
    dbPeakMapped = juce::jmax(dbPeakMapped, yMax);
    
    juce::Rectangle<float> meterFillRect = meterBounds.withY(dbPeakMapped);
    g.setColour(juce::Colours::orange);
    g.fillRect(meterFillRect);
    
    // Red rectangle fill for peaks above threshold value
    if (dbPeak > dbThreshold)
    {
        auto yThreshold = juce::jmap(dbThreshold, NEGATIVE_INFINITY, MAX_DECIBELS, yMin, yMax);
                
        juce::Rectangle<float> thresholdFillRect = meterFillRect.withBottom(yThreshold);
        g.setColour(juce::Colours::red);
        g.fillRect(thresholdFillRect);
    }
    
    // Decaying Peak Level Tick Mark
    juce::Rectangle<float> peakLevelTickMark(meterFillRect);
    
    auto peakLevelTickYMapped = juce::jmap(peakHoldEnabled ? decayingValueHolder.getHeldValue() : dbPeak,
                                           NEGATIVE_INFINITY,
                                           MAX_DECIBELS,
                                           yMin,
                                           yMax);
    peakLevelTickYMapped = juce::jlimit(yMax, meterFillRect.getY(), peakLevelTickYMapped);
    peakLevelTickMark.setY(peakLevelTickYMapped);
    peakLevelTickMark.setBottom( peakLevelTickMark.getY() + 2 );
    
    g.setColour(juce::Colours::white);
    g.fillRect(peakLevelTickMark);
}

void Meter::update(float dbLevel)
{
    dbPeak = dbLevel;
    if (peakHoldEnabled)
    {
        decayingValueHolder.updateHeldValue(dbPeak);
    }
    repaint();
}

void Meter::resetHold()
{
    decayingValueHolder.resetHeldValue();
}

//==============================================================================
//MARK: - MacroMeter

MacroMeter::MacroMeter(juce::ValueTree _vt)
: peakTextMeter(_vt),
  peakMeter(_vt),
  averageMeter(_vt),
  averager(30, NEGATIVE_INFINITY)
{
    addAndMakeVisible(peakTextMeter);
    addAndMakeVisible(peakMeter);
    addAndMakeVisible(averageMeter);
}

void MacroMeter::resized()
{
    auto bounds = getLocalBounds();
    auto width = bounds.getWidth();
    auto height = bounds.getHeight();
    
    auto tempFont = juce::Font(textHeight);
    
    int peakMeterWidth = 8;
    int averageMeterWidth = width - peakMeterWidth;
    int headerHeight = 0;
    
    peakMeter.setTopLeftPosition(bounds.getX(), bounds.getY()+textHeight+headerHeight);
    peakMeter.setSize(peakMeterWidth, height-textHeight-headerHeight);
    
    averageMeter.setTopLeftPosition(peakMeter.getRight()+2, peakMeter.getY());
    averageMeter.setSize(averageMeterWidth, peakMeter.getHeight());
    
    int textMeterWidth = tempFont.getStringWidth("-00.0") + 2;
    peakTextMeter.setBounds(averageMeter.getX() + averageMeter.getWidth()/2 - textMeterWidth/2,
                            averageMeter.getY() - (textHeight+2),
                            textMeterWidth,
                            textHeight+2);
}

void MacroMeter::updateLevel(float level)
{
    peakTextMeter.update(level);
    peakMeter.update(level);
    
    averager.add(level);
    averageMeter.update(averager.getAvg());
}

void MacroMeter::updateThreshold(float dbLevel)
{
    peakMeter.setThreshold(dbLevel);
    peakTextMeter.setThreshold(dbLevel);
    averageMeter.setThreshold(dbLevel);
}

void MacroMeter::setAveragerIntervals(int numElements)
{
    averager.resize(size_t(numElements), averager.getAvg());
}

void MacroMeter::setPeakHoldEnabled(bool isEnabled)
{
    peakMeter.setPeakHoldEnabled(isEnabled);
    averageMeter.setPeakHoldEnabled(isEnabled);
}

void MacroMeter::resetHold()
{
    peakTextMeter.resetHold();
    peakMeter.resetHold();
    averageMeter.resetHold();
}

//==============================================================================
//MARK: - DbScale

void DbScale::paint(juce::Graphics &g)
{
    g.drawImage(bkgd, getLocalBounds().toFloat());
}

std::vector<Tick> DbScale::getTicks(int dbDivision, juce::Rectangle<int> meterBounds, int minDb, int maxDb)
{
    if(minDb > maxDb)
    {
        DBG("Warning! DbScale minDb is greater than maxDb (in function getTicks)! Swapping them.");
        std::swap(minDb, maxDb);
    }
        
    auto ticks = std::vector<Tick>();
    
    for(int db = minDb; db <= maxDb; db += dbDivision)
    {
        auto yMapped = juce::jmap(db, minDb, maxDb,
                                  meterBounds.getHeight() + meterBounds.getY(),
                                  meterBounds.getY());
        Tick tick;
        tick.db = db;
        tick.y = yMapped;
        ticks.push_back(tick);
    }
    
    return ticks;
}

void DbScale::buildBackgroundImage(int dbDivision,
                                   juce::Rectangle<int> meterBounds,
                                   int minDb,
                                   int maxDb)
{
    if(minDb > maxDb)
    {
        DBG("Warning! DbScale minDb is greater than maxDb (in function buildBackgroundImage)! Swapping them.");
        std::swap(minDb, maxDb);
    }
    
    juce::Rectangle<int> bounds = getLocalBounds();
    if(bounds.isEmpty())
    {
        DBG("Warning! DbScale component local bounds are empty!");
        return;
    }
    
    float globalScaleFactor = juce::Desktop::getInstance().getGlobalScaleFactor();
    
    auto globalScaleFactorTransform = juce::AffineTransform();
    globalScaleFactorTransform = globalScaleFactorTransform.scaled(globalScaleFactor);
    
    bkgd = juce::Image(juce::Image::PixelFormat::ARGB,
                       static_cast<int>( bounds.getWidth()),
                       static_cast<int>( bounds.getHeight()),
                       true);
    
    auto bkgdGraphicsContext = juce::Graphics(bkgd);
    bkgdGraphicsContext.addTransform(globalScaleFactorTransform);
    
    std::vector<Tick> ticks = getTicks(dbDivision,
                                       meterBounds,
                                       minDb,
                                       maxDb);

    bkgdGraphicsContext.setColour(juce::Colours::white);
    for(Tick tick : ticks)
    {
        int tickInt = static_cast<int>(tick.db);
        std::string tickString = std::to_string(tickInt);
        if(tickInt > 0) tickString.insert(0, "+");
        
        // NOTE: the text shifts downward by (height) pixels, but the text
        //       disappears if height is set to 0. This is causing the ticks to
        //       be one pixel below where they should be. Temporary fix is
        //       to just subtract 1 from (y) to counteract this.
        bkgdGraphicsContext.drawFittedText(tickString,
                                           0,                       //x
                                           tick.y - 1,              //y
                                           30,                      //width
                                           1,                       //height
                                           juce::Justification::centred,
                                           1);                      //max num lines
    }
}

//==============================================================================
//MARK: - StereoMeter

StereoMeter::StereoMeter(juce::ValueTree _vt, juce::String _meterName)
    : vt(_vt),
      leftMacroMeter(_vt),
      rightMacroMeter(_vt)
{
    vt.addListener(this);
    
    addAndMakeVisible(leftMacroMeter);
    addAndMakeVisible(rightMacroMeter);
    addAndMakeVisible(dbScale);
    
    label.setText("L  " + _meterName + "  R", juce::dontSendNotification);
    label.setBufferedToImage(true);
    addAndMakeVisible(label);
    
    // update value tree when threshold slider value is changed, and vice versa
    thresholdSlider.getValueObject().referTo(vt.getPropertyAsValue(IDs::thresholdValue, nullptr));
    thresholdSlider.setRange(NEGATIVE_INFINITY, MAX_DECIBELS);
    thresholdSlider.setDoubleClickReturnValue(true, 0.0f);
    thresholdSlider.setSliderStyle(juce::Slider::SliderStyle::LinearBarVertical);
    thresholdSlider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 10, 10);
    thresholdSlider.setLookAndFeel(&thresholdSliderLAF);
    addAndMakeVisible(thresholdSlider);
    
    float initialThresholdValue = vt.getProperty(IDs::thresholdValue);
    leftMacroMeter.updateThreshold(initialThresholdValue);
    rightMacroMeter.updateThreshold(initialThresholdValue);
}

StereoMeter::~StereoMeter()
{
    thresholdSlider.setLookAndFeel(nullptr);
}

void StereoMeter::valueTreePropertyChanged(juce::ValueTree& _vt, const juce::Identifier& _ID)
{
    if (_ID == IDs::thresholdValue)
    {
        float dbLevel = _vt.getProperty(IDs::thresholdValue);
        
        leftMacroMeter.updateThreshold(dbLevel);
        rightMacroMeter.updateThreshold(dbLevel);
        
        return;
    }
    else if (_ID == IDs::averagerIntervals)
    {
        int newNumAveragerIntervals = _vt.getProperty(IDs::averagerIntervals);
        
        leftMacroMeter.setAveragerIntervals(newNumAveragerIntervals);
        rightMacroMeter.setAveragerIntervals(newNumAveragerIntervals);
        
        return;
    }
    else if (_ID == IDs::peakHoldEnabled)
    {
        bool peakHoldEnabled = _vt.getProperty(IDs::peakHoldEnabled);
        
        leftMacroMeter.setPeakHoldEnabled(peakHoldEnabled);
        rightMacroMeter.setPeakHoldEnabled(peakHoldEnabled);
        
        return;
    }
}

void StereoMeter::resetHold()
{
    leftMacroMeter.resetHold();
    rightMacroMeter.resetHold();
}

void StereoMeter::resized()
{
    auto bounds = getLocalBounds();
    auto height = bounds.getHeight();
    int macroMeterWidth = 40;
    int macroMeterHeight = height - 30;
    int dbDivision = 6;
    
    leftMacroMeter.setTopLeftPosition(0, 0);
    leftMacroMeter.setSize(macroMeterWidth, macroMeterHeight);
    
    dbScale.setBounds(leftMacroMeter.getRight(),
                      leftMacroMeter.getY(),
                      30,
                      leftMacroMeter.getHeight() + 50);
    dbScale.buildBackgroundImage(dbDivision,
                                 leftMacroMeter.getBounds().withTrimmedTop(leftMacroMeter.getTextHeight()),
                                 NEGATIVE_INFINITY,
                                 MAX_DECIBELS);
    
    rightMacroMeter.setTopLeftPosition(leftMacroMeter.getRight() + dbScale.getWidth(), 0);
    rightMacroMeter.setSize(macroMeterWidth, macroMeterHeight);
    
    label.setBounds(leftMacroMeter.getX(),
                    leftMacroMeter.getBottom() + 10,
                    rightMacroMeter.getRight() - leftMacroMeter.getX(),
                    50);
    label.setJustificationType(juce::Justification::centredTop);
    
    thresholdSlider.setBounds(dbScale.getX(),
                              leftMacroMeter.getTextHeight(),
                              dbScale.getWidth(),
                              leftMacroMeter.getMeterHeight());
}

void StereoMeter::update(float leftChannelDb, float rightChannelDb)
{
    leftMacroMeter.updateLevel(leftChannelDb);
    rightMacroMeter.updateLevel(rightChannelDb);
}

//==============================================================================
//MARK: - ReadAllAfterWriteCircularBuffer

template<typename T>
ReadAllAfterWriteCircularBuffer<T>::ReadAllAfterWriteCircularBuffer(T fillValue)
{
    resize(1, fillValue);
}

template<typename T>
void ReadAllAfterWriteCircularBuffer<T>::resize(std::size_t s, T fillValue)
{
    data.assign(s, fillValue);
    
    resetWriteIndex();
}

template<typename T>
void ReadAllAfterWriteCircularBuffer<T>::clear(T fillValue)
{
    data.assign(getSize(), fillValue);
    
    resetWriteIndex();
}

template<typename T>
void ReadAllAfterWriteCircularBuffer<T>::write(T t)
{
    size_t writeIndexCached = writeIndex;
    size_t sizeCached = getSize();
    
    data[writeIndexCached] = t;
    
    writeIndexCached = (writeIndexCached == sizeCached - 1) ? 0 : writeIndexCached + 1;
    
    writeIndex = writeIndexCached;
}

template<typename T>
std::vector<T>& ReadAllAfterWriteCircularBuffer<T>::getData()
{
    return data;
}

template<typename T>
size_t ReadAllAfterWriteCircularBuffer<T>::getReadIndex() const
{
    size_t writeIndexCached = writeIndex;
    size_t sizeCached = getSize();
    
    size_t readIndex = (writeIndexCached == sizeCached - 1) ? 0 : writeIndexCached + 1;
        
    return readIndex;
}

template<typename T>
size_t ReadAllAfterWriteCircularBuffer<T>::getSize() const
{
    return data.size();
}

template<typename T>
void ReadAllAfterWriteCircularBuffer<T>::resetWriteIndex()
{
    writeIndex = 0;
}

//==============================================================================
//MARK: - Histogram

Histogram::Histogram(juce::ValueTree _vt, const juce::String& _title)
    : vt(_vt),
      title(_title)
{
    vt.addListener(this);
    dbThreshold = _vt.getProperty(IDs::thresholdValue);
}

void Histogram::valueTreePropertyChanged(juce::ValueTree& _vt, const juce::Identifier& _ID)
{
    if (_ID == IDs::thresholdValue)
    {
        dbThreshold = _vt.getProperty(IDs::thresholdValue);
    }
}

void Histogram::paint(juce::Graphics &g)
{
    juce::Rectangle<float> localBounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colours::black);

    displayPath(g, localBounds);
    
    g.setColour(juce::Colours::white);
    g.drawText(title, localBounds, juce::Justification(20)); //bottom-centered
}

void Histogram::resized()
{
    // use component width for buffer size
    buffer.resize(static_cast<size_t>(getWidth()), NEGATIVE_INFINITY);
}

void Histogram::mouseDown(__attribute__((unused)) const juce::MouseEvent &e)
{
    buffer.clear(NEGATIVE_INFINITY);
    repaint();
}

void Histogram::update(float value)
{
    buffer.write(value);
    repaint();
}

void Histogram::displayPath(juce::Graphics &g, juce::Rectangle<float> bounds)
{
    juce::Path fillPath = buildPath(path, buffer, bounds);
    
    if (!fillPath.isEmpty())
    {
        histogramColourGradient.point1.setXY(bounds.getX(), bounds.getBottom());
        histogramColourGradient.point2.setXY(bounds.getX(), bounds.getY());
        
        float dbThresholdMapped = juce::jmap(dbThreshold,
                                             NEGATIVE_INFINITY, MAX_DECIBELS,
                                             0.f, 1.f);
    
        juce::Colour belowThresholdColour = juce::Colours::orange.withAlpha(0.5f);
        juce::Colour aboveThresholdColour = juce::Colours::red.withAlpha(0.5f);
        histogramColourGradient.clearColours();
        histogramColourGradient.addColour(0, belowThresholdColour);
        histogramColourGradient.addColour(dbThresholdMapped, belowThresholdColour);
        histogramColourGradient.addColour(juce::jmin(dbThresholdMapped + 0.01f, 1.f), aboveThresholdColour);
        histogramColourGradient.addColour(1, aboveThresholdColour);
        
        g.setGradientFill(histogramColourGradient);
        g.fillPath(fillPath);
        
        g.setColour(juce::Colours::orange);
        g.strokePath(path, juce::PathStrokeType(1));
    }
}

juce::Path Histogram::buildPath(juce::Path &p, ReadAllAfterWriteCircularBuffer<float> &buffer, juce::Rectangle<float> bounds)
{
    p.clear();
    
    size_t bufferSizeCached = buffer.getSize();
    size_t readIndexCached = buffer.getReadIndex();
    std::vector<float>& dataCached = buffer.getData();
    float bottom = bounds.getBottom();
    
    auto map = [=](float db)
    {
        float result = juce::jmap(db, NEGATIVE_INFINITY, MAX_DECIBELS, bottom, 0.f);
        return result;
    };
    
    auto incrementAndWrap = [=](std::size_t readIndex)
    {
        return (readIndex == bufferSizeCached - 1) ? 0 : readIndex + 1;
    };
            
    p.startNewSubPath(0, map(dataCached[readIndexCached]));
    readIndexCached = incrementAndWrap(readIndexCached);
        
    for (size_t x = 1; x < bufferSizeCached - 1; ++x)
    {
        p.lineTo(x, map(dataCached[readIndexCached]));
        readIndexCached = incrementAndWrap(readIndexCached);
    }
    
    if (bounds.getHeight() <= 0)
    {
        return juce::Path();
    }
    else
    {
        juce::Path fillPath(p);
        fillPath.lineTo(bounds.getBottomRight());
        fillPath.lineTo(bounds.getBottomLeft());
        fillPath.closeSubPath();
        return fillPath;
    }
}

//==============================================================================
//MARK: - Goniometer

Goniometer::Goniometer(juce::AudioBuffer<float>& _buffer)
    : buffer(_buffer)
{
    internalBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples());
    internalBuffer.clear();
}

void Goniometer::resized()
{
    w = getWidth();
    h = getHeight();
    center = juce::Point<int>( w / 2, h / 2 );
    diameter = ((w > h) ? h : w) - 35;      // 35 pixels shorter than the smaller dimension
    radius = diameter / 2;
    
    backgroundImage = juce::Image(juce::Image::ARGB, w, h, true);
    juce::Graphics g(backgroundImage);
    buildBackground(g);
}

void Goniometer::buildBackground(juce::Graphics &g)
{
    juce::Array<juce::String> axisLabels{"+S", "L", "M", "R", "-S"};
    float centerX = static_cast<float>(center.getX());
    float centerY = static_cast<float>(center.getY());
    float radiusDotOrtho = radius * INV_SQRT_OF_2;
    int radiusInt = static_cast<int>(radius);
    int radiusDotOrthoInt = static_cast<int>(radiusDotOrtho);
    int axisLabelSize = 30;

    // circle
    g.setColour(juce::Colours::black);
    g.fillEllipse(centerX - radius,
                  centerY - radius,
                  diameter,
                  diameter);
    
    g.setColour(juce::Colours::grey);
    g.drawEllipse(centerX - radius,
                  centerY - radius,
                  diameter,
                  diameter,
                  1.f);
    
    // +S and -S axes (horizontal and vertical)
    g.fillRect(centerX - radius,
               centerY,
               diameter,
               1.f);
    g.fillRect(centerX,
               centerY - radius,
               1.f,
               diameter);

    // L and R axes (diagonals)
    juce::Point<float> lAxisEndpointA
    {
        centerX - radiusDotOrtho,
        centerY - radiusDotOrtho
    };
    juce::Point<float> rAxisEndpointA
    {
        centerX + radiusDotOrtho,
        centerY - radiusDotOrtho
    };
    juce::Point<float> lAxisEndpointB
    {
        centerX + radiusDotOrtho,
        centerY + radiusDotOrtho
    };
    juce::Point<float> rAxisEndpointB
    {
        centerX - radiusDotOrtho,
        centerY + radiusDotOrtho
    };
    
    g.drawLine(juce::Line<float>(lAxisEndpointA,
                                 lAxisEndpointB));
    g.drawLine(juce::Line<float>(rAxisEndpointA,
                                 rAxisEndpointB));
    
    // Draw axis labels
    g.setColour(juce::Colours::white);
    
    // S+
    g.drawText(axisLabels[0],
               center.getX() - radiusInt - axisLabelSize,
               center.getY() - axisLabelSize / 2,
               axisLabelSize,
               axisLabelSize,
               juce::Justification::centredRight);
    
    // L
    g.drawText(axisLabels[1],
               center.getX() - radiusDotOrthoInt - axisLabelSize,
               center.getY() - radiusDotOrthoInt - axisLabelSize,
               axisLabelSize,
               axisLabelSize,
               juce::Justification::bottomRight);
    
    // M
    g.drawText(axisLabels[2],
               center.getX() - axisLabelSize / 2,
               center.getY() - radiusInt - axisLabelSize,
               axisLabelSize,
               axisLabelSize,
               juce::Justification::centredBottom);
    
    // R
    g.drawText(axisLabels[3],
               center.getX() + radiusDotOrthoInt,
               center.getY() - radiusDotOrthoInt - axisLabelSize,
               axisLabelSize,
               axisLabelSize,
               juce::Justification::bottomLeft);
    
    // S-
    g.drawText(axisLabels[4],
               center.getX() + radiusInt,
               center.getY() - axisLabelSize / 2,
               axisLabelSize,
               axisLabelSize,
               juce::Justification::centredLeft);
}

void Goniometer::paint(juce::Graphics &g)
{
    float leftSample,
          rightSample,
          mid,
          side,
          midMapped,
          sideMapped,
          previousMidMapped = 0.f,
          previousSideMapped = 0.f,
          opacity = 0.f;
    float centerX = static_cast<float>(center.getX());
    float centerY = static_cast<float>(center.getY());
    int numSamples = buffer.getNumSamples();
    int finalSampleIndex = numSamples - 1;
        
    g.drawImageAt(backgroundImage, 0, 0);
    
    internalBuffer.copyFrom(0, 0, buffer, 0, 0, numSamples);
    internalBuffer.copyFrom(1, 0, buffer, 1, 0, numSamples);
    
    for (int i = 0; i < numSamples; ++i)
    {
        leftSample = internalBuffer.getSample(0, i) * scale;
        rightSample = internalBuffer.getSample(1, i) * scale;

        //mult by invsqrt(2) gives us half power or -3dB
        mid = (leftSample + rightSample) * INV_SQRT_OF_2;
        side = (leftSample - rightSample) * INV_SQRT_OF_2;
                
        midMapped = juce::jmap(mid,
                               -1.f,
                               1.f,
                               -radius,
                               radius);
        sideMapped = juce::jmap(side,
                                -1.f,
                                1.f,
                                -radius,
                                radius);
        
        if (   std::isnan(leftSample) || std::isnan(rightSample)
            || std::isinf(leftSample) || std::isinf(rightSample))
        {
            midMapped = previousMidMapped;
            sideMapped = previousSideMapped;
        }
            
        if (i == 0)
        {
            previousMidMapped = midMapped;
            previousSideMapped = sideMapped;
        }
        else
        {
            // Final sample in buffer gets a pretty dot for a sort of glowing-datapoint effect
            if (i == finalSampleIndex)
            {
                g.setColour(juce::Colours::antiquewhite);
                g.fillEllipse(centerX + sideMapped - 2,
                              centerY + midMapped - 2,
                              4,
                              4);
            }
            
            p.clear();
            p.startNewSubPath(centerX + previousSideMapped, centerY + previousMidMapped);
            p.lineTo(centerX + sideMapped, centerY + midMapped);
                        
            // Transparency scales from 0.5 to 1.0 as the buffer is traversed
            opacity = juce::jmap(static_cast<float>(i),
                                 1.f,
                                 static_cast<float>(numSamples) - 1,
                                 0.5f,
                                 1.0f);
                                    
            g.setColour(juce::Colour(0.1f, 0.3f, opacity, opacity));
            g.strokePath(p, juce::PathStrokeType(2.f));
            
            previousMidMapped = midMapped;
            previousSideMapped = sideMapped;
        }
    }
}

//==============================================================================
//MARK: - CorrelationMeter

CorrelationMeter::CorrelationMeter(juce::AudioBuffer<float>& _buffer, double _sampleRate)
    : buffer(_buffer)
{
    // Initialize moving-average windows via FIR low-pass filters
    
    using FilterDesign = juce::dsp::FilterDesign<float>;
    using WindowingMethod = juce::dsp::WindowingFunction<float>::WindowingMethod;
    
    FilterDesign::FIRCoefficientsPtr coefficientsPtr( FilterDesign::designFIRLowpassWindowMethod(10.f, //frequency
                                                                                                 _sampleRate,
                                                                                                 1, //order
                                                                                                 WindowingMethod::rectangular) );

    for (juce::dsp::FIR::Filter<float> &filter : filters)
    {
        filter = juce::dsp::FIR::Filter<float>(coefficientsPtr);
    }
}

void CorrelationMeter::paint(juce::Graphics &g)
{
    juce::Rectangle<int> meterArea = getLocalBounds()
    .withTrimmedBottom(20)
    .withTrimmedLeft(10)
    .withTrimmedRight(10);
    
    float slowMeterHeightPercentage = 0.75f;
    
    // Skinny peak-average meter on top
    drawAverage(g,
                meterArea.withTrimmedBottom(static_cast<int>( meterArea.getHeight() * slowMeterHeightPercentage )),
                peakAverager.getAvg(),
                true);
    // Thicker slow-average meter on bottom
    drawAverage(g,
                meterArea.withTrimmedTop(static_cast<int>( meterArea.getHeight() * (1 - slowMeterHeightPercentage) )),
                slowAverager.getAvg(),
                true);
    
    // Text Labels
    g.setColour(juce::Colours::white);
    g.drawText("-1", getLocalBounds(), juce::Justification(juce::Justification::Flags::bottomLeft));
    g.drawText("0",  getLocalBounds(), juce::Justification(juce::Justification::Flags::centredBottom));
    g.drawText("+1", getLocalBounds(), juce::Justification(juce::Justification::Flags::bottomRight));
}

void CorrelationMeter::update()
{
    int numSamples = buffer.getNumSamples();
    
    for (int iSample = 0; iSample < numSamples; ++iSample)
    {
        float leftSample = buffer.getSample(0, iSample);
        float rightSample = buffer.getSample(1, iSample);
        
        // Feed L and R samples into correlation math equation
        float numerator = filters[0].processSample( leftSample * rightSample );
        float denominator = sqrt( filters[1].processSample(juce::square(leftSample))
                                * filters[2].processSample(juce::square(rightSample)) );
        float c = numerator / denominator;
                
        // Feed correlation result into averagers
        if ( std::isnan(c) || std::isinf(c) )
        {
            slowAverager.add(0);
            peakAverager.add(0);
        }
        else
        {
            slowAverager.add(c);
            peakAverager.add(c);
        }
    }
    
    repaint();
}

void CorrelationMeter::drawAverage(juce::Graphics& g,
                                   juce::Rectangle<int> bounds,
                                   float average,
                                   bool drawBorder)
{
    int width = bounds.getWidth();
    int height = bounds.getHeight();
    int centerX = bounds.getCentreX();
    
    g.setColour(juce::Colours::black);
    g.fillRect(bounds);
    
    int averageMapped = static_cast<int>(juce::jmap(abs(average),
                                                    0.f,
                                                    width / 2.f));
    
    g.setColour(juce::Colours::orange);
    if (average < 0)
    {
        g.fillRect(centerX - averageMapped,
                   bounds.getY(),
                   averageMapped,
                   height);
    }
    else
    {
        g.fillRect(centerX,
                   bounds.getY(),
                   averageMapped,
                   height);
    }
    
    if (drawBorder)
    {
        g.setColour(juce::Colours::lightgrey);
        g.drawRect(bounds);
    }
}

//==============================================================================
//MARK: - StereoImageMeter

StereoImageMeter::StereoImageMeter(juce::ValueTree _vt, juce::AudioBuffer<float>& _buffer, double _sampleRate)
    : vt(_vt),
      goniometer(_buffer),
      correlationMeter(_buffer, _sampleRate)
{
    vt.addListener(this);
    
    addAndMakeVisible(goniometer);
    addAndMakeVisible(correlationMeter);
    
    goniometer.setScale( vt.getProperty(IDs::goniometerScale) );
}

void StereoImageMeter::valueTreePropertyChanged(juce::ValueTree& _vt, const juce::Identifier& _ID)
{
    if (_ID == IDs::goniometerScale)
    {
        goniometer.setScale( _vt.getProperty(IDs::goniometerScale) );
    }
}

void StereoImageMeter::resized()
{
    float gonioToCorrMeterHeightRatio = 0.9f;
    
    goniometer.setBoundsRelative(0.f,
                                 0.f,
                                 1.f,
                                 gonioToCorrMeterHeightRatio);
    
    correlationMeter.setBoundsRelative(0.f,
                                       gonioToCorrMeterHeightRatio,
                                       1.f,
                                       1.f - gonioToCorrMeterHeightRatio);
}

void StereoImageMeter::update()
{
    goniometer.repaint();
    correlationMeter.update();
}

//==============================================================================
//==============================================================================
//MARK: - PFM10AudioProcessorEditor

PFM10AudioProcessorEditor::PFM10AudioProcessorEditor (PFM10AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      valueTree(p.valueTree),
      editorAudioBuffer(2, 512),
      peakStereoMeter(valueTree, juce::String("Peak")),
      peakHistogram(valueTree, juce::String("Peak")),
      stereoImageMeter(valueTree, editorAudioBuffer, audioProcessor.getSampleRate())
{
    setSize (pluginWidth, pluginHeight);
    
    //setResizable(true, true);
    //setResizeLimits(600, 600,    //min
    //                900, 900);  //max
    
    addAndMakeVisible(peakStereoMeter);
    addAndMakeVisible(peakHistogram);
    addAndMakeVisible(stereoImageMeter);
    
    initMenus();
    
    startTimerHz(refreshRateHz);
}

PFM10AudioProcessorEditor::~PFM10AudioProcessorEditor()
{
}

void PFM10AudioProcessorEditor::initMenus()
{
    // Decay Rate Menu
    
    decayRateMenuLabel.setJustificationType(juce::Justification::centred);
    decayRateMenuLabel.setBufferedToImage(true);
    addAndMakeVisible(decayRateMenuLabel);
    
    decayRateMenu.addItem("-3dB/s",  DB_PER_SEC_3);
    decayRateMenu.addItem("-6dB/s",  DB_PER_SEC_6);
    decayRateMenu.addItem("-12dB/s", DB_PER_SEC_12);
    decayRateMenu.addItem("-24dB/s", DB_PER_SEC_24);
    decayRateMenu.addItem("-36dB/s", DB_PER_SEC_36);
    decayRateMenu.setTooltip("Peak Marker Decay Rate");
    decayRateMenu.onChange = [this] { onDecayRateMenuChanged(); };
    decayRateMenu.setSelectedId( decayRateMenuSelectByValue(valueTree.getProperty(IDs::decayRate)) );
    decayRateMenu.setBufferedToImage(true);
    addAndMakeVisible(decayRateMenu);
    
    // Averager Duration Menu
    
    averagerDurationMenuLabel.setJustificationType(juce::Justification::centred);
    averagerDurationMenuLabel.setBufferedToImage(true);
    addAndMakeVisible(averagerDurationMenuLabel);

    averagerDurationMenu.addItem("100ms",  AVERAGER_DURATION_MS_100);
    averagerDurationMenu.addItem("250ms",  AVERAGER_DURATION_MS_250);
    averagerDurationMenu.addItem("500ms",  AVERAGER_DURATION_MS_500);
    averagerDurationMenu.addItem("1000ms", AVERAGER_DURATION_MS_1000);
    averagerDurationMenu.addItem("2000ms", AVERAGER_DURATION_MS_2000);
    averagerDurationMenu.setTooltip("Averaging duration for RMS meters");
    averagerDurationMenu.onChange = [this] { onAveragerDurationMenuChanged(); };
    averagerDurationMenu.setSelectedId( averagerDurationMenuSelectByValue(valueTree.getProperty(IDs::averagerIntervals)) );
    averagerDurationMenu.setBufferedToImage(true);
    addAndMakeVisible(averagerDurationMenu);
    
    // Peak Hold Duration Menu
    
    peakHoldDurationMenuLabel.setJustificationType(juce::Justification::centred);
    peakHoldDurationMenuLabel.setBufferedToImage(true);
    addAndMakeVisible(peakHoldDurationMenuLabel);

    peakHoldDurationMenu.addItem("0s",   PEAK_HOLD_DURATION_MS_0);
    peakHoldDurationMenu.addItem("0.5s", PEAK_HOLD_DURATION_MS_500);
    peakHoldDurationMenu.addItem("2s",   PEAK_HOLD_DURATION_MS_2000);
    peakHoldDurationMenu.addItem("4s",   PEAK_HOLD_DURATION_MS_4000);
    peakHoldDurationMenu.addItem("6s",   PEAK_HOLD_DURATION_MS_6000);
    peakHoldDurationMenu.addItem("inf",  PEAK_HOLD_DURATION_MS_INF);
    peakHoldDurationMenu.setTooltip("Peak hold duration for meters");
    peakHoldDurationMenu.onChange = [this] { onPeakHoldDurationMenuChanged(); };
    peakHoldDurationMenu.setSelectedId( peakHoldDurationMenuSelectByValueTree(valueTree) );
    peakHoldDurationMenu.setBufferedToImage(true);
    addAndMakeVisible(peakHoldDurationMenu);
    
    // Peak Hold Reset Button
    
    peakHoldResetButton.setButtonText("Reset Hold");
    peakHoldResetButton.onClick = [this] { onPeakHoldResetButtonClicked(); };
    peakHoldResetButton.setVisible( valueTree.getProperty(IDs::peakHoldInf) );
    peakHoldResetButton.setBufferedToImage(true);
    addAndMakeVisible(peakHoldResetButton);
    
    // Goniometer Scale Rotary Slider
    
    goniometerScaleRotarySliderLabel.setJustificationType(juce::Justification::centred);
    goniometerScaleRotarySliderLabel.setBufferedToImage(true);
    addAndMakeVisible(goniometerScaleRotarySliderLabel);

    goniometerScaleRotarySlider.setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    goniometerScaleRotarySlider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::TextBoxBelow, true, 50, 20);
    goniometerScaleRotarySlider.textFromValueFunction = [](double value)
    {
        return juce::String( juce::roundToInt(value * 100) ) + "%";
    };
    goniometerScaleRotarySlider.setTooltip("Goniometer Scale");
    goniometerScaleRotarySlider.setRange(0.5f, 2.0f);
    goniometerScaleRotarySlider.getValueObject().referTo(valueTree.getPropertyAsValue(IDs::goniometerScale, nullptr));
    goniometerScaleRotarySlider.setDoubleClickReturnValue(true, 1.0f);
    goniometerScaleRotarySlider.setBufferedToImage(true);
    addAndMakeVisible(goniometerScaleRotarySlider);
}

int PFM10AudioProcessorEditor::decayRateMenuSelectByValue(int value)
{
    if      (value == 3)  return DB_PER_SEC_3;
    else if (value == 6)  return DB_PER_SEC_6;
    else if (value == 12) return DB_PER_SEC_12;
    else if (value == 24) return DB_PER_SEC_24;
    else if (value == 36) return DB_PER_SEC_36;
    else                  return 1;
}

void PFM10AudioProcessorEditor::onDecayRateMenuChanged()
{
    switch (decayRateMenu.getSelectedId())
    {
        case DB_PER_SEC_3:  valueTree.setProperty(IDs::decayRate,  3, nullptr); break;
        case DB_PER_SEC_6:  valueTree.setProperty(IDs::decayRate,  6, nullptr); break;
        case DB_PER_SEC_12: valueTree.setProperty(IDs::decayRate, 12, nullptr); break;
        case DB_PER_SEC_24: valueTree.setProperty(IDs::decayRate, 24, nullptr); break;
        case DB_PER_SEC_36: valueTree.setProperty(IDs::decayRate, 36, nullptr); break;
        default: break;
    }
}

int PFM10AudioProcessorEditor::averagerDurationMenuSelectByValue(int value)
{
    int valueMs = intervalsToDurationMs(value, refreshRateHz);
    
    if      (valueMs == 100)  return AVERAGER_DURATION_MS_100;
    else if (valueMs == 250)  return AVERAGER_DURATION_MS_250;
    else if (valueMs == 500)  return AVERAGER_DURATION_MS_500;
    else if (valueMs == 1000) return AVERAGER_DURATION_MS_1000;
    else if (valueMs == 2000) return AVERAGER_DURATION_MS_2000;
    else                      return 1;
}

void PFM10AudioProcessorEditor::onAveragerDurationMenuChanged()
{
    switch (averagerDurationMenu.getSelectedId())
    {
        case AVERAGER_DURATION_MS_100:
            valueTree.setProperty(IDs::averagerIntervals, durationMsToIntervals(100,  refreshRateHz), nullptr); break;
        case AVERAGER_DURATION_MS_250:
            valueTree.setProperty(IDs::averagerIntervals, durationMsToIntervals(250,  refreshRateHz), nullptr); break;
        case AVERAGER_DURATION_MS_500:
            valueTree.setProperty(IDs::averagerIntervals, durationMsToIntervals(500,  refreshRateHz), nullptr); break;
        case AVERAGER_DURATION_MS_1000:
            valueTree.setProperty(IDs::averagerIntervals, durationMsToIntervals(1000, refreshRateHz), nullptr); break;
        case AVERAGER_DURATION_MS_2000:
            valueTree.setProperty(IDs::averagerIntervals, durationMsToIntervals(2000, refreshRateHz), nullptr); break;
        default: break;
    }
}

int PFM10AudioProcessorEditor::peakHoldDurationMenuSelectByValueTree(juce::ValueTree& tree)
{
    bool enabled = tree.getProperty(IDs::peakHoldEnabled);
    bool inf     = tree.getProperty(IDs::peakHoldInf);
    int duration = tree.getProperty(IDs::peakHoldDuration);
    
    if      (! enabled)        return PEAK_HOLD_DURATION_MS_0;
    else if (inf)              return PEAK_HOLD_DURATION_MS_INF;
    else if (duration == 500)  return PEAK_HOLD_DURATION_MS_500;
    else if (duration == 2000) return PEAK_HOLD_DURATION_MS_2000;
    else if (duration == 4000) return PEAK_HOLD_DURATION_MS_4000;
    else if (duration == 6000) return PEAK_HOLD_DURATION_MS_6000;
    else                       return 1;
}

void PFM10AudioProcessorEditor::onPeakHoldDurationMenuChanged()
{
    switch (peakHoldDurationMenu.getSelectedId())
    {
        case PEAK_HOLD_DURATION_MS_0:
            valueTree.setProperty(IDs::peakHoldEnabled,  false, nullptr);
            valueTree.setProperty(IDs::peakHoldInf,      false, nullptr);
            peakHoldResetButton.setVisible(false);
            break;
        case PEAK_HOLD_DURATION_MS_500:
            valueTree.setProperty(IDs::peakHoldEnabled,  true,  nullptr);
            valueTree.setProperty(IDs::peakHoldInf,      false, nullptr);
            valueTree.setProperty(IDs::peakHoldDuration, 500,   nullptr);
            peakHoldResetButton.setVisible(false);
            break;
        case PEAK_HOLD_DURATION_MS_2000:
            valueTree.setProperty(IDs::peakHoldEnabled,  true,  nullptr);
            valueTree.setProperty(IDs::peakHoldInf,      false, nullptr);
            valueTree.setProperty(IDs::peakHoldDuration, 2000,  nullptr);
            peakHoldResetButton.setVisible(false);
            break;
        case PEAK_HOLD_DURATION_MS_4000:
            valueTree.setProperty(IDs::peakHoldEnabled,  true,  nullptr);
            valueTree.setProperty(IDs::peakHoldInf,      false, nullptr);
            valueTree.setProperty(IDs::peakHoldDuration, 4000,  nullptr);
            peakHoldResetButton.setVisible(false);
            break;
        case PEAK_HOLD_DURATION_MS_6000:
            valueTree.setProperty(IDs::peakHoldEnabled,  true,  nullptr);
            valueTree.setProperty(IDs::peakHoldInf,      false, nullptr);
            valueTree.setProperty(IDs::peakHoldDuration, 6000,  nullptr);
            peakHoldResetButton.setVisible(false);
            break;
        case PEAK_HOLD_DURATION_MS_INF:
            valueTree.setProperty(IDs::peakHoldEnabled,  true,  nullptr);
            valueTree.setProperty(IDs::peakHoldInf,      true,  nullptr);
            peakHoldResetButton.setVisible(true);
            break;
        default: break;
    }
}

void PFM10AudioProcessorEditor::onPeakHoldResetButtonClicked()
{
    peakStereoMeter.resetHold();
}

void PFM10AudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void PFM10AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto width = bounds.getWidth();
    auto height = bounds.getHeight();

    peakStereoMeter.setTopLeftPosition(0, 0);
    peakStereoMeter.setSize(120, height * 2/3);

    peakHistogram.setBounds(0,
                            peakStereoMeter.getBottom(),
                            width,
                            height - peakStereoMeter.getBottom());
    
    stereoImageMeter.setBounds(peakStereoMeter.getRight(),
                               0,
                               width - peakStereoMeter.getRight(),
                               height - peakHistogram.getHeight());
    
    // Menus
    int menuWidth = 100;
    int menuHeight = 30;
    int menuX = peakStereoMeter.getRight();
    int verticalSpaceBetweenMenus = 20;
    int goniometerScaleRotarySliderSize = 125;
    
    decayRateMenuLabel.setBounds(menuX,
                                 0,
                                 menuWidth,
                                 menuHeight);
    decayRateMenu.setBounds(menuX,
                            decayRateMenuLabel.getBottom(),
                            menuWidth,
                            menuHeight);
    
    averagerDurationMenuLabel.setBounds(menuX,
                                        decayRateMenu.getBottom() + verticalSpaceBetweenMenus,
                                        menuWidth,
                                        menuHeight);
    averagerDurationMenu.setBounds(menuX,
                                   averagerDurationMenuLabel.getBottom(),
                                   menuWidth,
                                   menuHeight);
    
    peakHoldDurationMenuLabel.setBounds(menuX,
                                        averagerDurationMenu.getBottom() + verticalSpaceBetweenMenus,
                                        menuWidth,
                                        menuHeight);
    peakHoldDurationMenu.setBounds(menuX,
                                   peakHoldDurationMenuLabel.getBottom(),
                                   menuWidth,
                                   menuHeight);
    
    peakHoldResetButton.setBounds(menuX,
                                  peakHoldDurationMenu.getBottom() + verticalSpaceBetweenMenus,
                                  menuWidth,
                                  menuHeight);
    
    goniometerScaleRotarySliderLabel.setBounds(stereoImageMeter.getRight() - goniometerScaleRotarySliderSize,
                                               stereoImageMeter.getY(),
                                               goniometerScaleRotarySliderSize,
                                               menuHeight);
    goniometerScaleRotarySlider.setBounds(stereoImageMeter.getRight() - goniometerScaleRotarySliderSize,
                                          goniometerScaleRotarySliderLabel.getBottom(),
                                          goniometerScaleRotarySliderSize,
                                          goniometerScaleRotarySliderSize);
}

void PFM10AudioProcessorEditor::timerCallback()
{
    if(audioProcessor.audioBufferFifo.getNumAvailableForReading() > 0)
    {
        // pull every element out of the audio buffer FIFO into the editor audio buffer
        while( audioProcessor.audioBufferFifo.pull(editorAudioBuffer) )
        {
        }
        
        // get the left channel's peak magnitude within the editor audio buffer
        float magLeftChannel = editorAudioBuffer.getMagnitude(0, 0, editorAudioBuffer.getNumSamples());
        float dbLeftChannel = juce::Decibels::gainToDecibels(magLeftChannel, NEGATIVE_INFINITY);
        
        // get the right channel's peak magnitude within the editor audio buffer
        float magRightChannel = editorAudioBuffer.getMagnitude(1, 0, editorAudioBuffer.getNumSamples());
        float dbRightChannel = juce::Decibels::gainToDecibels(magRightChannel, NEGATIVE_INFINITY);
        
        // feed them to the stereo peak meter
        peakStereoMeter.update(dbLeftChannel, dbRightChannel);
        
        // get the mono level (avg. of left and right channels), pass to histogram
        float magPeakMono = (magLeftChannel + magRightChannel) / 2;
        float dbPeakMono = juce::Decibels::gainToDecibels(magPeakMono, NEGATIVE_INFINITY);
        peakHistogram.update(dbPeakMono);
        
        // update the goniometer and correlation meter
        stereoImageMeter.update();
    }
}

int PFM10AudioProcessorEditor::getRefreshRateHz() const
{
    return refreshRateHz;
}
