#include "AkiDelayPresetUpgrader.hpp"

struct AkiDelayState_VERSION_1_0_0
{
	float m_DelayTime;
	float m_Feedback;
	float m_FiltFreq;
};

AkiDelayPresetUpgrader::AkiDelayPresetUpgrader (const AkiDelayState& initPreset, const AkiDelayPresetHeader& currentPresetHeader) :
	m_InitPreset( initPreset ),
	m_CurrentPresetHeader( currentPresetHeader )
{
}

AkiDelayPresetUpgrader::~AkiDelayPresetUpgrader()
{
}

void AkiDelayPresetUpgrader::upgradePresets()
{
	// first determine if the presets need to be initialized
	if ( m_PresetManager->needToInitializePresets() )
	{
		m_PresetManager->writeHeader<AkiDelayPresetHeader>( m_CurrentPresetHeader );

		for (unsigned int presetNum = 0; presetNum < m_PresetManager->getMaxNumPresets(); presetNum++)
		{
			m_PresetManager->writePreset<AkiDelayState>( m_InitPreset, presetNum );
		}
	}
	else
	{
		// read the preset header from the storage media and determine the version
		AkiDelayPresetHeader presetHeaderFromFile = m_PresetManager->retrieveHeader<AkiDelayPresetHeader>();

		if ( presetHeaderFromFile.presetsFileInitialized == false ) // if set to reinitialize presets, reinitialize
		{
			m_PresetManager->writeHeader<AkiDelayPresetHeader>( m_CurrentPresetHeader );

			for (unsigned int presetNum = 0; presetNum < m_PresetManager->getMaxNumPresets(); presetNum++)
			{
				m_PresetManager->writePreset<AkiDelayState>( m_InitPreset, presetNum );
			}
		}
		// if the versions don't match, we need to upgrade
		else if (       presetHeaderFromFile.versionMajor != m_CurrentPresetHeader.versionMajor ||
				presetHeaderFromFile.versionMinor != m_CurrentPresetHeader.versionMinor ||
				presetHeaderFromFile.versionPatch != m_CurrentPresetHeader.versionPatch )
		{
			// this is where the upgrading would happen
			// if ( 		presetHeaderFromFile.versionMajor == 1 &&
			// 		presetHeaderFromFile.versionMinor == 0 &&
			// 		presetHeaderFromFile.versionPatch == 0 )
			// {
			// 	this->upgradeFrom1_0_0To1_1_0();
			// 	this->upgradePresets(); // recursive in case it needs to upgrade from more than one version
			// }
		}
	}
}

// this is an example of how to upgrade presets
// void AkiDelayPresetUpgrader::upgradeFrom1_0_0To1_1_0()
// {
// 	// write header
// 	AkiDelayPresetHeader presetHeader = { 1, 1, 0, true };
// 	m_PresetManager->writeHeader<AkiDelayPresetHeader>( presetHeader );
//
// 	// TODO this takes up a lottttt of memory, figure out a good way to manage this memory
// 	// store all old presets in an array, IStorageMedia and use SRAM???
// 	AkiDelayState_VERSION_1_0_0 oldPresets[m_PresetManager->getMaxNumPresets()];
//
// 	for (unsigned int presetNum = 0; presetNum < m_PresetManager->getMaxNumPresets(); presetNum++)
// 	{
// 		oldPresets[presetNum] = m_PresetManager->retrievePreset<AkiDelayState_VERSION_1_0_0>( presetNum );
// 	}
//
// 	// write new presets with default values for monophonic, glide time, glide retrigger, and pitch bend semitones
// 	for (unsigned int presetNum = 0; presetNum < m_PresetManager->getMaxNumPresets(); presetNum++)
// 	{
// 		AkiDelayState_VERSION_1_1_0 newPreset =
// 		{
// 			oldPresets[presetNum].m_DelayTime,
// 			oldPresets[presetNum].m_Feedback,
// 			oldPresets[presetNum].m_FiltFreq,
// 			// some new values,...
// 		};
//
// 		m_PresetManager->writePreset<AkiDelayState_VERSION_1_1_0>( newPreset, presetNum );
// 	}
// }
