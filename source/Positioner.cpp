#include "Positioner.h"

#include "Elements.h"
#include "Immersive.h"
#include "ActorBars.h"
#include "FloatText.h"
#include "Discovery.h"
#include "Settings.h"
#include "Widgets.h"
#include "utils/Logger.h"

#include <RE/Skyrim.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <mutex>

namespace positioner
{
	namespace
	{
		// One clip of an element. "base" is where the HUD (or the widget mod) itself puts the clip; our
		// offset and scale ride on top of it. When the owner moves the clip on its own (a charge meter
		// appearing, the compass shifting for the shout meter), the value we read differs from the one we
		// last wrote, and that value becomes the new base - so we follow it instead of fighting it.
		struct Part
		{
			std::string  path;         // full path from _root
			std::string  skyui;        // a discovered SkyUI widget: its SWF - the path is its slot this session, found at each resolve
			RE::GFxValue obj;          // cached handle, re-resolved regularly
			bool         found = false;
			bool         duplicate = false;         // reaches a clip another part already moves: skipped
			bool         haveBase = false;
			double       baseX = 0, baseY = 0, baseXS = 100, baseYS = 100;
			double       centerX = 0, centerY = 0;  // the clip's own centre, in its local units (scale about it)
			double       parentSX = 1, parentSY = 1; // its ancestors' combined scale: an offset in stage units is this much
			                                         // in the parent's (the compass holder is 85%, TrueHUD's player widget 65%)
			bool         touched = false;            // our values are on it now
			double       lastX = 0, lastY = 0, lastXS = 100, lastYS = 100;
			bool         hiddenByUs = false;
			bool         visibleBefore = true;       // what the owner had when we hid it
			// alpha, followed the way position is: the owner's own (the game's bar fade, TrueHUD, a built widget's 0/100) is
			// the base; ours is base x the fade multiplier, written only on change
			bool         alphaTouched = false;
			double       baseAlpha = 100, lastAlpha = 100;
		};

		struct Tracked
		{
			RE::GFxMovieView*   movie = nullptr;     // the movie its parts live in (HUD or the widget's menu)
			std::vector<Part>   parts;
			// Its movie's visible stage (GetVisibleFrameRect): offsets are entered in HUD units and scaled into the
			// movie's own - STB Widgets' movies draw 1.8x larger than the HUD's (measured 2026-09-27: 100 units moved
			// the gold widget 180 px) - and its boxes are scaled back to HUD units for the outline.
			float               left = 0, top = 0, width = 1280, height = 720;
			// Declared after parts ON PURPOSE: a widget menu can close (a loading screen), and holding the menu keeps
			// its movie alive while the parts' clip handles still point into it. Member-wise assignment runs in
			// declaration order, so the old handles are released before the old menu is let go.
			RE::GPtr<RE::IMenu> menuRef;
			// The movie too: holding the menu does not hold its movie. A widget mod that swaps or recreates its menu's
			// movie between our once-a-second checks freed the clips our parts point into - Njordlinger crashed in
			// ApplyPart's GetDisplayInfo during a held spell cast (2026-10-04, crash-2026-10-04-07-49-15, CastingBar menu)
			RE::GPtr<RE::GFxMovieView> movieRef;
			float               fade = 1.0F;         // Show's fader (bFade): 1 shown .. 0 hidden; kept across rebuilds of its parts
		};

		std::vector<Tracked>  g_el;                  // [element]
		RE::GFxMovieView*     g_hudMovie = nullptr;
		unsigned long long    g_frame = 0;
		bool                  g_wasEnabled = true;

		std::mutex       g_stateLock;
		State            g_state;

		// "Show" (in / out of combat): the player's combat flag, read only while some element uses it, held for 3 s after
		// the fight ends so an element does not blink between foes (the Oblivion version's rule, 2026-10-03)
		std::atomic<int>                      g_forceCombat{ -1 };
		// the context modes' tests (DevBench forceContext): -1 the game's own, 0 / 1 forced
		std::atomic<int>                      g_forceInterior{ -1 }, g_forceWeapon{ -1 }, g_forceSneak{ -1 };
		std::atomic<int>                      g_forceAim{ -1 }, g_forceEye{ -1 };   // build 4's tests: aiming 0/1, the eye's frame 1..101

		// Phase 3 build 4 - the author API: another mod hides an element (and gives it back) with the ModEvent
		// "HPM_SetElementHidden" (strArg the element key, numArg 1 hide / 0 show). Not saved: the author asks again after a load.
		std::mutex        g_authorLock;
		std::vector<bool> g_authorHidden;

