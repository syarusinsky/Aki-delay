#include "AkiDelayManager.hpp"

#include "AkiDelayConstants.hpp"
#include "SRAM_23K256.hpp"
#include "PresetManager.hpp"
#include "MidiHandler.hpp"
#include "IAkiDelayPresetEventListener.hpp"

#include <string.h>
#include <algorithm>
#include <random>

AkiDelayManager::AkiDelayManager (IStorageMedia* delayBufferStorage, MidiHandler* midiHandler, PresetManager* presetManager) :
	m_StorageMedia( delayBufferStorage ),
	m_MidiHandler( midiHandler ),
	m_PresetManager( presetManager ),
	m_PresetHeader( {1, 0, 0, true} ),
	m_DelayTime( 0.0f ),
	m_Feedback( 0.0f ),
	m_FiltFreq( 20000.0f ),
	m_DelayBufferSize( (Sram_23K256::SRAM_SIZE * 4) / sizeof(uint16_t) ), // size of 4 srams installed on Gen_FX_SYN rev 2
	m_WriteIndex( ABUFFER_SIZE ),
	m_ReadIndex( 0 ),
	m_GlideDirection( true ),
	m_NoiseGate( 0.02f, 100.0f, 100 ),
	m_Filt(),
	m_SoftClipper(),
	m_PresetToSendOrReceive( this->getState() ),
	m_PresetToSendOrReceiveNum( 0 ),
	m_DevId( 0 ),
	m_SenderId( 0 )
{
	this->bindToAkiDelayParameterEventSystem();
	this->bindToSalSysexEventSystem();
}

AkiDelayManager::~AkiDelayManager()
{
	this->unbindFromAkiDelayParameterEventSystem();
	this->unbindFromSalSysexEventSystem();
}

void AkiDelayManager::setDelayTime (float delayTime)
{
	// if delay time is less, we need to glide forward towards write index
	if ( delayTime < m_DelayTime )
	{
		m_GlideDirection = true;
	}
	else if ( delayTime > m_DelayTime ) // likewise if delay time is more, we need to glide backwards away from write index
	{
		m_GlideDirection = false;
	}
	m_DelayTime = delayTime;
}

void AkiDelayManager::setFeedback (float feedback)
{
	m_Feedback = feedback;
}

void AkiDelayManager::setFiltFreq (float filtFreq)
{
	m_FiltFreq = filtFreq;
	m_Filt.setCoefficients( filtFreq );
}

AkiDelayState AkiDelayManager::getState()
{
	AkiDelayState state = { m_DelayTime, m_Feedback, m_FiltFreq };

	return state;
}

void AkiDelayManager::setState (const AkiDelayState& state)
{
	this->setDelayTime( state.m_DelayTime );
	this->setFeedback( state.m_Feedback );
	this->setFiltFreq( state.m_FiltFreq );
}

void AkiDelayManager::loadCurrentPreset()
{
	if ( m_PresetManager )
	{
		AkiDelayState preset = m_PresetManager->retrievePreset<AkiDelayState>( m_PresetManager->getCurrentPresetNum() );
		this->setState( preset );
	}
}

AkiDelayPresetHeader AkiDelayManager::getPresetHeader()
{
	return m_PresetHeader;
}

