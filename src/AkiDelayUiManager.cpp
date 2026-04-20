#include "AkiDelayUiManager.hpp"

#include "AkiDelayConstants.hpp"
#include "IAkiDelayParameterEventListener.hpp"
#include "Graphics.hpp"
#include "Font.hpp"
#include "Sprite.hpp"
#include "IPotEventListener.hpp"
#include "IButtonEventListener.hpp"
#include "SRAM_23K256.hpp"

static const unsigned int HIDDEN_MENU_HOLD_MAX = 255; // defines how long to hold both buttons to get to hidden menu

AkiDelayUiManager::AkiDelayUiManager (uint8_t* fontData, uint8_t* mainImageData, uint8_t* hiddenImageData) :
	Surface( 128, 64, CP_FORMAT::MONOCHROME_1BIT ),
	m_Font( new Font(fontData) ),
	m_MainImage( new Sprite(mainImageData) ),
	m_HiddenImage( new Sprite(hiddenImageData) ),
	m_CurrentMenu( AKIDELAY_MENUS::MAIN ),
	m_Effect1BtnState( BUTTON_STATE::FLOATING ),
	m_Effect2BtnState( BUTTON_STATE::FLOATING ),
	m_HiddenMenuHoldIncr( 0 ),
	m_Effect1PotCached( 0.0f ),
	m_Effect2PotCached( 0.0f ),
	m_Effect3PotCached( 0.0f ),
	m_Effect1PotLocked( false ),
	m_Effect2PotLocked( false ),
	m_Effect3PotLocked( false ),
	m_Pot1StabilizerBuf{ 0.0f },
	m_Pot2StabilizerBuf{ 0.0f },
	m_Pot3StabilizerBuf{ 1.0f }, // we never want the filter value to be 0
	m_Pot1StabilizerIndex( 0 ),
	m_Pot2StabilizerIndex( 0 ),
	m_Pot3StabilizerIndex( 0 ),
	m_Pot1StabilizerValue( 0.0f ),
	m_Pot2StabilizerValue( 0.0f ),
	m_Pot3StabilizerValue( 1.0f )
{
	m_Graphics->setFont( m_Font );

	this->bindToPotEventSystem();
	this->bindToButtonEventSystem();
	this->bindToAkiDelayPresetEventSystem();
}

AkiDelayUiManager::~AkiDelayUiManager()
{
	delete m_Font;
	delete m_MainImage;
	delete m_HiddenImage;

	this->unbindFromPotEventSystem();
	this->unbindFromButtonEventSystem();
	this->unbindFromAkiDelayPresetEventSystem();
}

void AkiDelayUiManager::draw()
{
	m_Graphics->setColor( false );
	m_Graphics->fill();
	m_Graphics->setColor( true );
	m_Graphics->drawSprite( 0.0f, 0.0f, *m_MainImage );

	IAkiDelayLCDRefreshEventListener::PublishEvent( AkiDelayLCDRefreshEvent(0, 0, m_FrameBuffer->getWidth(), m_FrameBuffer->getHeight(), 0) );
}

void AkiDelayUiManager::onAkiDelayPresetChangedEvent (const AkiDelayPresetEvent& presetEvent)
{
	this->lockAllPots();

	this->updatePresetString( presetEvent.getPresetNum() + 1 );
}

