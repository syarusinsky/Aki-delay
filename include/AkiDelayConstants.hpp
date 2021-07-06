#ifndef AKIDELAYCONSTANTS_HPP
#define AKIDELAYCONSTANTS_HPP

#include "AudioConstants.hpp"

#ifdef TARGET_BUILD
const unsigned int AKI_DELAY_MAX_GLIDE_SAMPLES = 426; // similar to host value considering 40kHz sampling rate
#else
const unsigned int AKI_DELAY_MAX_GLIDE_SAMPLES = ABUFFER_SIZE * 2; // should be 512 * 2 = 1024
#endif

#endif
