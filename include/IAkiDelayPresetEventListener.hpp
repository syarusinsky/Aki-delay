#ifndef IAKIDELAYPRESETEVENTLISTENER_HPP
#define IAKIDELAYPRESETEVENTLISTENER_HPP

/*******************************************************************
 * An IAkiDelayPresetEventListener specifies a simple interface which
 * a subclass can use to be notified of AkiDelay preset events.
*******************************************************************/

#include "AkiDelayManager.hpp"
#include "IEventListener.hpp"

class AkiDelayPresetEvent : public IEvent
{
	public:
		AkiDelayPresetEvent (const AkiDelayState& preset, unsigned int presetNum, unsigned int channel);
		~AkiDelayPresetEvent() override;

		AkiDelayState getPreset() const { return m_Preset; }
		unsigned int getPresetNum() const { return m_PresetNum; }

	private:
		AkiDelayState m_Preset;
		unsigned int m_PresetNum;
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