void AkiDelayUiManager::onPotEvent (const PotEvent& potEvent)
{
	unsigned int channel = potEvent.getChannel();
	POT_CHANNEL channelEnum = static_cast<POT_CHANNEL>( channel );
	float percentage = potEvent.getPercentage();

	float outputVal = 0.0f;
	float* potStabilizerBuf = nullptr;
	unsigned int* potStabilizerIndex = nullptr;
	float* potStabilizerValue = nullptr;
	float allowedScatterLeft = 0.0f;
	float allowedScatterRight = 0.0f;
	bool* lockedStatus = nullptr;
	float* lockCachedVal = nullptr;

	if ( channelEnum == POT_CHANNEL::EFFECT1 )
	{
		unsigned int numSamplesInSrams = ( Sram_23K256::SRAM_SIZE * 4 ) / sizeof(uint16_t);
		float maxDelayTime = static_cast<float>( numSamplesInSrams ) / static_cast<float>( SAMPLE_RATE );
		outputVal =  maxDelayTime * percentage; // delay time
		potStabilizerBuf = m_Pot1StabilizerBuf;
		potStabilizerIndex = &m_Pot1StabilizerIndex;
		potStabilizerValue = &m_Pot1StabilizerValue;
		float allowableScatter = maxDelayTime * AKI_DELAY_POT_STABIL_ALLOWED_SCATTER;
		allowedScatterLeft = m_Pot1StabilizerValue - allowableScatter;
		allowedScatterRight = m_Pot1StabilizerValue + allowableScatter;
		lockedStatus = &m_Effect1PotLocked;
		lockCachedVal = &m_Effect1PotCached;
	}
	else if ( channelEnum == POT_CHANNEL::EFFECT2 )
	{
		outputVal = percentage; // feedback percentage
		potStabilizerBuf = m_Pot2StabilizerBuf;
		potStabilizerIndex = &m_Pot2StabilizerIndex;
		potStabilizerValue = &m_Pot2StabilizerValue;
		allowedScatterLeft = m_Pot2StabilizerValue - AKI_DELAY_POT_STABIL_ALLOWED_SCATTER; // feedback is already a percentage
		allowedScatterRight = m_Pot2StabilizerValue + AKI_DELAY_POT_STABIL_ALLOWED_SCATTER;
		lockedStatus = &m_Effect2PotLocked;
		lockCachedVal = &m_Effect2PotCached;
	}
	else if ( channelEnum == POT_CHANNEL::EFFECT3 )
	{
		outputVal = ( AKI_DELAY_MAX_FILT_FREQ * percentage ) + AKI_DELAY_MIN_FILT_FREQ; // filter frequency
		potStabilizerBuf = m_Pot3StabilizerBuf;
		potStabilizerIndex = &m_Pot3StabilizerIndex;
		potStabilizerValue = &m_Pot3StabilizerValue;
		float allowableScatter = AKI_DELAY_MAX_FILT_FREQ * AKI_DELAY_POT_STABIL_ALLOWED_SCATTER;
		allowedScatterLeft = m_Pot3StabilizerValue - allowableScatter;
		allowedScatterRight = m_Pot3StabilizerValue + allowableScatter;
		lockedStatus = &m_Effect3PotLocked;
		lockCachedVal = &m_Effect3PotCached;
	}
	else
	{
		return;
	}

	float averageValue = outputVal;
	if ( potEvent.skipStabilization() )
	{
		// fix the stabilization for the next time this function is run
		for ( unsigned int index = 0; index < AKI_DELAY_POT_STABIL_NUM; index++ )
		{
			potStabilizerBuf[index] = averageValue;
		}
	}
	else
	{
		// stabilize the potentiometer value by averaging all the values in the stabilizer buffers
		for ( unsigned int index = 0; index < AKI_DELAY_POT_STABIL_NUM; index++ )
		{
			averageValue += potStabilizerBuf[index];
		}
		averageValue = averageValue / ( static_cast<float>(AKI_DELAY_POT_STABIL_NUM) + 1.0f );
	}

	// only if the average breaks our 'hysteresis' do we actually set a new pot value
	if ( averageValue < allowedScatterLeft || averageValue > allowedScatterRight )
	{
		*potStabilizerValue = averageValue;

		if ( ! *lockedStatus ) // if not locked
		{
			this->updateParameterString( averageValue, channelEnum );
			IAkiDelayParameterEventListener::PublishEvent( AkiDelayParameterEvent(*potStabilizerValue, channel) );
		}
		else // if locked, update locked status
		{
			if ( this->hasBrokenLock(*lockedStatus, *lockCachedVal, *potStabilizerValue) )
			{
				this->updateParameterString( averageValue, channelEnum );
				IAkiDelayParameterEventListener::PublishEvent( AkiDelayParameterEvent(*potStabilizerValue, channel) );
			}
		}
	}

	// write value to buffer and increment index
	potStabilizerBuf[*potStabilizerIndex] = outputVal;
	*potStabilizerIndex = ( *potStabilizerIndex + 1 ) % AKI_DELAY_POT_STABIL_NUM;
}

void AkiDelayUiManager::lockAllPots()
{
	m_Effect1PotLocked = true;
	m_Effect2PotLocked = true;
	m_Effect3PotLocked = true;
	m_Effect1PotCached = m_Pot1StabilizerValue;
	m_Effect2PotCached = m_Pot2StabilizerValue;
	m_Effect3PotCached = m_Pot3StabilizerValue;
}

