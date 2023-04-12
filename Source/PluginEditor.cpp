/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// JUCE Components and custom classes
//==============================================================================

template<typename T>
Averager<T>::Averager(size_t numElements, T initialValue)
{
    resize(numElements, initialValue);
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
    for (T element : elements)
    {
        element = initialValue;
    }
    
    writeIndex = 0;
    avg = 0.f;
    sum = static_cast<T>(0);
    
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

DecayingValueHolder::DecayingValueHolder()
{
    // default starting decay rate 0.1 db/frame
    decayRatePerFrame = 0.1f;
    
    startTimerHz(60);
}

void DecayingValueHolder::updateHeldValue(float input)
{
    if (input > heldValue)
    {
        peakTime = getNow();
        heldValue = input;
        decayRateMultiplier = 1;
    }
}

bool DecayingValueHolder::isOverThreshold() const
{
    return (heldValue > threshold);
}

void DecayingValueHolder::setHoldTime(int ms)
{
    holdTime = ms;
}

void DecayingValueHolder::setDecayRate(float dbPerSec)
{
    // note: getTimerInterval() returns milliseconds
    decayRatePerFrame = dbPerSec * (getTimerInterval() / 1000);
}

void DecayingValueHolder::timerCallback()
{
    juce::int64 now = getNow();
    
    if ((now - peakTime) > holdTime)
    {
        heldValue -= decayRatePerFrame * decayRateMultiplier;
        
        heldValue = juce::jlimit(NEGATIVE_INFINITY,
                                 MAX_DECIBELS,
                                 heldValue);
        
        decayRateMultiplier += 2;
        
        if (heldValue <= NEGATIVE_INFINITY)
        {
            resetDecayRateMultiplier();
        }
    }
}

juce::int64 DecayingValueHolder::getNow()
{
    return juce::Time::currentTimeMillis();
}

//==============================================================================

ValueHolder::ValueHolder()
{
    timeOfPeak = juce::Time::currentTimeMillis();
    startTimerHz(60);
}

ValueHolder::~ValueHolder()
{
    stopTimer();
}

void ValueHolder::timerCallback()
{
    juce::int64 now = juce::Time::currentTimeMillis();
    juce::int64 elapsed = now - timeOfPeak;
    
    if (elapsed > durationToHoldForMs)
    {
        isOverThreshold = (currentValue > threshold);
        heldValue = NEGATIVE_INFINITY;
    }
}

void ValueHolder::setThreshold(float th)
{
    threshold = th;
    isOverThreshold = (currentValue > threshold);
}

void ValueHolder::updateHeldValue(float v)
{
    if (v > threshold)
    {
        isOverThreshold = true;
        timeOfPeak = juce::Time::currentTimeMillis();
        
        if (v > heldValue)
        {
            heldValue = v;
        }
    }
    
    currentValue = v;
}

//==============================================================================

TextMeter::TextMeter()
{
    valueHolder.setThreshold(0.f);
    valueHolder.updateHeldValue(NEGATIVE_INFINITY);
}

void TextMeter::paint(juce::Graphics &g)
{
    juce::Colour textColor;
    float valueToDisplay;
    
    if (valueHolder.getIsOverThreshold())
    {
        g.fillAll(juce::Colours::red);
        textColor = juce::Colours::black;
        
        valueToDisplay = valueHolder.getHeldValue();
    }
    else
    {
        g.fillAll(juce::Colours::black);
        textColor = juce::Colours::white;

        valueToDisplay = valueHolder.getCurrentValue();
    }
    
    juce::String textToDisplay;
    if (valueToDisplay > NEGATIVE_INFINITY)
    {
        textToDisplay = juce::String(valueToDisplay, 1);
        textToDisplay = textToDisplay.trimEnd();
    }
    else
    {
        textToDisplay = juce::String("-inf");
    }
    
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

//==============================================================================

void Meter::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    
    juce::Rectangle<float> meterFillRect(getLocalBounds().toFloat());
    float yMin = meterFillRect.getBottom();
    float yMax = meterFillRect.getY();
    
    auto dbPeakMapped = juce::jmap(dbPeak, NEGATIVE_INFINITY, MAX_DECIBELS, yMin, yMax);
    dbPeakMapped = juce::jmax(dbPeakMapped, yMax);
    meterFillRect.setY(dbPeakMapped);
    
    g.setColour(juce::Colours::orange);
    g.fillRect(meterFillRect);
    
    // Decaying Peak Level Tick Mark
    juce::Rectangle<float> peakLevelTickMark(meterFillRect);
    
    auto peakLevelTickYMapped = juce::jmap(decayingValueHolder.getHeldValue(),
                                           NEGATIVE_INFINITY,
                                           MAX_DECIBELS,
                                           yMin,
                                           yMax);
    //peakLevelTickYMapped = juce::jmax(peakLevelTickYMapped, yMax);
    peakLevelTickYMapped = juce::jlimit(yMax, meterFillRect.getY(), peakLevelTickYMapped);
    peakLevelTickMark.setY(peakLevelTickYMapped);
    peakLevelTickMark.setBottom( peakLevelTickMark.getY() + 2 );
    
    g.setColour(juce::Colours::white);
    g.fillRect(peakLevelTickMark);
}

void Meter::update(float dbLevel)
{
    dbPeak = dbLevel;
    decayingValueHolder.updateHeldValue(dbPeak);
    repaint();
}

//==============================================================================

MacroMeter::MacroMeter()
: averager(30, 0)
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

//==============================================================================

void DbScale::paint(juce::Graphics &g)
{
    g.drawImage(bkgd, getLocalBounds().toFloat());
}

std::vector<Tick> DbScale::getTicks(int dbDivision,
                       juce::Rectangle<int> meterBounds,
                       int minDb, int maxDb)
{
    if(minDb > maxDb)
    {
        DBG("Warning! DbScale minDb is greater than maxDb (in function getTicks)! Swapping them.");
        std::swap(minDb, maxDb);
    }
    
    //u_int numTicks = static_cast<u_int>( ((maxDb - minDb) / dbDivision) + 1);
    
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
    
    // For debugging purposes:
    //bkgdGraphicsContext.fillAll(juce::Colours::black);
    
    std::vector<Tick> ticks = getTicks(dbDivision,
                                       meterBounds,
                                       minDb,
                                       maxDb);

    bkgdGraphicsContext.setColour(juce::Colours::white);
    for(Tick tick : ticks)
    {
        int tickInt = static_cast<int>(tick.db);
        std::string tickString = std::to_string(tickInt);
        if(tickInt > 0) tickString.insert(0,"+");
        
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
        
        //DBG(tickString << " at y position " << std::to_string(tick.y));
    }
}

//==============================================================================

StereoMeter::StereoMeter(juce::ValueTree _vt, juce::String _meterName)
    : vt(_vt)
{
    vt.addListener(this);
    
    addAndMakeVisible(leftMacroMeter);
    addAndMakeVisible(rightMacroMeter);
    addAndMakeVisible(dbScale);
    
    label.setText("L  " + _meterName + "  R", juce::dontSendNotification);
    addAndMakeVisible(label);
    
    // update value tree when threshold slider value is changed
    thresholdSlider.onValueChange = [this] {vt.setProperty("thresholdValue", thresholdSlider.getValue(), nullptr);};
    // threshold slider range, style, look-and-feel
    thresholdSlider.setRange(0.0f, 1.0f);
    thresholdSlider.setSliderStyle(juce::Slider::SliderStyle::LinearBarVertical);
    thresholdSlider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 10, 10);
    thresholdSlider.setLookAndFeel(&thresholdSliderLAF);
    // add and make slider visible
    addAndMakeVisible(thresholdSlider);
}

