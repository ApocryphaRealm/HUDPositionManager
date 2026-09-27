#include "Elements.h"

namespace hud
{
	const std::vector<Element>& Elements()
	{
		// Paths are relative to _root.HUDMovieBaseInstance. Where the vanilla movie and common HUD
		// replacers name an element differently, every known name is listed: the ones the running
		// HUD has are used, the rest are reported missing and skipped.
		static const std::vector<Element> kElements = {
			{ "Health", "Health", { "Health" } },
			{ "Magicka", "Magicka", { "Magica" } },
			{ "Stamina", "Stamina", { "Stamina" } },
			{ "LeftCharge", "Left charge meter", { "BottomLeftLockInstance.LeftHandChargeMeterInstance" } },
			{ "RightCharge", "Right charge meter", { "BottomRightLockInstance.RightHandChargeMeterInstance" } },
			{ "Compass", "Compass", { "CompassShoutMeterHolder" } },
			{ "Crosshair", "Crosshair", { "Crosshair" } },
			{ "EnemyHealth", "Enemy health", { "EnemyHealth_mc" } },
			{ "StealthMeter", "Stealth meter", { "StealthMeterInstance" } },
			{ "Subtitles", "Subtitles", { "SubtitleTextHolder" } },
			{ "ArrowInfo", "Ammo count", { "ArrowInfoInstance" } },
			{ "Messages", "Notifications", { "MessagesBlock" } },
			{ "QuestUpdate", "Quest updates", { "QuestUpdateBaseInstance" } },
			{ "ActivatePrompt", "Activate prompt", { "RolloverText", "RolloverInfoText", "RolloverButton_tf", "RolloverGrayBar_mc", "ActivateButton_tf" } },
			{ "LocationText", "Location name", { "LocationLockBase" } },
		};
		return kElements;
	}

	int IndexOf(const std::string& a_key)
	{
		const auto& els = Elements();
		for (std::size_t i = 0; i < els.size(); ++i) {
			if (a_key == els[i].key) {
				return static_cast<int>(i);
			}
		}
		return -1;
	}
}
