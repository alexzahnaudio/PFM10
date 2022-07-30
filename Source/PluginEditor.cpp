/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    
    auto bounds = getLocalBounds();
    
    juce::Rectangle<float> rect;
    rect.setBottom(bounds.getBottom());
    rect.setWidth(bounds.getWidth());
    rect.setX(0);
    float yMin = bounds.getBottom();
    float yMax = bounds.getY();
    
    auto dbPeakMapped = juce::jmap(dbPeak, NEGATIVE_INFINITY, MAX_DECIBELS, yMin, yMax);
    
    // TO DO: Limit dbPeakMapped to yMax.
    //         Currently the meter fill rect will go ABOVE the top of the meter
    //         if dbPeak is greater than MAX_DECIBELS, resulting in a black bar
    //         at the bottom of the meter.
        
    rect.setY(dbPeakMapped);
    
    // TO DO: Make the rect color RED instead of orange if dbPeak > MAX_DECIBELS
    
    g.setColour(juce::Colours::orange);
    g.fillRect(rect);
}

void Meter::update(float dbLevel)
{
    dbPeak = dbLevel;
    repaint();
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
//==============================================================================
PFM10AudioProcessorEditor::PFM10AudioProcessorEditor (PFM10AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    addAndMakeVisible(meter);
    addAndMakeVisible(dbScale);
    addAndMakeVisible(textMeter);
    
    startTimerHz(60);
    
    setSize (600, 450);
}

PFM10AudioProcessorEditor::~PFM10AudioProcessorEditor()
{
    
}

//==============================================================================
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
    
    meter.setTopLeftPosition(bounds.getX(), bounds.getY()+60);
    meter.setSize(width/8, height/2);
    
    dbScale.setBounds(meter.getRight(),
                      0,
                      30,
                      getHeight());
    dbScale.buildBackgroundImage(6, //db division
                                 meter.getBounds(),
                                 NEGATIVE_INFINITY,
                                 MAX_DECIBELS);
    
    int textHeight = 12;
    auto tempFont = juce::Font(textHeight);
    int textMeterWidth = tempFont.getStringWidth("-00.0") + 2;
    textMeter.setBounds(meter.getX() + meter.getWidth()/2 - textMeterWidth/2,
                        meter.getY() - (textHeight+2),
                        textMeterWidth,
                        textHeight+2);
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
        float leftChannelMag = editorAudioBuffer.getMagnitude(0, 0, editorAudioBuffer.getNumSamples());
        float dbLeftChannelMag = juce::Decibels::gainToDecibels(leftChannelMag, NEGATIVE_INFINITY);
        meter.update(dbLeftChannelMag);
        textMeter.update(dbLeftChannelMag);
    }
}
