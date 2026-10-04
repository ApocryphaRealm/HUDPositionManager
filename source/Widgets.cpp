#include "Widgets.h"

#include "Elements.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <RE/Skyrim.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <format>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

namespace widgets
{
	namespace
	{
		// the HUD's mode flags each widget owns (HUDMovieBaseInstance.ShowElements(mode) sets _visible = hasOwnProperty(mode)):
		// the modes the vanilla bars own - shown in play, sneaking, swimming and on a horse; hidden in dialogue and menus
		constexpr std::array kModes{ "All", "StealthMode", "Swimming", "HorseMode", "WarHorseMode" };
		constexpr const char* kMarker = "hpmInHudElements";

		struct Built
		{
			std::size_t  element = 0;
			RE::GFxValue holder;           // HPM_<key> under HUDMovieBaseInstance
			bool         created = false;
			bool         registered = false;
			bool         loaded = false;   // the SWF's Fill / Value are there
			float        value = -1.0F;    // 0..1 shown now (-1 = never written)
			bool         shown = false;
			float        forced = -1.0F;   // DevBench: held value
			std::string  text;             // the Value field's text, when the widget has one
			int          style = 0;        // the art loaded: 0 Element::swf, 1 Element::swf2
			int          ringSegs = -1;    // a Ring's Seg0..SegN-1 count (-1 = not counted yet, 0 = no ring)
			int          ringShown = -1;   // segments shown now
			int          meterFrames = -1; // a Meter's _totalframes (-1 = not read yet, 0 = no meter)
			int          meterShown = -1;  // the frame it stands on now
			unsigned long long createdFrame = 0;
		};

		std::vector<Built>    g_built;
		RE::GFxMovieView*     g_hud = nullptr;
		std::mutex            g_lock;      // the DevBench tool reads g_built on its own thread
		// 0 = no load seen yet, 1 = a save is loading (kPreLoadGame), 2 = loaded (kPostLoadGame / kNewGame). State 0 exists
		// because coc from the main menu starts play with NEITHER message (2026-10-04, HPM Minimal: nothing ever read).
		std::atomic<int>      g_gameState{ 0 };

		// where a widget first sits, as a fraction of the HUD's VISIBLE stage (its centre); the positioner's offset rides on
		// top. HUDMovieBaseInstance's origin is near the stage's centre (measured 2026-10-04: 616.65, 475.45 on a 1280x960
		// stage), so a position in its space is the stage point less that origin - a fixed local point drew off screen.
		struct Spot { float fx, fy; };
		Spot DefaultSpot(const std::string& a_key)
		{
			if (a_key == "Breath") { return { 0.5F, 0.80F }; }       // centred, above the bars' row
			if (a_key == "CastingBar") { return { 0.5F, 0.58F }; }   // centred, under the crosshair
			if (a_key == "InfoTime") { return { 0.88F, 0.80F }; }     // bottom right, stacked: time, level, gold, weight
			if (a_key == "ShoutCooldown") { return { 0.5F, 0.86F }; } // centred, just above the compass row
			if (a_key == "InfoLevel") { return { 0.88F, 0.84F }; }
			if (a_key == "InfoGold") { return { 0.88F, 0.88F }; }
			if (a_key == "InfoWeight") { return { 0.88F, 0.92F }; }
			if (a_key == "Detection") { return { 0.5F, 0.62F }; }    // centred, under the casting bar (above the crosshair it met notifications - 2026-10-04)
			return { 0.5F, 0.5F };
		}

		// ------------------------------------------------------------------ the values (game data, main thread)

		// Breath: the time the player has been under water against the game's breath allowance. Shown only under water,
		// and never with water breathing. The allowance is the fActorSwimBreathBase game setting (20 s when unread).
		std::string g_breath = "{}";   // the last breath read: underwater flag, underWaterTimer, allowance (test readout)

