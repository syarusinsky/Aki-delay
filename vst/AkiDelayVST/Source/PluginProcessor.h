/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include "AudioBuffer.hpp"
#include "FakeStorageDevice.hpp"
#include "AkiDelayManager.hpp"
#include "AkiDelayUiManager.hpp"
#include "IAkiDelayLCDRefreshEventListener.hpp"
#include "SampleRateConverter.hpp"
#include "SRAM_23K256.hpp"

#include <JuceHeader.h>

//==============================================================================
/**
*/
class AkiDelayVSTAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    AkiDelayVSTAudioProcessor();
    ~AkiDelayVSTAudioProcessor() override;

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

    AkiDelayUiManager& getAkiDelayUiManager() { return akiDelayUiManager; }

    AudioProcessorValueTreeState& getVTS() { return apvts; }

    constexpr static float MAX_DELAY_TIME = static_cast<float>((Sram_23K256::SRAM_SIZE * 4)) / 2.0f / SAMPLE_RATE;

private:
    ::AudioBuffer<uint16_t> sAudioBuffer;

    FakeStorageDevice fakeStorageDevice;

    AkiDelayManager akiDelayManager;
    AkiDelayUiManager akiDelayUiManager;

    SampleRateConverter<float, uint16_t> sampleRateConverter;

    UndoManager undoManager;
    AudioProcessorValueTreeState apvts;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AkiDelayVSTAudioProcessor)
};
