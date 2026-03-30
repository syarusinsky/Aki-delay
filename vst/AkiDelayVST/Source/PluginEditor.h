/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class AkiDelayVSTAudioProcessorEditor  : public juce::AudioProcessorEditor, private juce::Slider::Listener, private juce::Button::Listener,
                                         private IAkiDelayLCDRefreshEventListener
{
public:
    AkiDelayVSTAudioProcessorEditor (AkiDelayVSTAudioProcessor&);
    ~AkiDelayVSTAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void sliderValueChanged (juce::Slider* slider) override;
    void buttonClicked (juce::Button* button) override;
    void onAkiDelayLCDRefreshEvent (const AkiDelayLCDRefreshEvent& lcdRefreshEvent) override;

    void copyFrameBufferToImage (unsigned int xStart, unsigned int yStart, unsigned int xEnd, unsigned int yEnd);

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    AkiDelayVSTAudioProcessor& audioProcessor;

    juce::Slider delayTimeSldr;
    juce::Label delayTimeLbl;

    juce::Slider feedbackSldr;
    juce::Label feedbackLbl;

    juce::Slider filtFreqSldr;
    juce::Label filtFreqLbl;

    juce::TextButton prevPresetBtn;
    juce::Label presetNumLbl;
    juce::TextButton nextPresetBtn;
    juce::TextButton writePresetBtn;

    juce::Image screenRep;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AkiDelayVSTAudioProcessorEditor)
};
