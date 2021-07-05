#ifndef AKIDELAYCONSTANTS_HPP
#define AKIDELAYCONSTANTS_HPP

#include "AudioConstants.hpp"

const float AKI_DELAY_TIME_MAX = 1.6384;
#ifdef TARGET_BUILD
const unsigned int AKI_DELAY_MAX_GLIDE_SAMPLES = ABUFFER_SIZE * 2 * 2; // should 256 * 2 * 2 = 1024
#else
const unsigned int AKI_DELAY_MAX_GLIDE_SAMPLES = ABUFFER_SIZE * 2; // should be 512 * 2 = 1024
#endif

#endif
