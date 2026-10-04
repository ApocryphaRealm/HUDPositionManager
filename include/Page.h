#pragma once

#include <string>

// The "HUD Position Manager" page on the Apocrypha Menu Framework (AMF.h, Dear ImGui 1.90.8): Presets, Layout (the
// switches and a tab per element) and Combined widgets - the Oblivion Remastered version's page (2026-10-04). Drawn on
// AMF's render thread: it reads positioner::GetState() and changes settings through settings::Update().
namespace page
{
	inline constexpr const char* kModName = "HUD Position Manager";
	void Register();

	// The move sliders' range on the element tab the page last drew, in percent of the screen - what Free placement
	// changes (DevBench hud.position op range)
	struct OpenRange
	{
		std::string element;
		double      minX = 0, maxX = 0, minY = 0, maxY = 0;
		bool        unlocked = false, valid = false;
	};
	OpenRange LastOpenRange();
}
