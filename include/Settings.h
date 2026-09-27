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
		bool                        highlight = true;  // bHighlight:General - outline the element being edited
		std::vector<ElementSetting> elements;          // in hud::Elements() order
	};

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
