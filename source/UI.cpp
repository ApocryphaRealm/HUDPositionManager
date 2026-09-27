#include "UI.h"

#include "Elements.h"
#include "Positioner.h"
#include "SKSEMenuFramework.h"
#include "Settings.h"
#include "utils/Logger.h"
#include "utils/Strings.h"
#include "utils/Toggle.h"

#include <string>
#include <vector>

namespace UI
{
	namespace
	{
		constexpr float kStageW = 1280.0F;   // the HUD movie's stage, the space the element boxes are measured in
		constexpr float kStageH = 720.0F;

		std::string ElementLabel(const hud::Element& a_el)
		{
			return strings::TR((std::string("HPM_El_") + a_el.key).c_str(), a_el.name);
		}

		// The outline of the element being edited, drawn over everything (render thread: it only reads the
		// box the HUD hook measured on the main thread).
		void DrawHighlight(int a_index, const positioner::State& a_state)
		{
			if (a_index < 0 || a_index >= static_cast<int>(a_state.elements.size())) { return; }
			const auto& e = a_state.elements[static_cast<std::size_t>(a_index)];
			if (!e.hasBounds) { return; }
			ImGuiMCP::ImGuiIO*    io = ImGuiMCP::GetIO();
			ImGuiMCP::ImDrawList* dl = ImGuiMCP::GetForegroundDrawList();
			if (!io || !dl) { return; }
			const float sx = io->DisplaySize.x / kStageW, sy = io->DisplaySize.y / kStageH;
			const ImGuiMCP::ImVec2 a{ e.xMin * sx - 3.0F, e.yMin * sy - 3.0F };
			const ImGuiMCP::ImVec2 b{ e.xMax * sx + 3.0F, e.yMax * sy + 3.0F };
			ImGuiMCP::ImDrawListManager::AddRectFilled(dl, a, b, IM_COL32(255, 210, 64, 36), 3.0F, 0);
			ImGuiMCP::ImDrawListManager::AddRect(dl, a, b, IM_COL32(255, 210, 64, 230), 3.0F, 0, 2.0F);
		}

		bool RenderElement(std::size_t a_i, settings::ElementSetting& a_e, const positioner::State& a_state)
		{
			bool changed = false;
			const auto* es = a_i < a_state.elements.size() ? &a_state.elements[a_i] : nullptr;
			const bool widget = hud::IsWidget(hud::Elements()[a_i]);
			if (!a_state.hudSeen) {
				ImGuiMCP::TextDisabled("%s", strings::TR("HPM_NoHud", "The HUD has not been shown yet - load a game to see this element."));
			} else if (widget && (!es || !es->menuOpen)) {
				ImGuiMCP::TextWrapped("%s", strings::TR("HPM_WidgetClosed", "This widget is not showing: the mod that adds it is not installed, or has it switched off. Its settings are kept."));
			} else if (!es || es->partsFound == 0) {
				ImGuiMCP::TextWrapped("%s", strings::TR("HPM_NotFound", "Not found in your HUD: the HUD you use may not have this element, or names it differently. Its settings are kept but do nothing."));
			} else {
				ImGuiMCP::TextDisabled("%s", strings::TR("HPM_Found", "In your HUD - changes show at once."));
			}
			const std::string id = std::string("##") + hud::Elements()[a_i].key;
			changed |= ImGuiMCP::SliderFloat((std::string(strings::TR("HPM_MoveX", "Move left / right")) + id + "x").c_str(), &a_e.offsetX, -640.0F, 640.0F, "%.0f");
			changed |= ImGuiMCP::SliderFloat((std::string(strings::TR("HPM_MoveY", "Move up / down")) + id + "y").c_str(), &a_e.offsetY, -360.0F, 360.0F, "%.0f");
			changed |= ImGuiMCP::SliderFloat((std::string(strings::TR("HPM_Size", "Size")) + id + "s").c_str(), &a_e.scale, 0.25F, 3.0F, "%.2fx");
			changed |= ImGuiMCP::Toggle((std::string(strings::TR("HPM_Hide", "Hide")) + id + "h").c_str(), &a_e.hide);
			// "Move with": another element whose offset this one also takes - a widget beside a bar follows the bar
			{
				const auto& els = hud::Elements();
				std::vector<std::string> labels{ strings::TR("HPM_MoveWithNone", "Nothing - on its own") };
				std::vector<int>         index{ -1 };
				for (std::size_t j = 0; j < els.size(); ++j) {
					if (j == a_i || hud::IsWidget(els[j])) { continue; }   // follow a HUD element, never itself or another widget
					labels.push_back(ElementLabel(els[j]));
					index.push_back(static_cast<int>(j));
				}
				int current = 0;
				for (std::size_t k = 0; k < index.size(); ++k) {
					if (index[k] == a_e.follow) { current = static_cast<int>(k); }
				}
				std::vector<const char*> ptrs;
				for (const auto& l : labels) { ptrs.push_back(l.c_str()); }
				if (ImGuiMCP::Combo((std::string(strings::TR("HPM_MoveWith", "Move with")) + id + "f").c_str(), &current, ptrs.data(), static_cast<int>(ptrs.size()))) {
					a_e.follow = index[static_cast<std::size_t>(current)];
					changed = true;
				}
			}
			if (ImGuiMCP::Button((std::string(strings::TR("HPM_ResetOne", "Reset this element")) + id + "r").c_str())) {
				a_e = settings::ElementSetting{};
				changed = true;
			}
			if (a_e.scale < 0.25F) { a_e.scale = 0.25F; }   // a typed 0 would make it vanish; Hide is the way to do that
			return changed;
		}
	}

