#pragma once

#include <string>

namespace RE
{
	class GFxMovieView;
}

// The widgets HUD Position Manager BUILDS (phase 2, 2026-10-04 - the owner: "add any widgets that Skyrim doesn't already
// have to our mod so that it can create the widget and also manage it and its appearance and when it becomes visible").
//
// Each is an element of the table (hud::Element::swf set) whose art is its own SWF - Interface\HUDPositionManager\
// widgets\<file>.swf, reskinnable, the clip contract in tools/swfgen.py - loaded into the HUD movie the way SkyUI loads
// its widgets: a HOLDER clip HPM_<key> under HUDMovieBaseInstance (position, size, the HUD's mode flags, registered in
// HudElements as Dragon's Eye Minimap's compass ring is, so the game hides it in menus like a vanilla element) and the
// SWF loaded into its child "widget" (loadMovie replaces the child, never the holder). The positioner moves the holder
// like any other HUD clip. Everything here runs on the MAIN thread from the HUD hook; data is read a few times a second
// and a clip is written only when its value changed.
namespace widgets
{
	void Tick(RE::GFxMovieView* a_hud, unsigned long long a_frame);

	// The game's state for reading: false from kPreLoadGame until kPostLoadGame / kNewGame. No widget reads game data
	// while false, nor while the Loading Menu is open - the HUD advances during a load screen, and a save that is still
	// rebuilding the player crashed GetGoldAmount (crash-2026-10-04-11-46-31, found by the primary session).
	void SetGameReady(bool a_ready);

	// DevBench: per built widget - created, registered, loaded, shown, the value it shows; a_force >= 0 holds a widget's
	// value (0..1) and shows it, for testing without the situation it reports (-1 = live again)
	std::string StateJson();
	bool        Force(const std::string& a_key, float a_value);
	std::string LoadUrl(const std::string& a_key, const std::string& a_url);   // test: load another SWF into a widget
}
