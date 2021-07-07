#ifndef AKIDELAYCONSTANTS_HPP
#define AKIDELAYCONSTANTS_HPP

#include "AudioConstants.hpp"

const unsigned int AKI_DELAY_MAX_GLIDE_SAMPLES = 128; // ABUFFER_SIZE; // similar to host value considering 40kHz sampling rate

const float AKI_DELAY_MAX_FILT_FREQ = 20000.0f;
const float AKI_DELAY_MIN_FILT_FREQ = 1.0f;

const unsigned int AKI_DELAY_POT_STABIL_NUM = 50; // pot stabilization stuff, see AkiDelayUiManager
const float AKI_DELAY_POT_STABIL_ALLOWED_SCATTER = 0.05f; // allow 5% of jitter on pots

enum class POT_CHANNEL : unsigned int
{
	DELAY_TIME 	= 0,
	FEEDBACK 	= 1,
	FILT_FREQ 	= 2
};

enum class BUTTON_CHANNEL: unsigned int
{
	EFFECT_BTN_1 	= 0,
	EFFECT_BTN_2 	= 1
};

#endif
