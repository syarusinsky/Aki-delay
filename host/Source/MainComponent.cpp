/*
   ==============================================================================

   This file was auto-generated!

   ==============================================================================
   */

#ifdef __unix__
#include <unistd.h>
#elif defined(_WIN32) || defined(WIN32)
#include <windows.h>
#endif

#include "MainComponent.h"

#include "AkiDelayConstants.hpp"
#include "CPPFile.hpp"
#include "ColorProfile.hpp"
#include "FrameBuffer.hpp"
#include "Font.hpp"
#include "Sprite.hpp"
#include "SRAM_23K256.hpp"

const unsigned int FONT_FILE_SIZE = 779;
const unsigned int LOGO_FILE_SIZE = 119;

const int OpRadioId = 1001;
const int WaveRadioId = 1002;

static bool resetMaxAndMins = false;

//==============================================================================
MainComponent::MainComponent() :
	sAudioBuffer(),
	fakeStorageDevice( Sram_23K256::SRAM_SIZE * 4 ), // sram size on Gen_FX_SYN boards, with four srams installed
	akiDelayManager( &fakeStorageDevice ),
	writer(),
	delayTimeSldr(),
	delayTimeLbl(),
	feedbackSldr(),
	feedbackLbl(),
	filtFreqSldr(),
	filtFreqLbl(),
	audioSettingsBtn( "Audio Settings" ),
	prevPresetBtn( "Prev Preset" ),
	presetNumLbl( "Preset Number", "1" ),
	nextPresetBtn( "Next Preset" ),
	writePresetBtn( "Write Preset" ),
	audioSettingsComponent( deviceManager, 2, 2, &audioSettingsBtn ),
	screenRep( juce::Image::RGB, 256, 128, true ) // this is actually double the size so we can actually see it
{
	// load font and logo from file
	char* fontBytes = new char[FONT_FILE_SIZE];
	char* logoBytes = new char[LOGO_FILE_SIZE];

	const unsigned int pathMax = 1000;
#ifdef __unix__
	char result[pathMax];
	ssize_t count = readlink( "/proc/self/exe", result, pathMax );
	std::string assetsPath( result, (count > 0) ? count : 0 );
	std::string strToRemove( "host/Builds/LinuxMakefile/build/FMSynth" );

	std::string::size_type i = assetsPath.find( strToRemove );

	if ( i != std::string::npos )
	{
		assetsPath.erase( i, strToRemove.length() );
	}

	assetsPath += "assets/";

	std::string fontPath = assetsPath + "Smoll.sff";
	std::string logoPath = assetsPath + "theroomdisconnectlogo.sif";
#elif defined(_WIN32) || defined(WIN32)
	// TODO need to actually implement this for windows
#endif
	std::ifstream fontFile( fontPath, std::ifstream::binary );
	if ( ! fontFile )
	{
		std::cout << "FAILED TO OPEN FONT FILE!" << std::endl;
	}
	fontFile.read( fontBytes, FONT_FILE_SIZE );
	fontFile.close();

	Font* font = new Font( (uint8_t*)fontBytes );

	std::ifstream logoFile( logoPath, std::ifstream::binary );
	if ( ! logoFile )
	{
		std::cout << "FAILED TO OPEN LOGO FILE!" << std::endl;
	}
	logoFile.read( logoBytes, LOGO_FILE_SIZE );
	logoFile.close();

	Sprite* logo = new Sprite( (uint8_t*)logoBytes );

	// Some platforms require permissions to open input channels so request that here
	if ( juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
			&& ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio) )
	{
		juce::RuntimePermissions::request( juce::RuntimePermissions::recordAudio,
				[&] (bool granted) { if (granted)  setAudioChannels (2, 2); } );
	}
	else
	{
		// Specify the number of input and output channels that we want to open
		setAudioChannels (2, 2);
	}

	// TODO still need to do this
	// connecting the audio buffer to the voice manager
	// sAudioBuffer.registerCallback( &armor8VoiceManager );

	// juce audio device setup
	juce::AudioDeviceManager::AudioDeviceSetup deviceSetup = juce::AudioDeviceManager::AudioDeviceSetup();
	deviceSetup.sampleRate = 96000;
	deviceManager.initialise( 2, 2, 0, true, juce::String(), &deviceSetup );

	// basic juce logging
	// juce::Logger* log = juce::Logger::getCurrentLogger();
	// int sampleRate = deviceManager.getCurrentAudioDevice()->getCurrentSampleRate();
	// log->writeToLog( juce::String(sampleRate) );
	// log->writeToLog( juce::String(deviceManager.getCurrentAudioDevice()->getCurrentBufferSizeSamples()) );

	// optionally we can write wav files for debugging
	// juce::WavAudioFormat wav;
	// juce::File tempFile( "TestAudio.wav" );
	// juce::OutputStream* outStream( tempFile.createOutputStream() );
	// writer = wav.createWriterFor( outStream, sampleRate, 2, wav.getPossibleBitDepths().getLast(), NULL, 0 );

	// log->writeToLog( juce::String(wav.getPossibleBitDepths().getLast()) );
	// log->writeToLog( tempFile.getFullPathName() );

	// this file can also be used for debugging
	testFile.open( "JuceMainComponentOutput.txt" );

	// adding all child components
	addAndMakeVisible( delayTimeSldr );
	// this isn't actually as long of a delay time as on target, since sample rates are different
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

	addAndMakeVisible( audioSettingsBtn );
	audioSettingsBtn.addListener( this );

	addAndMakeVisible( prevPresetBtn );
	prevPresetBtn.addListener( this );

	addAndMakeVisible( presetNumLbl );

	addAndMakeVisible( nextPresetBtn );
	nextPresetBtn.addListener( this );

	addAndMakeVisible( writePresetBtn );
	writePresetBtn.addListener( this );

	// Make sure you set the size of the component after
	// you add any child components.
	setSize( 800, 600 );

	// start timer for fake loading
	this->startTimer( 33 );

	sAudioBuffer.registerCallback( &akiDelayManager );
}

