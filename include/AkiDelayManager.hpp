#ifndef AKIDELAYMANAGER_HPP
#define AKIDELAYMANAGER_HPP

#include "IBufferCallback.hpp"
#include "IStorageMedia.hpp"
#include "AudioConstants.hpp"
#include "OnePoleFilter.hpp"
#include "SoftClipper.hpp"
#include "IAkiDelayParameterEventListener.hpp"
#include "ISalSysexEventListener.hpp"
#include "NoiseGate.hpp"

#include <stdint.h>

class MidiHandler;
class PresetManager;

// the AkiDelayState struct makes saving states for presets easier, since it's easily serializable
struct AkiDelayState
{
	float m_DelayTime;
	float m_Feedback;
	float m_FiltFreq;
};

// the AkiDelayPresetHeader is intended to be used as a header for the PresetManager, it tracks the preset version
struct AkiDelayPresetHeader
{
	int versionMajor;
	int versionMinor;
	int versionPatch;

	bool presetsFileInitialized;

	bool operator!= (const AkiDelayPresetHeader& other)
	{
		if (versionMajor == other.versionMajor && versionMinor == other.versionMinor && versionPatch == other.versionPatch)
		{
			return false;
		}

		return true;
	}
};

class AkiDelayManager : public IBufferCallback<uint16_t>, public IAkiDelayParameterEventListener, public ISalSysexEventListener
{
	public:
		AkiDelayManager (IStorageMedia* delayBufferStorage, MidiHandler* midiHandler, PresetManager* presetManager);
		~AkiDelayManager() override;

		void setDelayTime (float delayTime); // delayTime should be in seconds
		void setFeedback (float feedback); // feedback should be in percentage
		void setFiltFreq (float filtFreq); // filtFreq should be in hertz

		AkiDelayState getState();
		void setState (const AkiDelayState& state);
		void loadCurrentPreset();

		AkiDelayPresetHeader getPresetHeader();

		void call (uint16_t* writeBuffer) override;

		void onAkiDelayParameterEvent (const AkiDelayParameterEvent& paramEvent) override;
		void onSalSysexEvent( const SalSysexEvent& salSysexEvent) override;

		uint8_t getDevId() { return m_DevId; }

	private:
		IStorageMedia* 		m_StorageMedia; // where delay buffer sits

		MidiHandler* 		m_MidiHandler;
		PresetManager* 		m_PresetManager;
		AkiDelayPresetHeader 	m_PresetHeader;

		float 			m_DelayTime;
		float 			m_Feedback;
		float 			m_FiltFreq;

		unsigned int 		m_DelayBufferSize;
		unsigned int 		m_WriteIndex;
		unsigned int 		m_ReadIndex;

		bool 			m_GlideDirection; // if true, we're gliding our read pointer forwards toward the write pointer, else backwards

		NoiseGate<int16_t> 	m_NoiseGate;
		OnePoleFilter<float> 	m_Filt;
		SoftClipper<uint16_t> 	m_SoftClipper;

		AkiDelayState 		m_PresetToSendOrReceive;
		unsigned int 		m_PresetToSendOrReceiveNum;
		uint8_t 		m_DevId;
		uint8_t 		m_SenderId; // the other unit's id in the preset exchange
		uint8_t 		m_RequestedPresetNum;
		bool 			m_SendingOrReceivingAllPresets = false;
		unsigned int 		m_NibbleIndex = 0;

		uint8_t generateRandomDevId();
		uint16_t getNumNibblesInPreset();
};

#endif