void AkiDelayManager::call (uint16_t* writeBuffer)
{
	// first offset for noise gate
	int16_t* writeBufferInt16 = reinterpret_cast<int16_t*>( writeBuffer );
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		// for hardware dac
		// writeBuffer[sample] -= 2048;

		// for spi dac
		writeBuffer[sample] -= 32767;
	}

	m_NoiseGate.call( writeBufferInt16 );

	// offset back
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		// for hardware dac
		// writeBuffer[sample] += 2048;

		// for spi dac
		writeBuffer[sample] += 32767;
	}

	// set correct read index, feedback amount, and filter frequency for this block
	unsigned int delayTimeInSamples = static_cast<unsigned int>( m_DelayTime * SAMPLE_RATE );
	// ensure that the read index at least lags behind the write index by ABUFFER_SIZE samples
	int newReadIndex = m_WriteIndex - ( (delayTimeInSamples < ABUFFER_SIZE) ? ABUFFER_SIZE : delayTimeInSamples );
	newReadIndex = ( newReadIndex < 0 ) ? m_DelayBufferSize + newReadIndex : newReadIndex;
	newReadIndex &= ~(0b1); // ensure it is even, to align the data
	int oldReadIndex = m_ReadIndex;
	float feedback = m_Feedback;

	// we will likely have to glide between some samples, so calculate how many here
	unsigned int samplesToGlide = 0;
	if ( oldReadIndex != newReadIndex && m_GlideDirection ) // gliding forwards towards write index
	{
		samplesToGlide = ( newReadIndex > oldReadIndex ) ? newReadIndex - oldReadIndex : (m_DelayBufferSize - oldReadIndex) + newReadIndex;

		if ( samplesToGlide > AKI_DELAY_MAX_GLIDE_SAMPLES )
		{
			newReadIndex = ( oldReadIndex + AKI_DELAY_MAX_GLIDE_SAMPLES ) % m_DelayBufferSize;
			samplesToGlide = AKI_DELAY_MAX_GLIDE_SAMPLES;
		}
	}
	else if ( oldReadIndex != newReadIndex ) // gliding backwards away from write index
	{
		samplesToGlide = ( newReadIndex < oldReadIndex ) ? oldReadIndex - newReadIndex : (m_DelayBufferSize - newReadIndex) + oldReadIndex;

		if ( samplesToGlide > AKI_DELAY_MAX_GLIDE_SAMPLES )
		{
			int difference = oldReadIndex - AKI_DELAY_MAX_GLIDE_SAMPLES;
			newReadIndex = ( difference >= 0 ) ? difference : m_DelayBufferSize + difference;
			samplesToGlide = AKI_DELAY_MAX_GLIDE_SAMPLES;
		}
	}
	else
	{
		m_GlideDirection = true;
	}
	samplesToGlide += ABUFFER_SIZE; // also need to incorporate the old or new read block

	// TODO for some reason there's some funny business with MakeSharedDataNull, so we do this... investigate this later
	SharedData<uint8_t> readData = SharedData<uint8_t>::MakeSharedData( 1 );
	// scope to destroy glide data
	{
		// read data from the storage device from the read index to the new read index or vice versa depending on glide direction
		SharedData<uint8_t> glideData = SharedData<uint8_t>::MakeSharedData( 1 ); // TODO see above note
		if ( (m_GlideDirection && (oldReadIndex + samplesToGlide) <= m_DelayBufferSize)
				|| (! m_GlideDirection && (newReadIndex + samplesToGlide) <= m_DelayBufferSize) )
		{
			// in this case we don't wrap around the storage buffer
			SharedData<uint8_t> tempData = SharedData<uint8_t>::MakeSharedData( 1 ); // TODO see above note
			if ( m_GlideDirection ) // gliding forwards towards write index
			{
				tempData = m_StorageMedia->readFromMedia( samplesToGlide * sizeof(uint16_t), oldReadIndex * sizeof(uint16_t) );
			}
			else // gliding backwards away from write index
			{
				tempData = m_StorageMedia->readFromMedia( samplesToGlide * sizeof(uint16_t), newReadIndex * sizeof(uint16_t) );
			}

			glideData = tempData;
		}
		else
		{
			glideData = SharedData<uint8_t>::MakeSharedData( samplesToGlide * sizeof(uint16_t) );
			uint16_t* glideDataPtr = reinterpret_cast<uint16_t*>( glideData.getPtr() );

			// in this case we carefully wrap around the storage buffer
			unsigned int endIndex = ( oldReadIndex > newReadIndex ) ? oldReadIndex : newReadIndex;
			unsigned int firstHalfSize = m_DelayBufferSize - endIndex;
			SharedData<uint8_t> tempFirstHalf = m_StorageMedia->readFromMedia( firstHalfSize * sizeof(uint16_t), endIndex * sizeof(uint16_t) );
			uint16_t* tempFirstHalfPtr = reinterpret_cast<uint16_t*>( tempFirstHalf.getPtr() );
			unsigned int secondHalfSize = samplesToGlide - firstHalfSize;
			SharedData<uint8_t> tempSecondHalf = m_StorageMedia->readFromMedia( secondHalfSize * sizeof(uint16_t), 0 );
			uint16_t* tempSecondHalfPtr = reinterpret_cast<uint16_t*>( tempSecondHalf.getPtr() );

			for ( unsigned int sample = 0; sample < firstHalfSize; sample++ )
			{
				glideDataPtr[sample] = tempFirstHalfPtr[sample];
			}
			for ( unsigned int sample = firstHalfSize; sample < samplesToGlide; sample++ )
			{
				glideDataPtr[sample] = tempSecondHalfPtr[sample - firstHalfSize];
			}
		}

		// linearly interpolate between the glide samples into the read samples
		uint16_t* glideDataPtr = reinterpret_cast<uint16_t*>( glideData.getPtr() );
		float glideSampleIncr = static_cast<float>( samplesToGlide ) * ( 1.0f / ABUFFER_SIZE );
		float currentGlideSampleNum = 0.0f;
		if ( ! m_GlideDirection ) // gliding backwards away from write index
		{
			glideSampleIncr = glideSampleIncr * -1.0f;
			currentGlideSampleNum = static_cast<float>( samplesToGlide + glideSampleIncr );
		}
		readData = SharedData<uint8_t>::MakeSharedData( ABUFFER_SIZE * sizeof(uint16_t) );
		uint16_t* readDataPtr = reinterpret_cast<uint16_t*>( readData.getPtr() );
		for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
		{
			readDataPtr[sample] = glideDataPtr[static_cast<unsigned int>(currentGlideSampleNum)];
			currentGlideSampleNum += glideSampleIncr;
		}
	}

	// increment read index
	if ( m_GlideDirection ) // gliding forwards towards write head
	{
		m_ReadIndex = ( newReadIndex + ABUFFER_SIZE ) % m_DelayBufferSize;
	}
	else // gliding backwards away from write head
	{
		m_ReadIndex = ( newReadIndex - ABUFFER_SIZE > 0 ) ? newReadIndex - ABUFFER_SIZE : m_DelayBufferSize + ( newReadIndex - ABUFFER_SIZE );
	}

	// scope to destroy write data
	{
		// write the data currently in the write buffer (which was read from ADC) to the storage device at the write index
		SharedData<uint8_t> writeData = SharedData<uint8_t>::MakeSharedData( ABUFFER_SIZE * sizeof(uint16_t) );
		uint16_t* writeDataPtr = reinterpret_cast<uint16_t*>( writeData.getPtr() );
		uint16_t* readDataPtr = reinterpret_cast<uint16_t*>( readData.getPtr() );

		for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
		{
			const float signedWrite = static_cast<float>( writeBuffer[sample] ) - 32768.0f;
			const float signedRead = static_cast<float>( readDataPtr[sample] ) - 32768.0f;

			const float outSample = ( signedWrite + (signedRead * feedback) ) * 0.5f;
			const float filteredSample = m_Filt.processSample( outSample );
			const float unclippedUnsignedSample = filteredSample + 32768.0f;
			const float clippedUnsignedSample = std::clamp( unclippedUnsignedSample, 0.0f, 65535.0f );

			writeDataPtr[sample] = static_cast<uint16_t>( clippedUnsignedSample );
		}

		// we don't need to worry about the wrapping issue since writing is always done in block sizes that fit nicely into the storage buffer
		m_StorageMedia->writeToMedia( writeData, m_WriteIndex * sizeof(uint16_t) );
		m_WriteIndex = ( m_WriteIndex + ABUFFER_SIZE ) % m_DelayBufferSize;
	}

	// write read data to write buffer
	uint16_t* readDataPtr = reinterpret_cast<uint16_t*>( readData.getPtr() );
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		writeBuffer[sample] = m_SoftClipper.processSample( readDataPtr[sample] );
	}
}

