#include "ActorBars.h"

#include "Settings.h"
#include "Tint.h"
#include "utils/Logger.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <format>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace actorbars
{
	namespace
	{
		using clock = std::chrono::steady_clock;
		constexpr int         kPoolMax = 20;
		constexpr const char* kArt = "HUDPositionManager/widgets/infobar.swf";
		constexpr const char* kMarker = "hpmInHudElements";
		constexpr int         kLevelThreshold = 10;   // TrueHUD's uLevelThreshold default: this many levels apart changes the colour
		// the HUD's modes a clip in HudElements must claim, or the first mode change (a menu, dialogue, sneaking) hides it for
		// good - the built widgets set the same (review 2026-10-04, H1)
		constexpr std::array  kModes{ "All", "StealthMode", "Swimming", "HorseMode", "WarHorseMode" };
		// 0 no load message yet (coc from the main menu sends none: ready once the positioner sees the player in a loaded
		// world), 1 a save loading (kPreLoadGame: no actor is touched), 2 loaded - the built widgets' three states (H2)
		std::atomic<int>      g_ready{ 0 };

		struct Bar
		{
			RE::GFxValue    holder;
			bool            created = false;
			bool            loaded = false;
			bool            registered = false;
			RE::ActorHandle actor;            // empty = free
			float           alpha = 0.0F;     // 0..100 now
			bool            want = false;     // should be seen (projected on screen and chosen)
			float           fill = -1.0F;
			float           phantom = -1.0F;
			clock::time_point phantomHold{};
			std::string     name;
			int             level = -1;
			float           sx = 0, sy = 0;   // the stage position written
			float           scale = -1.0F;
			RE::FormID      formId = 0;       // the actor's, set when the bar is given one (DevBench reads it off the main thread)
			unsigned long long createdFrame = 0;
			bool            failed = false;   // its art never loaded: the slot is given up (logged once)
			float           mag = -1.0F, sta = -1.0F;     // B2: the sub-bars' fills written
			int             showMag = -1, showSta = -1;   // ... and whether each is shown (-1: not written yet)
			float           lastHp = -1.0F;               // B3: the health read last, the damage summed since, when it last grew
			float           damage = 0.0F;
			clock::time_point damageAt{};
			std::string     damageText;                   // ... and the counter written
			std::uint32_t   levelColor = 0;               // the level number's colour written (0: not yet)
			std::string     tint = "-";                   // B7: the [Colors] applied ("-" = not yet)
		};

		// The pool's clips point into the HUD movie, so the movie is held for as long as they are (review M1, the positioner's
		// Tracked pattern): g_hudRef is declared BEFORE g_pool and the swap releases the clips first, then the old movie.
		// Holding it also means a new movie can never reuse the old one's address unseen.
		RE::GPtr<RE::GFxMovieView> g_hudRef;
		std::array<Bar, kPoolMax> g_pool;
		std::mutex                g_lock;            // DevBench reads the pool on its own thread
		std::atomic<bool>         g_pin{ false };

		// who you hit and who hit you, and when (TESHitEvent) - "when hit" bars
		std::mutex                                        g_hitLock;
		std::unordered_map<std::uint32_t, clock::time_point> g_hits;

		class HitSink final : public RE::BSTEventSink<RE::TESHitEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
			{
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!a_event || !player) { return RE::BSEventNotifyControl::kContinue; }
				RE::TESObjectREFR* other = nullptr;
				if (a_event->cause.get() == player) { other = a_event->target.get(); }
				else if (a_event->target.get() == player) { other = a_event->cause.get(); }
				if (other && other->As<RE::Actor>()) {
					std::lock_guard l(g_hitLock);
					g_hits[other->GetFormID()] = clock::now();
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		bool HitRecently(RE::Actor* a_actor)
		{
			std::lock_guard l(g_hitLock);
			const auto it = g_hits.find(a_actor->GetFormID());
			return it != g_hits.end() && clock::now() - it->second < std::chrono::seconds(10);
		}

		// hits older than the 10 s window are dropped at each scan: the map stays small, and a recycled 0xFF.. form ID (a new
		// spawn) never inherits an old "hit recently" (review L2)
		void PruneHits()
		{
			std::lock_guard l(g_hitLock);
			const auto now = clock::now();
			std::erase_if(g_hits, [&](const auto& a_kv) { return now - a_kv.second >= std::chrono::seconds(10); });
		}

		bool Create(Bar& a_b, int a_index, RE::GFxValue& a_base)
		{
			const std::string name = std::format("HPM_IB{}", a_index);
			RE::GFxValue existing;
			if (a_base.GetMember(name.c_str(), &existing) && existing.IsDisplayObject()) {
				a_b.holder = existing;
			} else {
				RE::GFxValue depth;
				double       d = 16000.0 + a_index;
				if (a_base.Invoke("getNextHighestDepth", &depth) && depth.IsNumber()) { d = std::max(d, depth.GetNumber()); }
				std::array<RE::GFxValue, 2> args{ RE::GFxValue{ name.c_str() }, RE::GFxValue{ d } };
				if (!a_base.Invoke("createEmptyMovieClip", &a_b.holder, args.data(), args.size()) || !a_b.holder.IsDisplayObject()) { return false; }
				RE::GFxValue::DisplayInfo info;
				if (a_b.holder.GetDisplayInfo(&info)) {
					info.SetAlpha(0.0);
					a_b.holder.SetDisplayInfo(info);
				}
				RE::GFxValue child;
				std::array<RE::GFxValue, 2> childArgs{ RE::GFxValue{ "widget" }, RE::GFxValue{ 1.0 } };
				if (!a_b.holder.Invoke("createEmptyMovieClip", &child, childArgs.data(), childArgs.size()) || !child.IsDisplayObject()) { return false; }
				RE::GFxValue url{ kArt };
				child.Invoke("loadMovie", nullptr, &url, 1);
			}
			for (const char* mode : kModes) { a_b.holder.SetMember(mode, RE::GFxValue{ true }); }
			a_b.created = true;
			// registered in HudElements: the HUD's own modes (menus, dialogue) hide it as they hide the rest
			RE::GFxValue elements;
			if (a_base.GetMember("HudElements", &elements) && elements.IsArray() && !a_b.holder.HasMember(kMarker)) {
				elements.PushBack(a_b.holder);
				a_b.holder.SetMember(kMarker, RE::GFxValue{ true });
			}
			a_b.registered = true;
			return true;
		}

		void SetText(RE::GFxValue& a_widget, const char* a_field, const std::string& a_text)
		{
			RE::GFxValue f;
			if (a_widget.GetMember(a_field, &f) && f.IsDisplayObject()) { f.SetText(a_text.c_str()); }
		}

		void SetScaleX(RE::GFxValue& a_widget, const char* a_clip, double a_xs)
		{
			RE::GFxValue c;
			RE::GFxValue::DisplayInfo di;
			if (a_widget.GetMember(a_clip, &c) && c.IsDisplayObject() && c.GetDisplayInfo(&di) && std::abs(di.GetXScale() - a_xs) > 0.05) {
				di.SetScale(a_xs, di.GetYScale());
				c.SetDisplayInfo(di);
			}
		}

		// a resource's fraction against the shown max (base + permanent + temporary), as the game's own bar
		float FractionOf(RE::Actor* a_actor, RE::ActorValue a_av)
		{
			auto* avo = a_actor->AsActorValueOwner();
			if (!avo) { return 0.0F; }
			const float mx = avo->GetPermanentActorValue(a_av) + a_actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, a_av);
			return mx > 0.0F ? std::clamp(avo->GetActorValue(a_av) / mx, 0.0F, 1.0F) : 0.0F;
		}

		void SetVisible(RE::GFxValue& a_widget, const char* a_clip, bool a_on)
		{
			RE::GFxValue c;
			RE::GFxValue::DisplayInfo di;
			if (a_widget.GetMember(a_clip, &c) && c.IsDisplayObject() && c.GetDisplayInfo(&di)) {
				di.SetVisible(a_on);
				c.SetDisplayInfo(di);
			}
		}

		// B2: one sub-bar - shown or not by its mode (0 never, 1 when not full, 2 always), and its fill while shown
		void WriteResource(RE::GFxValue& a_widget, const char* a_frame, const char* a_fill, int a_mode, float a_value, int& a_shown, float& a_written)
		{
			const int show = (a_mode == 2 || (a_mode == 1 && a_value < 0.995F)) ? 1 : 0;
			if (show != a_shown) {
				SetVisible(a_widget, a_frame, show == 1);
				SetVisible(a_widget, a_fill, show == 1);
				a_shown = show;
			}
			if (show == 1 && std::abs(a_value - a_written) > 0.002F) {
				SetScaleX(a_widget, a_fill, a_value * 100.0);
				a_written = a_value;
			}
		}

		// which characters get a bar: [InfoBars] rules, nearest first, at most uMaxCount
		std::vector<RE::ActorHandle> Choose(const settings::InfoBars& a_s)
		{
			std::vector<std::pair<float, RE::ActorHandle>> picks;
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* lists = RE::ProcessLists::GetSingleton();
			if (!player || !lists) { return {}; }
			PruneHits();
			const bool  playerFights = player->IsInCombat();
			const auto  ppos = player->GetPosition();
			float       nearestD = 1e30F;
			RE::ActorHandle nearest;
			for (auto& handle : lists->highActorHandles) {
				auto actor = handle.get();
				if (!actor || actor.get() == player || actor->IsDead() || !actor->Is3DLoaded() || actor->IsDisabled()) { continue; }
				const float d = ppos.GetDistance(actor->GetPosition());
				if (d > a_s.maxDistance) { continue; }
				if (d < nearestD) { nearestD = d; nearest = handle; }
				bool take = false;
				if (actor->IsPlayerTeammate()) {
					take = a_s.teammates == 2 || (a_s.teammates == 1 && playerFights);
				} else if (actor->IsHostileToActor(player)) {
					take = a_s.hostiles == 2 || (a_s.hostiles == 1 && (actor->IsInCombat() || HitRecently(actor.get())));
				} else {
					take = a_s.others == 2 || (a_s.others == 1 && HitRecently(actor.get()));
				}
				// hidden behind walls and hills, as TrueHUD's bars are: line of sight, checked here at the scan's four a second
				// only for those that would get a bar (a bar already up keeps it until the next scan)
				if (take) {
					bool unused = false;
					if (!player->HasLineOfSight(actor.get(), unused)) { take = false; }
				}
				if (take) { picks.emplace_back(d, handle); }
			}
			if (g_pin.load() && nearest && std::ranges::none_of(picks, [&](const auto& p) { return p.second == nearest; })) {
				picks.emplace_back(nearestD, nearest);
			}
			std::ranges::sort(picks, {}, &std::pair<float, RE::ActorHandle>::first);
			std::vector<RE::ActorHandle> out;
			for (const auto& [d, h] : picks) {
				if (static_cast<int>(out.size()) >= std::clamp(a_s.maxCount, 1, kPoolMax)) { break; }
				out.push_back(h);
			}
			return out;
		}
	}

	void Register()
	{
		static HitSink sink;
		if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton()) {
			holder->AddEventSink<RE::TESHitEvent>(&sink);
			logger::info("info bars: hit sink registered");
		}
	}

	void Reset()
	{
		std::lock_guard l(g_lock);
		for (auto& b : g_pool) {
			b.actor = {};   // a handle from the old game could resolve to another reference in the new one (H2)
			b.want = false;
			b.name.clear();
			b.level = -1;
			b.fill = b.phantom = -1.0F;
		}
		std::lock_guard h(g_hitLock);
		g_hits.clear();
	}

	void SetReady(bool a_ready)
	{
		g_ready = a_ready ? 2 : 1;
		if (!a_ready) { Reset(); }
	}

	void Tick(RE::GFxMovieView* a_hud, unsigned long long a_frame, float a_left, float a_top, float a_width, float a_height, bool a_read)
	{
		static settings::InfoBars s;
		static settings::Colors   col;
		if (a_read || a_frame % 60 == 1) {
			const auto snap = settings::Get();
			s = snap.ib;
			col = snap.col;
		}
		const std::string tintWant = col.health + "|" + col.magicka + "|" + col.stamina + "|" + col.phantom;
		std::lock_guard l(g_lock);
		if (!a_hud) { return; }
		if (a_hud != g_hudRef.get()) {
			for (auto& b : g_pool) { b = Bar{}; }        // the old movie's clips first ...
			g_hudRef = RE::GPtr<RE::GFxMovieView>{ a_hud };   // ... then the old movie
		}
		auto* ui = RE::UI::GetSingleton();
		const bool loading = !ui || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
		if (g_ready.load() == 1 || loading) {   // a save loading or a load screen: let every bar go, read nothing
			a_read = false;
			for (auto& b : g_pool) { b.actor = {}; }
		}
		const bool anyUsed = std::ranges::any_of(g_pool, [](const Bar& b) { return static_cast<bool>(b.actor) || b.alpha > 0.0F; });
		if (!s.enabled && !anyUsed) { return; }   // off: nothing read, nothing written (the pool is made only once used)
		RE::GFxValue base;
		if (!a_hud->GetVariable(&base, "_root.HUDMovieBaseInstance") || !base.IsObject()) { return; }
		const int count = std::clamp(s.maxCount, 1, kPoolMax);

		// the choice, about four times a second (every fifteenth read frame is too rare: a_read is every sixth frame):
		// keep a character in the bar it already has, give new ones free bars
		if (a_read && (a_frame % 12) == 0) {
			std::vector<RE::ActorHandle> chosen = s.enabled ? Choose(s) : std::vector<RE::ActorHandle>{};
			for (auto& b : g_pool) {
				if (b.actor && std::ranges::find(chosen, b.actor) == chosen.end()) { b.want = false; b.actor = {}; }
			}
			for (const auto& h : chosen) {
				if (std::ranges::any_of(g_pool, [&](const Bar& b) { return b.actor == h; })) { continue; }
				for (int i = 0; i < count; ++i) {
					auto& b = g_pool[i];
					if (!b.actor && b.alpha <= 0.0F && !b.failed) {
						b.actor = h;
						auto who = h.get();
						b.formId = who ? who->GetFormID() : 0;
						b.name.clear();
						b.level = -1;
						b.fill = b.phantom = -1.0F;
						b.mag = b.sta = -1.0F;
						b.lastHp = -1.0F;
						b.damage = 0.0F;
						b.damageText.clear();
						b.levelColor = 0;
						break;
					}
				}
			}
		}

		// every frame: project each bar's character to the screen, fade, and write values ten times a second
		auto* cam = RE::Main::WorldRootCamera();
		// a stage point into HUDMovieBaseInstance's space: less its origin, divided by its scale - a HUD that scales the base
		// (a HUD-scale mod) would otherwise draw the bars off the characters (review L5); read once a frame
		RE::GFxValue::DisplayInfo baseInfo;
		const bool   haveBase = base.GetDisplayInfo(&baseInfo);
		const double ox = haveBase ? baseInfo.GetX() : 0.0, oy = haveBase ? baseInfo.GetY() : 0.0;
		const double bsx = haveBase && std::abs(baseInfo.GetXScale()) > 1.0 ? baseInfo.GetXScale() / 100.0 : 1.0;
		const double bsy = haveBase && std::abs(baseInfo.GetYScale()) > 1.0 ? baseInfo.GetYScale() / 100.0 : 1.0;
		static auto last = clock::now();
		const auto  now = clock::now();
		const float dt = std::clamp(std::chrono::duration<float>(now - last).count(), 0.0F, 0.1F);
		last = now;
		auto* player = RE::PlayerCharacter::GetSingleton();
		for (int i = 0; i < kPoolMax; ++i) {
			auto& b = g_pool[i];
			if (!b.actor && b.alpha <= 0.0F) { continue; }
			if (b.failed) { b.actor = {}; continue; }
			if (!b.created) {
				if (i >= count || !Create(b, i, base)) { continue; }
				b.createdFrame = a_frame;
			}
			RE::GFxValue widget;
			const bool   haveWidget = b.holder.GetMember("widget", &widget) && widget.IsDisplayObject();
			if (!b.loaded) {
				RE::GFxValue fill;
				b.loaded = haveWidget && widget.GetMember("Fill", &fill) && fill.IsDisplayObject();
				if (b.loaded) {   // B2: the sub-bars start hidden; the first read shows those its mode wants
					for (const char* c : { "Frame2", "Fill2", "Frame3", "Fill3" }) { SetVisible(widget, c, false); }
					b.showMag = b.showSta = 0;
				}
				if (!b.loaded) {
					// art that never arrives (infobar.swf missing or broken, or a holder left without its child) would hold the
					// slot and its character forever: after ~5 s the slot is given up (review L3)
					if (a_frame - b.createdFrame > 300) {
						b.failed = true;
						b.actor = {};
						logger::warn("info bars: bar {} - {} never loaded; that bar is not used this session", i, haveWidget ? kArt : "its widget clip");
					}
					continue;
				}
			}
			if (!haveWidget) { continue; }
			// B7: [Colors] - health on the bar, magicka and stamina under it, the phantom; once per change
			if (b.tint != tintWant) {
				tint::ApplyTo(widget, "Fill", col.health);
				tint::ApplyTo(widget, "Fill2", col.magicka);
				tint::ApplyTo(widget, "Fill3", col.stamina);
				tint::ApplyTo(widget, "Phantom", col.phantom);
				b.tint = tintWant;
			}
			auto actor = b.actor.get();
			bool onScreen = false;
			if (actor && cam && player) {
				RE::NiPoint3 pt = actor->GetPosition();
				// uAnchor: over the head (TrueHUD's default), or on the chest - a bar in the body's middle
				pt.z += (s.anchor == 0 ? actor->GetHeight() * 0.6F : actor->GetHeight()) + s.offsetZ;
				float x = 0, y = 0, z = 0;
				if (cam->WorldPtToScreenPt3(pt, x, y, z, 1e-5F) && z > 0.0F && x > -0.05F && x < 1.05F && y > -0.05F && y < 1.05F) {
					onScreen = true;
					const float sx = a_left + x * a_width, sy = a_top + (1.0F - y) * a_height;
					const float d = player->GetPosition().GetDistance(actor->GetPosition());
					const float sc = s.fScale * (s.scaleWithDistance ? std::clamp(1.0F - d / std::max(s.maxDistance, 1.0F) * 0.5F, 0.5F, 1.0F) : 1.0F);
					RE::GFxValue::DisplayInfo info;
					if (b.holder.GetDisplayInfo(&info) && (std::abs(sx - b.sx) > 0.1F || std::abs(sy - b.sy) > 0.1F || std::abs(sc - b.scale) > 0.005F)) {
						info.SetPosition((sx - ox) / bsx, (sy - oy) / bsy);
						info.SetScale(sc * 100.0, sc * 100.0);
						b.holder.SetDisplayInfo(info);
						b.sx = sx; b.sy = sy; b.scale = sc;
					}
				}
			}
			b.want = actor && onScreen && !actor->IsDead();
			if (a_read && actor) {
				const float f = FractionOf(actor.get(), RE::ActorValue::kHealth);
				if (std::abs(f - b.fill) > 0.002F) {
					SetScaleX(widget, "Fill", f * 100.0);
					b.fill = f;
				}
				// B2: magicka and stamina under the bar, by the character's group - read only when its mode is not Never
				const int   res = actor->IsPlayerTeammate() ? s.resTeammates : (player && actor->IsHostileToActor(player) ? s.resHostiles : s.resOthers);
				const float mg = res != 0 ? FractionOf(actor.get(), RE::ActorValue::kMagicka) : 1.0F;
				const float st = res != 0 ? FractionOf(actor.get(), RE::ActorValue::kStamina) : 1.0F;
				WriteResource(widget, "Frame2", "Fill2", res, mg, b.showMag, b.mag);
				WriteResource(widget, "Frame3", "Fill3", res, st, b.showSta, b.sta);
				// the name and level, written on a change of character, level or switch
				const char* n = actor->GetDisplayFullName();
				const std::string name = s.showName ? (n && *n ? n : " ") : " ";
				if (name != b.name) {
					b.name = name;
					SetText(widget, "Value", name);
				}
				// the level, coloured by how it compares with the player's (TrueHUD's difficulty colours): red 10 or more above,
				// grey 10 or more below, the art's own colour between; written on a change of level or colour
				const int lv = s.showLevel ? actor->GetLevel() : 0;
				std::uint32_t color = 0xC8C0B0;
				if (s.levelColors && lv > 0 && player) {
					const int d = lv - static_cast<int>(player->GetLevel());
					color = d >= kLevelThreshold ? 0xE05A4A : (d <= -kLevelThreshold ? 0x8A8A8A : 0xC8C0B0);
				}
				if (lv != b.level || color != b.levelColor) {
					b.level = lv;
					b.levelColor = color;
					RE::GFxValue f;
					if (widget.GetMember("Value2", &f) && f.IsDisplayObject()) {
						f.SetTextHTML(std::format(R"(<p align="right"><font face="$EverywhereFont" size="10" color="#{:06X}">{}</font></p>)", color,
							lv > 0 ? std::to_string(lv) : std::string(" ")).c_str());
					}
				}
				// the damage counter: what the bar lost in the last fDamageCounterSeconds, summed while hits keep coming
				if (s.damageCounter) {
					const float hp = actor->AsActorValueOwner() ? actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth) : 0.0F;
					if (b.lastHp >= 0.0F && hp < b.lastHp - 0.5F) {
						b.damage += b.lastHp - hp;
						b.damageAt = now;
					}
					b.lastHp = hp;
					if (b.damage > 0.0F && now - b.damageAt > std::chrono::milliseconds(static_cast<int>(s.damageSeconds * 1000.0F))) { b.damage = 0.0F; }
				} else {
					b.damage = 0.0F;
					b.lastHp = -1.0F;
				}
				const std::string dmgText = b.damage >= 1.0F ? std::format("-{:.0f}", b.damage) : std::string(" ");
				if (dmgText != b.damageText) {
					b.damageText = dmgText;
					SetText(widget, "Value3", dmgText);
				}
			}
			// the recent loss (Phantom), as the player bars: holds, then eases down a full bar a second
			if (b.fill >= 0.0F) {
				if (b.phantom < 0.0F || b.fill >= b.phantom) {
					b.phantom = b.fill;
					b.phantomHold = now + std::chrono::milliseconds(750);
				} else if (now >= b.phantomHold) {
					b.phantom = std::max(b.fill, b.phantom - dt);
				}
				SetScaleX(widget, "Phantom", b.phantom * 100.0);
			}
			// fade in / out over a quarter second
			const float target = b.want ? 100.0F : 0.0F;
			const float a = target > b.alpha ? std::min(target, b.alpha + dt * 400.0F) : std::max(target, b.alpha - dt * 400.0F);
			if (std::abs(a - b.alpha) > 0.01F || (a == 0.0F && b.alpha != 0.0F)) {
				RE::GFxValue::DisplayInfo info;
				if (b.holder.GetDisplayInfo(&info)) {
					info.SetAlpha(a);
					b.holder.SetDisplayInfo(info);
				}
			}
			b.alpha = a;
		}
	}

	std::string StateJson()
	{
		std::lock_guard l(g_lock);
		std::string out = "[";
		for (int i = 0; i < kPoolMax; ++i) {
			const auto& b = g_pool[i];
			if (!b.actor && b.alpha <= 0.0F) { continue; }
			// the FormID stored on the main thread: DevBench's thread never resolves a handle (review L6)
			out += std::format(R"({}{{"bar":{},"actor":"{:08X}","name":"{}","level":{},"fill":{:.3f},"phantom":{:.3f},"alpha":{:.0f},"want":{},"x":{:.1f},"y":{:.1f},"scale":{:.2f},"loaded":{},"failed":{},"magicka":{:.3f},"stamina":{:.3f},"showMagicka":{},"showStamina":{},"damage":"{}","levelColor":"{:06X}"}})",
				out.size() > 1 ? "," : "", i, b.actor ? b.formId : 0u, b.name, b.level, b.fill, b.phantom, b.alpha, b.want, b.sx, b.sy, b.scale, b.loaded, b.failed,
				b.mag, b.sta, b.showMag, b.showSta, b.damageText, b.levelColor);
		}
		return out + "]";
	}

	void PinNearest(bool a_on)
	{
		g_pin = a_on;
		logger::info("info bars: the nearest character {}", a_on ? "pinned (test)" : "no longer pinned");
	}
}
