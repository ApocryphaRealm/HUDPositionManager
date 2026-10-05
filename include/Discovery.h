#pragma once

#include <string>

namespace RE
{
	class GFxMovieView;
}

// Phase 4 (ImmersiveHUD side), build D1 - discovering other mods' HUD widgets, READ-ONLY (PHASE4-DISCOVERY-PLAN.md). Three
// kinds, as ImmersiveHUD finds them: SkyUI widgets (_root.WidgetContainer.<n>, keyed by the SWF they loaded - the slot
// number shifts between sessions), clips another mod added under HUDMovieBaseInstance (keyed by name + the SWF they
// loaded), and overlay menus (keyed by menu name + their movie's file; a menu counts only by its flags - nothing that
// pauses, takes the cursor or a menu context - and never a vanilla, AMF or TestBench menu). D1 lists what it finds; it
// moves nothing. Main thread only (the HUD's AdvanceMovie), apart from the list DevBench reads.
namespace discovery
{
	// once a frame from the positioner: scans 5 s after a new HUD movie, then every 5 s, in play only
	void Tick(RE::GFxMovieView* a_hud, unsigned long long a_frame, bool a_ready);

	// DevBench: what the last scan found, and a scan on the next HUD frame
	std::string StateJson();
	void        Rescan();

	// D2: a SkyUI widget's slot this session (_root.WidgetContainer.<n>) by the SWF it loaded, "" until a scan has seen it -
	// the positioner resolves a discovered SkyUI element through this (the slot number shifts between sessions)
	std::string SkyuiPath(const std::string& a_source);

	// widgets cached this session that have no tab yet (the page says a restart gives them one)
	int NewSinceStart();
}
