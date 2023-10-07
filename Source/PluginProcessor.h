/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#define USE_TEST_OSCILLATOR false
#define MUTE_TEST_OSCILLATOR true

#include <JuceHeader.h>
#include <array>
#include "Identifiers.h"

template<typename T, size_t Size>           // T will be juce::AudioBuffer<float>
struct Fifo
{
    size_t getSize() const noexcept {return Size;}
    
    void prepare(int numSamples, int numChannels)
    {
        for (auto& buffer : buffers)
        {
            buffer.setSize(numChannels,
                           numSamples,
                           false,           // clear everything?
                           true,            // including the extra space?
                           true);           // avoid reallocating?
            buffer.clear();
        }
    }
    
    bool push(const T& t)
    {
        auto scopedWrite = abstractFifo.write(1);
        if (scopedWrite.blockSize1 > 0)
        {
            buffers[scopedWrite.startIndex1] = t;
            return true;
        }
        return false;
    }
    
    bool pull(T& t)
    {
        auto scopedRead = abstractFifo.read(1);
        if (scopedRead.blockSize1 > 0)
        {
            t = buffers[scopedRead.startIndex1];
            return true;
        }
        return false;
    }
    
    int getNumAvailableForReading() const
    {
        return abstractFifo.getNumReady();
    }
    
    int getAvailableSpace() const
    {
        return abstractFifo.getFreeSpace();
    }
private:
    juce::AbstractFifo abstractFifo { Size };
    std::array<T, Size> buffers;
};

//==============================================================================
//==============================================================================
/**
*/
class PFM10AudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    PFM10AudioProcessor();
    ~PFM10AudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    //==============================================================================
    juce::ValueTree valueTree;
    Fifo<juce::AudioBuffer<float>, 6> audioBufferFifo;

private:
    //==============================================================================
    void initDefaultValueTree (juce::ValueTree& tree);
    bool hasNeededProperties (juce::ValueTree& tree);
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PFM10AudioProcessor)
    
#if USE_TEST_OSCILLATOR
    juce::dsp::Oscillator<float> testOscillator { [](float x) {return std::sin(x);} };
    juce::dsp::Gain<float> gain;
#endif
};
