/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PFM10AudioProcessor::PFM10AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
       valueTree(IDs::root)
{
#if PERFETTO
    MelatoninPerfetto::get().beginSession();
#endif
    
    initDefaultValueTree(valueTree);
}

PFM10AudioProcessor::~PFM10AudioProcessor()
{
#if PERFETTO
    MelatoninPerfetto::get().endSession();
#endif
}

//==============================================================================
const juce::String PFM10AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PFM10AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PFM10AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PFM10AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PFM10AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PFM10AudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PFM10AudioProcessor::getCurrentProgram()
{
    return 0;
}

void PFM10AudioProcessor::setCurrentProgram (__attribute__((unused)) int index)
{
}

const juce::String PFM10AudioProcessor::getProgramName (__attribute__((unused)) int index)
{
    return {};
}

void PFM10AudioProcessor::changeProgramName (__attribute__((unused)) int index, __attribute__((unused)) const juce::String& newName)
{
}

//==============================================================================
void PFM10AudioProcessor::prepareToPlay (__attribute__((unused)) double sampleRate, int samplesPerBlock)
{
    TRACE_DSP();
    
    audioBufferFifo.prepare(samplesPerBlock, getTotalNumOutputChannels());
    
#if USE_TEST_OSCILLATOR
    juce::dsp::ProcessSpec processSpec;
    processSpec.maximumBlockSize = samplesPerBlock;
    processSpec.sampleRate = sampleRate;
    processSpec.numChannels = getTotalNumOutputChannels();
    
    testOscillator.prepare(processSpec);
    gain.prepare(processSpec);
#endif
}

void PFM10AudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PFM10AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void PFM10AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, __attribute__((unused)) juce::MidiBuffer& midiMessages)
{
    TRACE_DSP();
    
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

#if USE_TEST_OSCILLATOR
    buffer.clear();
    
    juce::dsp::AudioBlock<float> audioBlock { buffer };
    
    testOscillator.setFrequency(JUCE_LIVE_CONSTANT(440.0f));
    gain.setGainDecibels(JUCE_LIVE_CONSTANT(-3.0f));
    
    int numSamplesToProcess = buffer.getNumSamples();
    for (int i = 0; i < numSamplesToProcess; ++i)
    {
        float nextOscillatorSample = testOscillator.processSample(0.f);
        audioBlock.setSample(0, i, nextOscillatorSample);
        audioBlock.setSample(1, i, nextOscillatorSample);
    }
    
    gain.process( juce::dsp::ProcessContextReplacing<float>(audioBlock) );
#endif
    
    audioBufferFifo.push(buffer);
    
#if USE_TEST_OSCILLATOR && MUTE_TEST_OSCILLATOR
    buffer.clear();
#endif
}

//==============================================================================
bool PFM10AudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PFM10AudioProcessor::createEditor()
{
    return new PFM10AudioProcessorEditor (*this);
}

//==============================================================================
void PFM10AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    
    juce::MemoryOutputStream outputStream = juce::MemoryOutputStream(destData, false);
    
    valueTree.writeToStream(outputStream);
}

void PFM10AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    
    juce::ValueTree loadedTree = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    
    if (loadedTree.isValid() && hasNeededProperties(loadedTree))
    {
        valueTree.copyPropertiesAndChildrenFrom(loadedTree, nullptr);
    }
    else
    {
        // Do nothing. Do not overwrite the value tree's default values.
    }
}

//==============================================================================

void PFM10AudioProcessor::initDefaultValueTree (juce::ValueTree& tree)
{
    // Set Up Properties using Identifiers
    tree.setProperty(IDs::thresholdValue,    DefaultPropertyValues::thresholdValue,    nullptr);
    tree.setProperty(IDs::decayRate,         DefaultPropertyValues::decayRate,         nullptr);
    tree.setProperty(IDs::averagerIntervals, DefaultPropertyValues::averagerIntervals, nullptr);
    tree.setProperty(IDs::peakHoldEnabled,   DefaultPropertyValues::peakHoldEnabled,   nullptr);
    tree.setProperty(IDs::peakHoldInf,       DefaultPropertyValues::peakHoldInf,       nullptr);
    tree.setProperty(IDs::peakHoldDuration,  DefaultPropertyValues::peakHoldDuration,  nullptr);
    tree.setProperty(IDs::goniometerScale,   DefaultPropertyValues::goniometerScale,   nullptr);
}

bool PFM10AudioProcessor::hasNeededProperties (juce::ValueTree& tree)
{
    if (! tree.hasProperty(IDs::thresholdValue))    return false;
    if (! tree.hasProperty(IDs::decayRate))         return false;
    if (! tree.hasProperty(IDs::averagerIntervals)) return false;
    if (! tree.hasProperty(IDs::peakHoldEnabled))   return false;
    if (! tree.hasProperty(IDs::peakHoldInf))       return false;
    if (! tree.hasProperty(IDs::peakHoldDuration))  return false;
    if (! tree.hasProperty(IDs::goniometerScale))   return false;
    if (! tree.hasProperty(IDs::decayRate))         return false;
    
    return true;
}

//==============================================================================

// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PFM10AudioProcessor();
}
