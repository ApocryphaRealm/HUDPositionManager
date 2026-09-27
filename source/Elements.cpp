#include "Elements.h"

namespace hud
{
	const std::vector<Element>& Elements()
	{
		// HUD paths are relative to _root.HUDMovieBaseInstance. Where the vanilla movie and HUD replacers
		// name an element differently, every known name is listed: the ones the running HUD has are used,
		// the rest are reported missing and skipped. Names mapped from the running Norden UI HUD, 2026-09-27.
		static const std::vector<Element> kElements = {
			{ "Health", "Health", { "Health" } },
			{ "Magicka", "Magicka", { "Magica" } },
			{ "Stamina", "Stamina", { "Stamina" } },
			{ "LeftCharge", "Left charge meter", { "BottomLeftLockInstance.LeftHandChargeMeterInstance" } },
			{ "RightCharge", "Right charge meter", { "BottomRightLockInstance.RightHandChargeMeterInstance" } },
			{ "CombinedCharge", "Combined charge meters", { "ChargeMeters" } },
			// The compass and the shout meter share one holder in the vanilla HUD; each is its own element so
			// either can move alone - and so a HUD that separates the shout meter (Dragonborn UI) is covered
			// by the same two tabs (the owner, 2026-09-27).
			{ "Compass", "Compass", { "CompassShoutMeterHolder.Compass" } },
			{ "ShoutMeter", "Shout meter", { "CompassShoutMeterHolder.ShoutMeterInstance", "CompassShoutMeterHolder.ShoutWarningInstance",
											 "CompassShoutMeterHolder.ShoutWarningInstanceAlt", "CompassShoutMeterHolder.ShoutMeterBarAlt",
											 "ShoutMeterInstance", "ShoutMeter_mc" } },
			{ "Crosshair", "Crosshair", { "Crosshair" } },
			{ "EnemyHealth", "Enemy health", { "EnemyHealth_mc" } },
			{ "StealthMeter", "Stealth meter", { "StealthMeterInstance" } },
			{ "Subtitles", "Subtitles", { "SubtitleTextHolder" } },
			{ "ArrowInfo", "Ammo count", { "ArrowInfoInstance" } },
			{ "Messages", "Notifications", { "MessagesBlock" } },
			{ "QuestUpdate", "Quest updates", { "QuestUpdateBaseInstance" } },
			{ "ActivatePrompt", "Activate prompt", { "RolloverText", "RolloverInfoText", "RolloverButton_tf", "RolloverGrayBar_mc", "ActivateButton_tf",
													 "RolloverName_mc", "RolloverInfo_mc", "ActivateButton" } },
			{ "LocationText", "Location name", { "LocationLockBase" } },
			{ "LevelUp", "Level-up meter", { "LevelUpInstance" } },
			{ "AnimLetters", "Word wall letters", { "AnimLetterInstance" } },
			{ "Clock", "Clock", { "TimeDisplay" } },

			// Widgets from other mods: each is its own menu, moved by its _root.
			{ "WidgetGold", "Gold widget", { "" }, "goldWidget" },
			{ "WidgetWeight", "Carry weight widget", { "" }, "weightWidget" },
			{ "WidgetLevel", "Level widget", { "" }, "lvlWidget" },
			{ "WidgetResist", "Resistances widget", { "" }, "resistWidget" },
			{ "WidgetEquip", "Equipment widget", { "" }, "equipWidget_STB" },
			{ "WidgetShout", "Shout widget", { "" }, "shoutWidget" },
			{ "WidgetGameTime", "Game time widget", { "" }, "gametimeWidget" },
			{ "WidgetPlayTime", "Play time widget", { "" }, "playtimeWidget" },
			{ "WidgetOxygen", "Oxygen meter", { "" }, "oxygenMeter2" },
			{ "WidgetCasting", "Casting bar", { "" }, "CastingBar" },
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
