#include "Settings.h"

#include "Elements.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <format>
#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace settings
{
	namespace
	{
		std::string g_iniPath;
		std::mutex  g_lock;
		Snapshot    g_snap;
		bool        g_dirty = false;
		std::chrono::steady_clock::time_point g_lastEdit{};

		logger::level ClampLevel(long a_raw)
		{
			return (a_raw >= 0 && a_raw <= static_cast<long>(logger::level::off)) ? static_cast<logger::level>(a_raw)
																				   : logger::level::info;
		}

		std::string Trim(std::string a_s)
		{
			while (!a_s.empty() && (a_s.back() == ' ' || a_s.back() == '\t' || a_s.back() == '\r')) { a_s.pop_back(); }
			std::size_t i = 0;
			while (i < a_s.size() && (a_s[i] == ' ' || a_s[i] == '\t')) { ++i; }
			return a_s.substr(i);
		}

		std::string Lower(std::string a_s)
		{
			std::ranges::transform(a_s, a_s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_s;
		}

		std::string Num(float a_v)
		{
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.2f", a_v);
			std::string s{ buf };
			while (!s.empty() && s.back() == '0') { s.pop_back(); }
			if (!s.empty() && s.back() == '.') { s.pop_back(); }
			return s.empty() || s == "-0" ? "0" : s;
		}

		std::string KeyOf(int a_i)
		{
			const auto& els = hud::Elements();
			return (a_i >= 0 && static_cast<std::size_t>(a_i) < els.size()) ? els[static_cast<std::size_t>(a_i)].key : "";
		}

		// The layout rows (the elements, the group, HUD always visible) - what a preset holds.
		std::vector<std::tuple<std::string, std::string, std::string>> LayoutRows(const Snapshot& a_s)
		{
			std::vector<std::tuple<std::string, std::string, std::string>> rows;
			const auto& els = hud::Elements();
			for (std::size_t i = 0; i < els.size() && i < a_s.elements.size(); ++i) {
				const auto& e = a_s.elements[i];
				rows.emplace_back(els[i].key, "fX", Num(e.x));
				rows.emplace_back(els[i].key, "fY", Num(e.y));
				rows.emplace_back(els[i].key, "fScale", Num(e.scale));
				rows.emplace_back(els[i].key, "fLength", Num(e.stretchX));
				rows.emplace_back(els[i].key, "fHeight", Num(e.stretchY));
				rows.emplace_back(els[i].key, "bHide", e.hide ? "1" : "0");
				rows.emplace_back(els[i].key, "iShow", std::to_string(e.show));
				if (els[i].fades) { rows.emplace_back(els[i].key, "bAlwaysVisible", e.alwaysVisible ? "1" : "0"); }
				if (els[i].swf2) { rows.emplace_back(els[i].key, "iStyle", e.style == 1 ? "1" : "0"); }
				rows.emplace_back(els[i].key, "sMoveWith", KeyOf(e.follow));
			}
			std::string members;
			for (const int m : a_s.group.members) { members += (members.empty() ? "" : ",") + KeyOf(m); }
			rows.emplace_back("Group", "sMembers", members);
			rows.emplace_back("Group", "fX", Num(a_s.group.x));
			rows.emplace_back("Group", "fY", Num(a_s.group.y));
			return rows;
		}

		// Every (section, key, value) this mod owns, from a snapshot.
		std::vector<std::tuple<std::string, std::string, std::string>> Rows(const Snapshot& a_s)
		{
			std::vector<std::tuple<std::string, std::string, std::string>> rows;
			rows.emplace_back("General", "bEnabled", a_s.enabled ? "1" : "0");
			rows.emplace_back("General", "bLinkBars", a_s.linkBars ? "1" : "0");
			rows.emplace_back("General", "bLinkWidgets", a_s.linkWidgets ? "1" : "0");
			rows.emplace_back("General", "bAlwaysVisible", a_s.alwaysVisible ? "1" : "0");
			rows.emplace_back("General", "bUnlocked", a_s.unlocked ? "1" : "0");
			rows.emplace_back("General", "bFade", a_s.fade ? "1" : "0");
			rows.emplace_back("General", "iFadeInSpeed", std::to_string(a_s.fadeIn));
			rows.emplace_back("General", "iFadeOutSpeed", std::to_string(a_s.fadeOut));
			rows.emplace_back("General", "iOpacityMin", std::to_string(a_s.opacityMin));
			rows.emplace_back("General", "iOpacityMax", std::to_string(a_s.opacityMax));
			rows.emplace_back("Immersive", "bEnabled", a_s.imm.enabled ? "1" : "0");
			rows.emplace_back("Immersive", "iToggleKey", std::to_string(a_s.imm.key));
			rows.emplace_back("Immersive", "iToggleButton", std::to_string(a_s.imm.button));
			rows.emplace_back("Immersive", "bHoldMode", a_s.imm.hold ? "1" : "0");
			rows.emplace_back("Immersive", "fDisplaySeconds", std::format("{:.1f}", a_s.imm.seconds));
			rows.emplace_back("Immersive", "bStartVisible", a_s.imm.startVisible ? "1" : "0");
			rows.emplace_back("Immersive", "bHoldBarsWhenShown", a_s.imm.holdBars ? "1" : "0");
			rows.emplace_back("PlayerBars", "bEnabled", a_s.pb.enabled ? "1" : "0");
			rows.emplace_back("PlayerBars", "uHealthMode", std::to_string(a_s.pb.healthMode));
			rows.emplace_back("PlayerBars", "uMagickaMode", std::to_string(a_s.pb.magickaMode));
			rows.emplace_back("PlayerBars", "uStaminaMode", std::to_string(a_s.pb.staminaMode));
			rows.emplace_back("PlayerBars", "bPhantom", a_s.pb.phantom ? "1" : "0");
			rows.emplace_back("PlayerBars", "fPhantomSeconds", std::format("{:.2f}", a_s.pb.phantomSeconds));
			rows.emplace_back("PlayerBars", "bMountStamina", a_s.pb.mountStamina ? "1" : "0");
			rows.emplace_back("PlayerBars", "bSurvivalPenalty", a_s.pb.survivalPenalty ? "1" : "0");
			rows.emplace_back("PlayerBars", "bShowValues", a_s.pb.showValues ? "1" : "0");
			rows.emplace_back("Immersive", "bShowInCombat", a_s.imm.inCombat ? "1" : "0");
			rows.emplace_back("Immersive", "bShowWeaponDrawn", a_s.imm.weaponDrawn ? "1" : "0");
			auto layout = LayoutRows(a_s);
			rows.insert(rows.end(), layout.begin(), layout.end());
			return rows;
		}

		void Clamp(Snapshot& a_s)
		{
			const auto& els = hud::Elements();
			a_s.elements.resize(els.size());
			for (std::size_t i = 0; i < a_s.elements.size(); ++i) {
				auto& e = a_s.elements[i];
				e.x = std::clamp(e.x, -kMoveX, kMoveX);
				e.y = std::clamp(e.y, -kMoveY, kMoveY);
				// a scale of 0 or garbage would make the element vanish (Hide is the way to do that); 1 is the HUD's own size
				if (!(e.scale > 0.05F && e.scale < 10.0F)) { e.scale = 1.0F; }
				e.scale = std::clamp(e.scale, kScaleMin, kScaleMax);
				if (!(e.stretchX > 0.05F)) { e.stretchX = 1.0F; }
				if (!(e.stretchY > 0.05F)) { e.stretchY = 1.0F; }
				e.stretchX = std::clamp(e.stretchX, kScaleMin, kScaleMax);
				e.stretchY = std::clamp(e.stretchY, kScaleMin, kScaleMax);
				if (!els[i].stretch) { e.stretchX = e.stretchY = 1.0F; }
				if (e.show < 0 || e.show > 9 || e.show == 7) { e.show = 0; }   // 7 (a lock-on target) is not built yet
				if (e.show == 9 && std::string_view(els[i].key) != "Crosshair" && std::string_view(els[i].key) != "StealthMeter") { e.show = 0; }
				if (!els[i].fades) { e.alwaysVisible = false; }
				if (e.follow == static_cast<int>(i) || e.follow >= static_cast<int>(els.size())) { e.follow = -1; }
			}
			a_s.pb.healthMode = std::clamp(a_s.pb.healthMode, 0, 4);
			a_s.pb.magickaMode = std::clamp(a_s.pb.magickaMode, 0, 4);
			a_s.pb.staminaMode = std::clamp(a_s.pb.staminaMode, 0, 4);
			if (!(a_s.pb.phantomSeconds >= 0.0F)) { a_s.pb.phantomSeconds = 0.75F; }
			a_s.pb.phantomSeconds = std::min(std::round(a_s.pb.phantomSeconds * 100.0F) / 100.0F, 3.0F);
			a_s.imm.key = std::clamp(a_s.imm.key, 0, 255);
			a_s.imm.button = std::max(a_s.imm.button, 0);
			if (!(a_s.imm.seconds >= 0.0F)) { a_s.imm.seconds = 0.0F; }
			a_s.imm.seconds = std::min(std::round(a_s.imm.seconds * 10.0F) / 10.0F, 10.0F);
			a_s.fadeIn = std::clamp(a_s.fadeIn, 1, 20);
			a_s.fadeOut = std::clamp(a_s.fadeOut, 1, 20);
			a_s.opacityMax = std::clamp(a_s.opacityMax, 0, 100);
			a_s.opacityMin = std::clamp(a_s.opacityMin, 0, a_s.opacityMax);
			std::vector<int> members;
			for (const int m : a_s.group.members) {
				if (m >= 0 && static_cast<std::size_t>(m) < els.size() && std::ranges::find(members, m) == members.end()) { members.push_back(m); }
			}
			a_s.group.members = std::move(members);
			a_s.group.x = std::clamp(a_s.group.x, -kMoveX, kMoveX);
			a_s.group.y = std::clamp(a_s.group.y, -kMoveY, kMoveY);
		}

		using Entries = std::unordered_map<std::string, std::string>;   // "section.key" (lower case) -> value

		bool ReadIni(const std::filesystem::path& a_path, Entries& a_out)
		{
			std::ifstream in(a_path);
			if (!in) { return false; }
			std::string line, section;
			while (std::getline(in, line)) {
				line = Trim(line);
				if (line.empty() || line[0] == ';' || line[0] == '#') { continue; }
				if (line.front() == '[' && line.back() == ']') { section = Lower(Trim(line.substr(1, line.size() - 2))); continue; }
				const auto eq = line.find('=');
				if (eq == std::string::npos) { continue; }
				std::string val = Trim(line.substr(eq + 1));
				if (const auto sc = val.find(';'); sc != std::string::npos) { val = Trim(val.substr(0, sc)); }
				a_out[section + "." + Lower(Trim(line.substr(0, eq)))] = val;
			}
			return true;
		}

		const std::string* Find(const Entries& a_e, const std::string& a_key)
		{
			const auto it = a_e.find(Lower(a_key));
			return it != a_e.end() ? &it->second : nullptr;
		}

		bool  Flag(const std::string& a_v) { return a_v != "0" && Lower(a_v) != "false" && !a_v.empty(); }
		float F(const std::string& a_v, float a_fallback)
		{
			try { return std::stof(a_v); } catch (...) { return a_fallback; }
		}

		// The layout keys of a settings or preset file into a_s (the element sections, the group, HUD always visible).
		// a_sawFollow: which elements the file gives a sMoveWith. Returns how many elements differ from the game's layout.
		int ReadLayout(const Entries& a_e, Snapshot& a_s, std::vector<bool>& a_sawFollow)
		{
			const auto& els = hud::Elements();
			a_s.elements.resize(els.size());
			a_sawFollow.assign(els.size(), false);
			if (const auto* v = Find(a_e, "General.bAlwaysVisible")) { a_s.alwaysVisible = Flag(*v); }
			// the fade belongs to a layout too, so a preset carries it
			if (const auto* v = Find(a_e, "General.bFade")) { a_s.fade = Flag(*v); }
			if (const auto* v = Find(a_e, "General.iFadeInSpeed")) { a_s.fadeIn = static_cast<int>(F(*v, 10.0F)); }
			if (const auto* v = Find(a_e, "General.iFadeOutSpeed")) { a_s.fadeOut = static_cast<int>(F(*v, 5.0F)); }
			if (const auto* v = Find(a_e, "General.iOpacityMin")) { a_s.opacityMin = static_cast<int>(F(*v, 0.0F)); }
			if (const auto* v = Find(a_e, "General.iOpacityMax")) { a_s.opacityMax = static_cast<int>(F(*v, 100.0F)); }
			int moved = 0;
			for (std::size_t i = 0; i < els.size(); ++i) {
				const std::string k = els[i].key;
				auto&             e = a_s.elements[i];
				if (const auto* v = Find(a_e, k + ".fX")) { e.x = F(*v, 0.0F); }
				else if (const auto* o = Find(a_e, k + ".fOffsetX")) { e.x = F(*o, 0.0F) / kStageW * 100.0F; }   // 1.0: HUD units
				if (const auto* v = Find(a_e, k + ".fY")) { e.y = F(*v, 0.0F); }
				else if (const auto* o = Find(a_e, k + ".fOffsetY")) { e.y = F(*o, 0.0F) / kStageH * 100.0F; }
				if (const auto* v = Find(a_e, k + ".fScale")) { e.scale = F(*v, 1.0F); }
				if (const auto* v = Find(a_e, k + ".fLength")) { e.stretchX = F(*v, 1.0F); }
				if (const auto* v = Find(a_e, k + ".fHeight")) { e.stretchY = F(*v, 1.0F); }
				if (const auto* v = Find(a_e, k + ".bHide")) { e.hide = Flag(*v); }
				if (const auto* v = Find(a_e, k + ".iShow")) { e.show = static_cast<int>(F(*v, 0.0F)); }
				if (const auto* v = Find(a_e, k + ".bAlwaysVisible")) { e.alwaysVisible = Flag(*v); }
				if (const auto* v = Find(a_e, k + ".iStyle")) { e.style = (*v == "1") ? 1 : 0; }
				if (const auto* v = Find(a_e, k + ".sMoveWith")) {
					e.follow = v->empty() ? -1 : hud::IndexOf(*v);
					a_sawFollow[i] = true;
				}
				moved += e.IsDefault() ? 0 : 1;
			}
			if (const auto* v = Find(a_e, "Group.sMembers")) {
				a_s.group.members.clear();
				std::stringstream ss(*v);
				std::string       part;
				while (std::getline(ss, part, ',')) {
					if (const int idx = hud::IndexOf(Trim(part)); idx >= 0) { a_s.group.members.push_back(idx); }
				}
			}
			if (const auto* v = Find(a_e, "Group.fX")) { a_s.group.x = F(*v, 0.0F); }
			if (const auto* v = Find(a_e, "Group.fY")) { a_s.group.y = F(*v, 0.0F); }
			return moved;
		}

		void Load()
		{
			Snapshot s;
			for (std::size_t i = 0; i < hud::Elements().size(); ++i) {
				s.elements.push_back(DefaultFor(i));   // a key the INI lacks keeps its shipped default
			}
			Entries entries;
			if (!ReadIni(g_iniPath, entries)) {
				logger::warn("{} not found; every element starts where the HUD puts it", g_iniPath);
			}
			if (const auto* v = Find(entries, "General.bEnabled")) { s.enabled = Flag(*v); }
			if (const auto* v = Find(entries, "General.bLinkBars")) { s.linkBars = Flag(*v); }
			if (const auto* v = Find(entries, "General.bLinkWidgets")) { s.linkWidgets = Flag(*v); }
			if (const auto* v = Find(entries, "General.bUnlocked")) { s.unlocked = Flag(*v); }
			if (const auto* v = Find(entries, "Immersive.bEnabled")) { s.imm.enabled = Flag(*v); }
			if (const auto* v = Find(entries, "Immersive.iToggleKey")) { s.imm.key = static_cast<int>(F(*v, 45.0F)); }
			if (const auto* v = Find(entries, "Immersive.iToggleButton")) { s.imm.button = static_cast<int>(F(*v, 0.0F)); }
			if (const auto* v = Find(entries, "Immersive.bHoldMode")) { s.imm.hold = Flag(*v); }
			if (const auto* v = Find(entries, "Immersive.fDisplaySeconds")) { s.imm.seconds = F(*v, 0.0F); }
			if (const auto* v = Find(entries, "Immersive.bStartVisible")) { s.imm.startVisible = Flag(*v); }
			if (const auto* v = Find(entries, "Immersive.bHoldBarsWhenShown")) { s.imm.holdBars = Flag(*v); }
			if (const auto* v = Find(entries, "Immersive.bShowInCombat")) { s.imm.inCombat = Flag(*v); }
			if (const auto* v = Find(entries, "PlayerBars.bEnabled")) { s.pb.enabled = Flag(*v); }
			if (const auto* v = Find(entries, "PlayerBars.uHealthMode")) { s.pb.healthMode = static_cast<int>(F(*v, 1.0F)); }
			if (const auto* v = Find(entries, "PlayerBars.uMagickaMode")) { s.pb.magickaMode = static_cast<int>(F(*v, 1.0F)); }
			if (const auto* v = Find(entries, "PlayerBars.uStaminaMode")) { s.pb.staminaMode = static_cast<int>(F(*v, 1.0F)); }
			if (const auto* v = Find(entries, "PlayerBars.bPhantom")) { s.pb.phantom = Flag(*v); }
			if (const auto* v = Find(entries, "PlayerBars.fPhantomSeconds")) { s.pb.phantomSeconds = F(*v, 0.75F); }
			if (const auto* v = Find(entries, "PlayerBars.bMountStamina")) { s.pb.mountStamina = Flag(*v); }
			if (const auto* v = Find(entries, "PlayerBars.bSurvivalPenalty")) { s.pb.survivalPenalty = Flag(*v); }
			if (const auto* v = Find(entries, "PlayerBars.bShowValues")) { s.pb.showValues = Flag(*v); }
			if (const auto* v = Find(entries, "Immersive.bShowWeaponDrawn")) { s.imm.weaponDrawn = Flag(*v); }
			if (const auto* v = Find(entries, "General.uLogLevel")) { debug::logLevel = ClampLevel(static_cast<long>(F(*v, 2.0F))); }
			std::vector<bool> sawFollow;
			const int         moved = ReadLayout(entries, s, sawFollow);
			// an element whose sMoveWith the INI does not set takes its default under the INI's link toggles, which
			// are only known once the whole file is read
			for (std::size_t i = 0; i < s.elements.size(); ++i) {
				if (!sawFollow[i]) { s.elements[i].follow = DefaultFor(i, s.linkBars, s.linkWidgets).follow; }
			}
			Clamp(s);
			const bool migrated = Find(entries, "Health.fOffsetX") != nullptr && Find(entries, "Health.fX") == nullptr;
			std::lock_guard lk(g_lock);
			g_snap = std::move(s);
			g_dirty = migrated;   // a 1.0 INI is rewritten once in the new units
			g_lastEdit = std::chrono::steady_clock::now();
			logger::info("settings loaded from {}: layout {}, {} element(s) changed, {} moving as one, bars {}{}", g_iniPath,
				g_snap.enabled ? "on" : "off", moved, g_snap.group.members.size(), g_snap.alwaysVisible ? "always visible" : "as the game decides",
				migrated ? " (1.0 offsets converted to percent of the screen)" : "");
		}

		// Rewrite a file's lines with the given rows in place (comments and unknown keys stay), add what is missing at
		// the end of its section, and drop the 1.0 keys a 1.1 row replaced.
		void Merge(std::vector<std::string>& a_lines, const std::vector<std::tuple<std::string, std::string, std::string>>& a_rows)
		{
			// 1.0's offset keys: their values now live in fX / fY
			std::string sec;
			std::erase_if(a_lines, [&](const std::string& a_l) {
				const std::string t = Trim(a_l);
				if (!t.empty() && t.front() == '[' && t.back() == ']') { sec = t.substr(1, t.size() - 2); return false; }
				const auto eq = t.find('=');
				if (eq == std::string::npos || hud::IndexOf(sec) < 0) { return false; }
				const std::string k = Trim(t.substr(0, eq));
				return k == "fOffsetX" || k == "fOffsetY";
			});
			for (const auto& [section, key, val] : a_rows) {
				int  sectionEnd = -1;
				bool inSec = false, done = false;
				for (std::size_t i = 0; i < a_lines.size(); ++i) {
					const std::string t = Trim(a_lines[i]);
					if (!t.empty() && t.front() == '[' && t.back() == ']') {
						inSec = (t.substr(1, t.size() - 2) == section);
						if (inSec) { sectionEnd = static_cast<int>(i); }
						continue;
					}
					if (!inSec) { continue; }
					if (!t.empty()) { sectionEnd = static_cast<int>(i); }
					const auto eq = t.find('=');
					if (eq != std::string::npos && Trim(t.substr(0, eq)) == key) {
						a_lines[i] = key + "=" + val;
						done = true;
						break;
					}
				}
				if (done) { continue; }
				if (sectionEnd < 0) {
					if (!a_lines.empty() && !Trim(a_lines.back()).empty()) { a_lines.emplace_back(); }
					a_lines.push_back("[" + section + "]");
					a_lines.push_back(key + "=" + val);
				} else {
					a_lines.insert(a_lines.begin() + sectionEnd + 1, key + "=" + val);
				}
			}
		}

		std::vector<std::string> ReadLines(const std::filesystem::path& a_path)
		{
			std::vector<std::string> lines;
			std::ifstream            in(a_path);
			std::string              l;
			while (std::getline(in, l)) {
				if (!l.empty() && l.back() == '\r') { l.pop_back(); }
				lines.push_back(l);
			}
			return lines;
		}

		bool WriteLines(const std::filesystem::path& a_path, const std::vector<std::string>& a_lines)
		{
			std::ofstream out(a_path, std::ios::binary | std::ios::trunc);
			if (!out) { return false; }
			for (const auto& l : a_lines) { out << l << "\r\n"; }
			return true;
		}
	}

	ElementSetting DefaultFor(std::size_t a_index, bool a_linkBars, bool a_linkWidgets)
	{
		ElementSetting e;
		const auto& els = hud::Elements();
		if (a_index < els.size() && els[a_index].moveWith && (hud::IsWidget(els[a_index]) ? a_linkWidgets : a_linkBars)) {
			e.follow = hud::IndexOf(els[a_index].moveWith);
		}
		return e;
	}

	void ApplyLink(Snapshot& a_s, bool a_widgets, bool a_on)
	{
		const auto& els = hud::Elements();
		for (std::size_t i = 0; i < els.size() && i < a_s.elements.size(); ++i) {
			if (!els[i].moveWith || hud::IsWidget(els[i]) != a_widgets) { continue; }
			a_s.elements[i].follow = a_on ? hud::IndexOf(els[i].moveWith) : -1;
		}
	}

	void Init(const std::string& a_iniFileName)
	{
		g_iniPath = "Data/SKSE/Plugins/" + a_iniFileName;
		Load();
	}

	const std::string& GetIniPath() { return g_iniPath; }

	Snapshot Get()
	{
		std::lock_guard lk(g_lock);
		return g_snap;
	}

	void Publish(const Snapshot& a_s)
	{
		Snapshot s = a_s;
		Clamp(s);
		std::lock_guard lk(g_lock);
		g_snap = std::move(s);
		g_dirty = true;
		g_lastEdit = std::chrono::steady_clock::now();
	}

	void Update(const std::function<void(Snapshot&)>& a_change)
	{
		std::lock_guard lk(g_lock);
		a_change(g_snap);
		Clamp(g_snap);
		g_dirty = true;
		g_lastEdit = std::chrono::steady_clock::now();
	}

	void MaybeSave()
	{
		{
			std::lock_guard lk(g_lock);
			if (!g_dirty || std::chrono::steady_clock::now() - g_lastEdit < std::chrono::milliseconds(1500)) {
				return;
			}
		}
		Save();
	}

	bool Save()
	{
		Snapshot s;
		{
			std::lock_guard lk(g_lock);
			s = g_snap;
			g_dirty = false;
		}
		// ordinary file I/O - usvfs redirects that correctly, where the Win32 profile API silently writes nowhere (rule 16)
		auto lines = ReadLines(g_iniPath);
		Merge(lines, Rows(s));
		if (!WriteLines(g_iniPath, lines)) {
			logger::error("Could not write {}; the settings stay for this session only", g_iniPath);
			return false;
		}
		logger::debug("settings saved to {}", g_iniPath);
		return true;
	}

	// ---------------------------------------------------------------- presets

	std::filesystem::path PresetsFolder() { return std::filesystem::path("Data/SKSE/Plugins/HUDPositionManager/presets"); }

	std::vector<PresetInfo> ListPresets()
	{
		std::vector<PresetInfo> out;
		std::error_code         ec;
		for (const auto& entry : std::filesystem::directory_iterator(PresetsFolder(), ec)) {
			if (!entry.is_regular_file(ec) || Lower(entry.path().extension().string()) != ".ini") { continue; }
			PresetInfo p;
			p.path = entry.path();
			p.name = entry.path().stem().string();
			Entries e;
			if (ReadIni(p.path, e)) {
				if (const auto* v = Find(e, "Preset.sName"); v && !v->empty()) { p.name = *v; }
				if (const auto* v = Find(e, "Preset.sAuthor")) { p.author = *v; }
				if (const auto* v = Find(e, "Preset.sNote")) { p.note = *v; }
			}
			out.push_back(std::move(p));
		}
		std::ranges::sort(out, [](const PresetInfo& a, const PresetInfo& b) { return Lower(a.name) < Lower(b.name); });
		return out;
	}

	bool LoadPreset(const std::filesystem::path& a_path)
	{
		Entries e;
		if (!ReadIni(a_path, e)) {
			logger::warn("preset: {} could not be read", a_path.string());
			return false;
		}
		int moved = 0;
		{
			std::lock_guard lk(g_lock);
			// a preset is the whole layout: what it leaves out is the game's own (each element's default "Move with" kept)
			for (std::size_t i = 0; i < g_snap.elements.size(); ++i) { g_snap.elements[i] = DefaultFor(i, g_snap.linkBars, g_snap.linkWidgets); }
			g_snap.group = Group{};
			std::vector<bool> sawFollow;
			const auto        keepFollow = g_snap.elements;
			moved = ReadLayout(e, g_snap, sawFollow);
			for (std::size_t i = 0; i < g_snap.elements.size(); ++i) {
				if (!sawFollow[i]) { g_snap.elements[i].follow = keepFollow[i].follow; }
			}
			Clamp(g_snap);
			g_dirty = true;
			g_lastEdit = std::chrono::steady_clock::now() - std::chrono::seconds(5);   // saved on the next frame
		}
		logger::info("preset: {} loaded - {} element(s) changed from the game's layout", a_path.string(), moved);
		return true;
	}

	namespace
	{
		bool WritePreset(const std::filesystem::path& a_path, const std::string& a_name, const std::string& a_author, const std::string& a_note)
		{
			const Snapshot s = Get();
			std::vector<std::string> lines{
				"; HUD Position Manager preset - a whole layout. Load it from the Presets tab; every element the file leaves",
				"; out goes back to the game's own layout. fX / fY are a percentage of the screen.",
				"[Preset]", "sName=" + a_name, "sAuthor=" + a_author, "sNote=" + a_note, "",
				"[General]", std::string("bAlwaysVisible=") + (s.alwaysVisible ? "1" : "0"),
				std::string("bFade=") + (s.fade ? "1" : "0"), "iFadeInSpeed=" + std::to_string(s.fadeIn),
				"iFadeOutSpeed=" + std::to_string(s.fadeOut), "iOpacityMin=" + std::to_string(s.opacityMin),
				"iOpacityMax=" + std::to_string(s.opacityMax),
			};
			Merge(lines, LayoutRows(s));
			if (!WriteLines(a_path, lines)) {
				logger::error("preset: {} could not be written", a_path.string());
				return false;
			}
			return true;
		}
	}

	bool UpdatePreset(const std::filesystem::path& a_path)
	{
		Entries e;
		if (!ReadIni(a_path, e)) {
			logger::warn("preset: {} could not be read - not updated", a_path.string());
			return false;
		}
		const auto* name = Find(e, "Preset.sName");
		const auto* author = Find(e, "Preset.sAuthor");
		const auto* note = Find(e, "Preset.sNote");
		const bool  ok = WritePreset(a_path, name && !name->empty() ? *name : a_path.stem().string(), author ? *author : "", note ? *note : "");
		if (ok) { logger::info("preset: {} updated with the current layout", a_path.string()); }
		return ok;
	}

	std::filesystem::path SavePreset(std::string a_name, const std::string& a_author, const std::string& a_note)
	{
		// a file name from the name: what Windows refuses is dropped; a taken name gets the next free number
		std::string stem;
		for (const char c : a_name) {
			if (std::strchr("\\/:*?\"<>|", c) == nullptr && static_cast<unsigned char>(c) >= 0x20) { stem.push_back(c); }
		}
		while (!stem.empty() && (stem.back() == ' ' || stem.back() == '.')) { stem.pop_back(); }
		while (!stem.empty() && stem.front() == ' ') { stem.erase(stem.begin()); }
		if (stem.empty()) { stem = "My layout"; }
		std::error_code ec;
		std::filesystem::create_directories(PresetsFolder(), ec);
		std::filesystem::path path = PresetsFolder() / (stem + ".ini");
		for (int n = 2; std::filesystem::exists(path, ec) && n < 1000; ++n) {
			path = PresetsFolder() / std::format("{} {}.ini", stem, n);
		}
		if (!WritePreset(path, path.stem().string(), a_author, a_note)) { return {}; }
		logger::info("preset: the current layout saved to {}", path.string());
		return path;
	}

	bool DeletePreset(const std::filesystem::path& a_path)
	{
		std::error_code ec;
		// only a file inside the presets folder
		const auto folder = std::filesystem::weakly_canonical(PresetsFolder(), ec);
		const auto file = std::filesystem::weakly_canonical(a_path, ec);
		if (file.parent_path() != folder) {
			logger::warn("preset: {} is not in the presets folder - not deleted", a_path.string());
			return false;
		}
		const bool ok = std::filesystem::remove(file, ec) && !ec;
		logger::info("preset: {} {}", a_path.string(), ok ? "deleted" : "could not be deleted");
		return ok;
	}
}
