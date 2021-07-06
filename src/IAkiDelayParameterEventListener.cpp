#include "IAkiDelayParameterEventListener.hpp"

// instantiating IAkiDelayParameterEventListener's event dispatcher
EventDispatcher<IAkiDelayParameterEventListener, AkiDelayParameterEvent,
		&IAkiDelayParameterEventListener::onAkiDelayParameterEvent> IAkiDelayParameterEventListener::m_EventDispatcher;

AkiDelayParameterEvent::AkiDelayParameterEvent (float value, unsigned int channel) :
	IEvent( channel ),
	m_Value( value )
{
}

AkiDelayParameterEvent::~AkiDelayParameterEvent()
{
}

float AkiDelayParameterEvent::getValue() const
{
	return m_Value;
}

IAkiDelayParameterEventListener::~IAkiDelayParameterEventListener()
{
	this->unbindFromAkiDelayParameterEventSystem();
}

void IAkiDelayParameterEventListener::bindToAkiDelayParameterEventSystem()
{
	m_EventDispatcher.bind( this );
}

void IAkiDelayParameterEventListener::unbindFromAkiDelayParameterEventSystem()
{
	m_EventDispatcher.unbind( this );
}

void IAkiDelayParameterEventListener::PublishEvent (const AkiDelayParameterEvent& paramEvent)
{
	m_EventDispatcher.dispatch( paramEvent );
}
