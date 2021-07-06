#include "AkiDelayUiManager.hpp"

#include "AkiDelayConstants.hpp"
#include "IAkiDelayParameterEventListener.hpp"
#include "IAkiDelayLCDRefreshEventListener.hpp"
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
	m_Pot3StabilizerIndex( 0 )
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
	// m_Graphics->drawSprite( 0.0f, 0.0f, *m_MainImage );
	m_Graphics->setColor( false );
	m_Graphics->fill();
	m_Graphics->setColor( true );
	m_Graphics->drawSprite( 0.0f, 0.0f, *m_MainImage );
	// TODO need to send a screen refresh event

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

	if ( channelEnum == POT_CHANNEL::DELAY_TIME )
	{
		unsigned int numSamplesInSrams = ( Sram_23K256::SRAM_SIZE * 4 ) / sizeof(uint16_t);
		float maxDelayTime = static_cast<float>( numSamplesInSrams ) / static_cast<float>( SAMPLE_RATE );
		outputVal =  maxDelayTime * percentage; // delay time
		potStabilizerBuf = m_Pot1StabilizerBuf;
		potStabilizerIndex = &m_Pot1StabilizerIndex;
	}
	else if ( channelEnum == POT_CHANNEL::FEEDBACK )
	{
		outputVal = percentage; // feedback percentage
		potStabilizerBuf = m_Pot2StabilizerBuf;
		potStabilizerIndex = &m_Pot2StabilizerIndex;
	}
	else if ( channelEnum == POT_CHANNEL::FILT_FREQ )
	{
		outputVal = ( 20000.0f * percentage ) + 1.0f; // filter frequency
		potStabilizerBuf = m_Pot3StabilizerBuf;
		potStabilizerIndex = &m_Pot3StabilizerIndex;
	}
	else
	{
		return;
	}

	// stabilize the potentiometer value by averaging all the values in the stabilizer buffers
	float averageValue = outputVal;
	for ( unsigned int index = 0; index < AKI_DELAY_POT_STABIL_NUM; index++ )
	{
		averageValue += potStabilizerBuf[index];
	}
	averageValue = averageValue / ( static_cast<float>(AKI_DELAY_POT_STABIL_NUM) + 1.0f );
	potStabilizerBuf[*potStabilizerIndex] = averageValue;

	// increment potStabilizer index
	*potStabilizerIndex = ( *potStabilizerIndex + 1 ) % AKI_DELAY_POT_STABIL_NUM;

	IAkiDelayParameterEventListener::PublishEvent( AkiDelayParameterEvent(averageValue, channel) );
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
	m_Graphics->drawSprite( 0.0f, 0.0f, *m_HiddenImage );
	// TODO need to send a screen refresh event
}