MainComponent::~MainComponent()
{
	// This shuts down the audio device and clears the audio source.
	shutdownAudio();
	delete writer;
	testFile.close();
}

void MainComponent::timerCallback()
{
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
	// This function will be called when the audio device is started, or when
	// its settings (i.e. sample rate, block size, etc) are changed.

	// You can use this function to initialise any resources you might need,
	// but be careful - it will be called on the audio thread, not the GUI thread.

	// For more details, see the help for AudioProcessor::prepareToPlay()
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
	// Your audio-processing code goes here!

	// For more details, see the help for AudioProcessor::getNextAudioBlock()
	try
	{
		const float* inBufferL = bufferToFill.buffer->getReadPointer( 0, bufferToFill.startSample );
		const float* inBufferR = bufferToFill.buffer->getReadPointer( 1, bufferToFill.startSample );
		float* outBufferL = bufferToFill.buffer->getWritePointer( 0, bufferToFill.startSample );
		float* outBufferR = bufferToFill.buffer->getWritePointer( 1, bufferToFill.startSample );

		static uint16_t maxOut = 0;
		static uint16_t minOut = 0;
		if ( resetMaxAndMins ) { maxOut = 0; minOut = 0; resetMaxAndMins = false; }
		for ( auto sample = bufferToFill.startSample; sample < bufferToFill.numSamples; ++sample )
		{
			uint16_t sampleToReadBuffer = ( (inBufferL[sample] + 1.0f) * 0.5f ) * 4096.0f;
			uint16_t sampleOut = sAudioBuffer.getNextSample( sampleToReadBuffer );
			float sampleOutFloat = static_cast<float>( ((sampleOut / 4096.0f) * 2.0f) - 1.0f );
			if ( sampleOut > maxOut ) maxOut = sampleOut;
			if ( sampleOut < minOut ) minOut = sampleOut;
			outBufferL[sample] = sampleOutFloat;
			outBufferR[sample] = sampleOutFloat;
		}
		// std::cout << "maxOut: " << std::to_string(maxOut) << std::endl;
		// std::cout << "minOut: " << std::to_string(minOut) << std::endl;

		sAudioBuffer.pollToFillBuffers();
	}
	catch ( std::exception& e )
	{
		std::cout << "Exception caught in getNextAudioBlock: " << e.what() << std::endl;
	}
}

void MainComponent::releaseResources()
{
	// This will be called when the audio device stops, or when it is being
	// restarted due to a setting change.
	//
	// For more details, see the help for AudioProcessor::releaseResources()
	std::cout << "Resources released, resetting max and min" << std::endl;
	resetMaxAndMins = true;
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
	// (Our component is opaque, so we must completely fill the background with a solid colour)
	g.fillAll( getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId) );

	// You can add your drawing code here!
	g.drawImageWithin( screenRep, 0, 150, getWidth(), 120, juce::RectanglePlacement::centred | juce::RectanglePlacement::doNotResize );
}