		class ModEventSink final : public RE::BSTEventSink<SKSE::ModCallbackEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
			{
				if (!a_event) { return RE::BSEventNotifyControl::kContinue; }
				static int seen = 0;
				const bool ours = std::string_view(a_event->eventName.c_str()).starts_with("HPM_");
				if (seen < 5 || ours) {   // the first few ModEvents any mod sends, and every one of ours
					++seen;
					logger::info("author API: ModEvent \"{}\" (\"{}\", {})", a_event->eventName.c_str(), a_event->strArg.c_str(), a_event->numArg);
				}
				if (std::string_view(a_event->eventName.c_str()) != "HPM_SetElementHidden") { return RE::BSEventNotifyControl::kContinue; }
				const std::string key = a_event->strArg.c_str();
				const int         i = hud::IndexOf(key);
				if (i < 0) {
					logger::warn("author API: no element \"{}\"", key);
					return RE::BSEventNotifyControl::kContinue;
				}
				const bool hide = a_event->numArg != 0.0F;
				{
					std::lock_guard l(g_authorLock);
					if (g_authorHidden.size() < hud::Elements().size()) { g_authorHidden.resize(hud::Elements().size(), false); }
					g_authorHidden[static_cast<std::size_t>(i)] = hide;
				}
				const char* who = a_event->sender ? a_event->sender->GetFormEditorID() : nullptr;
				logger::info("author API: {} {} by {}", key, hide ? "hidden" : "given back",
					who && *who ? std::string(who) : (a_event->sender ? std::format("form {:08X}", a_event->sender->GetFormID()) : std::string("a script")));
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// the sneak eye's frame (1 hidden .. 101 detected) - the HUD's own, read only while the eye's mode uses it
		int EyeFrame(RE::GFxMovieView* a_hud)
		{
			const int f = g_forceEye.load();
			if (f >= 0) { return f; }
			RE::GFxValue v;
			if (a_hud && a_hud->GetVariable(&v, "_root.HUDMovieBaseInstance.StealthMeterInstance.SneakAnimInstance._currentframe") && v.IsNumber()) {
				return static_cast<int>(v.GetNumber());
			}
			return 1;
		}
		bool                                  g_combat = false;
		std::chrono::steady_clock::time_point g_combatSeen{};

		bool InCombat(bool a_needed)
		{
			const int forced = g_forceCombat.load();
			if (forced >= 0) { return forced == 1; }
			if (!a_needed) { return false; }
			auto* player = RE::PlayerCharacter::GetSingleton();
			const bool now = player && player->IsInCombat();
			const auto t = std::chrono::steady_clock::now();
			if (now) {
				g_combatSeen = t;
				if (!g_combat) { logger::debug("combat: in"); }
				g_combat = true;
			} else if (g_combat && t - g_combatSeen > std::chrono::seconds(3)) {
				logger::debug("combat: out (3 s after the last sign)");
				g_combat = false;
			}
			return g_combat;
		}

		// in play, not in a menu that pauses the game: "Always visible" holds the bars only then
		bool InGameplay()
		{
			auto* ui = RE::UI::GetSingleton();
			return ui && !ui->GameIsPaused();
		}

		// The clip-listing request, answered on the main thread.
		std::mutex              g_listLock;
		std::condition_variable g_listCv;
		int                     g_listDepth = 0;   // 0 = no request pending
		std::string             g_listMenu;
		std::string             g_listResult;
		bool                    g_listDone = false;

		constexpr double kEpsPos = 0.2;    // Scaleform stores positions in twips (1/20 px): a read-back can differ by that much
		constexpr double kEpsScale = 0.05;

		std::string PathFor(const hud::Element& a_el, const char* a_part)
		{
			if (!hud::IsWidget(a_el)) {
				return std::string("_root.HUDMovieBaseInstance.") + a_part;
			}
			return (a_part && a_part[0]) ? std::string("_root.") + a_part : std::string("_root");
		}

		void BuildParts(std::size_t a_i, RE::GFxMovieView* a_movie, RE::GPtr<RE::IMenu> a_menu)
		{
			const auto& el = hud::Elements()[a_i];
			Tracked t;
			t.movie = a_movie;
			t.menuRef = std::move(a_menu);
			t.movieRef = RE::GPtr<RE::GFxMovieView>{ a_movie };
			if (a_movie) {
				const RE::GRectF r = a_movie->GetVisibleFrameRect();
				if (r.right - r.left > 1.0F && r.bottom - r.top > 1.0F) {
					t.left = r.left; t.top = r.top; t.width = r.right - r.left; t.height = r.bottom - r.top;
				}
				logger::debug("{}: its movie's visible stage is {:.1f},{:.1f} {:.1f}x{:.1f}", el.key, t.left, t.top, t.width, t.height);
			}
			for (const char* p : el.parts) {
				Part part;
				if (p && p[0] == '@') {
					part.skyui = p + 1;   // a discovered SkyUI widget (phase 4, D2): resolved through discovery::SkyuiPath
				} else {
					part.path = PathFor(el, p);
				}
				t.parts.push_back(std::move(part));
			}
			t.fade = g_el[a_i].fade;
			g_el[a_i] = std::move(t);
		}

		// A widget menu, looked up on the main thread (safe here, never on the render thread). Null when it is
		// not open or has no movie.
		RE::GPtr<RE::IMenu> OpenMenu(const char* a_menu)
		{
			auto* ui = RE::UI::GetSingleton();
			if (!ui) { return {}; }
			auto menu = ui->GetMenu(a_menu);
			return (menu && menu->uiMovie) ? menu : RE::GPtr<RE::IMenu>{};
		}

		bool Resolve(Part& a_part, RE::GFxMovieView* a_movie)
		{
			if (!a_movie) { a_part.found = false; return false; }
			if (!a_part.skyui.empty()) {
				// its slot of _root.WidgetContainer, as the last discovery scan saw it (empty until one has)
				const std::string path = discovery::SkyuiPath(a_part.skyui);
				if (path != a_part.path) {
					if (!a_part.path.empty()) { logger::debug("{} moved to {}", a_part.skyui, path.empty() ? "nowhere" : path); }
					a_part.path = path;
					a_part.found = false;
					a_part.haveBase = false;
					a_part.touched = false;
				}
				if (a_part.path.empty()) { return false; }
			}
			RE::GFxValue v;
			if (a_movie->GetVariable(&v, a_part.path.c_str()) && v.IsDisplayObject()) {
				a_part.obj = v;
				if (!a_part.found) {
					logger::debug("found {} ({})", a_part.path, fmt::ptr(a_movie));
				}
				a_part.found = true;
				return true;
			}
			if (a_part.found) {
				logger::debug("{} is gone from its movie", a_part.path);
			}
			a_part.found = false;
			a_part.haveBase = false;
			a_part.touched = false;
			return false;
		}

