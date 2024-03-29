#include "AkiDelayManager.hpp"

#include "AkiDelayConstants.hpp"
#include "SRAM_23K256.hpp"

#include <string.h>

AkiDelayManager::AkiDelayManager (IStorageMedia* delayBufferStorage) :
	m_StorageMedia( delayBufferStorage ),
	m_DelayTime( 0.0f ),
	m_Feedback( 0.0f ),
	m_FiltFreq( 20000.0f ),
	m_DelayBufferSize( (Sram_23K256::SRAM_SIZE * 4) / sizeof(uint16_t) ), // size of 4 srams installed on Gen_FX_SYN rev 2
	m_WriteIndex( ABUFFER_SIZE ),
	m_ReadIndex( 0 ),
	m_GlideDirection( true ),
	m_NoiseGate( 0.02f, 100.0f, 100 ),
	m_Filt(),
	m_SoftClipper()
{
	this->bindToAkiDelayParameterEventSystem();
}

AkiDelayManager::~AkiDelayManager()
{
	this->unbindFromAkiDelayParameterEventSystem();
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

void AkiDelayManager::call (uint16_t* writeBuffer)
{
	// first offset for noise gate
	int16_t* writeBufferInt16 = reinterpret_cast<int16_t*>( writeBuffer );
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		writeBuffer[sample] -= 2048;
	}

	m_NoiseGate.call( writeBufferInt16 );

	// offset back
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		writeBuffer[sample] += 2048;
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
			float outSample = ( writeBuffer[sample] + (readDataPtr[sample] * feedback) ) * 0.4f;
			float filteredSample = m_Filt.processSample( outSample );

			writeDataPtr[sample] = static_cast<uint16_t>( filteredSample );
		}

		// we don't need to worry about the wrapping issue since writing is always done in block sizes that fit nicely into the storage buffer
		m_StorageMedia->writeToMedia( writeData, m_WriteIndex * sizeof(uint16_t) );
		m_WriteIndex = ( m_WriteIndex + ABUFFER_SIZE ) % m_DelayBufferSize;
	}

	// write read data to write buffer
	uint16_t* readDataPtr = reinterpret_cast<uint16_t*>( readData.getPtr() );
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		// we also need to offset the 1.5 gain from the soft clipper, otherwise the clipping sounds a bit ugly
		writeBuffer[sample] = m_SoftClipper.processSample( static_cast<uint16_t>(readDataPtr[sample] * 0.816497f) );
	}
}

void AkiDelayManager::onAkiDelayParameterEvent (const AkiDelayParameterEvent& paramEvent)
{
	unsigned int channel = paramEvent.getChannel();
	POT_CHANNEL channelEnum = static_cast<POT_CHANNEL>( channel );
	float valueToSet = paramEvent.getValue();

	if ( channelEnum == POT_CHANNEL::DELAY_TIME )
	{
		this->setDelayTime( valueToSet );
	}
	else if ( channelEnum == POT_CHANNEL::FEEDBACK )
	{
		this->setFeedback( valueToSet );
	}
	else if ( channelEnum == POT_CHANNEL::FILT_FREQ )
	{
		this->setFiltFreq( valueToSet );
	}
}
