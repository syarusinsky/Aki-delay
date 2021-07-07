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
	m_EffectBtn1PrevState( BUTTON_STATE::FLOATING ),
	m_EffectBtn2PrevState( BUTTON_STATE::FLOATING ),
	m_HiddenMenuHoldIncr( 0 ),
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
}

AkiDelayUiManager::~AkiDelayUiManager()
{
	delete m_Font;
	delete m_MainImage;
	delete m_HiddenImage;

	this->unbindFromPotEventSystem();
	this->unbindFromButtonEventSystem();
}

void AkiDelayUiManager::draw()
{
	m_Graphics->setColor( false );
	m_Graphics->fill();
	m_Graphics->setColor( true );
	m_Graphics->drawSprite( 0.0f, 0.0f, *m_MainImage );

	IAkiDelayLCDRefreshEventListener::PublishEvent( AkiDelayLCDRefreshEvent(0, 0, m_FrameBuffer->getWidth(), m_FrameBuffer->getHeight(), 0) );
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

	if ( channelEnum == POT_CHANNEL::DELAY_TIME )
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
	}
	else if ( channelEnum == POT_CHANNEL::FEEDBACK )
	{
		outputVal = percentage; // feedback percentage
		potStabilizerBuf = m_Pot2StabilizerBuf;
		potStabilizerIndex = &m_Pot2StabilizerIndex;
		potStabilizerValue = &m_Pot2StabilizerValue;
		allowedScatterLeft = m_Pot2StabilizerValue - AKI_DELAY_POT_STABIL_ALLOWED_SCATTER; // feedback is already a percentage
		allowedScatterRight = m_Pot2StabilizerValue + AKI_DELAY_POT_STABIL_ALLOWED_SCATTER;
	}
	else if ( channelEnum == POT_CHANNEL::FILT_FREQ )
	{
		outputVal = ( AKI_DELAY_MAX_FILT_FREQ * percentage ) + AKI_DELAY_MIN_FILT_FREQ; // filter frequency
		potStabilizerBuf = m_Pot3StabilizerBuf;
		potStabilizerIndex = &m_Pot3StabilizerIndex;
		potStabilizerValue = &m_Pot3StabilizerValue;
		float allowableScatter = AKI_DELAY_MAX_FILT_FREQ * AKI_DELAY_POT_STABIL_ALLOWED_SCATTER;
		allowedScatterLeft = m_Pot3StabilizerValue - allowableScatter;
		allowedScatterRight = m_Pot3StabilizerValue + allowableScatter;
	}
	else
	{
		return;
	}

#ifdef TARGET_BUILD
	// stabilize the potentiometer value by averaging all the values in the stabilizer buffers
	float averageValue = outputVal;
	for ( unsigned int index = 0; index < AKI_DELAY_POT_STABIL_NUM; index++ )
	{
		averageValue += potStabilizerBuf[index];
	}
	averageValue = averageValue / ( static_cast<float>(AKI_DELAY_POT_STABIL_NUM) + 1.0f );

	// only if the average breaks our 'hysteresis' do we actually set a new pot value
	if ( averageValue < allowedScatterLeft || averageValue > allowedScatterRight )
	{
		*potStabilizerValue = averageValue;

		this->updateParameterString( averageValue, channelEnum );
	}

	// write value to buffer and increment index
	potStabilizerBuf[*potStabilizerIndex] = outputVal;
	*potStabilizerIndex = ( *potStabilizerIndex + 1 ) % AKI_DELAY_POT_STABIL_NUM;
#else
	*potStabilizerValue = outputVal;
	this->updateParameterString( outputVal, channelEnum );
#endif

	IAkiDelayParameterEventListener::PublishEvent( AkiDelayParameterEvent(*potStabilizerValue, channel) );
}

void AkiDelayUiManager::onButtonEvent (const ButtonEvent& buttonEvent)
{
	unsigned int channel = buttonEvent.getChannel();
	BUTTON_CHANNEL channelEnum = static_cast<BUTTON_CHANNEL>( channel );
	BUTTON_STATE newState = buttonEvent.getButtonState();

	BUTTON_STATE* stateToUpdate = nullptr;

	if ( channelEnum == BUTTON_CHANNEL::EFFECT_BTN_1 )
	{
		stateToUpdate = &m_EffectBtn1PrevState;
	}
	else if ( channelEnum == BUTTON_CHANNEL::EFFECT_BTN_2 )
	{
		stateToUpdate = &m_EffectBtn2PrevState;
	}

	if ( stateToUpdate )
	{
		if ( (*stateToUpdate == BUTTON_STATE::PRESSED && newState == BUTTON_STATE::PRESSED)
				|| (*stateToUpdate == BUTTON_STATE::HELD && newState == BUTTON_STATE::PRESSED) )
		{
			*stateToUpdate = BUTTON_STATE::HELD;
		}
		else if ( (*stateToUpdate == BUTTON_STATE::RELEASED && newState == BUTTON_STATE::RELEASED)
				|| (*stateToUpdate == BUTTON_STATE::FLOATING && newState == BUTTON_STATE::RELEASED) )
		{
		}
		else
		{
			*stateToUpdate = newState;
		}

		if ( m_EffectBtn1PrevState == BUTTON_STATE::HELD && m_EffectBtn2PrevState == BUTTON_STATE::HELD )
		{
			m_HiddenMenuHoldIncr++;

			if ( m_HiddenMenuHoldIncr == HIDDEN_MENU_HOLD_MAX ) // == so this only happens once
			{
				this->switchToHiddenMenu();

				m_HiddenMenuHoldIncr = HIDDEN_MENU_HOLD_MAX;
			}
		}
		else
		{
			m_HiddenMenuHoldIncr = 0;
		}
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

void AkiDelayUiManager::updateParameterString (float value, const POT_CHANNEL& channel)
{
	// TODO lots of repetition here, clean it up eventually...
	if ( channel == POT_CHANNEL::DELAY_TIME )
	{
		unsigned int dTimeInt = value * 100.0f;
		char buffer[10] = { '0' };
		this->intToCString( dTimeInt, buffer, 10 );
		char bufferFinal[10] = { '0' };
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

		IAkiDelayLCDRefreshEventListener::PublishEvent( this->generatePartialLCDRefreshEvent(xStart, yStart, xEnd, yEnd) );
	}
	else if ( channel == POT_CHANNEL::FEEDBACK )
	{
		unsigned int feedbackInt = value * 100.0f;
		char buffer[10] = { '0' };
		this->intToCString( feedbackInt, buffer, 10 );
		char bufferFinal[10] = { '0' };
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

		IAkiDelayLCDRefreshEventListener::PublishEvent( this->generatePartialLCDRefreshEvent(xStart, yStart, xEnd, yEnd) );
	}
	else if ( channel == POT_CHANNEL::FILT_FREQ )
	{
		unsigned int filtFreqInt = value * 0.1f;
		char buffer[10] = { '0' };
		this->intToCString( filtFreqInt, buffer, 10 );
		char bufferFinal[10] = { '0' };
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

		IAkiDelayLCDRefreshEventListener::PublishEvent( this->generatePartialLCDRefreshEvent(xStart, yStart, xEnd, yEnd) );
	}
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
