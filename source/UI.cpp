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

		std::string ElementLabel(const hud::Element& a_el)
		{
			return strings::TR((std::string("HPM_El_") + a_el.key).c_str(), a_el.name);
		}

		bool RenderElement(std::size_t a_i, settings::ElementSetting& a_e, const positioner::State& a_state, bool a_linkBars, bool a_linkWidgets)
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
				a_e = settings::DefaultFor(a_i, a_linkBars, a_linkWidgets);
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

	// A toggle's explanation, on its own indented line under the toggle - beside it, AMF's theme makes it read as part
	// of the label
	void Hint(const char* a_fmt, const char* a_text)
	{
		ImGuiMCP::Indent();
		ImGuiMCP::TextDisabled(a_fmt, a_text);
		ImGuiMCP::Unindent();
	}

	void __stdcall RenderPage()
	{
		strings::Tick();
		settings::Snapshot s = settings::Get();
		const positioner::State state = positioner::GetState();
		bool changed = false;

		ImGuiMCP::TextWrapped("%s", strings::TR("HPM_Intro", "Move, resize or hide each part of the HUD. Changes show in the HUD at once and are saved automatically."));
		changed |= ImGuiMCP::Toggle(strings::TR("HPM_Enabled", "Apply my layout"), &s.enabled);
		Hint("%s", strings::TR("HPM_EnabledHint", "off: every element back where the HUD puts it"));
		// Whether the user's UI mod links the bars and the widgets around them (Norden UI does): on moves them as one
		if (ImGuiMCP::Toggle(strings::TR("HPM_LinkBars", "Move the three bars together"), &s.linkBars)) {
			settings::ApplyLink(s, false, s.linkBars);
			changed = true;
		}
		Hint("%s", strings::TR("HPM_LinkBarsHint", "Magicka and Stamina move with Health"));
		if (ImGuiMCP::Toggle(strings::TR("HPM_LinkWidgets", "Widgets around the bars move with them"), &s.linkWidgets)) {
			settings::ApplyLink(s, true, s.linkWidgets);
			changed = true;
		}
		Hint("%s", strings::TR("HPM_LinkWidgetsHint", "for a UI that places widgets around the bars, like Norden UI"));

		if (ImGuiMCP::BeginTabBar("HudPositionElements", ImGuiMCP::ImGuiTabBarFlags_FittingPolicyScroll | ImGuiMCP::ImGuiTabBarFlags_TabListPopupButton)) {
			const auto& els = hud::Elements();
			for (std::size_t i = 0; i < els.size() && i < s.elements.size(); ++i) {
				// "###key" pins the tab's id to the element, so a language switch keeps the selected tab
				const std::string tab = ElementLabel(els[i]) + "###" + els[i].key;
				if (ImGuiMCP::BeginTabItem(tab.c_str())) {
					changed |= RenderElement(i, s.elements[i], state, s.linkBars, s.linkWidgets);
					ImGuiMCP::EndTabItem();
				}
			}
			ImGuiMCP::EndTabBar();
		}

		ImGuiMCP::SeparatorText("");
		if (ImGuiMCP::Button(strings::TR("HPM_ResetAll", "Reset every element"))) {
			for (std::size_t i = 0; i < s.elements.size(); ++i) { s.elements[i] = settings::DefaultFor(i, s.linkBars, s.linkWidgets); }
			changed = true;
		}
		if (changed) {
			settings::Publish(s);   // the HUD hook applies it on the next frame and saves once the edits settle
		}
	}
}
