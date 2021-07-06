#include "IAkiDelayLCDRefreshEventListener.hpp"

// instantiating IAkiDelayLCDRefreshEventListener's event dispatcher
EventDispatcher<IAkiDelayLCDRefreshEventListener, AkiDelayLCDRefreshEvent,
		&IAkiDelayLCDRefreshEventListener::onAkiDelayLCDRefreshEvent> IAkiDelayLCDRefreshEventListener::m_EventDispatcher;

AkiDelayLCDRefreshEvent::AkiDelayLCDRefreshEvent (unsigned int xStart, unsigned int yStart, unsigned int xEnd, unsigned int yEnd,
							unsigned int channel) :
	IEvent( channel ),
	m_XStart( xStart ),
	m_YStart( yStart ),
	m_XEnd( xEnd ),
	m_YEnd( yEnd )
{
}

AkiDelayLCDRefreshEvent::~AkiDelayLCDRefreshEvent()
{
}

unsigned int AkiDelayLCDRefreshEvent::getXStart() const
{
	return m_XStart;
}

unsigned int AkiDelayLCDRefreshEvent::getYStart() const
{
	return m_YStart;
}

unsigned int AkiDelayLCDRefreshEvent::getXEnd() const
{
	return m_XEnd;
}

unsigned int AkiDelayLCDRefreshEvent::getYEnd() const
{
	return m_YEnd;
}

IAkiDelayLCDRefreshEventListener::~IAkiDelayLCDRefreshEventListener()
{
	this->unbindFromAkiDelayLCDRefreshEventSystem();
}

void IAkiDelayLCDRefreshEventListener::bindToAkiDelayLCDRefreshEventSystem()
{
	m_EventDispatcher.bind( this );
}

void IAkiDelayLCDRefreshEventListener::unbindFromAkiDelayLCDRefreshEventSystem()
{
	m_EventDispatcher.unbind( this );
}

void IAkiDelayLCDRefreshEventListener::PublishEvent (const AkiDelayLCDRefreshEvent& lcdRefreshEvent)
{
	m_EventDispatcher.dispatch( lcdRefreshEvent );
}
