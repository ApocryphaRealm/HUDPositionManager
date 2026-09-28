#pragma once

#include <string>
#include <vector>

namespace RE
{
	class HUDMenu;
}

// The part that touches the live movies. Everything here runs on the MAIN thread, from the
// HUDMenu::AdvanceMovie hook (the logic library, 2026-08-29: a draw callback runs on the render
// thread and must never reach into RE::UI or a movie). The settings page and the DevBench tool
// only ever see the snapshots it leaves behind.
namespace positioner
{
	// Every frame the HUD advances: apply the settings to each element's clips (HUD elements and other
	// mods' widget menus alike), measure element boxes for the DevBench tool, answer a pending clip-listing request,
	// save settings that have settled.
	void Tick(RE::HUDMenu* a_hud);

	// What the page draws and the tool reports: per element, whether the running game has it and where
	// it sits (in its movie's _root space - the 1280x720 stage for the HUD).
	struct ElementState
	{
		int   partsFound = 0;
		int   partsTotal = 0;
		bool  menuOpen = false;   // a widget: its menu is open (a HUD element: the HUD is)
		bool  hasBounds = false;
		float xMin = 0, yMin = 0, xMax = 0, yMax = 0;
	};
	struct State
	{
		bool                      hudSeen = false;   // the hook has run at least once
		unsigned long long        frames = 0;
		std::vector<ElementState> elements;           // in hud::Elements() order
	};
	State GetState();


	// A listing of a movie's clips as JSON: the HUD's under _root.HUDMovieBaseInstance (a_menu empty), or
	// any open menu's under _root (a_menu = its name) - the research op for mapping element names. Asked
	// from any thread; answered on the main thread within the next frames; "" when nothing answered in time.
	std::string ListClips(const std::string& a_menu, int a_depth, int a_timeoutMs);
}
