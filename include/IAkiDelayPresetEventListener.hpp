#ifndef IAKIDELAYPRESETEVENTLISTENER_HPP
#define IAKIDELAYPRESETEVENTLISTENER_HPP

/*******************************************************************
 * An IAkiDelayPresetEventListener specifies a simple interface which
 * a subclass can use to be notified of AkiDelay preset events.
*******************************************************************/

#include "AkiDelayManager.hpp"
#include "IEventListener.hpp"

enum class AkiDelayPresetEventTypeEnum
{
	LOAD_PRESET,
	SEND_PRESET_REQUEST,
	SEND_ALL_PRESETS_REQUEST,
	ACCEPT_PRESET,
	ACCEPT_ALL_PRESETS,
	DENY_PRESET,
	FINISHED_SENDING_OR_RECEIVING_PRESETS,
};

class AkiDelayPresetEvent : public IEvent
{
	public:
		AkiDelayPresetEvent (const AkiDelayState& preset, unsigned int presetNum, unsigned int channel,
					const AkiDelayPresetEventTypeEnum& type = AkiDelayPresetEventTypeEnum::LOAD_PRESET);
		~AkiDelayPresetEvent() override;

		AkiDelayState getPreset() const { return m_Preset; }
		unsigned int getPresetNum() const { return m_PresetNum; }

		AkiDelayPresetEventTypeEnum getType() const { return m_Type; }

	private:
		AkiDelayState 			m_Preset;
		unsigned int 			m_PresetNum;
		AkiDelayPresetEventTypeEnum 	m_Type;
};

class IAkiDelayPresetEventListener : public IEventListener
{
	public:
		virtual ~IAkiDelayPresetEventListener();

		virtual void onAkiDelayPresetChangedEvent (const AkiDelayPresetEvent& preset) = 0;

		void bindToAkiDelayPresetEventSystem();
		void unbindFromAkiDelayPresetEventSystem();

		static void PublishEvent (const AkiDelayPresetEvent& preset);

	private:
		static EventDispatcher<IAkiDelayPresetEventListener, AkiDelayPresetEvent,
					&IAkiDelayPresetEventListener::onAkiDelayPresetChangedEvent> m_EventDispatcher;
};

#endif // IAKIDELAYPRESETLISTENER_HPP