		// Two names can reach one clip (a HUD keeps extra references to its clips: Norden UI's shout meter bar
		// is both CompassShoutMeterHolder.ShoutMeterBarAlt and ShoutMeterBarAlt). Moving it through both would
		// add the offset twice and, with the base following what we read, keep adding it every frame - so
		// every clip is moved through one part only, across all elements.
		void MarkDuplicates()
		{
			std::vector<const RE::GFxValue*> seen;
			for (auto& t : g_el) {
				for (auto& p : t.parts) {
					p.duplicate = false;
					if (!p.found) { continue; }
					for (const auto* s : seen) {
						if (*s == p.obj) {
							if (!p.duplicate) { logger::debug("{} reaches a clip another part already moves; used once", p.path); }
							p.duplicate = true;
							break;
						}
					}
					if (!p.duplicate) { seen.push_back(&p.obj); }
				}
			}
		}

		// The clip's own centre in its local coordinates, so a scale change keeps the element centred.
		void MeasureCenter(Part& a_part)
		{
			// HPM's own widgets: the holder's origin IS the art's centre (Widgets.cpp centres the loaded art on it), and the
			// art loads a few frames after the holder is first seen - a bounds measured then sat half the widget's width off,
			// so a Size below 1 slid the widget right (2026-10-04, the Norden layout: scaled meters started at their centre).
			if (a_part.path.find(".HPM_") != std::string::npos) {
				a_part.centerX = a_part.centerY = 0.0;
				return;
			}
			RE::GFxValue bounds;
			RE::GFxValue self = a_part.obj;
			if (a_part.obj.Invoke("getBounds", &bounds, &self, 1) && bounds.IsObject()) {
				RE::GFxValue a, b, c, d;
				if (bounds.GetMember("xMin", &a) && bounds.GetMember("xMax", &b) && bounds.GetMember("yMin", &c) && bounds.GetMember("yMax", &d) &&
					a.IsNumber() && b.IsNumber() && c.IsNumber() && d.IsNumber() && b.GetNumber() - a.GetNumber() < 100000.0) {
					a_part.centerX = (a.GetNumber() + b.GetNumber()) * 0.5;
					a_part.centerY = (c.GetNumber() + d.GetNumber()) * 0.5;
					return;
				}
			}
			a_part.centerX = a_part.centerY = 0.0;  // no bounds: scale about the clip's registration point instead
		}

		// The combined scale of the clip's ancestors, from _root down to its parent.
		void MeasureParents(Part& a_part, RE::GFxMovieView* a_movie)
		{
			a_part.parentSX = a_part.parentSY = 1.0;
			if (!a_movie) { return; }
			const auto last = a_part.path.rfind('.');
			if (last == std::string::npos) { return; }   // _root itself: the stage is its parent
			std::string prefix;
			std::size_t pos = 0;
			while (pos < last) {
				const auto next = a_part.path.find('.', pos);
				const auto end = (next == std::string::npos || next > last) ? last : next;
				prefix = a_part.path.substr(0, end);
				pos = end + 1;
				if (prefix == "_root") { continue; }   // the root's own scale is part of the stage mapping, not the offset
				RE::GFxValue v;
				RE::GFxValue::DisplayInfo info;
				if (a_movie->GetVariable(&v, prefix.c_str()) && v.IsDisplayObject() && v.GetDisplayInfo(&info)) {
					a_part.parentSX *= info.GetXScale() / 100.0;
					a_part.parentSY *= info.GetYScale() / 100.0;
				}
			}
			if (!(std::abs(a_part.parentSX) > 0.01)) { a_part.parentSX = 1.0; }   // a zero-scale parent: keep the offset as given
			if (!(std::abs(a_part.parentSY) > 0.01)) { a_part.parentSY = 1.0; }
		}

		void Restore(Part& a_part, RE::GFxValue::DisplayInfo& a_info)
		{
			a_info.SetPosition(a_part.baseX, a_part.baseY);
			a_info.SetScale(a_part.baseXS, a_part.baseYS);
			if (a_part.hiddenByUs) {
				a_info.SetVisible(a_part.visibleBefore);
				a_part.hiddenByUs = false;
			}
			if (a_part.alphaTouched) {
				a_info.SetAlpha(a_part.baseAlpha);
				a_part.alphaTouched = false;
			}
			a_part.obj.SetDisplayInfo(a_info);
			a_part.touched = false;
		}