void AkiDelayManager::onAkiDelayParameterEvent (const AkiDelayParameterEvent& paramEvent)
{
	unsigned int channel = paramEvent.getChannel();
	PARAM_CHANNEL channelEnum = static_cast<PARAM_CHANNEL>( channel );
	float valueToSet = paramEvent.getValue();

	if ( channelEnum == PARAM_CHANNEL::DELAY_TIME )
	{
		this->setDelayTime( valueToSet );
	}
	else if ( channelEnum == PARAM_CHANNEL::FEEDBACK )
	{
		this->setFeedback( valueToSet );
	}
	else if ( channelEnum == PARAM_CHANNEL::FILT_FREQ )
	{
		this->setFiltFreq( valueToSet );
	}
	else if ( channelEnum == PARAM_CHANNEL::NEXT_PRESET )
	{
		if ( m_PresetManager )
		{
			AkiDelayState preset = m_PresetManager->nextPreset<AkiDelayState>();
			this->setState( preset );
			IAkiDelayPresetEventListener::PublishEvent(
						AkiDelayPresetEvent(this->getState(), m_PresetManager->getCurrentPresetNum(), 0) );
		}
	}
	else if ( channelEnum == PARAM_CHANNEL::PREV_PRESET )
	{
		if ( m_PresetManager )
		{
			AkiDelayState preset = m_PresetManager->prevPreset<AkiDelayState>();
			this->setState( preset );
			IAkiDelayPresetEventListener::PublishEvent(
						AkiDelayPresetEvent(this->getState(), m_PresetManager->getCurrentPresetNum(), 0) );
		}
	}
	else if ( channelEnum == PARAM_CHANNEL::WRITE_PRESET )
	{
		if ( m_PresetManager )
		{
			AkiDelayState presetToWrite = this->getState();
			m_PresetManager->writePreset<AkiDelayState>( presetToWrite, m_PresetManager->getCurrentPresetNum() );
			IAkiDelayPresetEventListener::PublishEvent(
						AkiDelayPresetEvent(this->getState(), m_PresetManager->getCurrentPresetNum(), 0) );
		}
	}
	else if ( channelEnum == PARAM_CHANNEL::SEND_PRESET && m_DevId == 0 && m_SenderId == 0 ) // ensure no preset exchange is taking place
	{
		// send this preset
		m_SendingOrReceivingAllPresets = false;
		m_NibbleIndex = 0;
		m_DevId = this->generateRandomDevId(); // use a random id for the sender
		m_SenderId = 0;
		const uint16_t numNibblesInPreset = this->getNumNibblesInPreset();
		SalSysexEvent sendPresetEvent
			= SalSysexEvent::buildRequestToSendPresetEvent( m_DevId, AKI_DELAY_MODEL_ID, m_SenderId, m_PresetManager->getCurrentPresetNum(), numNibblesInPreset );
		m_MidiHandler->processSalSysexEvent( sendPresetEvent );
	}
	else if ( channelEnum == PARAM_CHANNEL::SEND_ALL_PRESETS && m_DevId == 0 && m_SenderId == 0 ) // ensure no preset exchange is taking place
	{
		// send all presets
		m_SendingOrReceivingAllPresets = true;
		m_NibbleIndex = 0;
		m_DevId = this->generateRandomDevId(); // use a random id for the sender
		m_SenderId = 0;
		const uint16_t numNibblesInPreset = this->getNumNibblesInPreset();
		SalSysexEvent sendAllPresetsEvent
			= SalSysexEvent::buildRequestToSendAllPresetsEvent( m_DevId, AKI_DELAY_MODEL_ID, m_SenderId, 0, numNibblesInPreset );
		m_MidiHandler->processSalSysexEvent( sendAllPresetsEvent );
	}
	else if ( channelEnum == PARAM_CHANNEL::ACCEPT_PRESET )
	{
		// send accepted message
		const uint16_t numNibblesInPreset = this->getNumNibblesInPreset();
		SalSysexEvent acceptPresetOrPresetsEvent
			= SalSysexEvent::buildAcceptPresetOrPresetsEvent( m_DevId, AKI_DELAY_MODEL_ID, m_SenderId, m_RequestedPresetNum, numNibblesInPreset );
		m_MidiHandler->processSalSysexEvent( acceptPresetOrPresetsEvent );

		// go to receiving page
		IAkiDelayPresetEventListener::PublishEvent(
					AkiDelayPresetEvent(this->getState(), m_RequestedPresetNum, 0, AkiDelayPresetEventTypeEnum::ACCEPT_PRESET) );
	}
	else if ( channelEnum == PARAM_CHANNEL::DENY_PRESET )
	{
		// send denied message
		const uint16_t numNibblesInPreset = this->getNumNibblesInPreset();
		SalSysexEvent denyPresetOrPresetsEvent
			= SalSysexEvent::buildDenyPresetOrPresetsEvent( m_DevId, AKI_DELAY_MODEL_ID, m_SenderId, m_RequestedPresetNum, numNibblesInPreset );
		m_MidiHandler->processSalSysexEvent( denyPresetOrPresetsEvent );

		// restore dev id and return to main menu
		m_DevId = 0;
		m_SenderId = 0;
		IAkiDelayPresetEventListener::PublishEvent(
					AkiDelayPresetEvent(this->getState(), m_PresetManager->getCurrentPresetNum(), 0, AkiDelayPresetEventTypeEnum::DENY_PRESET) );
	}
}

