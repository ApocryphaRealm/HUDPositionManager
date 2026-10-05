#pragma once

#include <RE/Skyrim.h>

#include <string>

// Phase 4 build 4 (TrueHUD parity, PHASE4-TRUEHUD-PLAN.md section 2.5): floating text - a short text that rises over a
// character and fades. Two sources: the damage the player deals (a hit starts a short watch on the target's health, and
// each loss becomes "-N" over it), and other mods, through the ModEvent HPM_FloatingText (the form the event is sent on
// is the character it rises over - the player when it is not one; strArg the text; numArg the seconds, 0 the setting's).
// A pool of clips in the HUD movie (HPM_FT0..n, each loading Interface\HUDPositionManager\widgets\floattext.swf - the
// clip contract's Value), registered in HudElements so the HUD's own modes hide them in menus, projected through the
// world camera as the info bars are. Off unless [FloatingText] bEnabled=1. Main thread only (the HUD's AdvanceMovie),
// apart from the queue the event sinks fill.
namespace floattext
{
	// at kDataLoaded: the hit sink (the damage watch) and the ModEvent sink (the author API)
	void Register();

	// kPreLoadGame (false) / kPostLoadGame, kNewGame (true): nothing is read or kept across a load
	void SetReady(bool a_ready);

	// once a frame from the positioner: a_left.. the HUD's visible stage (its own units), a_read the 10-a-second value pass
	void Tick(RE::GFxMovieView* a_hud, unsigned long long a_frame, float a_left, float a_top, float a_width, float a_height, bool a_read);

	// DevBench: put a text over a character (a_formId 0 = the player) as the ModEvent would; the texts up now as JSON
	void        Add(RE::FormID a_formId, const std::string& a_text, float a_seconds);
	// DevBench: a hit by the player on a character, queued exactly as the hit sink queues one (the damage watch starts)
	void        SimulateHit(RE::FormID a_formId);
	std::string StateJson();
}