void MainComponent::resized()
{
	// This is called when the MainContentComponent is resized.
	// If you add any child components, this is where you should
	// update their positions.
	int sliderLeft = 120;
	delayTimeSldr.setBounds 	(sliderLeft, 20, getWidth() - sliderLeft - 10, 20);
	feedbackSldr.setBounds 		(sliderLeft, 60, getWidth() - sliderLeft - 10, 20);
	filtFreqSldr.setBounds 		(sliderLeft, 100, getWidth() - sliderLeft - 10, 20);
	audioSettingsBtn.setBounds 	(sliderLeft, 950, getWidth() - sliderLeft - 10, 20);
	prevPresetBtn.setBounds 	(sliderLeft + (getWidth() / 5) * 1, 1010, ((getWidth() - sliderLeft - 10) / 5), 20);
	presetNumLbl.setBounds 		(sliderLeft + (getWidth() / 5) * 2, 1010, ((getWidth() - sliderLeft - 10) / 5), 20);
	nextPresetBtn.setBounds 	((getWidth() / 5) * 3, 1010, ((getWidth() - sliderLeft - 10) / 5), 20);
	writePresetBtn.setBounds 	((getWidth() / 5) * 4, 1010, ((getWidth() - sliderLeft - 10) / 5), 20);
}

void MainComponent::sliderValueChanged (juce::Slider* slider)
{
	try
	{
		double val = slider->getValue();
		float percentage = (slider->getValue() - slider->getMinimum()) / (slider->getMaximum() - slider->getMinimum());

		if (slider == &delayTimeSldr)
		{
			akiDelayManager.setDelayTime( val );
		}
		else if (slider == &feedbackSldr)
		{
			akiDelayManager.setFeedback( percentage );
		}
		else if (slider == &filtFreqSldr)
		{
			akiDelayManager.setFiltFreq( val );
		}
	}
	catch (std::exception& e)
	{
		std::cout << "Exception caught in slider handler: " << e.what() << std::endl;
	}
}

void MainComponent::buttonClicked (juce::Button* button)
{
	try
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
	catch (std::exception& e)
	{
		std::cout << "Exception caught in buttonClicked(): " << e.what() << std::endl;
	}
}

void MainComponent::updateToggleState (juce::Button* button)
{
	try
	{
		bool isPressed = button->getToggleState();
	}
	catch (std::exception& e)
	{
		std::cout << "Exception in toggle button shit: " << e.what() << std::endl;
	}
}

void MainComponent::copyFrameBufferToImage (unsigned int xStart, unsigned int yStart, unsigned int xEnd, unsigned int yEnd)
{
	// TODO reimplement this
	/*
	ColorProfile* colorProfile = uiSim.getColorProfile();
	FrameBuffer* frameBuffer = uiSim.getFrameBuffer();
	unsigned int frameBufferWidth = frameBuffer->getWidth();

	for ( unsigned int pixelY = yStart; pixelY < yEnd + 1; pixelY++ )
	{
		for ( unsigned int pixelX = xStart; pixelX < xEnd + 1; pixelX++ )
		{
			if ( ! colorProfile->getPixel(frameBuffer->getPixels(), (pixelY * frameBufferWidth) + pixelX).m_M )
			{
				screenRep.setPixelAt( (pixelX * 2),     (pixelY * 2),     juce::Colour(0, 0, 0) );
				screenRep.setPixelAt( (pixelX * 2) + 1, (pixelY * 2),     juce::Colour(0, 0, 0) );
				screenRep.setPixelAt( (pixelX * 2),     (pixelY * 2) + 1, juce::Colour(0, 0, 0) );
				screenRep.setPixelAt( (pixelX * 2) + 1, (pixelY * 2) + 1, juce::Colour(0, 0, 0) );
			}
			else
			{
				screenRep.setPixelAt( (pixelX * 2),     (pixelY * 2),     juce::Colour(0, 97, 252) );
				screenRep.setPixelAt( (pixelX * 2) + 1, (pixelY * 2),     juce::Colour(0, 97, 252) );
				screenRep.setPixelAt( (pixelX * 2),     (pixelY * 2) + 1, juce::Colour(0, 97, 252) );
				screenRep.setPixelAt( (pixelX * 2) + 1, (pixelY * 2) + 1, juce::Colour(0, 97, 252) );
			}
		}
	}
	*/
}
