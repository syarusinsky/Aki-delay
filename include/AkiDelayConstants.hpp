#ifndef AKIDELAYCONSTANTS_HPP
#define AKIDELAYCONSTANTS_HPP

#include "AudioConstants.hpp"

#ifdef TARGET_BUILD
const unsigned int AKI_DELAY_MAX_GLIDE_SAMPLES = 426; // similar to host value considering 40kHz sampling rate
#else
const unsigned int AKI_DELAY_MAX_GLIDE_SAMPLES = ABUFFER_SIZE * 2; // should be 512 * 2 = 1024
#endif

const unsigned int AKI_DELAY_POT_STABIL_NUM = 10; // used for stabilizing the erratic potentiometer readings

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