void AkiDelayManager::onSalSysexEvent (const SalSysexEvent& salSysexEvent)
{
	if ( m_DevId == 0 && m_SenderId == 0 ) // if no preset exchange is currently taking place
	{
		if ( salSysexEvent.getType() == SalSysexTypeEnum::REQUEST_TO_SEND_PRESET )
		{
			// send message to ui to give option to accept or deny
			m_SenderId = salSysexEvent.getDevId();
			m_DevId = ( m_SenderId + 1 ) % 0x7F;
			m_RequestedPresetNum = salSysexEvent.getPresetNum();
			m_SendingOrReceivingAllPresets = false;
			m_NibbleIndex = 0;
			IAkiDelayPresetEventListener::PublishEvent(
						AkiDelayPresetEvent(this->getState(), salSysexEvent.getPresetNum(), 0, AkiDelayPresetEventTypeEnum::SEND_PRESET_REQUEST) );
		}
		else if ( salSysexEvent.getType() == SalSysexTypeEnum::REQUEST_TO_SEND_ALL_PRESETS )
		{
			// send message to ui to give option to accept or deny
			m_SenderId = salSysexEvent.getDevId();
			m_DevId = ( m_SenderId + 1 ) % 0x7F;
			m_RequestedPresetNum = salSysexEvent.getPresetNum();
			m_SendingOrReceivingAllPresets = true;
			m_NibbleIndex = 0;
			IAkiDelayPresetEventListener::PublishEvent(
						AkiDelayPresetEvent(this->getState(), salSysexEvent.getPresetNum(), 0, AkiDelayPresetEventTypeEnum::SEND_ALL_PRESETS_REQUEST) );
		}
	}
	else if ( m_DevId == salSysexEvent.getRecId() && (m_SenderId == 0 || m_SenderId == salSysexEvent.getDevId()) ) // if preset exchange is in progress and ids match
	{
		if ( salSysexEvent.getType() == SalSysexTypeEnum::ACCEPT_PRESET_OR_PRESETS )
		{
			// send the requested preset
			// note that since sal has a limited midi message size, multiple preset chunks are usually necessary for a single preset
			if ( m_SenderId == 0 )
			{
				m_SenderId = salSysexEvent.getDevId();
			}
			const uint16_t numNibblesInPreset = this->getNumNibblesInPreset();
			const uint8_t requestedPresetNum = salSysexEvent.getPresetNum();
			if ( m_SendingOrReceivingAllPresets )
			{
				m_PresetToSendOrReceive = m_PresetManager->retrievePreset<AkiDelayState>( requestedPresetNum );
			}
			else
			{
				m_PresetToSendOrReceive = this->getState();
			}
			SalSysexEvent sendPresetDataChunkEvent
				= SalSysexEvent::buildSendPresetDataChunkEvent( m_DevId, AKI_DELAY_MODEL_ID, m_SenderId, requestedPresetNum, numNibblesInPreset );

			// build the chunk
			while ( m_NibbleIndex < numNibblesInPreset )
			{
				uint8_t nibble = reinterpret_cast<uint8_t*>( &m_PresetToSendOrReceive )[ m_NibbleIndex / 2 ];
				if ( (m_NibbleIndex & 0b1) == 0 )
				{
					// this is the high nibble of the byte
					nibble = nibble >> 4;
				}
				else
				{
					// this is the low nibble of the byte
					nibble = nibble & 0b1111;
				}

				if ( ! sendPresetDataChunkEvent.writeNibble(nibble) )
				{
					// unsuccessful write due to midi message being full
					break;
				}
				else
				{
					// successful write
					m_NibbleIndex++;
				}
			}

			m_MidiHandler->processSalSysexEvent( sendPresetDataChunkEvent );
		}
		else if ( salSysexEvent.getType() == SalSysexTypeEnum::DENY_PRESET_OR_PRESETS )
		{
			// reset and return to main page
			m_DevId = 0;
			m_SenderId = 0;
			m_SendingOrReceivingAllPresets = false;
			m_NibbleIndex = 0;
			IAkiDelayPresetEventListener::PublishEvent(
						AkiDelayPresetEvent(this->getState(), salSysexEvent.getPresetNum(), 0, AkiDelayPresetEventTypeEnum::DENY_PRESET) );
		}
		else if ( salSysexEvent.getType() == SalSysexTypeEnum::SEND_PRESET_DATA_CHUNK )
		{
			const uint16_t numNibblesInPreset = this->getNumNibblesInPreset();
			const uint8_t requestedPresetNum = salSysexEvent.getPresetNum();
			const uint8_t* presetChunkNibbles = salSysexEvent.getPresetChunkNibbles();
			uint8_t presetChunkNibblesIndex = 0;
			uint8_t maxNibblesInMessage = salSysexEvent.getMaxNumNibblesInPresetChunkNibbles();
			uint8_t* presetToSendOrReceivePtr = reinterpret_cast<uint8_t*>( &m_PresetToSendOrReceive );

			// build the preset from the chunk
			while ( m_NibbleIndex < numNibblesInPreset && presetChunkNibblesIndex < maxNibblesInMessage )
			{
				const uint8_t nibble = presetChunkNibbles[presetChunkNibblesIndex];
				const unsigned int byteIndex = m_NibbleIndex / 2;

				if ( (m_NibbleIndex & 0b1) == 0 )
				{
					// this is the high nibble of the byte
					presetToSendOrReceivePtr[byteIndex] = ( nibble << 4 );
				}
				else
				{
					// this is the low nibble of the byte
					presetToSendOrReceivePtr[byteIndex] |= nibble;
				}

				m_NibbleIndex++;
				presetChunkNibblesIndex++;
			}

			if ( m_NibbleIndex == numNibblesInPreset )
			{
				// we have the full preset, send the received preset message
				m_NibbleIndex = 0; // reset since next preset we need to start at the first nibble
				SalSysexEvent receivedPresetEvent
					= SalSysexEvent::buildReceivedPresetEvent( m_DevId, AKI_DELAY_MODEL_ID, m_SenderId, requestedPresetNum, numNibblesInPreset );

				// save the preset
				if ( m_SendingOrReceivingAllPresets && requestedPresetNum != m_PresetManager->getMaxNumPresets() - 1)
				{
					m_PresetManager->writePreset<AkiDelayState>( m_PresetToSendOrReceive, requestedPresetNum );
				}
				else // receiving only one preset, or finished receiving all presets
				{
					const uint8_t presetNumToSaveTo = ( m_SendingOrReceivingAllPresets ) ? requestedPresetNum : m_PresetManager->getCurrentPresetNum();
					m_PresetManager->writePreset<AkiDelayState>( m_PresetToSendOrReceive, presetNumToSaveTo );
					this->setState( m_PresetToSendOrReceive );

					// return to main menu
					IAkiDelayPresetEventListener::PublishEvent(
						AkiDelayPresetEvent(this->getState(), salSysexEvent.getPresetNum(), 0, AkiDelayPresetEventTypeEnum::FINISHED_SENDING_OR_RECEIVING_PRESETS) );

					m_DevId = 0;
					m_SenderId = 0;
				}

				m_MidiHandler->processSalSysexEvent( receivedPresetEvent );
			}
			else // we don't have the full preset yet, request another chunk
			{
				SalSysexEvent acceptPresetOrPresetsEvent
					= SalSysexEvent::buildAcceptPresetOrPresetsEvent( m_DevId, AKI_DELAY_MODEL_ID, m_SenderId, requestedPresetNum, numNibblesInPreset );

				m_MidiHandler->processSalSysexEvent( acceptPresetOrPresetsEvent );
			}
		}
		else if ( salSysexEvent.getType() == SalSysexTypeEnum::RECEIVED_PRESET )
		{
			// send the next requested preset
			// note that since sal has a limited midi message size, multiple preset chunks are usually necessary for a single preset
			const uint16_t numNibblesInPreset = this->getNumNibblesInPreset();
			const uint8_t requestedPresetNum = ( m_SendingOrReceivingAllPresets ) ? salSysexEvent.getPresetNum() + 1 : m_PresetManager->getMaxNumPresets();

			m_NibbleIndex = 0; // reset since next preset we need to start at the first nibble

			if ( requestedPresetNum == m_PresetManager->getMaxNumPresets() )
			{
				m_DevId = 0;
				m_SenderId = 0;
				m_SendingOrReceivingAllPresets = false;

				// return to main menu
				IAkiDelayPresetEventListener::PublishEvent(
					AkiDelayPresetEvent(this->getState(), salSysexEvent.getPresetNum(), 0, AkiDelayPresetEventTypeEnum::FINISHED_SENDING_OR_RECEIVING_PRESETS) );
			}
			else
			{
				if ( m_SendingOrReceivingAllPresets )
				{
					m_PresetToSendOrReceive = m_PresetManager->retrievePreset<AkiDelayState>( requestedPresetNum );
				}
				else
				{
					m_PresetToSendOrReceive = this->getState();
				}
				SalSysexEvent sendPresetDataChunkEvent
					= SalSysexEvent::buildSendPresetDataChunkEvent( m_DevId, AKI_DELAY_MODEL_ID, m_SenderId, requestedPresetNum, numNibblesInPreset );

				// build the chunk
				while ( m_NibbleIndex < numNibblesInPreset )
				{
					uint8_t nibble = reinterpret_cast<uint8_t*>( &m_PresetToSendOrReceive )[ m_NibbleIndex / 2 ];
					if ( (m_NibbleIndex & 0b1) == 0 )
					{
						// this is the high nibble of the byte
						nibble = nibble >> 4;
					}
					else
					{
						// this is the low nibble of the byte
						nibble = nibble & 0b1111;
					}

					if ( ! sendPresetDataChunkEvent.writeNibble(nibble) )
					{
						// unsuccessful write due to midi message being full
						break;
					}
					else
					{
						// successful write
						m_NibbleIndex++;
					}
				}

				m_MidiHandler->processSalSysexEvent( sendPresetDataChunkEvent );
			}
		}
	}
}

uint8_t AkiDelayManager::generateRandomDevId()
{
	// generate a random device id
	std::random_device rd;
	std::mt19937 gen( rd() );
	std::uniform_int_distribution<int> distrib( 0x01, 0x7E ); // 0x7F since it must be a data byte instead of a status byte, ranges so that dev id and sender id are never zero

	return distrib( gen );
}

uint16_t AkiDelayManager::getNumNibblesInPreset()
{
	return sizeof( AkiDelayState ) * 2; // * 2 since we're handling nibbles not bytes
}
