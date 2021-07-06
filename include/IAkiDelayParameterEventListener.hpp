#ifndef IAKIDELAYPARAMETEREVENTLISTENER_HPP
#define IAKIDELAYPARAMETEREVENTLISTENER_HPP

#include "IEventListener.hpp"

class AkiDelayParameterEvent : public IEvent
{
	public:
		AkiDelayParameterEvent (float value, unsigned int channel);
		~AkiDelayParameterEvent() override;

		float getValue() const;

	private:
		float m_Value;
};

class IAkiDelayParameterEventListener : public IEventListener
{
	public:
		virtual ~IAkiDelayParameterEventListener();

		virtual void onAkiDelayParameterEvent (const AkiDelayParameterEvent& paramEvent) = 0;

		void bindToAkiDelayParameterEventSystem();
		void unbindFromAkiDelayParameterEventSystem();

		static void PublishEvent (const AkiDelayParameterEvent& paramEvent);

	private:
		static EventDispatcher<IAkiDelayParameterEventListener, AkiDelayParameterEvent,
					&IAkiDelayParameterEventListener::onAkiDelayParameterEvent> m_EventDispatcher;
};

#endif // IAKIDELAYPARAMETEREVENTLISTENER_HPP