		bool ReadBreath(float& a_value)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) { return false; }
			const auto& rt = player->GetActorRuntimeData();
			const bool under = rt.boolFlags.all(RE::Actor::BOOL_FLAGS::kUnderwater);
			g_breath = std::format(R"({{"under":{},"timer":{:.2f}}})", under, rt.underWaterTimer);
			if (!under) { return false; }
			if (auto* avo = player->AsActorValueOwner(); avo && avo->GetActorValue(RE::ActorValue::kWaterBreathing) > 0.0F) { return false; }
			// The air you have: fActorSwimBreathBase + fActorSwimBreathMult x 50 - measured 2026-10-04 by when drowning damage
			// began (underWaterTimer counts UP): base 10, mult 0.2 -> 20.5 s; mult 0 -> 10.4 s; stamina 100 or 200 -> the same
			// 20 s (so the 50 is not stamina). The base alone emptied the meter at half the real time.
			float base = 10.0F, mult = 0.2F;
			if (auto* gs = RE::GameSettingCollection::GetSingleton()) {
				if (auto* s = gs->GetSetting("fActorSwimBreathBase"); s && s->GetFloat() > 0.0F) { base = s->GetFloat(); }
				if (auto* s = gs->GetSetting("fActorSwimBreathMult"); s && s->GetFloat() >= 0.0F) { mult = s->GetFloat(); }
			}
			const float allowance = std::max(1.0F, base + mult * 50.0F);
			a_value = std::clamp(1.0F - rt.underWaterTimer / allowance, 0.0F, 1.0F);
			g_breath = std::format(R"({{"under":true,"timer":{:.2f},"allowance":{:.2f}}})", rt.underWaterTimer, allowance);
			static bool logged = false;
			if (!logged) {
				logged = true;
				logger::info("widgets: breath first read under water - underWaterTimer {:.2f} s of an allowance of {:.2f} s", rt.underWaterTimer, allowance);
			}
			return true;
		}

		// Casting: a spell charging in either hand, as the part of its charge time done. Shown only while charging.
		std::string g_casters = "[]";   // the last casting read: each hand's state / timer / charge time (test readout)

		bool ReadCasting(float& a_value)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) { return false; }
			float best = -1.0F;
			std::string seen;
			for (const auto source : { RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kRightHand }) {
				auto* caster = player->GetMagicCaster(source);
				if (!caster || !caster->currentSpell) { continue; }
				const auto state = caster->state.get();
				seen += std::format(R"({}{{"hand":{},"state":{},"timer":{:.3f},"charge":{:.3f}}})", seen.empty() ? "" : ",",
					static_cast<int>(source), static_cast<int>(state), caster->castingTimer, caster->currentSpell->GetChargeTime());
				// charged and still held: state 3 (kReady) with the timer at 0, until the button is let go (then the caster has
				// no spell) - measured 2026-10-04, Firebolt: 2 while charging, 3 once charged. Shown full until released.
				if (state == RE::MagicCaster::State::kReady) {
					best = 1.0F;
					continue;
				}
				if (state != RE::MagicCaster::State::kCharging && state != RE::MagicCaster::State::kUnk02) { continue; }
				const float charge = caster->currentSpell->GetChargeTime();
				if (!(charge > 0.0F)) { continue; }
				// castingTimer counts DOWN from the charge time (Firebolt 0.833: 0.813, then 0.693 - 2026-10-04), so the bar
				// fills as it runs out
				best = std::max(best, std::clamp(1.0F - caster->castingTimer / charge, 0.0F, 1.0F));
			}
			g_casters = "[" + seen + "]";   // the DevBench widgets op shows it (main thread here, read under g_lock)
			if (best < 0.0F) { return false; }
			a_value = best;
			return true;
		}

		// Detection: while the player sneaks, how close anyone is to seeing them - the game's own sneak eye. Its animation
		// (HUDMovieBaseInstance.StealthMeterInstance.SneakAnimInstance) runs frame 1 hidden .. 101 detected, and holds open
		// while an enemy still hunts (measured 2026-10-04: a bandit's raw detection level went -3, 39 (eye 62), 161 (eye
		// 101), then back to -2 with the eye still at 101). One value read, and it is exactly what the game decided.
		// A HUD without the eye: the highest Actor::RequestDetectionLevel of the player among the high-process actors,
		// about -5 unaware .. 100 detected. Nothing is read while the player is not sneaking.
		std::string g_detect = "[]";

		bool ReadDetection(float& a_value, RE::GFxValue& a_base)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* lists = RE::ProcessLists::GetSingleton();
			if (!player || !lists || !player->IsSneaking()) {
				g_detect = "[]";
				return false;
			}
			RE::GFxValue meter, anim, frame;
			if (a_base.GetMember("StealthMeterInstance", &meter) && meter.IsObject() && meter.GetMember("SneakAnimInstance", &anim) &&
				anim.IsObject() && anim.GetMember("_currentframe", &frame) && frame.IsNumber()) {
				g_detect = std::format(R"({{"eye":{:.0f}}})", frame.GetNumber());
				a_value = std::clamp(static_cast<float>((frame.GetNumber() - 1.0) / 100.0), 0.0F, 1.0F);
				return true;
			}
			std::int32_t best = std::numeric_limits<std::int32_t>::min();
			std::string  seen;
			int          listed = 0;
			for (auto& handle : lists->highActorHandles) {
				auto actor = handle.get();
				if (!actor || actor.get() == player || actor->IsDead()) { continue; }
				const std::int32_t level = actor->RequestDetectionLevel(player);
				best = std::max(best, level);
				if (listed < 6) {
					++listed;
					seen += std::format(R"({}{{"name":"{}","level":{}}})", seen.empty() ? "" : ",", actor->GetName() ? actor->GetName() : "", level);
				}
			}
			g_detect = "[" + seen + "]";
			if (best == std::numeric_limits<std::int32_t>::min()) { return false; }   // nobody near: nothing to show
			a_value = std::clamp((static_cast<float>(best) + 5.0F) / 105.0F, 0.0F, 1.0F);
			return true;
		}

		// Gold the player carries: the gold entries of their InventoryChanges (gold picked up, bought or given always lands
		// there). Counted here, not with CommonLib's Actor::GetGoldAmount: its GetInventory, which also walks the base
		// container, read a garbage object pointer (0x160000) on SE 1.5.97 every time - crash-2026-10-04-12-12-14, after
		// the save had fully loaded. The walk is guarded: an access fault returns -2 and the widget switches off for the
		// session (logged once) instead of taking the game down. No C++ objects with destructors live in this function
		// (SEH and C++ unwinding do not mix).
		// An InventoryChanges entry's countDelta is the change FROM the base container, not a total (2026-10-04, HPM Minimal:
		// a save whose player base holds 140 gold read -140 with the deltas alone, the game's own count 0). Gold carried is
		// the base container's gold plus the deltas.
		std::int32_t CountGold(RE::TESContainer* a_base, RE::InventoryChanges* a_changes)
		{
			std::int32_t total = 0;
			if (a_base && a_base->containerObjects) {
				for (std::uint32_t i = 0; i < a_base->numContainerObjects; ++i) {
					auto* co = a_base->containerObjects[i];
					if (co && co->obj && co->obj->IsGold()) { total += co->count; }
				}
			}
			if (a_changes && a_changes->entryList) {
				for (auto* entry : *a_changes->entryList) {
					if (!entry || !entry->object) { continue; }
					if (entry->object->IsGold()) { total += entry->countDelta; }
				}
			}
			return total;
		}

		// the guard holds no objects itself (MSVC: no __try where unwinding is needed); a fault inside CountGold lands here
		std::int32_t CountGoldGuarded(RE::TESContainer* a_base, RE::InventoryChanges* a_changes)
		{
			__try {
				return CountGold(a_base, a_changes);
			} __except (1 /* EXCEPTION_EXECUTE_HANDLER */) {
				return -2;
			}
		}

		// The info widgets: always shown in play (the HUD's own modes hide them in dialogue and menus); text in Value.
		// Gold walks the inventory, so it is read once a second, not at every read.
		bool ReadGold(float& a_value, std::string& a_text)
		{
			static int          tick = 0;
			static std::int32_t gold = -1;
			static bool         broken = false;
			auto*               player = RE::PlayerCharacter::GetSingleton();
			if (!player || broken) { return false; }
			if (gold < 0 || ++tick >= 10) {
				tick = 0;
				RE::TESNPC* npc = player->GetActorBase();
				const std::int32_t counted = CountGoldGuarded(npc ? static_cast<RE::TESContainer*>(npc) : nullptr, player->GetInventoryChanges());
				if (counted == -2) {
					broken = true;
					logger::error("widgets: reading the player's gold faulted - the gold widget is off for this session");
					return false;
				}
				gold = counted;
			}
			a_value = 0.0F;
			a_text = std::to_string(gold);
			return true;
		}

		bool ReadWeight(float& a_value, std::string& a_text)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* avo = player ? player->AsActorValueOwner() : nullptr;
			if (!avo) { return false; }
			const float carried = avo->GetActorValue(RE::ActorValue::kInventoryWeight);
			const float most = avo->GetActorValue(RE::ActorValue::kCarryWeight);
			a_value = most > 0.0F ? std::clamp(carried / most, 0.0F, 1.0F) : 0.0F;
			a_text = std::format("{:.0f} / {:.0f}", carried, most);
			return true;
		}

		// Level: the level as text, the progress to the next one as the fill (the player's skills data, xp of
		// levelThreshold - what the game's level-up meter shows).
		bool ReadLevel(float& a_value, std::string& a_text)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) { return false; }
			a_text = std::to_string(player->GetLevel());
			a_value = 0.0F;
			if (auto* skills = player->GetPlayerRuntimeData().skills; skills && skills->data && skills->data->levelThreshold > 0.0F) {
				a_value = std::clamp(skills->data->xp / skills->data->levelThreshold, 0.0F, 1.0F);
			}
			return true;
		}

		// Game time: the in-game hour as hh:mm (Calendar::GetHour, 0..24). Always shown in play.
		bool ReadTime(float& a_value, std::string& a_text)
		{
			auto* cal = RE::Calendar::GetSingleton();
			if (!cal) { return false; }
			const float hour = std::clamp(cal->GetHour(), 0.0F, 23.999F);
			const int   h = static_cast<int>(hour);
			const int   m = static_cast<int>((hour - static_cast<float>(h)) * 60.0F);
			a_value = 0.0F;
			a_text = std::format("{:02}:{:02}", h, m);
			return true;
		}

		// Shout cooldown: while the voice recovers, the time left (Actor::GetVoiceRecoveryTime, seconds) as a bar draining
		// to empty and the whole seconds as text. The full length is the time left when the cooldown began (the largest
		// value seen since it was last zero). Hidden while the voice is ready.
		bool ReadShout(float& a_value, std::string& a_text)
		{
			static float total = 0.0F;
			auto*        player = RE::PlayerCharacter::GetSingleton();
			if (!player) { return false; }
			const float left = player->GetVoiceRecoveryTime();
			if (!(left > 0.05F)) {
				total = 0.0F;
				return false;
			}
			total = std::max(total, left);
			a_value = std::clamp(left / total, 0.0F, 1.0F);
			a_text = std::format("{:.0f}", std::ceil(left));
			return true;
		}

		bool ReadValue(const std::string& a_key, float& a_value, std::string& a_text, RE::GFxValue& a_base)
		{
			if (a_key == "InfoTime") { return ReadTime(a_value, a_text); }
			if (a_key == "ShoutCooldown") { return ReadShout(a_value, a_text); }
			if (a_key == "InfoGold") { return ReadGold(a_value, a_text); }
			if (a_key == "InfoWeight") { return ReadWeight(a_value, a_text); }
			if (a_key == "InfoLevel") { return ReadLevel(a_value, a_text); }
			if (a_key == "Breath") { return ReadBreath(a_value); }
			if (a_key == "CastingBar") { return ReadCasting(a_value); }
			if (a_key == "Detection") { return ReadDetection(a_value, a_base); }
			return false;
		}

		// ------------------------------------------------------------------ the clips

		// the style the player picked for a built widget with two (settings iStyle), and that style's art
		int StyleOf(std::size_t a_element)
		{
			const auto& el = hud::Elements()[a_element];
			if (!el.swf2) { return 0; }
			const auto& s = settings::Get();
			return a_element < s.elements.size() && s.elements[a_element].style == 1 ? 1 : 0;
		}

		const char* ArtFor(std::size_t a_element, int a_style)
		{
			const auto& el = hud::Elements()[a_element];
			return a_style == 1 && el.swf2 ? el.swf2 : el.swf;
		}

		// The style changed on the page: the new art loads into the same holder (position, size and visibility stay HPM's),
		// and the widget is measured and written afresh once it arrives.
		void Restyle(Built& a_b, int a_style)
		{
			RE::GFxValue widget;
			if (!a_b.holder.GetMember("widget", &widget) || !widget.IsDisplayObject()) { return; }
			const char* art = ArtFor(a_b.element, a_style);
			RE::GFxValue url{ art };
			widget.Invoke("loadMovie", nullptr, &url, 1);
			a_b.style = a_style;
			a_b.loaded = false;
			a_b.value = -1.0F;
			a_b.text.clear();
			a_b.ringSegs = a_b.ringShown = -1;
			a_b.meterFrames = a_b.meterShown = -1;
			logger::info("widgets: {} style {} - loadMovie(\"{}\")", hud::Elements()[a_b.element].key, a_style, art);
		}

		bool Create(Built& a_b, RE::GFxMovieView* a_hud, RE::GFxValue& a_base, unsigned long long a_frame)
		{
			const auto& el = hud::Elements()[a_b.element];
			const std::string name = std::string("HPM_") + el.key;
			RE::GFxValue existing;
			if (a_base.GetMember(name.c_str(), &existing) && existing.IsDisplayObject()) {
				a_b.holder = existing;   // a HUD that kept our clip across a reload
			} else {
				RE::GFxValue depth;
				double       d = 15000.0 + static_cast<double>(a_b.element);
				if (a_base.Invoke("getNextHighestDepth", &depth) && depth.IsNumber()) { d = std::max(d, depth.GetNumber()); }
				std::array<RE::GFxValue, 2> args{ RE::GFxValue{ name.c_str() }, RE::GFxValue{ d } };
				if (!a_base.Invoke("createEmptyMovieClip", &a_b.holder, args.data(), args.size()) || !a_b.holder.IsDisplayObject()) {
					static bool warned = false;
					if (!warned) { warned = true; logger::warn("widgets: createEmptyMovieClip failed on HUDMovieBaseInstance; the built widgets will not draw"); }
					return false;
				}
				// the art is centred on its spot once it has loaded (Write); until then the spot is the holder's origin
				const Spot s = DefaultSpot(el.key);
				const RE::GRectF stage = a_hud->GetVisibleFrameRect();
				RE::GFxValue::DisplayInfo baseInfo;
				const double ox = a_base.GetDisplayInfo(&baseInfo) ? baseInfo.GetX() : 0.0, oy = a_base.GetDisplayInfo(&baseInfo) ? baseInfo.GetY() : 0.0;
				const double sx = stage.left + (stage.right - stage.left) * s.fx, sy = stage.top + (stage.bottom - stage.top) * s.fy;
				RE::GFxValue::DisplayInfo info;
				if (a_b.holder.GetDisplayInfo(&info)) {
					info.SetPosition(sx - ox, sy - oy);
					info.SetAlpha(0.0);   // hidden until its situation shows it
					a_b.holder.SetDisplayInfo(info);
				}
				RE::GFxValue child;
				std::array<RE::GFxValue, 2> childArgs{ RE::GFxValue{ "widget" }, RE::GFxValue{ 1.0 } };
				if (!a_b.holder.Invoke("createEmptyMovieClip", &child, childArgs.data(), childArgs.size()) || !child.IsDisplayObject()) {
					logger::warn("widgets: {}: its widget child could not be made", el.key);
					return false;
				}
				// the art: a path relative to Data\Interface, as SkyUI's widgets are loaded
				a_b.style = StyleOf(a_b.element);
				const char* art = ArtFor(a_b.element, a_b.style);
				RE::GFxValue url{ art };
				const bool called = child.Invoke("loadMovie", nullptr, &url, 1);
				logger::info("widgets: {} created at depth {:.0f}, loadMovie(\"{}\") {}", el.key, d, art, called ? "called" : "FAILED");
			}
			for (const char* mode : kModes) { a_b.holder.SetMember(mode, RE::GFxValue{ true }); }
			a_b.created = true;
			a_b.createdFrame = a_frame;
			a_b.loaded = false;
			a_b.value = -1.0F;
			a_b.shown = false;
			return true;
		}

		void Register(Built& a_b, RE::GFxValue& a_base)
		{
			if (a_b.registered) { return; }
			RE::GFxValue elements;
			if (!a_base.GetMember("HudElements", &elements) || !elements.IsArray()) {
				static bool warned = false;
				if (!warned) { warned = true; logger::warn("widgets: HUDMovieBaseInstance.HudElements is not an array; the built widgets will not follow the HUD's modes"); }
				return;
			}
			if (!a_b.holder.HasMember(kMarker)) {
				elements.PushBack(a_b.holder);
				a_b.holder.SetMember(kMarker, RE::GFxValue{ true });
			}
			a_b.registered = true;
			logger::debug("widgets: {} registered in HudElements ({} elements)", hud::Elements()[a_b.element].key, elements.GetArraySize());
		}

		void Write(Built& a_b, float a_value, bool a_shown, const std::string& a_text = {})
		{
			RE::GFxValue widget;
			if (!a_b.holder.GetMember("widget", &widget) || !widget.IsDisplayObject()) { return; }
			if (!a_b.loaded) {
				// loaded once its art is there: a meter's Fill, or a text widget's Frame
				// ... and only the art asked for: after a style change the OLD movie stays until the new one arrives, and taking
				// it for the new one wrote the text into the old field and never again (2026-10-04, the Level badge kept "100")
				{
					RE::GFxValue url;
					std::string want = ArtFor(a_b.element, a_b.style);
					want = want.substr(want.rfind('/') + 1);
					std::string have = widget.GetMember("_url", &url) && url.IsString() ? url.GetString() : "";
					// _url comes back percent-encoded ("level%5Fbadge.swf" - an underscore is %5F): decode before comparing
					std::string decoded;
					for (std::size_t i = 0; i < have.size(); ++i) {
						if (have[i] == '%' && i + 2 < have.size() && std::isxdigit(static_cast<unsigned char>(have[i + 1])) && std::isxdigit(static_cast<unsigned char>(have[i + 2]))) {
							decoded += static_cast<char>(std::stoi(have.substr(i + 1, 2), nullptr, 16));
							i += 2;
						} else {
							decoded += have[i];
						}
					}
					have = decoded;
					auto lower = [](std::string a_s) { for (auto& c : a_s) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); } return a_s; };
					if (lower(have).find(lower(want)) == std::string::npos) { return; }   // still loading
				}
				RE::GFxValue part;
				a_b.loaded = (widget.GetMember("Fill", &part) && part.IsDisplayObject()) || (widget.GetMember("Frame", &part) && part.IsDisplayObject());
				if (!a_b.loaded) { return; }   // the SWF is still loading
				// centre the art on its spot. The Frame is the widget's size (the clip contract), so its bounds are the centre -
				// a reskin's extras outside it (Norden's level flash) no longer pull the widget off its spot. Without a usable
				// Frame, the whole art's _width / _height as before (art drawn from its top-left corner).
				RE::GFxValue::DisplayInfo w;
				if (widget.GetDisplayInfo(&w)) {
					RE::GFxValue frame, bounds, self = widget;
					RE::GFxValue a, b, c, d;
					if (widget.GetMember("Frame", &frame) && frame.IsDisplayObject() && frame.Invoke("getBounds", &bounds, &self, 1) && bounds.IsObject() &&
						bounds.GetMember("xMin", &a) && bounds.GetMember("xMax", &b) && bounds.GetMember("yMin", &c) && bounds.GetMember("yMax", &d) &&
						a.IsNumber() && b.IsNumber() && c.IsNumber() && d.IsNumber() && b.GetNumber() > a.GetNumber()) {
						w.SetPosition(-(a.GetNumber() + b.GetNumber()) / 2.0, -(c.GetNumber() + d.GetNumber()) / 2.0);
						widget.SetDisplayInfo(w);
					} else {
						RE::GFxValue width, height;
						if (widget.GetMember("_width", &width) && widget.GetMember("_height", &height) && width.IsNumber() && height.IsNumber()) {
							w.SetPosition(-width.GetNumber() / 2.0, -height.GetNumber() / 2.0);
							widget.SetDisplayInfo(w);
						}
					}
				}
				logger::info("widgets: {} art loaded", hud::Elements()[a_b.element].key);
			}
			if (a_shown && std::abs(a_value - a_b.value) > 0.002F) {
				RE::GFxValue fill;
				RE::GFxValue::DisplayInfo info;
				if (widget.GetMember("Fill", &fill) && fill.IsDisplayObject() && fill.GetDisplayInfo(&info)) {
					info.SetScale(std::clamp(a_value, 0.0F, 1.0F) * 100.0, info.GetYScale());
					fill.SetDisplayInfo(info);
				}
				a_b.value = a_value;
			}
			// a Ring (the badge's XP ring): Seg0..SegN-1, the first value x N shown - any number of segments, any art
			if (a_shown && a_b.ringSegs != 0) {
				RE::GFxValue ring;
				if (widget.GetMember("Ring", &ring) && ring.IsDisplayObject()) {
					if (a_b.ringSegs < 0) {
						int n = 0;
						RE::GFxValue seg;
						while (n < 360 && ring.GetMember(("Seg" + std::to_string(n)).c_str(), &seg) && seg.IsDisplayObject()) { ++n; }
						a_b.ringSegs = n;
					}
					const int want = static_cast<int>(std::lround(std::clamp(a_value, 0.0F, 1.0F) * static_cast<float>(a_b.ringSegs)));
					if (want != a_b.ringShown) {
						for (int i = 0; i < a_b.ringSegs; ++i) {
							RE::GFxValue seg;
							if (ring.GetMember(("Seg" + std::to_string(i)).c_str(), &seg) && seg.IsDisplayObject()) {
								RE::GFxValue::DisplayInfo di;
								if (seg.GetDisplayInfo(&di)) { di.SetVisible(i < want); seg.SetDisplayInfo(di); }
							}
						}
						a_b.ringShown = want;
					}
				} else {
					a_b.ringSegs = 0;
				}
			}
			// a Meter: a multi-frame sprite stepped to frame 1 + value x (frames - 1) - a frame-animated meter, as the game's own
			// level meter is (Norden UI's badge ring: 141 frames). gotoAndStop also stops it playing on its own.
			if (a_shown && a_b.meterFrames != 0) {
				RE::GFxValue meter, total;
				if (widget.GetMember("Meter", &meter) && meter.IsDisplayObject()) {
					if (a_b.meterFrames < 0) {
						a_b.meterFrames = meter.GetMember("_totalframes", &total) && total.IsNumber() ? static_cast<int>(total.GetNumber()) : 0;
					}
					if (a_b.meterFrames > 0) {
						const int frame = 1 + static_cast<int>(std::lround(std::clamp(a_value, 0.0F, 1.0F) * static_cast<float>(a_b.meterFrames - 1)));
						if (frame != a_b.meterShown) {
							RE::GFxValue arg{ static_cast<double>(frame) };
							meter.Invoke("gotoAndStop", nullptr, &arg, 1);
							a_b.meterShown = frame;
						}
					}
				} else {
					a_b.meterFrames = 0;
				}
			}
			if (a_shown && !a_text.empty() && a_text != a_b.text) {   // the Value field, written only when the text changes
				RE::GFxValue field;
				if (widget.GetMember("Value", &field) && field.IsDisplayObject()) {
					field.SetText(a_text.c_str());
					a_b.text = a_text;
				}
			}
			if (a_shown != a_b.shown) {
				RE::GFxValue::DisplayInfo info;
				if (a_b.holder.GetDisplayInfo(&info)) {
					info.SetAlpha(a_shown ? 100.0 : 0.0);
					a_b.holder.SetDisplayInfo(info);
				}
				a_b.shown = a_shown;
			}
		}
	}

	void Tick(RE::GFxMovieView* a_hud, unsigned long long a_frame)
	{
		if (!a_hud) { return; }
		std::scoped_lock l(g_lock);
		const auto& els = hud::Elements();
		if (g_built.empty()) {
			for (std::size_t i = 0; i < els.size(); ++i) {
				if (els[i].swf) { g_built.push_back({ i }); }
			}
		}
		if (a_hud != g_hud) {   // a new HUD movie (a load): every widget is made again
			g_hud = a_hud;
			for (auto& b : g_built) { b = Built{ b.element }; }
		}
		RE::GFxValue base;
		if (!a_hud->GetVariable(&base, "_root.HUDMovieBaseInstance") || !base.IsObject()) { return; }
		// the values ten times a second at 60 fps - and never before a save has finished loading or during a load screen
		// (the HUD advances under the Loading Menu, while the save is still rebuilding the player)
		bool readNow = (a_frame % 6) == 0;
		if (readNow) {
			auto* ui = RE::UI::GetSingleton();
			const int st = g_gameState.load();
			bool ready = st == 2;
			if (st == 0) {   // no load message yet: read once the player stands in a loaded world (coc from the main menu)
				auto* player = RE::PlayerCharacter::GetSingleton();
				ready = player && player->GetParentCell() && player->Is3DLoaded();
			}
			readNow = ready && ui && !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
		}
		for (auto& b : g_built) {
			if (!b.created) {
				if ((a_frame % 30) != 0 || !Create(b, a_hud, base, a_frame)) { continue; }
			}
			Register(b, base);
			if (!readNow) { continue; }
			float       v = 0.0F;
			bool        shown = false;
			std::string text;
			if (b.forced >= 0.0F) {
				v = b.forced;
				shown = true;
			} else {
				shown = ReadValue(els[b.element].key, v, text, base);
			}
			if (b.created && hud::Elements()[b.element].swf2) {
				const int want = StyleOf(b.element);
				if (want != b.style) { Restyle(b, want); }
			}
			Write(b, v, shown, text);
		}
	}

	void SetGameReady(bool a_ready)
	{
		g_gameState = a_ready ? 2 : 1;
		logger::info("widgets: game {} - built widgets {} reading", a_ready ? "ready" : "loading", a_ready ? "start" : "stop");
	}

	std::string StateJson()
	{
		std::scoped_lock l(g_lock);
		std::string out = "[";
		for (const auto& b : g_built) {
			out += std::format(R"({}{{"key":"{}","created":{},"registered":{},"loaded":{},"shown":{},"value":{:.3f},"forced":{:.3f}}})",
				out.size() > 1 ? "," : "", hud::Elements()[b.element].key, b.created, b.registered, b.loaded, b.shown, b.value, b.forced);
		}
		return out + R"(],"casters":)" + g_casters + R"(,"detect":)" + g_detect + R"(,"breath":)" + g_breath;
	}

	std::string LoadUrl(const std::string& a_key, const std::string& a_url)
	{
		std::scoped_lock l(g_lock);
		const auto& els = hud::Elements();
		for (auto& b : g_built) {
			if (a_key != els[b.element].key || !b.created) { continue; }
			RE::GFxValue widget;
			if (!b.holder.GetMember("widget", &widget) || !widget.IsDisplayObject()) { return "no widget child"; }
			RE::GFxValue url{ a_url.c_str() };
			const bool called = widget.Invoke("loadMovie", nullptr, &url, 1);
			b.loaded = false;
			logger::info("widgets: {} test loadMovie(\"{}\") {}", a_key, a_url, called ? "called" : "FAILED");
			return called ? "called" : "FAILED";
		}
		return "no such built widget";
	}

	bool Force(const std::string& a_key, float a_value)
	{
		std::scoped_lock l(g_lock);
		const auto& els = hud::Elements();
		for (auto& b : g_built) {
			if (a_key == els[b.element].key) {
				b.forced = a_value < 0.0F ? -1.0F : std::clamp(a_value, 0.0F, 1.0F);
				logger::info("widgets: {} {}", a_key, b.forced < 0.0F ? "live again" : std::format("held at {:.2f} (test)", b.forced));
				return true;
			}
		}
		return false;
	}
}
