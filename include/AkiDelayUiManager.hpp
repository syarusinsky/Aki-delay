#ifndef AKIDELAYUIMANAGER_HPP
#define AKIDELAYUIMANAGER_HPP

#include "Surface.hpp"
#include "IPotEventListener.hpp"
#include "IButtonEventListener.hpp"

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

		void switchToHiddenMenu();

	private:
		Font* 		m_Font;

		Sprite* 	m_MainImage;
		Sprite* 	m_HiddenImage;

		AKIDELAY_MENUS 	m_CurrentMenu;

		BUTTON_STATE 	m_EffectBtn1PrevState;
		BUTTON_STATE 	m_EffectBtn2PrevState;
		unsigned int 	m_HiddenMenuHoldIncr;
};

#endif // AKIDELAYUIMANAGER_HPP
