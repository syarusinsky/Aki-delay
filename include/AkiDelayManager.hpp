#ifndef AKIDELAYMANAGER_HPP
#define AKIDELAYMANAGER_HPP

#include "IBufferCallback.hpp"
#include "IStorageMedia.hpp"
#include "AudioConstants.hpp"
#include "OnePoleFilter.hpp"
#include "SoftClipper.hpp"
#include "IAkiDelayParameterEventListener.hpp"
#include "NoiseGate.hpp"

#include <stdint.h>

class AkiDelayManager : public IBufferCallback<uint16_t>, public IAkiDelayParameterEventListener
{
	public:
		AkiDelayManager (IStorageMedia* delayBufferStorage);
		~AkiDelayManager() override;

		void setDelayTime (float delayTime); // delayTime should be in seconds
		void setFeedback (float feedback); // feedback should be in percentage
		void setFiltFreq (float filtFreq); // filtFreq should be in hertz

		void call (uint16_t* writeBuffer) override;

		void onAkiDelayParameterEvent (const AkiDelayParameterEvent& paramEvent) override;

	private:
		IStorageMedia* 	m_StorageMedia; // where delay buffer sits

		float 		m_DelayTime;
		float 		m_Feedback;
		float 		m_FiltFreq;

		unsigned int 	m_DelayBufferSize;
		unsigned int 	m_WriteIndex;
		unsigned int 	m_ReadIndex;

		bool 		m_GlideDirection; // if true, we're gliding our read pointer forwards toward the write pointer, else backwards

		NoiseGate<int16_t> 	m_NoiseGate;
		OnePoleFilter<float> 	m_Filt;
		SoftClipper<uint16_t> 	m_SoftClipper;
};

#endif
