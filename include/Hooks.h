#pragma once

#include "Positioner.h"
#include "utils/Logger.h"

// HUDMenu::AdvanceMovie (vtable slot 5) runs every frame the HUD advances, on the MAIN thread -
// where writing to the HUD movie is safe (the Dragon's Eye Minimap pattern). The HUD's own
// ActionScript runs first, inside the original call, so our values are the last ones written
// before the frame is drawn.
namespace hooks
{
	class HUDMenu
	{
	public:
		static inline REL::Relocation<std::uintptr_t> vTable{ RE::VTABLE_HUDMenu[0] };
		static inline REL::Relocation<void (RE::HUDMenu::*)(float, std::uint32_t)> AdvanceMovie;

		static void Thunk(RE::HUDMenu* a_hud, float a_interval, std::uint32_t a_currentTime)
		{
			AdvanceMovie(a_hud, a_interval, a_currentTime);
			static bool first = true;
			if (first) {
				logger::debug("HUDMenu::AdvanceMovie hook fired for the first time (runs every frame; not logged again)");
				first = false;
			}
			if (a_hud) {
				positioner::Tick(a_hud);
			}
		}
	};

	inline void Install()
	{
		HUDMenu::AdvanceMovie = HUDMenu::vTable.write_vfunc(5, &HUDMenu::Thunk);
		logger::info("Hook installed: HUDMenu::AdvanceMovie (vfunc 5), vtable at {:#x}", HUDMenu::vTable.address());
	}
}
