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
	m_HiddenMenuHoldIncr( 0 )
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
}

void AkiDelayUiManager::onPotEvent (const PotEvent& potEvent)
{
	unsigned int channel = potEvent.getChannel();
	POT_CHANNEL channelEnum = static_cast<POT_CHANNEL>( channel );
	float percentage = potEvent.getPercentage();

	// TODO we need to filter the percentage A LOT for it to work on target

	if ( channelEnum == POT_CHANNEL::DELAY_TIME )
	{
		unsigned int numSamplesInSrams = ( Sram_23K256::SRAM_SIZE * 4 ) / sizeof(uint16_t);
		float maxDelayTime = static_cast<float>( numSamplesInSrams ) / static_cast<float>( SAMPLE_RATE );
		float delayTime =  maxDelayTime * percentage;
		IAkiDelayParameterEventListener::PublishEvent( AkiDelayParameterEvent(delayTime, channel) );
	}
	else if ( channelEnum == POT_CHANNEL::FEEDBACK )
	{
		IAkiDelayParameterEventListener::PublishEvent( AkiDelayParameterEvent(percentage, channel) );
	}
	else if ( channelEnum == POT_CHANNEL::FILT_FREQ )
	{
		float filterFreq = ( 20000.0f * percentage ) + 1.0f;
		IAkiDelayParameterEventListener::PublishEvent( AkiDelayParameterEvent(filterFreq, channel) );
	}
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
