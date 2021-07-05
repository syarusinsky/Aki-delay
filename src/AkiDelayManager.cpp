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
	m_GlideDirection( true )
{
}

AkiDelayManager::~AkiDelayManager()
{
}

void AkiDelayManager::setDelayTime (float delayTime)
{
	// if delay time is less, we need to glide forward towards write index
	m_GlideDirection = ( delayTime < m_DelayTime ) ? true : false;
	m_DelayTime = delayTime;
}

void AkiDelayManager::setFeedback (float feedback)
{
	m_Feedback = feedback;
}

void AkiDelayManager::setFiltFreq (float filtFreq)
{
	m_FiltFreq = filtFreq;
}

void AkiDelayManager::call (uint16_t* writeBuffer)
{
	// set correct read index, feedback amount, and filter frequency for this block
	unsigned int delayTimeInSamples = m_DelayTime * SAMPLE_RATE;
	// ensure that the read index at least lags behind the write index by ABUFFER_SIZE samples
	int newReadIndex = m_WriteIndex - ( (delayTimeInSamples < ABUFFER_SIZE) ? ABUFFER_SIZE : delayTimeInSamples );
	newReadIndex = ( newReadIndex < 0 ) ? m_DelayBufferSize + newReadIndex : newReadIndex;
	newReadIndex &= ~(0b1); // ensure it is even, to align the data
	float feedback = m_Feedback;

	// we will likely have to glide between some samples, so calculate how many here
	unsigned int samplesToGlide = 0;
	if ( m_ReadIndex != newReadIndex && m_GlideDirection ) // gliding forwards towards write index
	{
		samplesToGlide = ( newReadIndex > m_ReadIndex ) ? newReadIndex - m_ReadIndex : (m_DelayBufferSize - m_ReadIndex) + newReadIndex;

		if ( samplesToGlide > AKI_DELAY_MAX_GLIDE_SAMPLES )
		{
			newReadIndex = ( m_ReadIndex + AKI_DELAY_MAX_GLIDE_SAMPLES ) % m_DelayBufferSize;
			samplesToGlide = AKI_DELAY_MAX_GLIDE_SAMPLES;
		}
	}
	else if ( m_ReadIndex != newReadIndex ) // gliding backwards away from write index
	{
		samplesToGlide = ( newReadIndex < m_ReadIndex ) ? m_ReadIndex - newReadIndex : (m_DelayBufferSize - newReadIndex) + m_ReadIndex;

		if ( samplesToGlide > AKI_DELAY_MAX_GLIDE_SAMPLES )
		{
			int difference = m_ReadIndex - AKI_DELAY_MAX_GLIDE_SAMPLES;
			newReadIndex = ( difference >= 0 ) ? difference : m_DelayBufferSize + difference;
			samplesToGlide = AKI_DELAY_MAX_GLIDE_SAMPLES;
		}
	}
	samplesToGlide += ABUFFER_SIZE; // also need to incorporate the old or new read block

	// read data from the storage device from the read index to the new read index or vice versa depending on glide direction
	SharedData<uint8_t> glideData = SharedData<uint8_t>::MakeSharedData( samplesToGlide * sizeof(uint16_t) );
	uint16_t* glideDataPtr = reinterpret_cast<uint16_t*>( glideData.getPtr() );
	if ( (m_GlideDirection && (m_ReadIndex + samplesToGlide) <= m_DelayBufferSize)
			|| (! m_GlideDirection && (newReadIndex + samplesToGlide) <= m_DelayBufferSize) )
	{
		// in this case we don't wrap around the storage buffer
		SharedData<uint8_t> tempData = SharedData<uint8_t>::MakeSharedDataNull();
		if ( m_GlideDirection ) // gliding forwards towards write index
		{
			tempData = m_StorageMedia->readFromMedia( samplesToGlide * sizeof(uint16_t), m_ReadIndex * sizeof(uint16_t) );
		}
		else // gliding backwards away from write index
		{
			tempData = m_StorageMedia->readFromMedia( samplesToGlide * sizeof(uint16_t), newReadIndex * sizeof(uint16_t) );
		}

		glideData = tempData;
		// update pointer too
		glideDataPtr = reinterpret_cast<uint16_t*>( glideData.getPtr() );
	}
	else
	{
		// in this case we carefully wrap around the storage buffer
		unsigned int endIndex = 0; // the index at the end of the buffer that needs to wrap
		unsigned int beginIndex = 0; // the index at the beginning of the buffer
		if ( m_ReadIndex > newReadIndex )
		{
			endIndex = m_ReadIndex;
			beginIndex = newReadIndex;
		}
		else
		{
			endIndex = newReadIndex;
			beginIndex = m_ReadIndex;
		}
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
	float glideSampleIncr = static_cast<float>( samplesToGlide ) * ( 1.0f / ABUFFER_SIZE );
	float currentGlideSample = 0.0f;
	SharedData<uint8_t> readData = SharedData<uint8_t>::MakeSharedData( ABUFFER_SIZE * sizeof(uint16_t) );
	uint16_t* readDataPtr = reinterpret_cast<uint16_t*>( readData.getPtr() );
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		readDataPtr[sample] = glideDataPtr[static_cast<unsigned int>(currentGlideSample)];
		currentGlideSample += glideSampleIncr;
	}
	// increment read index
	m_ReadIndex = ( newReadIndex + ABUFFER_SIZE ) % m_DelayBufferSize;

	// write the data currently in the write buffer (which was read from ADC) to the storage device at the write index
	SharedData<uint8_t> writeData = SharedData<uint8_t>::MakeSharedData( ABUFFER_SIZE * sizeof(uint16_t) );
	uint16_t* writeDataPtr = reinterpret_cast<uint16_t*>( writeData.getPtr() );

	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		writeDataPtr[sample] = ( writeBuffer[sample] + (readDataPtr[sample] * feedback) ) * 0.55f;
	}

	// we don't need to worry about the wrapping issue since writing is always done in block sizes that fit nicely into the storage buffer
	m_StorageMedia->writeToMedia( writeData, m_WriteIndex * sizeof(uint16_t) );
	m_WriteIndex = ( m_WriteIndex + ABUFFER_SIZE ) % m_DelayBufferSize;

	// write read data to write buffer
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		writeBuffer[sample] = readDataPtr[sample];
	}
}