bool AkiDelayUiManager::hasBrokenLock (bool& potLockedVal, float& potCachedVal, float newPotVal)
{
	bool hasBrokenLowerRange  = newPotVal <= ( potCachedVal - m_PotChangeThreshold );
	bool hasBrokenHigherRange = newPotVal >= ( potCachedVal + m_PotChangeThreshold );

	if ( potLockedVal && (hasBrokenHigherRange || hasBrokenLowerRange) ) // if locked but broke threshold
	{
		potLockedVal = false;
		return true;
	}

	return false;
}

static bool _ignoreNextReleaseEffect1 = false;
static bool _ignoreNextReleaseEffect2 = false;

void AkiDelayUiManager::onButtonEvent (const ButtonEvent& buttonEvent)
{
	if ( buttonEvent.getButtonState() == BUTTON_STATE::RELEASED )
	{
		if ( buttonEvent.getChannel() == static_cast<unsigned int>(BUTTON_CHANNEL::EFFECT1) )
		{
			if ( _ignoreNextReleaseEffect1 )
			{
				_ignoreNextReleaseEffect1 = false;
			}
			else
			{
				if ( m_Effect2BtnState == BUTTON_STATE::HELD ) // double button press
				{
					_ignoreNextReleaseEffect2 = true;
					this->handleDoubleButtonPress();
				}
				else // single button press
				{
					this->handleEffect1SinglePress();
				}
			}
		}
		else if ( buttonEvent.getChannel() == static_cast<unsigned int>(BUTTON_CHANNEL::EFFECT2) )
		{
			if ( _ignoreNextReleaseEffect2 )
			{
				_ignoreNextReleaseEffect2 = false;
			}
			else
			{
				if ( m_Effect1BtnState == BUTTON_STATE::HELD ) // double button press
				{
					_ignoreNextReleaseEffect1 = true;
					this->handleDoubleButtonPress();
				}
				else // single button press
				{
					this->handleEffect2SinglePress();
				}
			}
		}
	}
}

static BUTTON_STATE _prevState1 = BUTTON_STATE::FLOATING;

void AkiDelayUiManager::processEffect1Btn (bool pressed)
{
	this->updateButtonState( m_Effect1BtnState, pressed );

	if ( _prevState1 != m_Effect1BtnState )
	{
		_prevState1 = m_Effect1BtnState;

		// this button event is then processed by this class' onButtonEvent with logic per menu
		IButtonEventListener::PublishEvent( ButtonEvent(_prevState1, static_cast<unsigned int>(BUTTON_CHANNEL::EFFECT1)) );
	}
}

static BUTTON_STATE _prevState2 = BUTTON_STATE::FLOATING;

void AkiDelayUiManager::processEffect2Btn (bool pressed)
{
	this->updateButtonState( m_Effect2BtnState, pressed );

	if ( _prevState2 != m_Effect2BtnState )
	{
		_prevState2 = m_Effect2BtnState;

		// this button event is then processed by this class' onButtonEvent with logic per menu
		IButtonEventListener::PublishEvent( ButtonEvent(_prevState2, static_cast<unsigned int>(BUTTON_CHANNEL::EFFECT2)) );
	}
}

void AkiDelayUiManager::switchToHiddenMenu()
{
	m_CurrentMenu = AKIDELAY_MENUS::HIDDEN;

	m_Graphics->setColor( false );
	m_Graphics->fill();
	m_Graphics->setColor( true );
	m_Graphics->drawSprite( 0.0f, 0.0f, *m_HiddenImage );

	IAkiDelayLCDRefreshEventListener::PublishEvent( AkiDelayLCDRefreshEvent(0, 0, m_FrameBuffer->getWidth(), m_FrameBuffer->getHeight(), 0) );
}

void AkiDelayUiManager::handleEffect1SinglePress()
{
	IAkiDelayParameterEventListener::PublishEvent( AkiDelayParameterEvent(0.0f, static_cast<unsigned int>(PARAM_CHANNEL::PREV_PRESET)) );
}

void AkiDelayUiManager::handleEffect2SinglePress()
{
	IAkiDelayParameterEventListener::PublishEvent( AkiDelayParameterEvent(0.0f, static_cast<unsigned int>(PARAM_CHANNEL::NEXT_PRESET)) );
}

