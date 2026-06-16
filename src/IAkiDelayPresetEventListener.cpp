#include "IAkiDelayPresetEventListener.hpp"

// instantiating IAkiDelayPresetEventListener's event dispatcher
EventDispatcher<IAkiDelayPresetEventListener, AkiDelayPresetEvent,
		&IAkiDelayPresetEventListener::onAkiDelayPresetChangedEvent> IAkiDelayPresetEventListener::m_EventDispatcher;

AkiDelayPresetEvent::AkiDelayPresetEvent (const AkiDelayState& preset, unsigned int presetNum, unsigned int channel, const AkiDelayPresetEventTypeEnum& type) :
	IEvent( channel ),
	m_Preset( preset ),
	m_PresetNum( presetNum ),
	m_Type( type )
{
}

AkiDelayPresetEvent::~AkiDelayPresetEvent()
{
}

IAkiDelayPresetEventListener::~IAkiDelayPresetEventListener()
{
	this->unbindFromAkiDelayPresetEventSystem();
}

void IAkiDelayPresetEventListener::bindToAkiDelayPresetEventSystem()
{
	m_EventDispatcher.bind( this );
}

void IAkiDelayPresetEventListener::unbindFromAkiDelayPresetEventSystem()
{
	m_EventDispatcher.unbind( this );
}

void IAkiDelayPresetEventListener::PublishEvent (const AkiDelayPresetEvent& preset)
{
	m_EventDispatcher.dispatch( preset );
}
