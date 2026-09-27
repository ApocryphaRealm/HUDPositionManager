#include "Elements.h"

namespace hud
{
	const std::vector<Element>& Elements()
	{
		// HUD paths are relative to _root.HUDMovieBaseInstance. Where the vanilla movie and HUD replacers
		// name an element differently, every known name is listed: the ones the running HUD has are used,
		// the rest are reported missing and skipped. Names mapped from the running Norden UI HUD, 2026-09-27.
		static const std::vector<Element> kElements = {
			// The three bars move as one block by default (Magicka and Stamina with Health): a UI overhaul groups
			// widgets around all three (Norden UI's level, gold, weight, resistances), so moving the block keeps
			// them together. Each can still be moved on its own, or set to move with nothing.
			{ "Health", "Health", { "Health" } },
			{ "Magicka", "Magicka", { "Magica" }, nullptr, "Health" },
			{ "Stamina", "Stamina", { "Stamina" }, nullptr, "Health" },
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

			// TrueHUD's player bars: Norden UI draws the bars you see with TrueHUD, over the HUD's own meters
			// (measured in game 2026-09-27: moving Health left a second bar behind). Each follows its HUD bar by
			// default, so moving Health moves the whole visible bar. Only the player widget moves - TrueHUD's
			// menu also holds bars anchored to actors in the world, which must stay where they are.
			{ "TrueHUDHealth", "TrueHUD health bar", { "TrueHUD.playerWidget.Health" }, "TrueHUD", "Health" },
			{ "TrueHUDMagicka", "TrueHUD magicka bar", { "TrueHUD.playerWidget.Magicka" }, "TrueHUD", "Magicka" },
			{ "TrueHUDStamina", "TrueHUD stamina bar", { "TrueHUD.playerWidget.Stamina" }, "TrueHUD", "Stamina" },
			// the rest of TrueHUD's player widget - its special bar, enchantment charge and shout indicator - which was
			// left behind as a thin line when the bars moved (2026-09-27)
			{ "TrueHUDOther", "TrueHUD special bars", { "TrueHUD.playerWidget.Special", "TrueHUD.playerWidget.SpecialMask",
														 "TrueHUD.playerWidget.EnchantmentChargeMeter", "TrueHUD.playerWidget.ShoutIndicator" },
			  "TrueHUD", "Health" },

			// Widgets from other mods: each is its own menu, moved by its _root. The ones Norden UI places around
			// the bars (STB Widgets' gold, weight, level, resistances, game time) move with Health by default.
			{ "WidgetGold", "Gold widget", { "" }, "goldWidget", "Health" },
			{ "WidgetWeight", "Carry weight widget", { "" }, "weightWidget", "Health" },
			{ "WidgetLevel", "Level widget", { "" }, "lvlWidget", "Health" },
			{ "WidgetResist", "Resistances widget", { "" }, "resistWidget", "Health" },
			{ "WidgetEquip", "Equipment widget", { "" }, "equipWidget_STB" },
			{ "WidgetShout", "Shout widget", { "" }, "shoutWidget" },
			{ "WidgetGameTime", "Game time widget", { "" }, "gametimeWidget", "Health" },
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