		// a_sx / a_sy: Size times Length / Height. a_holdAlpha: "Always visible" - the element's alpha is put back to 100
		// whenever the game's fade has lowered it (written only then, so a steady element costs a read per frame).
		// a_alphaMul: the fade (bFade) - the owner's alpha times this, 1 leaving it alone.
		void ApplyPart(Part& a_part, RE::GFxMovieView* a_movie, float a_offX, float a_offY, float a_sx, float a_sy, bool a_hide, bool a_active, bool a_holdAlpha,
			double a_alphaMul = 1.0)
		{
			RE::GFxValue::DisplayInfo info;
			if (!a_part.obj.GetDisplayInfo(&info)) {
				a_part.found = false;  // re-resolved on the next pass
				return;
			}
			const double x = info.GetX(), y = info.GetY(), xs = info.GetXScale(), ys = info.GetYScale();
			if (!a_part.haveBase) {
				a_part.baseX = x; a_part.baseY = y; a_part.baseXS = xs; a_part.baseYS = ys;
				a_part.haveBase = true;
				MeasureCenter(a_part);
				MeasureParents(a_part, a_movie);
				logger::debug("{}: placed at ({:.1f}, {:.1f}) scale {:.1f}/{:.1f}, centre ({:.1f}, {:.1f}), parents' scale {:.2f}/{:.2f}",
							  a_part.path, x, y, xs, ys, a_part.centerX, a_part.centerY, a_part.parentSX, a_part.parentSY);
			} else if (!a_part.touched) {
				a_part.baseX = x; a_part.baseY = y; a_part.baseXS = xs; a_part.baseYS = ys;  // follow the owner while we are idle
			} else {
				// the owner moved it since our last write: its new value is the new base
				if (std::abs(x - a_part.lastX) > kEpsPos) { a_part.baseX = x; }
				if (std::abs(y - a_part.lastY) > kEpsPos) { a_part.baseY = y; }
				if (std::abs(xs - a_part.lastXS) > kEpsScale) { a_part.baseXS = xs; }
				if (std::abs(ys - a_part.lastYS) > kEpsScale) { a_part.baseYS = ys; }
			}

			if (!a_active) {
				if (a_part.touched) {
					Restore(a_part, info);
				}
				return;
			}
			const double sx = a_sx, sy = a_sy;
			const double txs = a_part.baseXS * sx, tys = a_part.baseYS * sy;
			// keep the clip's centre where it was: move by the centre's growth, in the parent's units
			const double tx = a_part.baseX + a_offX / a_part.parentSX + a_part.centerX * a_part.baseXS / 100.0 * (1.0 - sx);
			const double ty = a_part.baseY + a_offY / a_part.parentSY + a_part.centerY * a_part.baseYS / 100.0 * (1.0 - sy);
			bool write = std::abs(x - tx) > kEpsPos || std::abs(y - ty) > kEpsPos || std::abs(xs - txs) > kEpsScale || std::abs(ys - tys) > kEpsScale;
			if (write) {
				info.SetPosition(tx, ty);
				info.SetScale(txs, tys);
			}
			if (a_hide && info.GetVisible()) {
				if (!a_part.hiddenByUs) {
					a_part.visibleBefore = true;
				}
				a_part.hiddenByUs = true;
				info.SetVisible(false);
				write = true;
			} else if (!a_hide && a_part.hiddenByUs) {
				info.SetVisible(a_part.visibleBefore);
				a_part.hiddenByUs = false;
				write = true;
			}
			// alpha: follow the owner's (anything it wrote since our last write is its new base), then hold it at 100
			// ("Always visible") and / or scale it by the fade - written only when it differs, given back when neither applies
			const double alpha = info.GetAlpha();
			if (!a_part.alphaTouched || std::abs(alpha - a_part.lastAlpha) > 0.5) { a_part.baseAlpha = alpha; }
			if (!a_hide && (a_holdAlpha || a_alphaMul < 0.999)) {
				const double target = (a_holdAlpha ? 100.0 : a_part.baseAlpha) * a_alphaMul;
				if (std::abs(alpha - target) > 0.5) {
					info.SetAlpha(target);
					write = true;
				}
				a_part.alphaTouched = true;
				a_part.lastAlpha = target;
			} else if (a_part.alphaTouched) {
				if (std::abs(alpha - a_part.baseAlpha) > 0.5) {
					info.SetAlpha(a_part.baseAlpha);
					write = true;
				}
				a_part.alphaTouched = false;
			}
			if (write) {
				a_part.obj.SetDisplayInfo(info);
			}
			a_part.touched = true;
			a_part.lastX = tx; a_part.lastY = ty; a_part.lastXS = txs; a_part.lastYS = tys;
		}

		bool Bounds(RE::GFxValue& a_obj, RE::GFxValue& a_space, float& a_l, float& a_t, float& a_r, float& a_b)
		{
			RE::GFxValue bounds;
			if (!a_obj.Invoke("getBounds", &bounds, &a_space, 1) || !bounds.IsObject()) { return false; }
			RE::GFxValue a, b, c, d;
			if (!(bounds.GetMember("xMin", &a) && bounds.GetMember("xMax", &b) && bounds.GetMember("yMin", &c) && bounds.GetMember("yMax", &d))) { return false; }
			if (!(a.IsNumber() && b.IsNumber() && c.IsNumber() && d.IsNumber())) { return false; }
			a_l = static_cast<float>(a.GetNumber()); a_r = static_cast<float>(b.GetNumber());
			a_t = static_cast<float>(c.GetNumber()); a_b = static_cast<float>(d.GetNumber());
			return a_r > a_l && a_b > a_t && a_r - a_l < 100000.0F;   // an empty clip reports a huge inverted box
		}

		// A widget moves its whole movie by its _root: its box is measured in the movie's own stage space,
		// which is where the root's children sit; getBounds(_root) of _root would move with it.
		bool ElementBox(Part& a_part, RE::GFxMovieView* a_movie, bool a_widget, float& a_l, float& a_t, float& a_r, float& a_b)
		{
			RE::GFxValue space;
			if (!a_movie || !a_movie->GetVariable(&space, "_root")) { return false; }
			if (!Bounds(a_part.obj, space, a_l, a_t, a_r, a_b)) { return false; }
			if (a_widget && a_part.path == "_root") {
				// getBounds(_root) of _root is in the root's own units: add the root's own offset and scale
				RE::GFxValue::DisplayInfo info;
				if (a_part.obj.GetDisplayInfo(&info)) {
					const float sx = static_cast<float>(info.GetXScale() / 100.0), sy = static_cast<float>(info.GetYScale() / 100.0);
					const float ox = static_cast<float>(info.GetX()), oy = static_cast<float>(info.GetY());
					a_l = a_l * sx + ox; a_r = a_r * sx + ox; a_t = a_t * sy + oy; a_b = a_b * sy + oy;
				}
			}
			return true;
		}