StereoMeter::~StereoMeter()
{
    thresholdSlider.setLookAndFeel(nullptr);
}

void StereoMeter::valueTreePropertyChanged(juce::ValueTree& _vt, const juce::Identifier& _ID)
{
    if (_ID == ID_thresholdValue)
    {
//        DBG(juce::String(_vt.getProperty(ID_thresholdValue)));
    }
}

void StereoMeter::resized()
{
    auto bounds = getLocalBounds();
    auto height = bounds.getHeight();
    int macroMeterWidth = 40;
    int macroMeterHeight = height-30;
    
    leftMacroMeter.setTopLeftPosition(0, 0);
    leftMacroMeter.setSize(macroMeterWidth, macroMeterHeight);
    
    dbScale.setBounds(leftMacroMeter.getRight(),
                      leftMacroMeter.getY(),
                      30,
                      leftMacroMeter.getHeight()+50);
    dbScale.buildBackgroundImage(6, //db division
                                 leftMacroMeter.getBounds().withTrimmedTop(leftMacroMeter.getTextHeight()),
                                 NEGATIVE_INFINITY,
                                 MAX_DECIBELS);
    
    rightMacroMeter.setTopLeftPosition(leftMacroMeter.getRight()+dbScale.getWidth(), 0);
    rightMacroMeter.setSize(macroMeterWidth, macroMeterHeight);
    
    label.setBounds(leftMacroMeter.getX(),
                    leftMacroMeter.getBottom()+10,
                    rightMacroMeter.getRight()-leftMacroMeter.getX(),
                    50);
    label.setJustificationType(juce::Justification(12)); // top-centered
    
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
    
    writeIndexCached = (writeIndexCached == sizeCached-1) ? 0 : writeIndexCached+1;
    
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
    
    size_t readIndex = (writeIndexCached == sizeCached-1) ? 0 : writeIndexCached+1;
        
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

Histogram::Histogram(const juce::String& title_)
    : title(title_)
{
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
        g.setColour(juce::Colours::orange.withAlpha(0.5f));
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
        float result = juce::jmap(db,NEGATIVE_INFINITY,MAX_DECIBELS,bottom,0.f);
        return result;
    };
    
    auto incrementAndWrap = [=](std::size_t readIndex)
    {
        return (readIndex == bufferSizeCached-1) ? 0 : readIndex+1;
    };
            
    p.startNewSubPath(0, map(dataCached[readIndexCached]));
    readIndexCached = incrementAndWrap(readIndexCached);
        
    for (size_t x = 1; x < bufferSizeCached-1; ++x)
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
    center = juce::Point<int>( w/2, h/2 );
    diameter = ((w > h) ? h : w) - 35;      // 35 pixels shorter than the smaller dimension
    radius = diameter/2;
    
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
               center.getY() - axisLabelSize/2,
               axisLabelSize,
               axisLabelSize,
               juce::Justification(34)); // centered right
    
    // L
    g.drawText(axisLabels[1],
               center.getX() - radiusDotOrthoInt - axisLabelSize,
               center.getY() - radiusDotOrthoInt - axisLabelSize,
               axisLabelSize,
               axisLabelSize,
               juce::Justification(18)); // bottom right
    
    // M
    g.drawText(axisLabels[2],
               center.getX() - axisLabelSize/2,
               center.getY() - radiusInt - axisLabelSize,
               axisLabelSize,
               axisLabelSize,
               juce::Justification(20)); // bottom center
    
    // R
    g.drawText(axisLabels[3],
               center.getX() + radiusDotOrthoInt,
               center.getY() - radiusDotOrthoInt - axisLabelSize,
               axisLabelSize,
               axisLabelSize,
               juce::Justification(17)); // bottom left
    
    // S-
    g.drawText(axisLabels[4],
               center.getX() + radiusInt,
               center.getY() - axisLabelSize/2,
               axisLabelSize,
               axisLabelSize,
               juce::Justification(33)); // centered left
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
        
    g.drawImageAt(backgroundImage, 0, 0);
    
    internalBuffer.copyFrom(0, 0, buffer, 0, 0, numSamples);
    internalBuffer.copyFrom(1, 0, buffer, 1, 0, numSamples);
    
    for (int i = 0; i < numSamples; ++i)
    {
        leftSample = internalBuffer.getSample(0, i);
        rightSample = internalBuffer.getSample(1, i);

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
            if (i == numSamples-1)
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
                        
            // Transparency scales from 0 to 1 as the buffer is traversed
            opacity = juce::jmap(static_cast<float>(i),
                                 1.f,
                                 static_cast<float>(numSamples)-1,
                                 0.f,
                                 1.f);
            // Apply this log scaling for an aesthetically "snappier" falloff
            opacity = juce::mapToLog10(opacity,
                                       0.01f,
                                       1.f);
                                    
            g.setColour(juce::Colour(0.1f, 0.3f, opacity, opacity));
            g.strokePath(p, juce::PathStrokeType(2.f));
            
            previousMidMapped = midMapped;
            previousSideMapped = sideMapped;
        }
    }
}

