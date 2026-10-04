#pragma once

#include <cstdint>
#include <string>

// Phase 3, ImmersiveHUD parity, build 2 (PHASE3-IHUD-PLAN.md): the HUD toggle. A key (keyboard) and a button
// (controller) show and hide every element whose Show is "Follow the HUD toggle" - press to toggle, or hold while the
// key is down (hold mode), or show for a few seconds (display duration). Off unless [Immersive] bEnabled=1.
//
// Threads: input events arrive on the main thread (an InputEvent sink); the positioner's Tick (main thread, the HUD's
// AdvanceMovie) asks Shown() once a frame and hands the settings over with Sync(); the page (render thread) arms and
// reads a binding capture through atomics.
namespace immersive
{
	// at kDataLoaded: the input sink (and, once, whether ImmersiveHUD itself is loaded)
	void Register();

	// a save loaded or a new game: the toggle starts as [Immersive] bStartVisible says
	void OnGameLoaded();

	// once a frame from Tick: the settings it needs (copied into atomics, so the input sink never touches the snapshot)
	void Sync(bool a_enabled, int a_key, int a_button, bool a_hold, float a_seconds, bool a_startVisible);

	// once a frame from Tick: is the HUD toggled on now? a_dt advances the display-duration timer, only in play
	bool Shown(float a_dt, bool a_gameplay);

	// ImmersiveHUD.dll is loaded: both would drive the same clips' alpha (the page says so)
	bool IhudPresent();

	// ---- the page's press-to-bind rows (DEM's and Wheeler's pattern): arm, then the next press is the binding
	enum class Bind : int { kNone = 0, kKeyboard = 1, kGamepad = 2 };
	void        Arm(Bind a_target);
	Bind        Armed();
	void        PageDrawn();   // the page drew this frame - an arm with the page closed is dropped, never eating a key
	std::string TakeStatus();  // a message for the page (a reserved key refused), once
	// a capture finished: the code bound (-1 none yet); the page writes it into the settings
	int         TakeCaptured(Bind& a_target);

	// names for the page: a DirectInput scan code / an XInput button mask
	std::string KeyName(int a_code);
	std::string ButtonName(int a_mask);

	// DevBench: press (a_down true) / release the toggle as the key would; state as JSON
	void        Simulate(bool a_down);
	std::string StateJson();
}
