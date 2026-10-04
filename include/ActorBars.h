#pragma once

#include <RE/Skyrim.h>

#include <string>

// Phase 4 build 2 (TrueHUD parity, PHASE4-TRUEHUD-PLAN.md section 2.2): health bars over the characters around the
// player - enemies in a fight with you, followers while you fight, anyone you hit. A pool of clips in the HUD movie
// (HPM_IB0..n, each loading Interface\HUDPositionManager\widgets\infobar.swf - the clip contract: Frame, Fill, Phantom,
// Value the name, Value2 the level), registered in HudElements so the HUD's own modes hide them in menus. Off unless
// [InfoBars] bEnabled=1. Main thread only (the HUD's AdvanceMovie).
namespace actorbars
{
	// at kDataLoaded: the hit sink (who you hit and who hit you)
	void Register();

	// once a frame from the positioner: a_left.. the HUD's visible stage (its own units), a_read the 10-a-second value pass
	void Tick(RE::GFxMovieView* a_hud, unsigned long long a_frame, float a_left, float a_top, float a_width, float a_height, bool a_read);

	// a new HUD movie (a load): every pooled clip is made again
	void Reset();

	// kPreLoadGame (false) / kPostLoadGame, kNewGame (true): no actor is read in between, and the pool is cleared
	void SetReady(bool a_ready);

	// DevBench: the bars in use, and a test pin on the nearest character
	std::string StateJson();
	void        PinNearest(bool a_on);
}
