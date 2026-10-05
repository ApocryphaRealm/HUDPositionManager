#include "Discovery.h"

#include "Elements.h"
#include "utils/Logger.h"

#include <RE/Skyrim.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <format>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace discovery
{
	namespace
	{
		struct Found
		{
			std::string kind;     // "skyui" | "hud" | "menu"
			std::string key;      // stable across sessions: the SWF (skyui), name + SWF (hud), menu name (menu)
			std::string where;    // the clip path now, or the menu name
			std::string source;   // the SWF it loaded, under Interface\ (lower case, decoded)
			bool        known = false;   // already a hand-named element (moved today): listed, never taken twice
		};

		std::mutex          g_lock;
		std::vector<Found>  g_found;
		std::atomic<bool>   g_rescan{ false };
		unsigned long long  g_lastScan = 0, g_movieSeen = 0;
		RE::GFxMovieView*   g_movie = nullptr;   // only compared, never used: the positioner holds the movie

		std::string Lower(std::string a_s)
		{
			for (auto& c : a_s) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
			return a_s;
		}

		// a movie URL as a key: percent-decoded, lower case, slashes forward, from "interface/" on (a full path and a
		// relative one name the same file)
		std::string Source(std::string a_url)
		{
			std::string out;
			for (std::size_t i = 0; i < a_url.size(); ++i) {
				if (a_url[i] == '%' && i + 2 < a_url.size() && std::isxdigit(static_cast<unsigned char>(a_url[i + 1])) && std::isxdigit(static_cast<unsigned char>(a_url[i + 2]))) {
					out += static_cast<char>(std::stoi(a_url.substr(i + 1, 2), nullptr, 16));
					i += 2;
				} else {
					out += a_url[i] == '\\' ? '/' : a_url[i];
				}
			}
			out = Lower(out);
			if (const auto at = out.find("interface/"); at != std::string::npos) { out = out.substr(at); }
			return out;
		}

		std::string UrlOf(const RE::GFxValue& a_clip)
		{
			RE::GFxValue u;
			return const_cast<RE::GFxValue&>(a_clip).GetMember("_url", &u) && u.IsString() ? std::string(u.GetString()) : std::string{};
		}

		// the vanilla menus, and our own tools' windows, are never widgets - whatever their flags say
		bool Denied(std::string_view a_name)
		{
			static const std::set<std::string, std::less<>> kDeny{
				"HUD Menu", "Console", "Console Native UI Menu", "InventoryMenu", "ContainerMenu", "BarterMenu", "GiftMenu",
				"Crafting Menu", "Dialogue Menu", "FavoritesMenu", "Journal Menu", "Lockpicking Menu", "LevelUp Menu",
				"Loading Menu", "MessageBoxMenu", "RaceSex Menu", "Sleep/Wait Menu", "Training Menu", "Tutorial Menu",
				"Credits Menu", "Mod Manager Menu", "Marketplace Menu", "Fader Menu", "Cursor Menu", "Kinect Menu",
				"Mist Menu", "TitleSequence Menu", "Main Menu", "Book Menu", "MapMenu", "StatsMenu", "TweenMenu",
				"MagicMenu", "Quantity Menu", "CreationClub Menu", "SafeZoneMenu", "Top Menu", "Overlay Menu",
				"Overlay Interaction Menu", "LoadWaitSpinner", "StreamingInstallMenu", "PluginExplorerMenu",
				"Bethesda.net Login Menu", "Debug Text Menu", "CustomMenu", "Hud", "TestBench", "DevBench",
			};
			if (kDeny.contains(a_name)) { return true; }
			const std::string l = Lower(std::string(a_name));
			return l.find("apocrypha") != std::string::npos || l.find("amf") == 0 || l.find("testbench") != std::string::npos ||
			       l.find("devbench") != std::string::npos || l.find("hudpositionmanager") != std::string::npos;
		}

		// a widget menu by its flags (ImmersiveHUD's test): open in play, and none of the flags of a real menu
		bool OverlayFlags(const RE::IMenu& a_menu)
		{
			using F = RE::IMenu::Flag;
			return !a_menu.menuFlags.any(F::kPausesGame, F::kUsesCursor, F::kUsesMenuContext, F::kModal, F::kFreezeFrameBackground,
				F::kFreezeFramePause, F::kUpdateUsesCursor, F::kApplicationMenu, F::kInventoryItemMenu);
		}

		void Scan(RE::GFxMovieView* a_hud)
		{
			std::vector<Found> out;
			const auto&        els = hud::Elements();
			std::set<std::string, std::less<>> namedMenus, namedClips;
			for (const auto& e : els) {
				if (e.menu) { namedMenus.insert(e.menu); }
				else {
					for (const char* p : e.parts) {
						std::string first = p;
						if (const auto dot = first.find('.'); dot != std::string::npos) { first = first.substr(0, dot); }
						namedClips.insert(first);
					}
				}
			}
			// the vanilla HUD's own alternates and inner parts, which a HUD replacer can re-parent straight under
			// HUDMovieBaseInstance and load from its own file (InfinityUI's compass pieces, 2026-10-04 in Njordlinger Test:
			// CompassCard, CompassFrame, their Alts, CompassRect - listed as widgets by the first D1). They belong to the
			// vanilla elements (the names ImmersiveHUD's DLL lists as parts of the compass, shout meter, bars and charge
			// meters), never to a discovered widget.
			for (const char* v : { "CompassCard", "CompassCardAlt", "CompassFrame", "CompassFrameAlt", "CompassRect", "CompassMask_mc",
					 "DirectionRect", "Compass", "CompassShoutMeterHolder", "ShoutMeterBarAlt", "ShoutWarningInstance",
					 "ShoutWarningInstanceAlt", "ShoutMeterInstance", "EnemyHealthMeter", "HealthMeterLeft", "MagickaMeter",
					 "StaminaMeter", "LeftChargeMeter", "RightChargeMeter", "ChargeMeterBaseAlt", "BottomLeftLockInstance",
					 "BottomRightLockInstance", "SneakAnimInstance", "SneakTextHolder" }) {
				namedClips.insert(v);
			}
			const std::string hudUrl = Source(a_hud->GetMovieDef() ? a_hud->GetMovieDef()->GetFileURL() : "");

			// SkyUI's widgets: each slot of _root.WidgetContainer, keyed by the SWF it loaded
			RE::GFxValue container;
			if (a_hud->GetVariable(&container, "_root.WidgetContainer") && container.IsDisplayObject()) {
				container.VisitMembers([&](const char* a_name, const RE::GFxValue& a_val) {
					if (!a_name || !a_val.IsDisplayObject()) { return; }
					const std::string src = Source(UrlOf(a_val));
					if (src.empty() || src == hudUrl) { return; }   // an empty slot, or one not loaded from its own file
					out.push_back({ "skyui", src, std::string("_root.WidgetContainer.") + a_name, src, false });
				});
			}
			// clips another mod added to the HUD: a child of HUDMovieBaseInstance loaded from a file of its own
			RE::GFxValue base;
			if (a_hud->GetVariable(&base, "_root.HUDMovieBaseInstance") && base.IsDisplayObject()) {
				base.VisitMembers([&](const char* a_name, const RE::GFxValue& a_val) {
					if (!a_name || !a_val.IsDisplayObject()) { return; }
					const std::string name = a_name;
					if (name.starts_with("HPM_") || namedClips.contains(name)) { return; }
					const std::string src = Source(UrlOf(a_val));
					if (src.empty() || src == hudUrl) { return; }   // the HUD's own clips share its file
					out.push_back({ "hud", name + "|" + src, "_root.HUDMovieBaseInstance." + name, src, false });
				});
			}
			// overlay menus: open now, flags of a widget, not denied
			if (auto* ui = RE::UI::GetSingleton()) {
				for (const auto& [name, entry] : ui->menuMap) {
					const auto& menu = entry.menu;
					if (!menu || !menu->uiMovie || !name.c_str() || Denied(name.c_str())) { continue; }
					if (!ui->IsMenuOpen(name) || !OverlayFlags(*menu)) { continue; }
					auto* def = menu->uiMovie->GetMovieDef();
					const std::string src = Source(def ? def->GetFileURL() : "");
					Found f{ "menu", name.c_str(), name.c_str(), src, namedMenus.contains(name.c_str()) };
					out.push_back(std::move(f));
				}
			}
			std::ranges::sort(out, [](const Found& a, const Found& b) { return a.kind != b.kind ? a.kind < b.kind : a.key < b.key; });
			std::lock_guard l(g_lock);
			if (out.size() != g_found.size() || !std::ranges::equal(out, g_found, [](const Found& a, const Found& b) { return a.key == b.key && a.where == b.where; })) {
				logger::info("discovery: {} found ({} SkyUI, {} HUD clips, {} overlay menus)", out.size(),
					std::ranges::count(out, std::string("skyui"), &Found::kind), std::ranges::count(out, std::string("hud"), &Found::kind),
					std::ranges::count(out, std::string("menu"), &Found::kind));
				for (const auto& f : out) {
					logger::info("discovery:   {} {} at {} [{}]{}", f.kind, f.key, f.where, f.source, f.known ? " - already an element" : "");
				}
			}
			g_found = std::move(out);
		}
	}

	void Tick(RE::GFxMovieView* a_hud, unsigned long long a_frame, bool a_ready)
	{
		if (!a_hud) { return; }
		if (a_hud != g_movie) {
			g_movie = a_hud;
			g_movieSeen = a_frame;
		}
		if (!a_ready) { return; }
		auto* ui = RE::UI::GetSingleton();
		if (!ui || ui->GameIsPaused() || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) { return; }
		const bool due = (a_frame - g_movieSeen > 300) && (a_frame - g_lastScan >= 300 || g_lastScan == 0);
		if (!due && !g_rescan.exchange(false)) { return; }
		g_lastScan = a_frame;
		Scan(a_hud);
	}

	std::string StateJson()
	{
		std::lock_guard l(g_lock);
		auto esc = [](const std::string& a_s) {
			std::string o;
			for (const char c : a_s) {
				if (c == '"' || c == '\\') { o += '\\'; }
				if (static_cast<unsigned char>(c) >= 0x20) { o += c; }
			}
			return o;
		};
		std::string out = "[";
		for (const auto& f : g_found) {
			out += std::format(R"({}{{"kind":"{}","key":"{}","where":"{}","source":"{}","known":{}}})", out.size() > 1 ? "," : "", f.kind, esc(f.key), esc(f.where),
				esc(f.source), f.known);
		}
		return out + "]";
	}

	void Rescan() { g_rescan = true; }
}
