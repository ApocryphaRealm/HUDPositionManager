// The "HUD Position Manager" page - the Oblivion Remastered version's page (Presets / Layout / Combined widgets, 2026-10-04,
// the owner: "update the Skyrim HUD position manager to the same controls ... as the Oblivion version") on the Skyrim
// HUD: a tab per element (move, size, length / height, hide, show, always visible, move with, reset), the switches, the
// presets and the Combined widgets. Drawn on AMF's render thread: it never touches a movie, it reads the positioner's
// snapshot and changes settings through settings::Update().
#include "Page.h"

#include <imgui.h>

#include "AMF.h"
#include "PreciseSlider.h"

#include "Elements.h"
#include "Positioner.h"
#include "Immersive.h"
#include "Settings.h"
#include "utils/Logger.h"
#include "utils/Strings.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <mutex>
#include <string>
#include <vector>

namespace page
{
	namespace
	{
		using strings::TR;
		using precise::PercentSlider;
		using precise::ScaleSlider;

		// An on/off switch (rule 32 - never a checkbox): the framework's own design, a red/green track and a white knob,
		// in fixed colours (AMF's theme leaves FrameBg clear)
		bool Switch(const char* a_label, bool* a_v)
		{
			ImGui::PushID(a_label);
			const float  h = ImGui::GetFrameHeight();
			const float  w = h * 2.0f;
			const float  r = h * 0.5f;
			const ImVec2 p = ImGui::GetCursorScreenPos();
			const bool   pressed = ImGui::InvisibleButton("##switch", ImVec2(w, h));
			if (pressed) { *a_v = !*a_v; }
			const bool  hot = ImGui::IsItemHovered() || ImGui::IsItemFocused();
			const ImU32 track = *a_v ? (hot ? IM_COL32(92, 191, 96, 255) : IM_COL32(76, 175, 80, 255))
			                         : (hot ? IM_COL32(207, 84, 84, 255) : IM_COL32(191, 68, 68, 255));
			auto* dl = ImGui::GetWindowDrawList();
			dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), track, r);
			dl->AddCircleFilled(ImVec2(p.x + r + (*a_v ? w - h : 0.0f), p.y + r), r - 2.0f, IM_COL32(240, 240, 240, 255), 32);
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::AlignTextToFramePadding();
			const char* hash = std::strstr(a_label, "##");   // what follows "##" is the id, never shown
			ImGui::TextUnformatted(a_label, hash ? hash : nullptr);
			ImGui::PopID();
			return pressed;
		}

		// greyed and WRAPPED: an unwrapped hint ran past the panel's edge on a narrower window (the Oblivion version, 2026-10-03)
		void Hint(const char* a_text)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			ImGui::TextWrapped("%s", a_text);
			ImGui::PopStyleColor();
		}

		std::string ElementName(std::size_t a_i)
		{
			const auto& els = hud::Elements();
			return a_i < els.size() ? TR((std::string("HPM_El_") + els[a_i].key).c_str(), els[a_i].name) : std::string{};
		}

		// what the element inherits from the element(s) it moves with and the Combined widgets, in percent (its own excluded)
		std::pair<double, double> MoveWithPct(const settings::Snapshot& a_s, std::size_t a_i)
		{
			double      x = 0, y = 0;
			bool        grouped = a_s.group.Has(static_cast<int>(a_i));
			int         next = a_i < a_s.elements.size() ? a_s.elements[a_i].follow : -1;
			std::size_t steps = 0;
			while (next >= 0 && static_cast<std::size_t>(next) < a_s.elements.size() && static_cast<std::size_t>(next) != a_i && steps++ < a_s.elements.size()) {
				x += a_s.elements[static_cast<std::size_t>(next)].x;
				y += a_s.elements[static_cast<std::size_t>(next)].y;
				grouped |= a_s.group.Has(next);
				next = a_s.elements[static_cast<std::size_t>(next)].follow;
			}
			if (grouped) { x += a_s.group.x; y += a_s.group.y; }
			return { x, y };
		}

		// The range an element's OWN slider may take without its art leaving the screen, in percent: the measured box
		// less the offset applied when it was measured is where the element sits with no offset; the screen is the HUD
		// movie's visible stage. False until the element has been measured.
		bool OffsetRange(const positioner::State& a_st, const settings::Snapshot& a_s, std::size_t a_i, double& a_minX, double& a_maxX, double& a_minY, double& a_maxY)
		{
			if (a_i >= a_st.elements.size() || !a_st.elements[a_i].hasBounds || a_st.stageW < 1.0F || a_st.stageH < 1.0F) { return false; }
			const auto& e = a_st.elements[a_i];
			const auto [wx, wy] = MoveWithPct(a_s, a_i);
			const double W = a_st.stageW, H = a_st.stageH;
			const double baseL = e.xMin - e.appliedX, baseR = e.xMax - e.appliedX, baseT = e.yMin - e.appliedY, baseB = e.yMax - e.appliedY;
			const double withX = wx / 100.0 * W, withY = wy / 100.0 * H;
			a_minX = (a_st.stageLeft - baseL - withX) / W * 100.0;
			a_maxX = (a_st.stageLeft + W - baseR - withX) / W * 100.0;
			a_minY = (a_st.stageTop - baseT - withY) / H * 100.0;
			a_maxY = (a_st.stageTop + H - baseB - withY) / H * 100.0;
			return a_maxX >= a_minX && a_maxY >= a_minY;
		}

		// a slider's range, frozen while it is held (a range that moved with the twice-a-second measurement made the
		// mouse's position mean a different value each frame - "teleporting", the Oblivion version 2026-09-29)
		struct HeldRange
		{
			bool   x = false, y = false;
			double minX = 0, maxX = 0, minY = 0, maxY = 0;
		};
		std::vector<HeldRange> g_held;
		HeldRange              g_heldGroup;
		std::mutex             g_rangeLock;   // the page draws on the render thread, DevBench reads on its listener
		OpenRange              g_lastRange;

		// ---------------------------------------------------------------- the Presets tab
		std::vector<settings::PresetInfo> g_presets;
		ULONGLONG                         g_presetsAt = 0;   // listed at most once a second while the tab is open
		int                               g_presetIndex = 0;
		char                              g_presetName[64] = "My layout";
		std::string                       g_presetNotice;
		std::string                       g_deleteArmed;     // the preset a first press of Delete named
		int                               g_saveIndex = 0;   // 0 = a new preset, else g_presets[i - 1] updated in place

		void PresetsTab()
		{
			const ULONGLONG now = GetTickCount64();
			if (now - g_presetsAt >= 1000) {
				g_presetsAt = now;
				g_presets = settings::ListPresets();
			}
			if (g_presetIndex >= static_cast<int>(g_presets.size())) { g_presetIndex = g_presets.empty() ? 0 : static_cast<int>(g_presets.size()) - 1; }
			ImGui::TextWrapped("%s", TR("HPM_PresetsIntro", "A preset is a whole layout: every element's position, size and visibility. Load one made by someone else, or save your own to switch between."));
			Hint(settings::PresetsFolder().generic_string().c_str());
			ImGui::Spacing();
			if (g_presets.empty()) {
				Hint(TR("HPM_PresetsNone", "No presets yet. Save your layout below, or put a preset file from another author in the folder above."));
			} else {
				std::vector<std::string> labels;
				for (const auto& p : g_presets) { labels.push_back(p.author.empty() ? p.name : std::format("{} ({})", p.name, p.author)); }
				std::vector<const char*> ptrs;
				for (const auto& l : labels) { ptrs.push_back(l.c_str()); }
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				ImGui::Combo((std::string(TR("HPM_Preset", "Preset")) + "##preset").c_str(), &g_presetIndex, ptrs.data(), static_cast<int>(ptrs.size()));
				const auto& cur = g_presets[static_cast<std::size_t>(g_presetIndex)];
				if (!cur.note.empty()) { Hint(cur.note.c_str()); }
				if (ImGui::Button((std::string(TR("HPM_PresetLoad", "Load this preset")) + "##load").c_str())) {
					const bool ok = settings::LoadPreset(cur.path);
					g_presetNotice = ok ? std::format("{}: {}", TR("HPM_PresetLoaded", "Loaded"), cur.name) : std::format("{}: {}", TR("HPM_PresetFailed", "Could not read"), cur.name);
				}
				ImGui::SameLine();
				const bool armed = g_deleteArmed == cur.path.string();
				if (ImGui::Button((std::string(armed ? TR("HPM_PresetDeleteSure", "Press again to delete") : TR("HPM_PresetDelete", "Delete")) + "##delete").c_str())) {
					if (armed) {
						const bool ok = settings::DeletePreset(cur.path);
						g_presetNotice = ok ? std::format("{}: {}", TR("HPM_PresetDeleted", "Deleted"), cur.name) : std::format("{}: {}", TR("HPM_PresetDeleteFailed", "Could not delete"), cur.name);
						g_deleteArmed.clear();
						g_presetsAt = 0;
					} else {
						g_deleteArmed = cur.path.string();
					}
				}
			}
			ImGui::Spacing();
			ImGui::SeparatorText(TR("HPM_PresetSaveGroup", "Save my layout"));
			{
				std::vector<std::string> labels{ TR("HPM_PresetNew", "New preset") };
				for (const auto& p : g_presets) { labels.push_back(p.name); }
				std::vector<const char*> ptrs;
				for (const auto& l : labels) { ptrs.push_back(l.c_str()); }
				if (g_saveIndex >= static_cast<int>(labels.size())) { g_saveIndex = 0; }
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				ImGui::Combo((std::string(TR("HPM_PresetSaveTo", "Save to")) + "##saveto").c_str(), &g_saveIndex, ptrs.data(), static_cast<int>(ptrs.size()));
			}
			if (g_saveIndex == 0) {
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				ImGui::InputText((std::string(TR("HPM_PresetName", "Name")) + "##name").c_str(), g_presetName, sizeof(g_presetName));
				Hint(TR("HPM_PresetNameHint", "With a controller, leave the name as it is: each save gets its own number."));
				if (ImGui::Button((std::string(TR("HPM_PresetSave", "Save as a preset")) + "##save").c_str())) {
					const auto saved = settings::SavePreset(g_presetName, "", "");
					g_presetNotice = saved.empty() ? std::string(TR("HPM_PresetSaveFailed", "The preset could not be written.")) : std::format("{}: {}", TR("HPM_PresetSaved", "Saved"), saved.stem().string());
					g_presetsAt = 0;
				}
			} else {
				const auto& target = g_presets[static_cast<std::size_t>(g_saveIndex - 1)];
				if (ImGui::Button((std::string(TR("HPM_PresetUpdate", "Update this preset with my layout")) + "##update").c_str())) {
					const bool ok = settings::UpdatePreset(target.path);
					g_presetNotice = ok ? std::format("{}: {}", TR("HPM_PresetSaved", "Saved"), target.name) : std::string(TR("HPM_PresetSaveFailed", "The preset could not be written."));
					g_presetsAt = 0;
				}
			}
			if (!g_presetNotice.empty()) { ImGui::TextWrapped("%s", g_presetNotice.c_str()); }
		}

		// ---------------------------------------------------------------- an element's tab
		void ElementTab(std::size_t a_i, const settings::Snapshot& a_s, const positioner::State& a_st)
		{
			const auto& els = hud::Elements();
			const auto& el = els[a_i];
			auto        e = a_s.elements[a_i];
			const auto* es = a_i < a_st.elements.size() ? &a_st.elements[a_i] : nullptr;
			const bool  widget = hud::IsWidget(el);
			const std::string id = std::string("##") + el.key;
			if (!a_st.hudSeen) {
				Hint(TR("HPM_NoHud", "The HUD has not been shown yet - load a game to see this element."));
			} else if (widget && (!es || !es->menuOpen)) {
				ImGui::TextWrapped("%s", TR("HPM_WidgetClosed", "This widget is not showing: the mod that adds it is not installed, or has it switched off. Its settings are kept."));
			} else if (!es || es->partsFound == 0) {
				ImGui::TextWrapped("%s", TR("HPM_NotFound", "Not found in your HUD: the HUD you use may not have this element, or names it differently. Its settings are kept but do nothing."));
			} else {
				Hint(TR("HPM_Found", "In your HUD - changes show at once."));
			}
			bool changed = false;
			// the sliders' range is the screen (as far as the element's art can go before it leaves the screen), the fixed
			// range until it has been measured or while Free placement is on
			double minX = -settings::kMoveX, maxX = settings::kMoveX, minY = -settings::kMoveY, maxY = settings::kMoveY;
			if (!a_s.unlocked) {
				double lo, hi, tlo, thi;
				if (OffsetRange(a_st, a_s, a_i, lo, hi, tlo, thi)) {
					minX = lo; maxX = hi; minY = tlo; maxY = thi;
				}
			}
			// hard lock: a percentage of the screen never leaves [-100, 100], and the current value always stays reachable
			minX = std::clamp(std::min(minX, static_cast<double>(e.x)), -100.0, 100.0);
			maxX = std::clamp(std::max(maxX, static_cast<double>(e.x)), minX, 100.0);
			minY = std::clamp(std::min(minY, static_cast<double>(e.y)), -100.0, 100.0);
			maxY = std::clamp(std::max(maxY, static_cast<double>(e.y)), minY, 100.0);
			if (g_held.size() != els.size()) { g_held.assign(els.size(), {}); }
			auto& held = g_held[a_i];
			if (held.x) { minX = held.minX; maxX = held.maxX; } else { held.minX = minX; held.maxX = maxX; }
			if (held.y) { minY = held.minY; maxY = held.maxY; } else { held.minY = minY; held.maxY = maxY; }
			{
				std::scoped_lock l(g_rangeLock);
				g_lastRange = { el.key, minX, maxX, minY, maxY, a_s.unlocked, true };
			}
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
			changed |= PercentSlider((std::string(TR("HPM_MoveX", "Move left / right")) + id + "x").c_str(), &e.x, minX, maxX);
			held.x = ImGui::IsItemActive();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
			changed |= PercentSlider((std::string(TR("HPM_MoveY", "Move up / down")) + id + "y").c_str(), &e.y, minY, maxY);
			held.y = ImGui::IsItemActive();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
			changed |= ScaleSlider((std::string(TR("HPM_Size", "Size")) + id + "s").c_str(), &e.scale, settings::kScaleMin, settings::kScaleMax);
			if (el.stretch) {   // a bar, a meter, the compass: length and height on their own, on top of the size
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				changed |= ScaleSlider((std::string(TR("HPM_Length", "Length")) + id + "l").c_str(), &e.stretchX, settings::kScaleMin, settings::kScaleMax);
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				changed |= ScaleSlider((std::string(TR("HPM_Height", "Height")) + id + "t").c_str(), &e.stretchY, settings::kScaleMin, settings::kScaleMax);
				Hint(TR("HPM_StretchHint", "Length and Height stretch the bar on one side each, on top of Size."));
			}
			changed |= Switch((std::string(TR("HPM_Hide", "Hide")) + id + "h").c_str(), &e.hide);
			if (!e.hide) {   // context-aware visibility (the Oblivion version, 2026-10-03)
				// the combo's rows and the iShow value each stands for (7, a lock-on target, is not offered yet)
				const char* shows[8]{ TR("HPM_ShowAlways", "Always"), TR("HPM_ShowCombat", "Only in combat"), TR("HPM_ShowNoCombat", "Only out of combat"),
					TR("HPM_ShowImmersive", "Follow the HUD toggle"), TR("HPM_ShowInterior", "Only indoors"), TR("HPM_ShowExterior", "Only outdoors"),
					TR("HPM_ShowWeapon", "Only with a weapon drawn"), TR("HPM_ShowSneak", "Only while sneaking") };
				static constexpr int kShowValue[9]{ 0, 1, 2, 3, 4, 5, 6, 8, 9 };
				// "when it matters" is offered on the crosshair and the sneak eye only
				const bool ctxEl = std::string_view(el.key) == "Crosshair" || std::string_view(el.key) == "StealthMeter";
				const char* rows[9]{ shows[0], shows[1], shows[2], shows[3], shows[4], shows[5], shows[6], shows[7],
					std::string_view(el.key) == "Crosshair" ? TR("HPM_ShowContextCross", "When it matters - while aiming, attacking or casting")
					                                         : TR("HPM_ShowContextEye", "When it matters - as strong as the detection") };
				const int count = ctxEl ? 9 : 8;
				int row = 0;
				for (int r = 0; r < count; ++r) { if (kShowValue[r] == e.show) { row = r; } }
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				if (ImGui::Combo((std::string(TR("HPM_Show", "Show")) + id + "v").c_str(), &row, rows, count)) {
					e.show = kShowValue[row];
					changed = true;
				}
				Hint(TR("HPM_ShowHint", "When this shows while you play. Only in combat: hidden while you explore, back as soon as a fight starts. Only out of combat: hidden during fights. While Show every element is on, everything shows."));
			}
			if (el.swf2) {   // a built widget with two art styles (the Level widget: Bar or Badge)
				const char* styles[2]{ TR("HPM_StyleBar", "Bar - the number in front of a bar"), TR("HPM_StyleBadge", "Badge - the number in a badge, a ring round it") };
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				changed |= ImGui::Combo((std::string(TR("HPM_Style", "Style")) + id + "st").c_str(), &e.style, styles, 2);
				Hint(TR("HPM_StyleHint", "Two looks for the same information - pick the one you like. A UI mod can reskin either."));
			}
			if (el.fades) {
				changed |= Switch((std::string(TR("HPM_AlwaysOne", "Always visible")) + id + "a").c_str(), &e.alwaysVisible);
				Hint(TR("HPM_AlwaysOneHint", "The game fades this out on its own. On: it stays shown while you play."));
			}
			// move with: nothing, or a HUD element (never itself, never another mod's widget)
			{
				std::vector<std::string> labels{ TR("HPM_MoveWithNone", "Nothing - on its own") };
				std::vector<int>         index{ -1 };
				int                      current = 0;
				for (std::size_t j = 0; j < els.size(); ++j) {
					if (j == a_i || hud::IsWidget(els[j])) { continue; }
					if (e.follow == static_cast<int>(j)) { current = static_cast<int>(labels.size()); }
					labels.push_back(ElementName(j));
					index.push_back(static_cast<int>(j));
				}
				std::vector<const char*> ptrs;
				for (const auto& l : labels) { ptrs.push_back(l.c_str()); }
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				if (ImGui::Combo((std::string(TR("HPM_MoveWith", "Move with")) + id + "f").c_str(), &current, ptrs.data(), static_cast<int>(ptrs.size()))) {
					e.follow = index[static_cast<std::size_t>(current)];
					changed = true;
				}
			}
			if (a_s.group.Has(static_cast<int>(a_i))) {
				Hint(TR("HPM_InGroupHint", "Also moves with the widgets on the Combined widgets tab."));
			}
			if (changed) {
				settings::Update([&](settings::Snapshot& s) { s.elements[a_i] = e; });
			}
			if (ImGui::Button((std::string(TR("HPM_ResetOne", "Reset this element")) + id + "r").c_str())) {
				settings::Update([&](settings::Snapshot& s) { s.elements[a_i] = settings::DefaultFor(a_i, s.linkBars, s.linkWidgets); });
				logger::info("page: {} reset", el.key);
			}
		}

		// ---------------------------------------------------------------- the Combined widgets tab
		void CombinedTab(const settings::Snapshot& a_s, const positioner::State& a_st)
		{
			const auto& els = hud::Elements();
			ImGui::TextWrapped("%s", TR("HPM_CombinedIntro", "Pick the widgets that should move as one. The sliders below move every picked widget together, on top of the position each has on its own tab."));
			ImGui::SeparatorText(TR("HPM_GroupMembers", "Widgets that move as one"));
			auto group = a_s.group;
			bool changed = false;
			std::vector<std::pair<std::size_t, float>> shift;   // (element, +1 joined / -1 left): its own offset keeps it in place
			for (std::size_t i = 0; i < els.size(); ++i) {
				bool on = group.Has(static_cast<int>(i));
				if (Switch((ElementName(i) + "##grp" + els[i].key).c_str(), &on)) {
					if (on) { group.members.push_back(static_cast<int>(i)); }
					else { std::erase(group.members, static_cast<int>(i)); }
					shift.emplace_back(i, on ? 1.0f : -1.0f);
					changed = true;
				}
			}
			settings::Snapshot now = a_s;
			now.group = group;
			for (const auto& [i, dir] : shift) {
				now.elements[i].x -= dir * group.x;
				now.elements[i].y -= dir * group.y;
			}
			ImGui::Spacing();
			if (group.members.size() < 2) {
				Hint(TR("HPM_GroupNone", "Pick two or more widgets above to move them together."));
			} else {
				// what every member can still travel: its own slider's range, less its own value
				double minX = -settings::kMoveX, maxX = settings::kMoveX, minY = -settings::kMoveY, maxY = settings::kMoveY;
				if (!a_s.unlocked) {
					for (const int m : group.members) {
						double lo, hi, tlo, thi;
						if (!OffsetRange(a_st, now, static_cast<std::size_t>(m), lo, hi, tlo, thi)) { continue; }
						const auto& e = now.elements[static_cast<std::size_t>(m)];
						minX = std::max(minX, group.x + lo - e.x);
						maxX = std::min(maxX, group.x + hi - e.x);
						minY = std::max(minY, group.y + tlo - e.y);
						maxY = std::min(maxY, group.y + thi - e.y);
					}
				}
				// the shared offset is never clamped here (a clamp on a tick moved every member - "fleeing"); the range
				// always holds the current value, and only the sliders move it
				minX = std::clamp(std::min(minX, static_cast<double>(group.x)), -100.0, 100.0);
				maxX = std::clamp(std::max(maxX, static_cast<double>(group.x)), minX, 100.0);
				minY = std::clamp(std::min(minY, static_cast<double>(group.y)), -100.0, 100.0);
				maxY = std::clamp(std::max(maxY, static_cast<double>(group.y)), minY, 100.0);
				auto& held = g_heldGroup;
				if (held.x) { minX = held.minX; maxX = held.maxX; } else { held.minX = minX; held.maxX = maxX; }
				if (held.y) { minY = held.minY; maxY = held.maxY; } else { held.minY = minY; held.maxY = maxY; }
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				changed |= PercentSlider((std::string(TR("HPM_MoveX", "Move left / right")) + "##groupx").c_str(), &group.x, minX, maxX);
				held.x = ImGui::IsItemActive();
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				changed |= PercentSlider((std::string(TR("HPM_MoveY", "Move up / down")) + "##groupy").c_str(), &group.y, minY, maxY);
				held.y = ImGui::IsItemActive();
				if (ImGui::Button((std::string(TR("HPM_GroupReset", "Reset the shared position")) + "##groupreset").c_str())) {
					group.x = 0.0f;
					group.y = 0.0f;
					changed = true;
				}
			}
			if (changed) {
				settings::Update([&](settings::Snapshot& s) {
					s.group = group;
					for (const auto& [i, dir] : shift) {   // joined: its own offset less the shared one; left: plus it
						s.elements[i].x -= dir * group.x;
						s.elements[i].y -= dir * group.y;
					}
				});
			}
		}

		// the bumpers (AMF::DeclareInnerTabs): which tab of each bar is open, and the one the bumpers asked for
		int         g_topTab = 0, g_topRequest = -1;
		int         g_elementTab = 0, g_elementRequest = -1;   // positions in the DISPLAYED order
		std::string g_elementOpen;                              // the key of the element tab open last frame
		std::string g_elementOrderId;                           // the displayed order last frame, to notice a change

		ImGuiTabItemFlags TopFlags(int a_index) { return g_topRequest == a_index ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None; }

		void DeclareTop()
		{
			g_topRequest = -1;
			if (const int r = AMF::DeclareInnerTabs(3, g_topTab); r >= 0 && r != g_topTab) { g_topRequest = r; }
		}

		// One press-to-bind row (Dragon's Eye Minimap's pattern): the binding's name, then Bind - which arms the next press on
		// that side - or, while armed, a Cancel the mouse can click. AMF's own keys are refused (the row says so).
		void BindRow(const char* a_label, const std::string& a_name, immersive::Bind a_side, const char* a_id)
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("%s: %s", a_label, a_name.empty() ? TR("HPM_Unbound", "none") : a_name.c_str());
			ImGui::SameLine();
			if (immersive::Armed() == a_side) {
				const char* waiting = a_side == immersive::Bind::kKeyboard ? TR("HPM_PressKey", "Press a key... (cancel)")
				                                                            : TR("HPM_PressButton", "Press a button... (cancel)");
				if (ImGui::Button((std::string(waiting) + a_id).c_str())) { immersive::Arm(immersive::Bind::kNone); }
			} else if (ImGui::Button((std::string(TR("HPM_Bind", "Bind")) + a_id).c_str())) {
				immersive::Arm(a_side);
			}
			if (!a_name.empty()) {
				ImGui::SameLine();
				if (ImGui::Button((std::string(TR("HPM_Clear", "Clear")) + a_id + "c").c_str())) {
					settings::Update([&](settings::Snapshot& s) { (a_side == immersive::Bind::kKeyboard ? s.imm.key : s.imm.button) = 0; });
				}
			}
		}

		// The HUD toggle (ImmersiveHUD parity, phase 3 build 2): off by default; on, a key / button shows and hides every
		// element whose Show is "Follow the HUD toggle".
		void ImmersiveSection(settings::Snapshot& v)
		{
			immersive::PageDrawn();
			{
				immersive::Bind side{};
				if (const int code = immersive::TakeCaptured(side); code >= 0) {
					settings::Update([&](settings::Snapshot& s) { (side == immersive::Bind::kKeyboard ? s.imm.key : s.imm.button) = code; });
					(side == immersive::Bind::kKeyboard ? v.imm.key : v.imm.button) = code;
				}
			}
			static std::string status;
			if (auto st = immersive::TakeStatus(); !st.empty()) { status = st; }
			ImGui::SeparatorText(TR("HPM_GroupImmersive", "HUD toggle"));
			if (Switch(TR("HPM_Immersive", "Immersive HUD"), &v.imm.enabled)) {
				settings::Update([&](settings::Snapshot& s) { s.imm.enabled = v.imm.enabled; });
				logger::info("page: HUD toggle {}", v.imm.enabled ? "on" : "off");
			}
			Hint(TR("HPM_ImmersiveHint", "On: a key shows and hides every element whose Show is 'Follow the HUD toggle' - they fade out of sight while you explore and back when you press it."));
			if (immersive::IhudPresent()) { Hint(TR("HPM_IhudPresent", "ImmersiveHUD is loaded too: both would fade the same parts of the HUD. Use one or the other.")); }
			if (!v.imm.enabled) { return; }
			BindRow(TR("HPM_ToggleKey", "Toggle key"), immersive::KeyName(v.imm.key), immersive::Bind::kKeyboard, "##immkey");
			BindRow(TR("HPM_ToggleButton", "Toggle button (controller)"), immersive::ButtonName(v.imm.button), immersive::Bind::kGamepad, "##immbtn");
			if (status == "reserved") { Hint(TR("HPM_KeyReserved", "That key is the menu's own. Press a different key.")); }
			if (immersive::Armed() == immersive::Bind::kNone) { status.clear(); }
			bool c = false;
			c |= Switch(TR("HPM_HoldMode", "Hold to show"), &v.imm.hold);
			Hint(TR("HPM_HoldModeHint", "On: the HUD shows only while the key is held. Off: each press shows or hides it."));
			if (!v.imm.hold) {
				c |= precise::TenthsSlider(TR("HPM_DisplaySeconds", "Show for"), &v.imm.seconds, 0.0F, 10.0F);
				Hint(TR("HPM_DisplaySecondsHint", "Above 0: a press shows the HUD for this long, then it fades again. 0: a press toggles it."));
			}
			c |= Switch(TR("HPM_ShowInCombatAll", "Also shown in combat"), &v.imm.inCombat);
			c |= Switch(TR("HPM_ShowWeaponAll", "Also shown with a weapon drawn"), &v.imm.weaponDrawn);
			c |= Switch(TR("HPM_StartVisible", "Shown after loading"), &v.imm.startVisible);
			c |= Switch(TR("HPM_HoldBars", "Keep the bars up while shown"), &v.imm.holdBars);
			Hint(TR("HPM_HoldBarsHint", "While the toggle shows the HUD, the bars the game fades when full stay shown."));
			if (c) {
				settings::Update([&](settings::Snapshot& s) {
					s.imm.hold = v.imm.hold; s.imm.seconds = v.imm.seconds; s.imm.startVisible = v.imm.startVisible; s.imm.holdBars = v.imm.holdBars;
					s.imm.inCombat = v.imm.inCombat; s.imm.weaponDrawn = v.imm.weaponDrawn;
				});
			}
			if (ImGui::Button(TR("HPM_ImmersiveAll", "Put the whole HUD on the toggle"))) {
				logger::info("page: {} element(s) put on the HUD toggle", PutAllOnToggle(true));
			}
			ImGui::SameLine();
			if (ImGui::Button(TR("HPM_ImmersiveNone", "Take it all off the toggle"))) {
				logger::info("page: {} element(s) taken off the HUD toggle", PutAllOnToggle(false));
			}
		}

		// HPM's own player bars (phase 4 build 1, TrueHUD's player widget): off by default
		void PlayerBarsSection(settings::Snapshot& v)
		{
			ImGui::SeparatorText(TR("HPM_TabBars", "Bars and loot"));
			bool c = Switch(TR("HPM_PB_Enabled", "HPM's player bars"), &v.pb.enabled);
			Hint(TR("HPM_PB_EnabledHint", "Health, Magicka and Stamina drawn by HPM, with recent loss and the Survival penalty. Hide the game's own bars on their tabs."));
			if (GetModuleHandleW(L"TrueHUD.dll")) { Hint(TR("HPM_TH_Present", "TrueHUD is installed and already draws this. HPM's version stays off unless you turn it on here.")); }
			if (v.pb.enabled) {
				const char* modes[5]{ TR("HPM_Never", "Never"), TR("HPM_PB_ModeDynamic", "When it changes"), TR("HPM_ShowCombat", "Only in combat"),
					TR("HPM_PB_ModeWhenOther", "When another bar shows"), TR("HPM_ShowAlways", "Always") };
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				c |= ImGui::Combo(TR("HPM_PB_HealthMode", "Show Health"), &v.pb.healthMode, modes, 5);
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				c |= ImGui::Combo(TR("HPM_PB_MagickaMode", "Show Magicka"), &v.pb.magickaMode, modes, 5);
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				c |= ImGui::Combo(TR("HPM_PB_StaminaMode", "Show Stamina"), &v.pb.staminaMode, modes, 5);
				c |= Switch(TR("HPM_Phantom", "Show recent loss"), &v.pb.phantom);
				if (v.pb.phantom) { c |= precise::TenthsSlider(TR("HPM_PhantomSeconds", "Recent loss lingers"), &v.pb.phantomSeconds, 0.0F, 3.0F); }
				c |= Switch(TR("HPM_PB_MountStamina", "Mount's Stamina while riding"), &v.pb.mountStamina);
				c |= Switch(TR("HPM_PB_SurvivalPenalty", "Show the Survival penalty"), &v.pb.survivalPenalty);
				Hint(TR("HPM_PB_SurvivalPenaltyHint", "In Survival Mode, the part of the bar that hunger, fatigue or cold has taken away is drawn at its end."));
				c |= Switch(TR("HPM_ShowValues", "Show the numbers"), &v.pb.showValues);
			}
			if (c) {
				settings::Update([&](settings::Snapshot& s) { s.pb = v.pb; });
			}

			// recent loot (phase 4 build 4)
			{
				bool lc = Switch(TR("HPM_RL_Enabled", "Recent loot list"), &v.rl.enabled);
				Hint(TR("HPM_RL_EnabledHint", "Each item you pick up is listed for a few seconds with its count."));
				if (v.rl.enabled) {
					lc |= Switch(TR("HPM_RL_HideVanilla", "Hide the game's 'added' message"), &v.rl.hideVanilla);
					lc |= precise::TenthsSlider(TR("HPM_Seconds", "Seconds shown"), &v.rl.seconds, 1.0F, 30.0F);
					lc |= precise::StepSlider(TR("HPM_MaxCount", "Most at once"), &v.rl.maxCount, 1, 6);
				}
				if (lc) { settings::Update([&](settings::Snapshot& s) { s.rl = v.rl; }); }
			}

			// the boss bar (phase 4 build 3)
			{
				bool bc = Switch(TR("HPM_BB_Enabled", "Boss bars"), &v.bb.enabled);
				Hint(TR("HPM_BB_EnabledHint", "A large bar on the screen for dragons, dragon priests and other bosses while you fight them."));
				if (bc) { settings::Update([&](settings::Snapshot& s) { s.bb.enabled = v.bb.enabled; }); }
			}

			// the info bars over characters (phase 4 build 2)
			bool ic = Switch(TR("HPM_IB_Enabled", "Bars over characters"), &v.ib.enabled);
			Hint(TR("HPM_IB_EnabledHint", "A Health bar over enemies, followers and others near you, with their name and level."));
			if (v.ib.enabled) {
				const char* hostile[3]{ TR("HPM_Never", "Never"), TR("HPM_IB_OnCombat", "When a fight starts"), TR("HPM_ShowAlways", "Always") };
				const char* team[3]{ TR("HPM_Never", "Never"), TR("HPM_IB_OnCombat", "When a fight starts"), TR("HPM_ShowAlways", "Always") };
				const char* other[3]{ TR("HPM_Never", "Never"), TR("HPM_IB_OnHit", "When hit"), TR("HPM_ShowAlways", "Always") };
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				ic |= ImGui::Combo(TR("HPM_IB_DisplayHostiles", "Enemies"), &v.ib.hostiles, hostile, 3);
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				ic |= ImGui::Combo(TR("HPM_IB_DisplayTeammates", "Followers"), &v.ib.teammates, team, 3);
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				ic |= ImGui::Combo(TR("HPM_IB_DisplayOthers", "Everyone else"), &v.ib.others, other, 3);
				ic |= Switch(TR("HPM_IB_DisplayName", "Name"), &v.ib.showName);
				ic |= Switch(TR("HPM_IB_IndicatorLevel", "Level number"), &v.ib.showLevel);
				ic |= precise::StepSlider(TR("HPM_MaxCount", "Most at once"), &v.ib.maxCount, 1, 20);
				int dist = static_cast<int>(v.ib.maxDistance / 64.0F);
				if (precise::StepSlider(TR("HPM_IB_MaxDistance", "Farthest distance"), &dist, 4, 128, "%d x 64")) { v.ib.maxDistance = dist * 64.0F; ic = true; }
				int oz = static_cast<int>(v.ib.offsetZ);
				if (precise::StepSlider(TR("HPM_IB_OffsetZ", "Height above"), &oz, -50, 200)) { v.ib.offsetZ = static_cast<float>(oz); ic = true; }
				ic |= ScaleSlider(TR("HPM_Size", "Size"), &v.ib.fScale, settings::kScaleMin, settings::kScaleMax);
				ic |= Switch(TR("HPM_IB_ScaleDistance", "Smaller with distance"), &v.ib.scaleWithDistance);
			}
			if (ic) {
				settings::Update([&](settings::Snapshot& s) { s.ib = v.ib; });
			}

			// floating text (phase 4 build 4): the damage you deal, and other mods' texts (the ModEvent HPM_FloatingText)
			bool fc = Switch(TR("HPM_FT_Enabled", "Floating text"), &v.ft.enabled);
			Hint(TR("HPM_FT_EnabledHint", "Short text that rises over a character and fades, from other mods or from damage."));
			if (v.ft.enabled) {
				fc |= Switch(TR("HPM_FT_DamageNumbers", "Damage numbers"), &v.ft.damageNumbers);
				Hint(TR("HPM_FT_DamageNumbersHint", "The damage you deal rises over the one you hit."));
				fc |= precise::TenthsSlider(TR("HPM_Seconds", "Seconds shown"), &v.ft.seconds, 0.5F, 5.0F);
				fc |= precise::StepSlider(TR("HPM_FT_Rise", "Rise speed"), &v.ft.rise, 0, 120);
				fc |= ScaleSlider(TR("HPM_Size", "Size"), &v.ft.fScale, settings::kScaleMin, settings::kScaleMax);
				fc |= Switch(TR("HPM_IB_ScaleDistance", "Smaller with distance"), &v.ft.scaleWithDistance);
			}
			if (fc) {
				settings::Update([&](settings::Snapshot& s) { s.ft = v.ft; });
			}
		}

		void LayoutTab(settings::Snapshot& v, const positioner::State& st)
		{
			const auto& els = hud::Elements();
			ImGui::TextWrapped("%s", TR("HPM_Intro", "Move, resize or hide each part of the HUD. Changes show in the HUD at once and are saved automatically."));
			if (Switch(TR("HPM_Enabled", "Apply my layout"), &v.enabled)) {
				settings::Update([&](settings::Snapshot& s) { s.enabled = v.enabled; });
				logger::info("page: layout {}", v.enabled ? "on" : "off");
			}
			Hint(TR("HPM_EnabledHint", "off: every element back where the HUD puts it"));
			if (Switch(TR("HPM_Unlocked", "Free placement"), &v.unlocked)) {
				settings::Update([&](settings::Snapshot& s) { s.unlocked = v.unlocked; });
				logger::info("page: free placement {}", v.unlocked ? "on" : "off");
			}
			Hint(TR("HPM_UnlockedHint", "On: the move sliders go all the way, past the edges of the screen, so an element can sit right at an edge or off screen. Off: an element stops where its art meets the edge of the screen."));
			// Whether the user's UI mod links the bars and the widgets around them (Norden UI does): on moves them as one
			if (Switch(TR("HPM_LinkBars", "Move the three bars together"), &v.linkBars)) {
				settings::Update([&](settings::Snapshot& s) { s.linkBars = v.linkBars; settings::ApplyLink(s, false, s.linkBars); });
			}
			Hint(TR("HPM_LinkBarsHint", "Magicka and Stamina move with Health"));
			if (Switch(TR("HPM_LinkWidgets", "Widgets around the bars move with them"), &v.linkWidgets)) {
				settings::Update([&](settings::Snapshot& s) { s.linkWidgets = v.linkWidgets; settings::ApplyLink(s, true, s.linkWidgets); });
			}
			Hint(TR("HPM_LinkWidgetsHint", "for a UI that places widgets around the bars, like Norden UI"));

			ImGui::SeparatorText(TR("HPM_GroupVisibility", "HUD visibility"));
			if (Switch(TR("HPM_AlwaysAll", "Always visible"), &v.alwaysVisible)) {
				settings::Update([&](settings::Snapshot& s) { s.alwaysVisible = v.alwaysVisible; });
				logger::info("page: bars {}", v.alwaysVisible ? "always visible" : "as the game decides");
			}
			Hint(v.alwaysVisible ? TR("HPM_AlwaysAllOnHint", "The bars stay shown while you play. Menus, dialogue and loading screens still hide the HUD.")
			                     : TR("HPM_AlwaysAllOffHint", "Off: the game decides. The bars fade out when they are full."));
			// phase 3, build 1: Show fades instead of hiding at once (ImmersiveHUD's fade speeds and opacity range)
			if (Switch(TR("HPM_Fade", "Fade instead of hiding at once"), &v.fade)) {
				settings::Update([&](settings::Snapshot& s) { s.fade = v.fade; });
				logger::info("page: Show {}", v.fade ? "fades" : "hides at once");
			}
			Hint(TR("HPM_FadeHint", "Show (in / out of combat) fades the element in and out instead of hiding it at once."));
			if (v.fade) {
				bool fchanged = false;
				fchanged |= precise::StepSlider(TR("HPM_FadeIn", "Fade-in speed"), &v.fadeIn, 1, 20);
				fchanged |= precise::StepSlider(TR("HPM_FadeOut", "Fade-out speed"), &v.fadeOut, 1, 20);
				Hint(TR("HPM_FadeSpeedHint", "Higher is faster: 10 fades in about 0.2 seconds, 5 in about 0.4."));
				fchanged |= precise::StepSlider(TR("HPM_OpacityMin", "Opacity when hidden"), &v.opacityMin, 0, 100, "%d %%");
				fchanged |= precise::StepSlider(TR("HPM_OpacityMax", "Opacity when shown"), &v.opacityMax, 0, 100, "%d %%");
				Hint(TR("HPM_OpacityHint", "How see-through an element faded by Show is when out of its moment, and when in it."));
				if (fchanged) {
					if (v.opacityMin > v.opacityMax) { v.opacityMin = v.opacityMax; }
					settings::Update([&](settings::Snapshot& s) {
						s.fadeIn = v.fadeIn; s.fadeOut = v.fadeOut; s.opacityMin = v.opacityMin; s.opacityMax = v.opacityMax;
					});
				}
			}
			ImGui::Spacing();

			ImmersiveSection(v);
			ImGui::Spacing();
			PlayerBarsSection(v);
			ImGui::Spacing();

			// the displayed order: the Combined widgets members first, in the order they were picked, then the rest
			std::vector<std::size_t> order;
			for (const int m : v.group.members) {
				if (m >= 0 && static_cast<std::size_t>(m) < els.size()) { order.push_back(static_cast<std::size_t>(m)); }
			}
			const std::size_t pinned = order.size();
			for (std::size_t i = 0; i < els.size(); ++i) {
				if (std::ranges::find(order, i) == order.end()) { order.push_back(i); }
			}
			std::string orderId;
			for (const auto i : order) { orderId += std::to_string(i) + "."; }
			const bool reordered = !g_elementOrderId.empty() && orderId != g_elementOrderId;
			g_elementOrderId = orderId;
			std::size_t shown = order.size();
			// the bar's ID carries the order: ImGui keeps a bar's first tab order, so a new membership is a new bar
			if (ImGui::BeginTabBar(("HudElements##" + orderId).c_str(), ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_TabListPopupButton)) {
				for (std::size_t d = 0; d < order.size(); ++d) {
					const std::size_t i = order[d];
					const bool keep = reordered && g_elementOpen == els[i].key;   // the open element stays open in the new order
					const ImGuiTabItemFlags f = (g_elementRequest == static_cast<int>(d) || keep) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
					// "###key" pins the tab's id to the element, so a language switch keeps the selected tab; a pinned
					// tab (a Combined widgets member) is marked
					const std::string label = std::string(d < pinned ? "+ " : "") + ElementName(i) + "###tab" + els[i].key;
					if (ImGui::BeginTabItem(label.c_str(), nullptr, f)) {
						shown = d;
						g_elementOpen = els[i].key;
						ElementTab(i, v, st);
						ImGui::EndTabItem();
					}
				}
				ImGui::EndTabBar();
			}
			// the Layout tab is open: its element tabs are the innermost bar, and the bumpers walk them in the displayed order
			if (shown < order.size()) { g_elementTab = static_cast<int>(shown); }
			g_elementRequest = -1;
			if (const int r = AMF::DeclareInnerTabs(static_cast<int>(order.size()), g_elementTab); r >= 0 && r != g_elementTab) { g_elementRequest = r; }
			ImGui::Spacing();
			ImGui::Separator();
			if (ImGui::Button(TR("HPM_ResetAll", "Reset every element"))) {
				settings::Update([&](settings::Snapshot& s) {
					for (std::size_t i = 0; i < s.elements.size(); ++i) { s.elements[i] = settings::DefaultFor(i, s.linkBars, s.linkWidgets); }
				});
				logger::info("page: every element reset");
			}
		}

		void Draw()
		{
			if (!AMF::UseFrameworkImGui()) {
				return;
			}
			strings::Tick();
			if (!ImGui::BeginTabBar("HpmTop", ImGuiTabBarFlags_None)) {
				return;
			}
			if (ImGui::BeginTabItem((std::string(TR("HPM_TabPresets", "Presets")) + "##tabpresets").c_str(), nullptr, TopFlags(0))) {
				g_topTab = 0;
				PresetsTab();
				ImGui::EndTabItem();
			}
			auto       v = settings::Get();
			const auto st = positioner::GetState();
			if (!ImGui::BeginTabItem((std::string(TR("HPM_TabLayout", "Layout")) + "##tablayout").c_str(), nullptr, TopFlags(1))) {
				if (ImGui::BeginTabItem((std::string(TR("HPM_TabCombined", "Combined widgets")) + "##tabcombined").c_str(), nullptr, TopFlags(2))) {
					g_topTab = 2;
					CombinedTab(v, st);
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
				DeclareTop();   // Presets or Combined widgets open: the bumpers walk the top bar
				return;
			}
			g_topTab = 1;
			LayoutTab(v, st);
			ImGui::EndTabItem();
			if (ImGui::BeginTabItem((std::string(TR("HPM_TabCombined", "Combined widgets")) + "##tabcombined").c_str(), nullptr, TopFlags(2))) {
				CombinedTab(v, st);
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}

	OpenRange LastOpenRange()
	{
		std::scoped_lock l(g_rangeLock);
		return g_lastRange;
	}

	void Register()
	{
		if (!AMF::IsInstalled()) {
			logger::info("Apocrypha Menu Framework not installed; HUD Position Manager positions the HUD from its INI and has no page");
			return;
		}
		if (AMF::RegisterPage(kModName, "Settings", &Draw)) {
			logger::info("AMF {}: the {} settings page is registered", AMF::Version(), kModName);
		} else {
			logger::warn("AMF refused the {} page", kModName);
		}
	}

	int PutAllOnToggle(bool a_on)
	{
		// what stays as it is: what is there to be read the moment it appears (the crosshair, the sneak eye, notifications,
		// subtitles, prompts, the location name, the enemy's health) and the widgets that already show only when they matter
		static const std::vector<std::string> kKeep{ "Crosshair", "StealthMeter", "Subtitles", "Messages", "QuestUpdate", "ActivatePrompt",
			"LocationText", "LevelUp", "AnimLetters", "EnemyHealth", "QuestMarker", "Breath", "CastingBar", "BowDraw", "ShoutCharge",
			"Detection", "ShoutCooldown", "InfoEffects" };
		const auto& els = hud::Elements();
		int         n = 0;
		settings::Update([&](settings::Snapshot& s) {
			for (std::size_t i = 0; i < els.size() && i < s.elements.size(); ++i) {
				auto& e = s.elements[i];
				if (a_on && e.show == 0 && std::ranges::find(kKeep, std::string(els[i].key)) == kKeep.end()) { e.show = 3; ++n; }
				if (!a_on && e.show == 3) { e.show = 0; ++n; }
			}
		});
		return n;
	}
}
