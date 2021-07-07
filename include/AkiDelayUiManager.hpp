#ifndef AKIDELAYUIMANAGER_HPP
#define AKIDELAYUIMANAGER_HPP

#include "AkiDelayConstants.hpp"
#include "Surface.hpp"
#include "IPotEventListener.hpp"
#include "IButtonEventListener.hpp"
#include "IAkiDelayLCDRefreshEventListener.hpp"

#include <stdint.h>

enum class AKIDELAY_MENUS : unsigned int
{
	MAIN,
	HIDDEN
};

class Font;
class Sprite;

class AkiDelayUiManager : public Surface, public IPotEventListener, public IButtonEventListener
{
	public:
		AkiDelayUiManager (uint8_t* fontData, uint8_t* mainImageData, uint8_t* hiddenImageData);
		~AkiDelayUiManager() override;

		void draw() override;

		void onPotEvent (const PotEvent& potEvent) override;

		void onButtonEvent (const ButtonEvent& buttonEvent) override;

	private:
		Font* 		m_Font;

		Sprite* 	m_MainImage;
		Sprite* 	m_HiddenImage;

		AKIDELAY_MENUS 	m_CurrentMenu;

		BUTTON_STATE 	m_EffectBtn1PrevState;
		BUTTON_STATE 	m_EffectBtn2PrevState;
		unsigned int 	m_HiddenMenuHoldIncr;

		float 		m_Pot1StabilizerBuf[AKI_DELAY_POT_STABIL_NUM];
		float 		m_Pot2StabilizerBuf[AKI_DELAY_POT_STABIL_NUM];
		float 		m_Pot3StabilizerBuf[AKI_DELAY_POT_STABIL_NUM];
		unsigned int 	m_Pot1StabilizerIndex;
		unsigned int 	m_Pot2StabilizerIndex;
		unsigned int 	m_Pot3StabilizerIndex;
		float 		m_Pot1StabilizerValue; // we use this as the actual value to send
		float 		m_Pot2StabilizerValue;
		float 		m_Pot3StabilizerValue;
		float 		m_Pot1StabilizerCachedPer; // cached percentage for hysteresis
		float 		m_Pot2StabilizerCachedPer;
		float 		m_Pot3StabilizerCachedPer;

		void switchToHiddenMenu();
		void updateParameterString (float value, const POT_CHANNEL& channel);

		AkiDelayLCDRefreshEvent generatePartialLCDRefreshEvent (float xStart, float yStart, float xEnd, float yEnd);

		// note: this truncates ungracefully if bufferLen is smaller than needed
		void intToCString (int val, char* buffer, unsigned int bufferLen);

		void concatDigitStr (int val, char* sourceBuffer, char* destBuffer, unsigned int offset, unsigned int digitWidth,
					int decimalPlaceIndex = -1);
};

#endif // AKIDELAYUIMANAGER_HPP
