#pragma once

#include "Positioner.h"
#include "Settings.h"

#include <atomic>
#include <string>
#include <vector>
#include "utils/Logger.h"

// HUDMenu::AdvanceMovie (vtable slot 5) runs every frame the HUD advances, on the MAIN thread -
// where writing to the HUD movie is safe (the Dragon's Eye Minimap pattern). The HUD's own
// ActionScript runs first, inside the original call, so our values are the last ones written
// before the frame is drawn.
namespace hooks
{
	// HUDMenu::ProcessMessage (vtable slot 4): the HUD's notifications arrive here as HUDData. Phase 4 build 4 (recent
	// loot): the game's own "<item> added" lines are matched against its GMSTs (sAddItemtoInventory / sAddItemsToInventory)
	// and dropped only when [RecentLoot] bHideVanillaMessage=1 - by default every one is passed on (the owner, 2026-10-04:
	// "add the hook but dont hide anything"). Counted either way, for DevBench.
	struct HUDMessages
	{
		static inline REL::Relocation<RE::UI_MESSAGE_RESULTS (RE::HUDMenu::*)(RE::UIMessage&)> ProcessMessage;
		static inline std::atomic<int>  seen{ 0 }, hidden{ 0 };
		static inline std::atomic<bool> hide{ false };

		// a GMST like "%s added" / "%s (%i) added" as its literal pieces, matched in order
		static bool Matches(const std::string& a_text, const char* a_gmst)
		{
			auto* gs = RE::GameSettingCollection::GetSingleton();
			auto* st = gs ? gs->GetSetting(a_gmst) : nullptr;
			const char* f = st && st->GetType() == RE::Setting::Type::kString ? st->GetString() : nullptr;
			if (!f || !*f) { return false; }
			std::string fmt = f, piece;
			std::vector<std::string> parts;
			for (std::size_t i = 0; i < fmt.size(); ++i) {
				if (fmt[i] == '%' && i + 1 < fmt.size()) {
					parts.push_back(piece);
					piece.clear();
					++i;
					continue;
				}
				piece += fmt[i];
			}
			parts.push_back(piece);
			std::size_t at = 0;
			for (const auto& p : parts) {
				if (p.empty()) { continue; }
				const auto found = a_text.find(p, at);
				if (found == std::string::npos) { return false; }
				at = found + p.size();
			}
			return true;
		}

		static RE::UI_MESSAGE_RESULTS Thunk(RE::HUDMenu* a_hud, RE::UIMessage& a_message)
		{
			if (auto* data = a_message.data ? skyrim_cast<RE::HUDData*>(a_message.data) : nullptr;
				data && data->type.underlying() == 1 /* a notification, on both build lines */ && data->text.c_str()) {
				const std::string text = data->text.c_str();
				if (Matches(text, "sAddItemtoInventory") || Matches(text, "sAddItemsToInventory")) {
					++seen;
					if (hide.load()) {
						++hidden;
						return RE::UI_MESSAGE_RESULTS::kHandled;
					}
				}
			}
			return ProcessMessage(a_hud, a_message);
		}
	};

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
			static unsigned frames = 0;
			if ((++frames % 60) == 0) { HUDMessages::hide = settings::Get().rl.hideVanilla; }   // once a second
		}
	};

	inline void Install()
	{
		HUDMessages::ProcessMessage = HUDMenu::vTable.write_vfunc(4, &HUDMessages::Thunk);
		logger::info("Hook installed: HUDMenu::ProcessMessage (vfunc 4) - the game's item-added notifications (hidden only with bHideVanillaMessage=1)");
		HUDMenu::AdvanceMovie = HUDMenu::vTable.write_vfunc(5, &HUDMenu::Thunk);
		logger::info("Hook installed: HUDMenu::AdvanceMovie (vfunc 5), vtable at {:#x}", HUDMenu::vTable.address());
	}
}