void AkiDelayUiManager::handleDoubleButtonPress()
{
	IAkiDelayParameterEventListener::PublishEvent( AkiDelayParameterEvent(0.0f, static_cast<unsigned int>(PARAM_CHANNEL::WRITE_PRESET)) );
}

void AkiDelayUiManager::updatePresetString (const unsigned int presetNum)
{
	constexpr static float presetTextXStart = 0.88f;
	constexpr static float presetTextYStart = 0.08f;
	constexpr static float presetTextXEnd = 0.98f;
	constexpr static float presetTextYEnd = 0.16f;
	constexpr static float presetTextSingleDigitOffset = 0.05f;
	static char buffer[3];
	this->intToCString( presetNum, buffer, 3 );
	m_Graphics->setColor( false );
	m_Graphics->drawBoxFilled( presetTextXStart, presetTextYStart, presetTextXEnd, presetTextYEnd );
	m_Graphics->setColor( true );
	const float textXStart = presetTextXStart + ( (presetNum < 10) ? presetTextSingleDigitOffset : 0 );
	m_Graphics->drawText( textXStart, presetTextYStart, buffer, 1.0f );

	const unsigned int uIntXStart = presetTextXStart * m_FrameBuffer->getWidth();
	const unsigned int uIntYStart = presetTextYStart * m_FrameBuffer->getHeight();
	const unsigned int uIntXEnd   = presetTextXEnd * m_FrameBuffer->getWidth();
	const unsigned int uIntYEnd   = presetTextYEnd * m_FrameBuffer->getHeight();
	IAkiDelayLCDRefreshEventListener::PublishEvent( AkiDelayLCDRefreshEvent(uIntXStart, uIntYStart, uIntXEnd, uIntYEnd, 0) );
}

void AkiDelayUiManager::updateButtonState (BUTTON_STATE& buttonState, bool pressed)
{
	if ( pressed )
	{
		if ( buttonState == BUTTON_STATE::FLOATING || buttonState == BUTTON_STATE::RELEASED )
		{
			buttonState = BUTTON_STATE::PRESSED;
		}
		else
		{
			buttonState = BUTTON_STATE::HELD;
		}
	}
	else
	{
		if ( buttonState == BUTTON_STATE::PRESSED || buttonState == BUTTON_STATE::HELD )
		{
			buttonState = BUTTON_STATE::RELEASED;
		}
		else
		{
			buttonState = BUTTON_STATE::FLOATING;
		}
	}
}

void AkiDelayUiManager::updateParameterString (float value, const POT_CHANNEL& channel)
{
	// TODO right now this isn't performant enough to run on Gen_FX_SYN rev 2,... so forget it
	/*
	// TODO lots of repetition here, clean it up eventually...
	if ( channel == POT_CHANNEL::EFFECT1 )
	{
		unsigned int dTimeInt = value * 100.0f;
		char buffer[5] = { '0' };
		this->intToCString( dTimeInt, buffer, 10 );
		char bufferFinal[5] = { '0' };
		bufferFinal[1] = '.';
		this->concatDigitStr( dTimeInt, buffer, bufferFinal, 0, 4, 2 );

		float xStart = 0.75f;
		float yStart = 0.1f;
		float xEnd = 0.98f;
		float yEnd = 0.22f;
		m_Graphics->setColor( false );
		m_Graphics->drawBoxFilled( xStart, yStart, xEnd, yEnd );
		m_Graphics->setColor( true );
		m_Graphics->drawText( xStart, yStart, bufferFinal, 1.0f );
	}
	else if ( channel == POT_CHANNEL::EFFECT2 )
	{
		unsigned int feedbackInt = value * 100.0f;
		char buffer[5] = { '0' };
		this->intToCString( feedbackInt, buffer, 10 );
		char bufferFinal[5] = { '0' };
		bufferFinal[1] = '.';
		this->concatDigitStr( feedbackInt, buffer, bufferFinal, 0, 4, 2 );

		float xStart = 0.75f;
		float yStart = 0.3f;
		float xEnd = 0.98f;
		float yEnd = 0.42f;
		m_Graphics->setColor( false );
		m_Graphics->drawBoxFilled( xStart, yStart, xEnd, yEnd );
		m_Graphics->setColor( true );
		m_Graphics->drawText( xStart, yStart + 0.018f, bufferFinal, 1.0f );
	}
	else if ( channel == POT_CHANNEL::EFFECT3 )
	{
		unsigned int filtFreqInt = value * 0.1f;
		char buffer[5] = { '0' };
		this->intToCString( filtFreqInt, buffer, 10 );
		char bufferFinal[5] = { '0' };
		this->concatDigitStr( filtFreqInt, buffer, bufferFinal, 0, 4 );
		bufferFinal[2] = '.'; // TODO techically inaccurate, but will have to do for now

		float xStart = 0.75f;
		float yStart = 0.5f;
		float xEnd = 0.98f;
		float yEnd = 0.62f;
		m_Graphics->setColor( false );
		m_Graphics->drawBoxFilled( xStart, yStart, xEnd, yEnd );
		m_Graphics->setColor( true );
		m_Graphics->drawText( xStart, yStart + 0.04f, bufferFinal, 1.0f );
	}
	*/
}