//==============================================================================

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
                                                    width/2.f));
    
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

StereoImageMeter::StereoImageMeter(juce::AudioBuffer<float>& _buffer, double _sampleRate)
    : goniometer(_buffer),
      correlationMeter(_buffer, _sampleRate)
{
    addAndMakeVisible(goniometer);
    addAndMakeVisible(correlationMeter);
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
PFM10AudioProcessorEditor::PFM10AudioProcessorEditor (PFM10AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      valueTree(juce::Identifier("root")),
      editorAudioBuffer(2, 512),
      peakStereoMeter(valueTree, juce::String("Peak")),
      peakHistogram(juce::String("Peak")),
      stereoImageMeter(editorAudioBuffer, audioProcessor.getSampleRate())
{
    initValueTree();
    
    setSize (pluginWidth, pluginHeight);
    
//    setResizable(false, false);
//    setResizeLimits(600, 600,    //min
//                    900, 900);  //max
    
    addAndMakeVisible(peakStereoMeter);
    addAndMakeVisible(peakHistogram);
    addAndMakeVisible(stereoImageMeter);
    
    startTimerHz(refreshRateHz);
}

PFM10AudioProcessorEditor::~PFM10AudioProcessorEditor()
{
}

void PFM10AudioProcessorEditor::initValueTree()
{
    // Property Identifiers
    static juce::Identifier thresholdValue ("thresholdValue");
    
    // Set Up Properties using Identifiers
    valueTree.setProperty(thresholdValue, 0.f, nullptr);
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
    peakStereoMeter.setSize(width / 4, height * 2/3);

    peakHistogram.setBounds(0,
                            peakStereoMeter.getBottom(),
                            width,
                            height - peakStereoMeter.getBottom());
    
    stereoImageMeter.setBounds(peakStereoMeter.getRight(),
                               0,
                               width - peakStereoMeter.getRight(),
                               height - peakHistogram.getHeight());
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
