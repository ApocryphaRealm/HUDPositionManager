#pragma once

// ============================================================================================
// The HUD elements HUD Position Manager moves, scales and hides. Each element is one or more
// clips of a live movie:
//  - a HUD element: clips of the HUD movie, named by their path under _root.HUDMovieBaseInstance -
//    the vanilla hudmenu.swf's instance names, which HUD replacers keep;
//  - a widget: another mod's own widget MENU (STB Widgets' gold, weight, level..., which are not
//    part of the HUD movie at all), moved as a whole by its movie's _root - the widget mod keeps
//    placing its clip inside it, and our offset rides on the root, so the two never fight.
// Every part of an element is moved by the same offset; a part the running game does not have is
// reported "not found" and skipped, and two names that reach the SAME clip are used once.
//
// Clean room (the owner's licence gate, 4. plans\skyhud-conversion\feasibility-2026-09-05.md):
// the names come from the vanilla HUD and from inspecting the RUNNING movies' display lists
// (the "clips" op of the hud.position DevBench tool) - never from another mod's files. The clips added in
// 1.1 (FloatingQuestMarkerInstance, TemperatureMeter_mc, the *Meter alternates) are the names ImmersiveHUD SKSE's
// shipped DLL lists (rule 62: a shipped mod's names are reference for WHAT it reaches, 2026-10-04).
// ============================================================================================

#include <string>
#include <vector>

namespace hud
{
	struct Element
	{
		const char*              key;               // INI section and stable id ("Health"); never translated
		const char*              name;              // English tab label; translated through HPM_El_<key>
		std::vector<const char*> parts;             // HUD: paths under _root.HUDMovieBaseInstance; widget: under _root ("" = _root itself)
		const char*              menu = nullptr;    // nullptr = the HUD movie; else the widget menu's name
		const char*              moveWith = nullptr; // the element it moves with by default (a key), or nullptr
		// 1.1, the Oblivion version's page (2026-10-04)
		bool                     stretch = false;   // Length / Height sliders (a bar, the compass, a meter)
		bool                     fades = false;     // the game fades it on its own: an "Always visible" switch
		bool                     bar = false;       // a resource bar
		// 1.1 phase 2: a widget this mod BUILDS - its art, relative to Data\Interface (widgets.h); its clip is
		// HPM_<key> under HUDMovieBaseInstance, so parts names that holder
		const char*              swf = nullptr;
	};

	// Built once; the order here is the tab order and the settings order.
	const std::vector<Element>& Elements();

	// The element's index by key, or -1.
	int IndexOf(const std::string& a_key);

	// True for a widget from another mod (its own menu), false for a HUD element.
	inline bool IsWidget(const Element& a_el) { return a_el.menu != nullptr; }
}