		std::string Escape(const char* a_s)
		{
			std::string out;
			for (const char* p = a_s ? a_s : ""; *p; ++p) {
				if (*p == '"' || *p == '\\') { out += '\\'; }
				if (static_cast<unsigned char>(*p) >= 0x20) { out += *p; }
			}
			return out;
		}

		void ListInto(std::string& a_out, RE::GFxValue& a_obj, RE::GFxValue& a_root, const std::string& a_prefix, int a_depth, bool& a_first)
		{
			std::vector<std::pair<std::string, RE::GFxValue>> kids;
			a_obj.VisitMembers([&](const char* a_name, const RE::GFxValue& a_val) {
				if (a_name && a_val.IsDisplayObject()) {
					kids.emplace_back(a_name, a_val);
				}
			});
			for (auto& [name, val] : kids) {
				RE::GFxValue::DisplayInfo info;
				const bool gotInfo = val.GetDisplayInfo(&info);
				float l = 0, t = 0, r = 0, b = 0;
				const bool hasBox = Bounds(val, a_root, l, t, r, b);
				char buf[512];
				std::snprintf(buf, sizeof(buf), "%s{\"path\":\"%s%s\",\"x\":%.1f,\"y\":%.1f,\"xscale\":%.1f,\"yscale\":%.1f,\"visible\":%s,\"alpha\":%.0f,\"box\":%s}",
							  a_first ? "" : ",", Escape(a_prefix.c_str()).c_str(), Escape(name.c_str()).c_str(),
							  gotInfo ? info.GetX() : 0.0, gotInfo ? info.GetY() : 0.0, gotInfo ? info.GetXScale() : 0.0, gotInfo ? info.GetYScale() : 0.0,
							  gotInfo && info.GetVisible() ? "true" : "false", gotInfo ? info.GetAlpha() : 0.0,
							  hasBox ? std::format("[{:.1f},{:.1f},{:.1f},{:.1f}]", l, t, r, b).c_str() : "null");
				a_out += buf;
				a_first = false;
				if (a_depth > 1) {
					ListInto(a_out, val, a_root, a_prefix + name + ".", a_depth - 1, a_first);
				}
			}
		}

		void AnswerListRequest()
		{
			int         depth = 0;
			std::string menu;
			{
				std::lock_guard lk(g_listLock);
				depth = g_listDepth;
				menu = g_listMenu;
			}
			if (depth <= 0) { return; }
			std::string out = "[";
			RE::GPtr<RE::IMenu> held = menu.empty() ? RE::GPtr<RE::IMenu>{} : OpenMenu(menu.c_str());
			RE::GFxMovieView*   movie = menu.empty() ? g_hudMovie : (held ? held->uiMovie.get() : nullptr);
			RE::GFxValue      base, root;
			const char*       basePath = menu.empty() ? "_root.HUDMovieBaseInstance" : "_root";
			if (movie && movie->GetVariable(&base, basePath) && base.IsObject() && movie->GetVariable(&root, "_root")) {
				bool first = true;
				if (!menu.empty()) {
					// the root itself first: its own position and scale are what a widget element moves
					RE::GFxValue::DisplayInfo info;
					if (root.GetDisplayInfo(&info)) {
						out += std::format(R"({{"path":"_root","x":{:.1f},"y":{:.1f},"xscale":{:.1f},"yscale":{:.1f},"visible":{},"alpha":{:.0f},"box":null}})",
										   info.GetX(), info.GetY(), info.GetXScale(), info.GetYScale(), info.GetVisible(), info.GetAlpha());
						first = false;
					}
				}
				ListInto(out, base, root, "", depth, first);
			}
			out += "]";
			{
				std::lock_guard lk(g_listLock);
				g_listResult = movie ? std::move(out) : std::string("null");
				g_listDone = true;
				g_listDepth = 0;
			}
			g_listCv.notify_all();
		}
	}

	void Tick(RE::HUDMenu* a_hud)
	{
		++g_frame;
		auto* hudMovie = (a_hud && a_hud->uiMovie) ? a_hud->uiMovie.get() : nullptr;
		if (!hudMovie) {
			static bool logged = false;
			if (!logged) { logger::debug("HUD advanced without a movie; nothing to position yet"); logged = true; }
			return;
		}
		const auto& els = hud::Elements();
		if (g_el.size() != els.size()) {
			g_el.assign(els.size(), {});
		}
		if (hudMovie != g_hudMovie) {
			logger::debug("HUD movie {} (was {}); finding the elements again", fmt::ptr(hudMovie), fmt::ptr(g_hudMovie));
			g_hudMovie = hudMovie;
		}

		widgets::Tick(hudMovie, g_frame);   // the widgets this mod builds exist before their parts are resolved
		{
			// the info bars (phase 4 build 2): values ten times a second, never under the Loading Menu or before the player stands
			// in a loaded world (the built widgets' load lesson)
			auto*      ui = RE::UI::GetSingleton();
			auto*      pl = RE::PlayerCharacter::GetSingleton();
			const bool read = (g_frame % 6) == 0 && ui && !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) && pl && pl->GetParentCell() && pl->Is3DLoaded();
			const RE::GRectF r = hudMovie->GetVisibleFrameRect();
			actorbars::Tick(hudMovie, g_frame, r.left, r.top, r.right - r.left, r.bottom - r.top, read);
			floattext::Tick(hudMovie, g_frame, r.left, r.top, r.right - r.left, r.bottom - r.top, read);
			// other mods' widgets (phase 4, D1: listed only, never moved) - a scan every 5 s in play
			discovery::Tick(hudMovie, g_frame, pl && pl->GetParentCell() && pl->Is3DLoaded());
		}

