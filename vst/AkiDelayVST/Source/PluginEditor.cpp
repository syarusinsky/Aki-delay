/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "SRAM_23K256.hpp"
#include "AudioConstants.hpp"

//==============================================================================
AkiDelayVSTAudioProcessorEditor::AkiDelayVSTAudioProcessorEditor (AkiDelayVSTAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      delayTimeSldr(),
      delayTimeLbl(),
      feedbackSldr(),
      feedbackLbl(),
      filtFreqSldr(),
      filtFreqLbl(),
      prevPresetBtn( "Prev Preset" ),
      presetNumLbl( "Preset Number", "1" ),
      nextPresetBtn( "Next Preset" ),
      writePresetBtn( "Write Preset" ),
      screenRep( juce::Image::RGB, 256, 128, true ) // this is actually double the size so we can actually see it
{
    // adding all child components
    addAndMakeVisible( delayTimeSldr );
    float maxDelayTime = static_cast<float>((Sram_23K256::SRAM_SIZE * 4)) / 2.0f / SAMPLE_RATE;
    delayTimeSldr.setRange( 0, maxDelayTime );
    delayTimeSldr.setTextValueSuffix( "Seconds" );
    delayTimeSldr.addListener( this );
    addAndMakeVisible( delayTimeLbl );
    delayTimeLbl.setText( "Delay Time", juce::dontSendNotification );
    delayTimeLbl.attachToComponent( &delayTimeSldr, true );

    addAndMakeVisible( feedbackSldr );
    feedbackSldr.setRange( 0, 99 );
    feedbackSldr.setTextValueSuffix( "%" );
    feedbackSldr.addListener( this );
    addAndMakeVisible( feedbackLbl );
    feedbackLbl.setText( "Feedback", juce::dontSendNotification );
    feedbackLbl.attachToComponent( &feedbackSldr, true );

    addAndMakeVisible( filtFreqSldr );
    filtFreqSldr.setRange( 1, 20000 );
    filtFreqSldr.setTextValueSuffix( "Hz" );
    filtFreqSldr.addListener( this );
    addAndMakeVisible( filtFreqLbl );
    filtFreqLbl.setText( "LPF Freq", juce::dontSendNotification );
    filtFreqLbl.attachToComponent( &filtFreqSldr, true );

    addAndMakeVisible( prevPresetBtn );
    prevPresetBtn.addListener( this );

    addAndMakeVisible( presetNumLbl );

    addAndMakeVisible( nextPresetBtn );
    nextPresetBtn.addListener( this );

    addAndMakeVisible( writePresetBtn );
    writePresetBtn.addListener( this );

    setSize( 800, 600 );
}

AkiDelayVSTAudioProcessorEditor::~AkiDelayVSTAudioProcessorEditor()
{
}

//==============================================================================
void AkiDelayVSTAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll( getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId) );

    // You can add your drawing code here!
    g.drawImageWithin( screenRep, 0, 150, getWidth(), 120, juce::RectanglePlacement::centred | juce::RectanglePlacement::doNotResize );
}

void AkiDelayVSTAudioProcessorEditor::resized()
{
    int sliderLeft = 120;
    delayTimeSldr.setBounds 	(sliderLeft, 20, getWidth() - sliderLeft - 10, 20);
    feedbackSldr.setBounds 	(sliderLeft, 60, getWidth() - sliderLeft - 10, 20);
    filtFreqSldr.setBounds 	(sliderLeft, 100, getWidth() - sliderLeft - 10, 20);
    prevPresetBtn.setBounds 	(sliderLeft + (getWidth() / 5) * 1, 1010, ((getWidth() - sliderLeft - 10) / 5), 20);
    presetNumLbl.setBounds 	(sliderLeft + (getWidth() / 5) * 2, 1010, ((getWidth() - sliderLeft - 10) / 5), 20);
    nextPresetBtn.setBounds 	((getWidth() / 5) * 3, 1010, ((getWidth() - sliderLeft - 10) / 5), 20);
    writePresetBtn.setBounds 	((getWidth() / 5) * 4, 1010, ((getWidth() - sliderLeft - 10) / 5), 20);
}

void AkiDelayVSTAudioProcessorEditor::sliderValueChanged (juce::Slider* slider)
{
    double val = slider->getValue();
    float percentage = (slider->getValue() - slider->getMinimum()) / (slider->getMaximum() - slider->getMinimum());

    if (slider == &delayTimeSldr)
    {
        // IPotEventListener::PublishEvent( PotEvent(percentage, static_cast<unsigned int>(POT_CHANNEL::DELAY_TIME)) );
    }
    else if (slider == &feedbackSldr)
    {
        // IPotEventListener::PublishEvent( PotEvent(percentage, static_cast<unsigned int>(POT_CHANNEL::FEEDBACK)) );
    }
    else if (slider == &filtFreqSldr)
    {
        // IPotEventListener::PublishEvent( PotEvent(percentage, static_cast<unsigned int>(POT_CHANNEL::FILT_FREQ)) );
    }
}

void AkiDelayVSTAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    // TODO reimplement these
    /*
    if (button == &prevPresetBtn)
    {
        uiSim.processPrevPresetBtn( true ); // pressed
        uiSim.processPrevPresetBtn( false ); // released
        uiSim.processPrevPresetBtn( false ); // floating
    }
    else if (button == &nextPresetBtn)
    {
        uiSim.processNextPresetBtn( true ); // pressed
        uiSim.processNextPresetBtn( false ); // released
        uiSim.processNextPresetBtn( false ); // floating
    }
    else if (button == &writePresetBtn)
    {
        uiSim.processWritePresetBtn( true ); // pressed
        uiSim.processWritePresetBtn( false ); // released
        uiSim.processWritePresetBtn( false ); // floating
    }
    */
}