	void __stdcall RenderPage();

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			logger::info("Apocrypha Menu Framework not installed; HUD Position Manager positions the HUD from its INI and has no page");
			return;
		}
		SKSEMenuFramework::SetSection("HUD Position Manager");
		SKSEMenuFramework::AddSectionItem("HUD elements", RenderPage);
		logger::info("Registered the HUD Position Manager page");
	}

	void __stdcall RenderPage()
	{
		strings::Tick();
		settings::Snapshot s = settings::Get();
		const positioner::State state = positioner::GetState();
		bool changed = false;

		ImGuiMCP::TextWrapped("%s", strings::TR("HPM_Intro", "Move, resize or hide each part of the HUD. Changes show in the HUD at once and are saved automatically."));
		changed |= ImGuiMCP::Toggle(strings::TR("HPM_Enabled", "Apply my layout"), &s.enabled);
		ImGuiMCP::SameLine(0.0F, 8.0F);
		ImGuiMCP::TextDisabled("%s", strings::TR("HPM_EnabledHint", "off: every element back where the HUD puts it"));
		changed |= ImGuiMCP::Toggle(strings::TR("HPM_Highlight", "Outline the element being edited"), &s.highlight);

		int selected = -1;
		if (ImGuiMCP::BeginTabBar("HudPositionElements", ImGuiMCP::ImGuiTabBarFlags_FittingPolicyScroll | ImGuiMCP::ImGuiTabBarFlags_TabListPopupButton)) {
			const auto& els = hud::Elements();
			for (std::size_t i = 0; i < els.size() && i < s.elements.size(); ++i) {
				// "###key" pins the tab's id to the element, so a language switch keeps the selected tab
				const std::string tab = ElementLabel(els[i]) + "###" + els[i].key;
				if (ImGuiMCP::BeginTabItem(tab.c_str())) {
					selected = static_cast<int>(i);
					changed |= RenderElement(i, s.elements[i], state);
					ImGuiMCP::EndTabItem();
				}
			}
			ImGuiMCP::EndTabBar();
		}
		positioner::SetSelected(selected);
		if (s.highlight) {
			DrawHighlight(selected, state);
		}

		ImGuiMCP::SeparatorText("");
		if (ImGuiMCP::Button(strings::TR("HPM_ResetAll", "Reset every element"))) {
			for (auto& e : s.elements) { e = settings::ElementSetting{}; }
			changed = true;
		}
		if (changed) {
			settings::Publish(s);   // the HUD hook applies it on the next frame and saves once the edits settle
		}
	}
}
