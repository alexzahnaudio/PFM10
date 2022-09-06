/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    
    //update average
    //update sum
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
    
    // TO DO: gradated color change on meter e.g. Red above 0db
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
: averager(60, 0)
{
    addAndMakeVisible(peakTextMeter);
    addAndMakeVisible(peakMeter);
    addAndMakeVisible(averageMeter);
}

//void MacroMeter::paint(juce::Graphics &g)
//{
//}

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

int MacroMeter::getTextHeight() const
{
    return textHeight;
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

StereoMeter::StereoMeter(juce::String meterName)
{
    addAndMakeVisible(leftMacroMeter);
    addAndMakeVisible(rightMacroMeter);
    addAndMakeVisible(dbScale);
    addAndMakeVisible(label);

    label.setText("L  " + meterName + "  R", juce::dontSendNotification);
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
//==============================================================================
PFM10AudioProcessorEditor::PFM10AudioProcessorEditor (PFM10AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      peakStereoMeter(juce::String("Peak")),
      peakHistogram(juce::String("Peak"))
{
    addAndMakeVisible(peakStereoMeter);
    addAndMakeVisible(peakHistogram);
    
    startTimerHz(refreshRateHz);
    
    setSize (800, 600);
}

PFM10AudioProcessorEditor::~PFM10AudioProcessorEditor()
{
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
    }
}

int PFM10AudioProcessorEditor::getRefreshRateHz() const
{
    return refreshRateHz;
}
