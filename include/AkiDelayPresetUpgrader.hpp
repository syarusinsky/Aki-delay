#ifndef AKIDELAYPRESETUPGRADER_HPP
#define AKIDELAYPRESETUPGRADER_HPP

/*************************************************************************
 * The AkiDelayPresetUpgrader is an IPresetUpgrader that is meant to be
 * passed to the PresetManager's upgradePresets function. It holds a
 * record of each version of the AkiDelayState preset struct in its
 * cpp file for comparisons when upgrading.
*************************************************************************/

#include "PresetManager.hpp"

#include "AkiDelayManager.hpp"

class AkiDelayPresetUpgrader : public IPresetUpgrader
{
	public:
		AkiDelayPresetUpgrader(const AkiDelayState& initPreset, const AkiDelayPresetHeader& currentPresetHeader);
		~AkiDelayPresetUpgrader() override;

		void upgradePresets() override;

	private:
		AkiDelayState 		m_InitPreset;
		AkiDelayPresetHeader 	m_CurrentPresetHeader;

		// this is where the upgrade functions would go, for example
		// void upgradeFrom1_0_0To1_1_0();
};

#endif // AKIDELAYPRESETUPGRADER_HPP