void AkiDelayUiManager::intToCString (int val, char* buffer, unsigned int bufferLen)
{
	if ( bufferLen == 0 ) return;

	unsigned int bufferIndex = 0;

	bool isNegative = val < 0;

	unsigned int valUInt = isNegative ? -val : val;

	while ( valUInt != 0 )
	{
		if ( bufferIndex == bufferLen - 1 )
		{
			buffer[bufferIndex] = '\0';
			return;
		}

		buffer[bufferIndex] = ( valUInt % 10 ) + '0';
		valUInt = valUInt / 10;
		bufferIndex++;
	}

	if ( isNegative && bufferIndex != bufferLen - 1 )
	{
		buffer[bufferIndex] = '-';
		bufferIndex++;
	}

	buffer[bufferIndex] = '\0';

	for ( int swapIndex = 0; swapIndex < bufferIndex/2; swapIndex++ )
	{
		buffer[swapIndex] ^= buffer[bufferIndex - swapIndex - 1];
		buffer[bufferIndex - swapIndex - 1] ^= buffer[swapIndex];
		buffer[swapIndex] ^= buffer[ bufferIndex - swapIndex - 1];
	}

	if ( val == 0 )
	{
		buffer[0] = '0';
		buffer[1] = '\0';
	}
}

void AkiDelayUiManager::concatDigitStr (int val, char* sourceBuffer, char* destBuffer, unsigned int offset, unsigned int digitWidth,
					int decimalPlaceIndex)
{
	int sourceNumDigits = 1;

	unsigned int valAbs = abs( val );

	if ( valAbs > 0 )
	{
		for (sourceNumDigits = 0; valAbs > 0; sourceNumDigits++)
		{
			valAbs = valAbs / 10;
		}
	}

	// if it's negative, we need an extra space
	if ( val < 0 ) sourceNumDigits += 1;

	bool usingDecimalPoint = false;

	// if it's got a decimal place, it also needs an extra space
	if ( decimalPlaceIndex > -1 )
	{
		usingDecimalPoint = true;
		sourceNumDigits += 1;
	}

	int numToSkipInt = sourceNumDigits - digitWidth;
	unsigned int numToSkip = abs( numToSkipInt );

	// this needs to be set after skipping the decimal place so that source buffer index is still correct
	unsigned int decimalPlaceOffset = 0;

	for ( unsigned int index = 0; index < digitWidth; index++ )
	{
		if ( index != decimalPlaceIndex - 1 )
		{
			if ( index < (numToSkip + decimalPlaceOffset) )
			{
				destBuffer[offset + index] = ' ';
			}
			else
			{
				destBuffer[offset + index] = sourceBuffer[index - numToSkip - decimalPlaceOffset];
			}
		}
		else
		{
			decimalPlaceOffset = 1;
		}
	}
}

AkiDelayLCDRefreshEvent AkiDelayUiManager::generatePartialLCDRefreshEvent (float xStart, float yStart, float xEnd, float yEnd)
{
	unsigned int xStartInt = xStart * static_cast<float>( m_FrameBuffer->getWidth() );
	unsigned int yStartInt = yStart * static_cast<float>( m_FrameBuffer->getHeight() );
	unsigned int xEndInt = xEnd * static_cast<float>( m_FrameBuffer->getWidth() );
	unsigned int yEndInt = yEnd * static_cast<float>( m_FrameBuffer->getHeight() );

	return AkiDelayLCDRefreshEvent( xStartInt, yStartInt, xEndInt, yEndInt, 0 );
}
