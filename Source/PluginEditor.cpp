/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

void Meter::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    
    auto bounds = getBounds();
    
    juce::Rectangle<float> rect;
    rect.setBottom(bounds.getBottom());
    rect.setWidth(bounds.getWidth());
    rect.setX(0);
    float yMin = bounds.getBottom();
    float yMax = bounds.getY();
    auto dbPeakMapped = juce::jmap(dbPeak, NEGATIVE_INFINITY, MAX_DECIBELS, yMin, yMax);
    rect.setY(dbPeakMapped);
    
    g.setColour(juce::Colours::orange);
    g.fillRect(rect);
}

void Meter::update(float dbLevel)
{
    dbPeak = dbLevel;
    repaint();
}

//==============================================================================
PFM10AudioProcessorEditor::PFM10AudioProcessorEditor (PFM10AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    addAndMakeVisible(meter);
    
    startTimerHz(60);
    
    setSize (400, 300);
}

PFM10AudioProcessorEditor::~PFM10AudioProcessorEditor()
{
    
}

//==============================================================================
void PFM10AudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void PFM10AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto width = bounds.getWidth();
    auto height = bounds.getHeight();
    
    meter.setTopLeftPosition(bounds.getX(), bounds.getY());
    meter.setSize(width/8, height/2);
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
    }
}