		const settings::Snapshot s = settings::Get();
		if (s.enabled != g_wasEnabled) {
			logger::info("HUD Position Manager {}", s.enabled ? "enabled: the saved layout is applied" : "disabled: every element back where it was");
			g_wasEnabled = s.enabled;
		}
		// re-resolve handles about once a second (rule 17: a clip or a widget menu can appear later)
		const bool resolveNow = (g_frame % 60) == 1;
		bool       anyResolved = false;
		for (std::size_t i = 0; i < els.size(); ++i) {
			const bool widget = hud::IsWidget(els[i]);
			bool       rebuilt = false;
			if (widget) {
				// every frame, a pointer compare: the menu we hold no longer shows the movie the parts point into (swapped,
				// or its menu closed) - move to the menu's current movie now, not at the next once-a-second check
				const bool stale = g_el[i].menuRef && g_el[i].menuRef->uiMovie.get() != g_el[i].movie;
				if (resolveNow || stale) {
					auto menu = OpenMenu(els[i].menu);
					RE::GFxMovieView* movie = menu ? menu->uiMovie.get() : nullptr;
					if (movie != g_el[i].movie || g_el[i].parts.empty()) {
						if ((movie != nullptr) != (g_el[i].movie != nullptr)) {
							logger::debug("widget menu {} {}", els[i].menu, movie ? "is open" : "is not open");
						}
						BuildParts(i, movie, std::move(menu));
						rebuilt = true;
					}
				}
			} else if (hudMovie != g_el[i].movie || g_el[i].parts.empty()) {
				BuildParts(i, hudMovie, {});
				rebuilt = true;
			}
			if (resolveNow || rebuilt) {
				for (auto& part : g_el[i].parts) {
					Resolve(part, g_el[i].movie);
					anyResolved = true;
				}
			}
		}
		if (anyResolved) {
			MarkDuplicates();
		}

