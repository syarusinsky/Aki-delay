#ifndef IAKIDELAYLCDREFRESHEVENTLISTENER_HPP
#define IAKIDELAYLCDREFRESHEVENTLISTENER_HPP

#include "IEventListener.hpp"

class AkiDelayLCDRefreshEvent : public IEvent
{
	public:
		AkiDelayLCDRefreshEvent (unsigned int xStart, unsigned int yStart, unsigned int xEnd, unsigned int yEnd,
						unsigned int channel);
		~AkiDelayLCDRefreshEvent() override;

		unsigned int getXStart() const;
		unsigned int getYStart() const;
		unsigned int getXEnd() const;
		unsigned int getYEnd() const;

	private:
		unsigned int m_XStart;
		unsigned int m_YStart;
		unsigned int m_XEnd;
		unsigned int m_YEnd;
};

class IAkiDelayLCDRefreshEventListener : public IEventListener
{
	public:
		virtual ~IAkiDelayLCDRefreshEventListener();

		virtual void onAkiDelayLCDRefreshEvent (const AkiDelayLCDRefreshEvent& lcdRefreshEvent) = 0;

		void bindToAkiDelayLCDRefreshEventSystem();
		void unbindFromAkiDelayLCDRefreshEventSystem();

		static void PublishEvent (const AkiDelayLCDRefreshEvent& lcdRefreshEvent);

	private:
		static EventDispatcher<IAkiDelayLCDRefreshEventListener, AkiDelayLCDRefreshEvent,
					&IAkiDelayLCDRefreshEventListener::onAkiDelayLCDRefreshEvent> m_EventDispatcher;
};

#endif // IAKIDELAYLCDREFRESHEVENTLISTENER_HPP
