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
		float xMin = 0, yMin = 0, xMax = 0, yMax = 0;   // in HUD stage units, as drawn now (our offset and size in it)
		float appliedX = 0, appliedY = 0;               // the offset we applied when the box was measured, HUD units
		bool  hiddenByShow = false;                     // "Show" (in / out of combat) is hiding it right now
		bool  alphaHeld = false;                        // "Always visible" is holding its alpha up right now
	};
	struct State
	{
		bool                      hudSeen = false;   // the hook has run at least once
		unsigned long long        frames = 0;
		std::vector<ElementState> elements;           // in hud::Elements() order
		// the HUD movie's visible stage (GetVisibleFrameRect): the screen, in HUD units - the page's slider ranges
		float                     stageLeft = 0, stageTop = 0, stageW = 1280, stageH = 720;
		bool                      inCombat = false;   // the combat state "Show" used this frame (with its 3 s linger)
	};
	State GetState();

	// test: -1 the game's own combat state, 0 out of combat, 1 in combat (DevBench forceCombat)
	void ForceCombat(int a_state);


	// A listing of a movie's clips as JSON: the HUD's under _root.HUDMovieBaseInstance (a_menu empty), or
	// any open menu's under _root (a_menu = its name) - the research op for mapping element names. Asked
	// from any thread; answered on the main thread within the next frames; "" when nothing answered in time.
	std::string ListClips(const std::string& a_menu, int a_depth, int a_timeoutMs);
}
