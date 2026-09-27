#pragma once

#include <string>
#include <vector>

namespace RE
{
	class HUDMenu;
}

// The part that touches the live HUD movie. Everything here runs on the MAIN thread, from the
// HUDMenu::AdvanceMovie hook (the logic library, 2026-08-29: a draw callback runs on the render
// thread and must never reach into RE::UI or a movie). The settings page and the DevBench tool
// only ever see the snapshots it leaves behind.
namespace positioner
{
	// Every frame the HUD advances: apply the settings to each element's clips, keep the highlight
	// box current, answer a pending clip-listing request, save settings that have settled.
	void Tick(RE::HUDMenu* a_hud);

	// What the page draws and the tool reports: per element, whether the running HUD has it and
	// where it sits (in the HUD movie's _root space, the 1280x720 stage).
	struct ElementState
	{
		int   partsFound = 0;
		int   partsTotal = 0;
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

	// The element whose tab is open on the settings page (-1 = none): its box is measured for the
	// highlight. Set from the render thread; read on the main thread.
	void SetSelected(int a_index);

	// A listing of the HUD movie's clips under _root.HUDMovieBaseInstance (depth 1, or 2), as JSON:
	// the research op for mapping a HUD's element names. Asked from any thread; answered on the
	// main thread within the next frames; "" when the HUD did not advance in time.
	std::string ListClips(int a_depth, int a_timeoutMs);
}
