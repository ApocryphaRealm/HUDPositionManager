#pragma once

#include "utils/Logger.h"

#include <string>
#include <vector>

// HUDPositionManager.ini. The settings page (render thread) and the HUD hook (main thread) share
// them through a snapshot under a lock: the page edits a copy and publishes it, the hook reads a
// copy each frame. Saving is debounced - a change is written a moment after the last edit - with
// ordinary file I/O that rewrites only this mod's keys in place, so comments survive (rule 16).
namespace settings
{
	struct ElementSetting
	{
		float offsetX = 0.0F;  // HUD units (the HUD movie's 1280x720 stage), added to where the HUD puts it
		float offsetY = 0.0F;
		float scale = 1.0F;    // times the size the HUD gives it, about the element's centre
		bool  hide = false;
		int   follow = -1;     // moves with another element (its index; -1 = on its own): a widget beside a bar follows the bar

		bool IsDefault() const { return offsetX == 0.0F && offsetY == 0.0F && scale == 1.0F && !hide; }
	};

	struct Snapshot
	{
		bool                        enabled = true;    // bEnabled:General - off puts every element back
		// The owner, 2026-09-27: "toggles ... to account for whether they have a UI mod that links all of these bars
		// and HUD widgets together so that they all move as one or separately". Each applies (on) or removes (off)
		// the element table's default "Move with" for its group; the per-tab "Move with" stays for anything custom.
		bool                        linkBars = true;     // bLinkBars:General - Magicka and Stamina move with Health
		bool                        linkWidgets = true;  // bLinkWidgets:General - widgets placed around the bars move with them
		std::vector<ElementSetting> elements;          // in hud::Elements() order
	};

	// An element's shipped defaults (its "Move with" comes from the element table, when its group's link is on).
	ElementSetting DefaultFor(std::size_t a_index, bool a_linkBars = true, bool a_linkWidgets = true);

	// Apply a group link to a snapshot: every element of the group (HUD bars / widgets with a default "Move with")
	// gets its table default when a_on, or moves on its own when off.
	void ApplyLink(Snapshot& a_s, bool a_widgets, bool a_on);

	void Init(const std::string& a_iniFileName);
	const std::string& GetIniPath();

	Snapshot Get();                       // a copy, any thread
	void     Publish(const Snapshot& a);  // any thread; marks the settings for a save
	void     MaybeSave();                 // main thread, every frame: writes once the edits have settled
	bool     Save();                      // writes now

	namespace debug
	{
		inline logger::level logLevel = logger::level::info;  // ships at info (uLogLevel=2), matching the INI
	}
}
