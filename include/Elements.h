#pragma once

// ============================================================================================
// The HUD elements HUD Position Manager moves, scales and hides. Each element is one or more
// clips of the live HUD movie, named by their path under _root.HUDMovieBaseInstance - the
// vanilla hudmenu.swf's instance names, which HUD replacers keep. Every part of an element is
// moved by the same offset; a part the running HUD does not have is simply reported "not in
// this HUD" and skipped, so a table entry never breaks a HUD that lacks it.
//
// Clean room (the owner's licence gate, 4. plans\skyhud-conversion\feasibility-2026-09-05.md):
// the names come from the vanilla HUD and from inspecting the RUNNING movie's display list
// (the "clips" op of the hud.position DevBench tool) - never from another mod's files.
// ============================================================================================

#include <string>
#include <vector>

namespace hud
{
	struct Element
	{
		const char*              key;    // INI section and stable id ("Health"); never translated
		const char*              name;   // English tab label; translated through HPM_El_<key>
		std::vector<const char*> parts;  // clip paths under _root.HUDMovieBaseInstance
	};

	// Built once; the order here is the tab order and the settings order.
	const std::vector<Element>& Elements();

	// The element's index by key, or -1.
	int IndexOf(const std::string& a_key);
}