		State     st;
		st.hudSeen = true;
		st.frames = g_frame;
		st.elements.resize(els.size());
		const auto& hudT = g_el[0];   // element 0 is a HUD element: its movie is the HUD's
		st.stageLeft = hudT.left; st.stageTop = hudT.top; st.stageW = hudT.width; st.stageH = hudT.height;
		bool needCombat = false, needInterior = false, needWeapon = false, needSneak = false;
		for (const auto& e : s.elements) {
			needCombat |= e.show == 1 || e.show == 2 || (e.show == 3 && s.imm.enabled && s.imm.inCombat);
			needInterior |= e.show == 4 || e.show == 5;
			needWeapon |= e.show == 6 || (e.show == 3 && s.imm.enabled && s.imm.weaponDrawn);
			needSneak |= e.show == 8;
		}
		bool needAim = false, needEye = false;
		for (std::size_t i = 0; i < els.size() && i < s.elements.size(); ++i) {
			if (s.elements[i].show != 9) { continue; }
			(std::string_view(els[i].key) == "Crosshair" ? needAim : needEye) = true;
		}
		const bool combat = InCombat(needCombat && s.enabled);
		st.inCombat = combat;
		const bool gameplay = InGameplay();
		const bool measureAll = (g_frame % 30) == 0;   // boxes for the page and the DevBench tool twice a second
		// the fade's clock: real time between HUD frames, capped so a long pause (a menu, a load) is one short step
		static auto lastTick = std::chrono::steady_clock::now();
		const auto  nowTick = std::chrono::steady_clock::now();
		const float dt = std::clamp(std::chrono::duration<float>(nowTick - lastTick).count(), 0.0F, 0.1F);
		lastTick = nowTick;
		// the HUD toggle ([Immersive]): its settings handed to the input side, and whether it shows the HUD now
		immersive::Sync(s.imm.enabled, s.imm.key, s.imm.button, s.imm.hold, s.imm.seconds, s.imm.startVisible);
		// the context, read only when an element uses it (each a flag read; the interior test a cell pointer's flag)
		auto*      pc = RE::PlayerCharacter::GetSingleton();
		auto       ctx = [](const std::atomic<int>& a_forced, bool a_needed, auto a_read) {
            const int f = a_forced.load();
            return f >= 0 ? f == 1 : (a_needed && a_read());
		};
		const bool interior = ctx(g_forceInterior, needInterior, [&] { auto* c = pc ? pc->GetParentCell() : nullptr; return c && c->IsInteriorCell(); });
		const bool weaponOut = ctx(g_forceWeapon, needWeapon, [&] { auto* a = pc ? pc->AsActorState() : nullptr; return a && a->IsWeaponDrawn(); });
		const bool sneaking = ctx(g_forceSneak, needSneak, [&] { return pc && pc->IsSneaking(); });
		// the crosshair "when it matters": a weapon out and an attack, a bow draw or a spell charging (build 4)
		const bool aiming = ctx(g_forceAim, needAim, [&] {
			auto* a = pc ? pc->AsActorState() : nullptr;
			if (!a || !a->IsWeaponDrawn()) { return false; }
			if (a->GetAttackState() != RE::ATTACK_STATE_ENUM::kNone) { return true; }
			for (const auto src : { RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kRightHand }) {
				auto* c = pc->GetMagicCaster(src);
				if (c && c->currentSpell && c->state.get() >= RE::MagicCaster::State::kUnk02) { return true; }
			}
			return false;
		});
		const int  eyeFrame = needEye ? EyeFrame(hudMovie) : 1;
		const bool eyeSneak = needEye && pc && pc->IsSneaking();
		// relinquish (ImmersiveHUD's lesson): bleeding out or dead, the whole HUD comes back and nothing fades it
		const auto* pcState = pc ? pc->AsActorState() : nullptr;
		const bool dying = pc && (pc->IsDead() || (pcState && pcState->IsBleedingOut()));
		std::vector<bool> authorHidden;
		{
			std::lock_guard l(g_authorLock);
			authorHidden = g_authorHidden;
		}
		// the toggle: its key, or (ImmersiveHUD's global rules) a fight or a drawn weapon
		const bool toggleShown = !s.imm.enabled || dying || immersive::Shown(dt, gameplay) || (s.imm.inCombat && combat) || (s.imm.weaponDrawn && weaponOut);
		st.toggleShown = toggleShown;
		st.interior = interior; st.weaponDrawn = weaponOut; st.sneaking = sneaking;
		for (std::size_t i = 0; i < els.size(); ++i) {
			const settings::ElementSetting es = i < s.elements.size() ? s.elements[i] : settings::ElementSetting{};
			// "Move with": the element it follows lends its offset (not its size - a widget beside a bar stays
			// its own size when the bar grows), and that one's own "Move with" too - TrueHUD's magicka bar follows
			// Magicka, which follows Health. A loop is cut after as many steps as there are elements. The Combined widgets
			// offset is added once, when this element or one it follows is a member.
			float pctX = es.x, pctY = es.y;
			bool  grouped = s.group.Has(static_cast<int>(i));
			{
				int         next = es.follow;
				std::size_t steps = 0;
				while (next >= 0 && static_cast<std::size_t>(next) < s.elements.size() && static_cast<std::size_t>(next) != i && steps++ < s.elements.size()) {
					pctX += s.elements[static_cast<std::size_t>(next)].x;
					pctY += s.elements[static_cast<std::size_t>(next)].y;
					grouped |= s.group.Has(next);
					next = s.elements[static_cast<std::size_t>(next)].follow;
				}
			}
			if (grouped) { pctX += s.group.x; pctY += s.group.y; }
			// percent of the screen into HUD stage units, then into this movie's own (1 for the HUD itself)
			float offX = pctX / 100.0F * hudT.width, offY = pctY / 100.0F * hudT.height;
			const bool contextShown = es.show == 9 && (std::string_view(els[i].key) == "Crosshair" ? aiming : eyeFrame > 1);
			const bool hideByShow = !dying && ((es.show == 1 && !combat) || (es.show == 2 && combat) || (es.show == 3 && !toggleShown) ||
			                        (es.show == 4 && !interior) || (es.show == 5 && interior) || (es.show == 6 && !weaponOut) || (es.show == 8 && !sneaking) ||
			                        (es.show == 9 && !contextShown));
			const bool byAuthor = i < authorHidden.size() && authorHidden[i];
			// bFade: Show fades the element toward its opacity range instead of hiding it at once (ImmersiveHUD's speeds:
			// each step is half a full fade a second, so 10 fades in 0.2 s and 5 in 0.4 s)
			// "Follow the HUD toggle" always fades (ImmersiveHUD's way); the other modes only with bFade
			const bool fadeShow = (s.fade && es.show != 0) || (es.show == 3 && s.imm.enabled) || es.show == 9;
			float&     fader = g_el[i].fade;
			if (fadeShow) {
				// the sneak eye "when it matters" is as strong as the detection: a quarter at the first notice, full when seen
				const bool  eye = es.show == 9 && std::string_view(els[i].key) == "StealthMeter";
				const float want = hideByShow ? 0.0F : (eye ? std::clamp(0.25F + 0.75F * static_cast<float>(eyeFrame - 1) / 100.0F, 0.25F, 1.0F) : 1.0F);
				const float speed = 0.5F * static_cast<float>(want > fader ? s.fadeIn : s.fadeOut);
				fader = want > fader ? std::min(want, fader + dt * speed) : std::max(want, fader - dt * speed);
			} else {
				fader = hideByShow ? 0.0F : 1.0F;
			}
			double alphaMul = fadeShow ? (s.opacityMin + (s.opacityMax - s.opacityMin) * fader) / 100.0 : 1.0;
			if (es.show == 9 && std::string_view(els[i].key) == "StealthMeter" && !eyeSneak) { alphaMul = 1.0; }
			// while the toggle shows the HUD, the bars the game fades are held up too ([Immersive] bHoldBarsWhenShown)
			const bool heldByToggle = es.show == 3 && s.imm.enabled && s.imm.holdBars && toggleShown;
			// the sneak eye "when it matters" while sneaking: its alpha set outright (ImmersiveHUD's "full control") - the
			// game tweens the eye's alpha by reading it back, so a multiplier on top compounds to 0 (measured 2026-10-04: the
			// eye read 0 with the fade at 0.625); not sneaking, the eye is the game's own again
			const bool eyeMode = es.show == 9 && std::string_view(els[i].key) == "StealthMeter";
			const bool holdAlpha = (els[i].fades && (es.alwaysVisible || s.alwaysVisible || heldByToggle) && gameplay) || (eyeMode && eyeSneak);
			const bool active = s.enabled && (!es.IsDefault() || offX != 0.0F || offY != 0.0F || holdAlpha);
			const float kx = hudT.width > 1.0F ? g_el[i].width / hudT.width : 1.0F;
			const float ky = hudT.height > 1.0F ? g_el[i].height / hudT.height : 1.0F;
			auto&      es2 = st.elements[i];
			es2.appliedX = active ? offX : 0.0F;
			es2.appliedY = active ? offY : 0.0F;
			es2.hiddenByShow = active && hideByShow && !es.hide;
			es2.alphaHeld = active && holdAlpha;
			es2.fade = fader;
			es2.alphaMul = static_cast<float>(alphaMul);
			offX *= kx;
			offY *= ky;
			es2.partsTotal = static_cast<int>(g_el[i].parts.size());
			es2.menuOpen = g_el[i].movie != nullptr;
			const bool measure = measureAll;
			// a mod's hide while the player's layout is not applied ("Apply my layout" off, or an element left at the game's own):
			// the element is hidden where the HUD puts it - not also moved, sized or faded by the layout (review M4)
			const bool authorOnly = byAuthor && !active;
			const float aOffX = authorOnly ? 0.0F : offX, aOffY = authorOnly ? 0.0F : offY;
			const float aSx = authorOnly ? 1.0F : es.scale * es.stretchX, aSy = authorOnly ? 1.0F : es.scale * es.stretchY;
			for (auto& part : g_el[i].parts) {
				if (!part.found || part.duplicate) { continue; }
				++es2.partsFound;
				ApplyPart(part, g_el[i].movie, aOffX, aOffY, aSx, aSy, es.hide || byAuthor || (hideByShow && !fadeShow),
					active || byAuthor, !authorOnly && holdAlpha, authorOnly ? 1.0 : alphaMul);
				float l, t, r, b;
				if (measure && ElementBox(part, g_el[i].movie, hud::IsWidget(els[i]), l, t, r, b)) {
					if (hud::IsWidget(els[i]) && g_el[i].width > 1.0F && g_el[i].height > 1.0F) {
						// this movie's stage -> the HUD's, for the outline and the tool
						const auto& h = g_el[0];
						auto mx = [&](float v) { return h.left + (v - g_el[i].left) * (h.width / g_el[i].width); };
						auto my = [&](float v) { return h.top + (v - g_el[i].top) * (h.height / g_el[i].height); };
						l = mx(l); r = mx(r); t = my(t); b = my(b);
					}
					if (!es2.hasBounds) { es2.xMin = l; es2.yMin = t; es2.xMax = r; es2.yMax = b; es2.hasBounds = true; }
					else { es2.xMin = std::min(es2.xMin, l); es2.yMin = std::min(es2.yMin, t); es2.xMax = std::max(es2.xMax, r); es2.yMax = std::max(es2.yMax, b); }
				}
			}
		}
		{
			std::lock_guard lk(g_stateLock);
			// keep the last measured box of an element this frame did not measure, so the highlight holds steady
			for (std::size_t i = 0; i < st.elements.size() && i < g_state.elements.size(); ++i) {
				if (!st.elements[i].hasBounds && g_state.elements[i].hasBounds && st.elements[i].partsFound > 0) {
					st.elements[i].hasBounds = true;
					st.elements[i].xMin = g_state.elements[i].xMin; st.elements[i].yMin = g_state.elements[i].yMin;
					st.elements[i].xMax = g_state.elements[i].xMax; st.elements[i].yMax = g_state.elements[i].yMax;
				}
			}
			g_state = std::move(st);
		}
		AnswerListRequest();
		settings::MaybeSave();
	}

