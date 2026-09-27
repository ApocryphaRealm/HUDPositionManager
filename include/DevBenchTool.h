#pragma once

// The "hud.position" DevBench tool (rules 31 and 64): read the live state, set an element's offset,
// scale or hide, save, and list the running HUD movie's clips - so everything the page does can be
// driven and checked headlessly.
namespace DevBenchTool
{
	void Init(bool a_lastAttempt = false);
}
