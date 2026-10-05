#include "Elements.h"

#include "utils/Logger.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <deque>
#include <format>
#include <fstream>
#include <set>

namespace hud
{
	namespace
	{
	const std::vector<Element>& Fixed()
	{
		// HUD paths are relative to _root.HUDMovieBaseInstance. Where the vanilla movie and HUD replacers
		// name an element differently, every known name is listed: the ones the running HUD has are used,
		// the rest are reported missing and skipped. Names mapped from the running Norden UI HUD, 2026-09-27.
		static const std::vector<Element> kElements = {
			// The three bars move as one block by default (Magicka and Stamina with Health): a UI overhaul groups
			// widgets around all three (Norden UI's level, gold, weight, resistances), so moving the block keeps
			// them together. Each can still be moved on its own, or set to move with nothing.
			{ "Health", "Health", { "Health" }, nullptr, nullptr, true, true, true },
			{ "Magicka", "Magicka", { "Magica" }, nullptr, "Health", true, true, true },
			{ "Stamina", "Stamina", { "Stamina" }, nullptr, "Health", true, true, true },
			{ "LeftCharge", "Left charge meter", { "BottomLeftLockInstance.LeftHandChargeMeterInstance" }, nullptr, nullptr, true },
			{ "RightCharge", "Right charge meter", { "BottomRightLockInstance.RightHandChargeMeterInstance" }, nullptr, nullptr, true },
			{ "CombinedCharge", "Combined charge meters", { "ChargeMeters" }, nullptr, nullptr, true },
			// The compass and the shout meter share one holder in the vanilla HUD; each is its own element so
			// either can move alone - and so a HUD that separates the shout meter (Dragonborn UI) is covered
			// by the same two tabs (the owner, 2026-09-27).
			{ "Compass", "Compass", { "CompassShoutMeterHolder.Compass" }, nullptr, nullptr, true },
			{ "ShoutMeter", "Shout meter", { "CompassShoutMeterHolder.ShoutMeterInstance", "CompassShoutMeterHolder.ShoutWarningInstance",
											 "CompassShoutMeterHolder.ShoutWarningInstanceAlt", "CompassShoutMeterHolder.ShoutMeterBarAlt",
											 "ShoutMeterInstance", "ShoutMeter_mc" } },
			{ "Crosshair", "Crosshair", { "Crosshair" } },
			{ "EnemyHealth", "Enemy health", { "EnemyHealth_mc" }, nullptr, nullptr, true },
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
			// 1.1 (2026-10-04): clips ImmersiveHUD SKSE reaches that 1.0 did not - the quest marker floating over the target,
			// and the survival mode temperature meter
			{ "QuestMarker", "Floating quest marker", { "FloatingQuestMarkerInstance" } },
			{ "Temperature", "Temperature meter", { "TemperatureMeter_mc" }, nullptr, nullptr, true },
			// 1.1 phase 2 (2026-10-04): widgets this mod builds, its own reskinnable SWFs (widgets.h) - vanilla Skyrim has none
			{ "Breath", "Breath meter", { "HPM_Breath" }, nullptr, nullptr, true, false, false, "HUDPositionManager/widgets/breath.swf" },
			{ "CastingBar", "Casting bar", { "HPM_CastingBar" }, nullptr, nullptr, true, false, false, "HUDPositionManager/widgets/casting.swf" },
			{ "BowDraw", "Bow draw", { "HPM_BowDraw" }, nullptr, nullptr, true, false, false, "HUDPositionManager/widgets/bowdraw.swf" },
			{ "ShoutCharge", "Shout charge", { "HPM_ShoutCharge" }, nullptr, nullptr, true, false, false, "HUDPositionManager/widgets/shoutcharge.swf" },
			{ "Detection", "Detection meter", { "HPM_Detection" }, nullptr, nullptr, true, false, false, "HUDPositionManager/widgets/detection.swf" },
			{ "PlayerHealth", "Health bar (HPM)", { "HPM_PlayerHealth" }, nullptr, nullptr, true, false, true, "HUDPositionManager/widgets/playerhealth.swf" },
			{ "PlayerMagicka", "Magicka bar (HPM)", { "HPM_PlayerMagicka" }, nullptr, nullptr, true, false, true, "HUDPositionManager/widgets/playermagicka.swf" },
			{ "PlayerStamina", "Stamina bar (HPM)", { "HPM_PlayerStamina" }, nullptr, nullptr, true, false, true, "HUDPositionManager/widgets/playerstamina.swf" },
			{ "RecentLoot", "Recent loot", { "HPM_RecentLoot" }, nullptr, nullptr, false, false, false, "HUDPositionManager/widgets/loot.swf" },
			{ "BossBars", "Boss bars", { "HPM_BossBars" }, nullptr, nullptr, true, false, true, "HUDPositionManager/widgets/bossbar.swf" },
			{ "InfoGold", "Gold", { "HPM_InfoGold" }, nullptr, nullptr, false, false, false, "HUDPositionManager/widgets/gold.swf" },
			{ "InfoWeight", "Carry weight", { "HPM_InfoWeight" }, nullptr, nullptr, false, false, false, "HUDPositionManager/widgets/weight.swf" },
			{ "InfoLevel", "Level", { "HPM_InfoLevel" }, nullptr, nullptr, true, false, false, "HUDPositionManager/widgets/level.swf",
			  "HUDPositionManager/widgets/level_badge.swf" },
			{ "InfoResist", "Resistances", { "HPM_InfoResist" }, nullptr, "Health", false, false, false, "HUDPositionManager/widgets/resist.swf" },
			{ "InfoEquip", "Equipped items", { "HPM_InfoEquip" }, nullptr, nullptr, false, false, false, "HUDPositionManager/widgets/equip.swf" },
			{ "InfoPlayTime", "Play time", { "HPM_InfoPlayTime" }, nullptr, nullptr, false, false, false, "HUDPositionManager/widgets/playtime.swf" },
			{ "InfoEffects", "Active effects", { "HPM_InfoEffects" }, nullptr, nullptr, false, false, false, "HUDPositionManager/widgets/effects.swf" },
			{ "SurvHunger", "Hunger", { "HPM_SurvHunger" }, nullptr, nullptr, true, false, false, "HUDPositionManager/widgets/hunger.swf" },
			{ "SurvFatigue", "Fatigue", { "HPM_SurvFatigue" }, nullptr, nullptr, true, false, false, "HUDPositionManager/widgets/fatigue.swf" },
			{ "SurvCold", "Cold", { "HPM_SurvCold" }, nullptr, nullptr, true, false, false, "HUDPositionManager/widgets/cold.swf" },
			{ "InfoTime", "Game time", { "HPM_InfoTime" }, nullptr, nullptr, false, false, false, "HUDPositionManager/widgets/time.swf" },
			{ "ShoutCooldown", "Shout cooldown", { "HPM_ShoutCooldown" }, nullptr, nullptr, true, false, false, "HUDPositionManager/widgets/shout.swf" },

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

	constexpr std::size_t kMaxDiscovered = 32;
	constexpr int         kPruneDays = 30;   // a cached widget not seen for this long is not loaded (its mod is gone)

	// the discovered elements' strings: a deque never moves what it holds, so the const char* in each Element stay valid
	std::deque<std::string> g_strings;
	const char* Keep(std::string a_s)
	{
		g_strings.push_back(std::move(a_s));
		return g_strings.back().c_str();
	}

	std::string Trim(std::string a_s)
	{
		while (!a_s.empty() && (a_s.back() == ' ' || a_s.back() == '\t' || a_s.back() == '\r')) { a_s.pop_back(); }
		std::size_t i = 0;
		while (i < a_s.size() && (a_s[i] == ' ' || a_s[i] == '\t')) { ++i; }
		return a_s.substr(i);
	}

	// days since the epoch for a "YYYY-MM-DD" (0 if unreadable)
	int Days(const std::string& a_date)
	{
		int y = 0, m = 0, d = 0;
		if (std::sscanf(a_date.c_str(), "%d-%d-%d", &y, &m, &d) != 3) { return 0; }
		const auto ymd = std::chrono::year_month_day{ std::chrono::year{ y }, std::chrono::month{ static_cast<unsigned>(m) }, std::chrono::day{ static_cast<unsigned>(d) } };
		return ymd.ok() ? static_cast<int>(std::chrono::sys_days{ ymd }.time_since_epoch().count()) : 0;
	}

	// the cache's widgets as elements: read once, at the first Elements() call (plugin load)
	void AppendDiscovered(std::vector<Element>& a_els)
	{
		std::ifstream in(DiscoveredPath());
		if (!in) { return; }
		struct Entry { std::string section, kind, where, source, name, seen; bool forgotten = false; };
		std::vector<Entry> entries;
		std::string line;
		while (std::getline(in, line)) {
			line = Trim(line);
			if (line.empty() || line[0] == ';') { continue; }
			if (line.front() == '[' && line.back() == ']') {
				const std::string sec = line.substr(1, line.size() - 2);
				if (sec != "General") { entries.push_back({ sec }); }
				continue;
			}
			if (entries.empty()) { continue; }
			const auto eq = line.find('=');
			if (eq == std::string::npos) { continue; }
			const std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
			auto& e = entries.back();
			if (k == "sKind") { e.kind = v; } else if (k == "sWhere") { e.where = v; } else if (k == "sSource") { e.source = v; }
			else if (k == "sName") { e.name = v; } else if (k == "sLastSeen") { e.seen = v; }
			else if (k == "bForgotten") { e.forgotten = v == "1"; }
		}
		const int today = static_cast<int>(std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()).time_since_epoch().count());
		// what the fixed table already moves is never taken twice
		std::set<std::string, std::less<>> fixedMenus, fixedClips, keys;
		for (const auto& el : a_els) {
			keys.insert(el.key);
			if (el.menu) { fixedMenus.insert(el.menu); }
			for (const char* p : el.parts) { fixedClips.insert(p); }
		}
		std::size_t added = 0, pruned = 0;
		for (const auto& e : entries) {
			if (added >= kMaxDiscovered) { break; }
			if (e.forgotten) { continue; }   // the player forgot it (D3)
			if (const int seen = Days(e.seen); seen > 0 && today - seen > kPruneDays) { ++pruned; continue; }
			Element el{};
			std::string key = "W_";
			for (const char c : e.section) { key += std::isalnum(static_cast<unsigned char>(c)) ? c : '_'; }
			if (keys.contains(key)) { continue; }
			if (e.kind == "menu" && !e.where.empty() && !fixedMenus.contains(e.where)) {
				el.found = 3;
				el.menu = Keep(e.where);
				el.parts = { "" };
			} else if (e.kind == "hud" && !e.where.empty() && !fixedClips.contains(e.where)) {
				el.found = 1;
				el.parts = { Keep(e.where) };
			} else if (e.kind == "skyui" && !e.source.empty()) {
				el.found = 2;
				el.parts = { Keep("@" + e.source) };   // its slot of _root.WidgetContainer, found by this SWF at each resolve
			} else {
				continue;
			}
			keys.insert(key);
			el.key = Keep(key);
			el.name = Keep(e.name.empty() ? e.section : e.name);
			el.source = Keep(e.source);
			a_els.push_back(std::move(el));
			++added;
		}
		logger::info("discovery: {} widget(s) from the cache get their own tabs{}", added, pruned ? std::format(" ({} not seen for {} days left out)", pruned, kPruneDays) : "");
	}

	std::size_t g_fixed = 0;
	}

	const std::vector<Element>& Elements()
	{
		static const std::vector<Element> all = [] {
			std::vector<Element> v = Fixed();
			g_fixed = v.size();
			AppendDiscovered(v);
			return v;
		}();
		return all;
	}

	std::size_t FixedCount()
	{
		(void)Elements();
		return g_fixed;
	}

	const char* DiscoveredPath() { return "Data/SKSE/Plugins/HUDPositionManager/discovered.ini"; }

	// a found widget's cache section: its kind and the name it is known by - the menu, the clip, or the SWF's file name
	std::string CacheSection(const std::string& a_kind, const std::string& a_where, const std::string& a_source)
	{
		std::string id = a_where;
		if (a_kind == "skyui") {
			id = a_source.substr(a_source.rfind('/') + 1);
			if (id.size() > 4 && id.ends_with(".swf")) { id.resize(id.size() - 4); }
		} else if (a_kind == "hud") {
			id = a_where.substr(a_where.rfind('.') + 1);
		}
		return a_kind + "." + id;
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