	void ClearAuthorHidden()
	{
		std::lock_guard l(g_authorLock);
		g_authorHidden.assign(g_authorHidden.size(), false);
	}

	void RegisterAuthorApi()
	{
		static ModEventSink sink;
		if (auto* src = SKSE::GetModCallbackEventSource()) {
			src->AddEventSink(&sink);
			logger::info("author API: listening for the ModEvent HPM_SetElementHidden");
		}
	}

	void ForceAimEye(int a_aim, int a_eye)
	{
		g_forceAim = a_aim < 0 ? -1 : (a_aim > 0 ? 1 : 0);
		g_forceEye = a_eye < 0 ? -1 : std::clamp(a_eye, 1, 101);
	}

	void ForceContext(int a_interior, int a_weapon, int a_sneak)
	{
		g_forceInterior = a_interior < 0 ? -1 : (a_interior > 0 ? 1 : 0);
		g_forceWeapon = a_weapon < 0 ? -1 : (a_weapon > 0 ? 1 : 0);
		g_forceSneak = a_sneak < 0 ? -1 : (a_sneak > 0 ? 1 : 0);
		logger::info("context for Show forced: interior {}, weapon {}, sneak {}", a_interior, a_weapon, a_sneak);
	}

	void ForceCombat(int a_state)
	{
		g_forceCombat = a_state < 0 ? -1 : (a_state > 0 ? 1 : 0);
		logger::info("combat state for Show {}", a_state < 0 ? "is the game's own again" : (a_state > 0 ? "forced in" : "forced out"));
	}

	State GetState()
	{
		std::lock_guard lk(g_stateLock);
		return g_state;
	}


	std::string ListClips(const std::string& a_menu, int a_depth, int a_timeoutMs)
	{
		std::unique_lock lk(g_listLock);
		g_listDone = false;
		g_listResult.clear();
		g_listMenu = a_menu;
		g_listDepth = a_depth < 1 ? 1 : (a_depth > 3 ? 3 : a_depth);
		if (!g_listCv.wait_for(lk, std::chrono::milliseconds(a_timeoutMs), [] { return g_listDone; })) {
			g_listDepth = 0;
			return {};
		}
		return g_listResult;
	}
}
