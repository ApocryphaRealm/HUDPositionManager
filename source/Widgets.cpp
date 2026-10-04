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
			// phase 4 build 1: the Phantom (the recent loss) and the Penalty (Survival's reduction, from the right end)
			int          phantomState = -1;   // -1 not looked for, 0 the art has none, 1 it has one
			float        phantom = -1.0F;     // where the phantom stands (0..1)
			std::chrono::steady_clock::time_point phantomHold{};   // it holds still until then, then eases down
			std::chrono::steady_clock::time_point lastWrite{};
			float        penalty = -1.0F;     // the Penalty's fraction written (-1 never)
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
			if (a_key == "BowDraw" || a_key == "ShoutCharge") { return { 0.5F, 0.58F }; }   // where the casting bar is: one at a time
			// HPM's own player bars sit where the game's do (magicka left, health centre, stamina right): hide the game's on
			// their tabs to use these instead
			if (a_key == "BossBars") { return { 0.5F, 0.12F }; }   // top centre, under the compass
			if (a_key == "PlayerHealth") { return { 0.5F, 0.93F }; }
			if (a_key == "PlayerMagicka") { return { 0.2F, 0.93F }; }
			if (a_key == "PlayerStamina") { return { 0.8F, 0.93F }; }
			if (a_key == "CastingBar") { return { 0.5F, 0.58F }; }   // centred, under the crosshair
			if (a_key == "InfoResist") { return { 0.135F, 0.985F }; } // under the health / magicka / stamina bars, their width
			if (a_key == "InfoEquip") { return { 0.85F, 0.85F }; }    // bottom right: the four-way cross
			if (a_key == "InfoPlayTime") { return { 0.88F, 0.66F }; }  // above the equip cross
			if (a_key == "InfoEffects") { return { 0.12F, 0.30F }; }  // top left, under where notifications start
			if (a_key == "SurvHunger") { return { 0.12F, 0.80F }; }   // over the bars, left: hunger, fatigue, cold
			if (a_key == "SurvFatigue") { return { 0.12F, 0.77F }; }
			if (a_key == "SurvCold") { return { 0.12F, 0.74F }; }
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

		// Bow draw: from the draw starting (attack state kBowDraw / kBowAttached) to fully drawn (kBowDrawn), held full
		// until the arrow is loosed. SE keeps no draw amount (PlayerCharacter's currentBowDrawAmount is VR only), so the
		// bar runs against the time the LAST full draw took - measured on every draw, so Quick Shot, a lighter bow or a
		// slowed time are all followed after one shot (the first draw of a session runs against 1 s).
		std::string g_bow = "{}";

		bool ReadBow(float& a_value)
		{
			using A = RE::ATTACK_STATE_ENUM;
			using clock = std::chrono::steady_clock;
			static bool              drawing = false;
			static clock::time_point start;
			static float             full = 1.0F;
			auto*                    player = RE::PlayerCharacter::GetSingleton();
			const auto*              st = player ? player->AsActorState() : nullptr;
			if (!st) { return false; }
			const auto attack = st->GetAttackState();
			const float elapsed = drawing ? std::chrono::duration<float>(clock::now() - start).count() : 0.0F;
			g_bow = std::format(R"({{"attack":{},"elapsed":{:.3f},"full":{:.3f}}})", static_cast<int>(attack), elapsed, full);
			if (attack == A::kBowDraw || attack == A::kBowAttached) {
				if (!drawing) {
					drawing = true;
					start = clock::now();
				}
				a_value = std::clamp(elapsed / full, 0.0F, 0.99F);
				return true;
			}
			if (attack == A::kBowDrawn) {
				if (drawing) {
					full = std::clamp(elapsed, 0.2F, 5.0F);
					drawing = false;
					logger::debug("widgets: bow fully drawn in {:.3f} s", full);
				}
				a_value = 1.0F;
				return true;
			}
			drawing = false;
			return false;
		}

		// Shout charge: while the shout button is held, how far toward the next word - the voice caster (kOther) charging,
		// timed from its start against the game's word times fShoutTime1 (the second word) and fShoutTime2 (the third),
		// as the Casting Bar mod's shout bar shows. Measured in game before the thresholds are trusted (logged once each).
		std::string g_shout = "{}";
		bool        g_voiceSeen = false;

		float Gmst(const char* a_name, float a_fallback)
		{
			auto* gs = RE::GameSettingCollection::GetSingleton();
			auto* s = gs ? gs->GetSetting(a_name) : nullptr;
			return s && s->GetType() == RE::Setting::Type::kFloat ? s->GetFloat() : a_fallback;
		}

		bool ReadShoutCharge(float& a_value)
		{
			using clock = std::chrono::steady_clock;
			static bool              charging = false;
			static clock::time_point start;
			auto*                    player = RE::PlayerCharacter::GetSingleton();
			if (!player) { return false; }
			// which caster charges a shout is measured, not assumed: both the voice (kOther) and instant casters are read
			bool        on = false;
			std::string seen;
			for (const auto source : { RE::MagicSystem::CastingSource::kOther, RE::MagicSystem::CastingSource::kInstant }) {
				auto* caster = player->GetMagicCaster(source);
				if (!caster) { continue; }
				const auto state = caster->state.get();
				seen += std::format(R"({}{{"source":{},"state":{},"spell":{},"timer":{:.3f}}})", seen.empty() ? "" : ",", static_cast<int>(source),
					static_cast<int>(state), caster->currentSpell != nullptr, caster->castingTimer);
				on |= caster->currentSpell && (state == RE::MagicCaster::State::kUnk01 || state == RE::MagicCaster::State::kUnk02 ||
											   state == RE::MagicCaster::State::kCharging || state == RE::MagicCaster::State::kReady);
			}
			static const float t1 = Gmst("fShoutTime1", 0.25F);
			static const float t2 = Gmst("fShoutTime2", 1.0F);
			const float held = charging ? std::chrono::duration<float>(clock::now() - start).count() : 0.0F;
			const float voice = player->GetActorRuntimeData().voiceTimer;
			g_shout = std::format(R"({{"casters":[{}],"voiceTimer":{:.3f},"held":{:.3f},"t1":{:.3f},"t2":{:.3f}}})", seen, voice, held, t1, t2);
			if (voice != 0.0F && !g_voiceSeen) {
				g_voiceSeen = true;
				logger::info("widgets: shout - voiceTimer {:.3f} seen ({})", voice, g_shout);
			}
			if (!on) {
				charging = false;
				return false;
			}
			if (!charging) {
				charging = true;
				start = clock::now();
			}
			a_value = std::clamp(t2 > 0.0F ? held / t2 : 1.0F, 0.0F, 1.0F);
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

		// Any item's count (the same walk as gold, for one form): the base container's count plus the changes' deltas.
		std::int32_t CountItem(RE::TESContainer* a_base, RE::InventoryChanges* a_changes, RE::TESBoundObject* a_item)
		{
			std::int32_t total = 0;
			if (a_base && a_base->containerObjects) {
				for (std::uint32_t i = 0; i < a_base->numContainerObjects; ++i) {
					auto* co = a_base->containerObjects[i];
					if (co && co->obj == a_item) { total += co->count; }
				}
			}
			if (a_changes && a_changes->entryList) {
				for (auto* entry : *a_changes->entryList) {
					if (entry && entry->object == a_item) { total += entry->countDelta; }
				}
			}
			return total;
		}

		std::int32_t CountItemGuarded(RE::TESContainer* a_base, RE::InventoryChanges* a_changes, RE::TESBoundObject* a_item)
		{
			__try {
				return CountItem(a_base, a_changes, a_item);
			} __except (1 /* EXCEPTION_EXECUTE_HANDLER */) {
				return -2;
			}
		}

		// The player's restoring potions by what they restore - [0] health, [1] magicka, [2] stamina: drinkable (not food,
		// not poison), the costliest effect beneficial and on that value. The base container's counts plus the changes'.
		void CountPotions(RE::TESContainer* a_base, RE::InventoryChanges* a_changes, std::int32_t (&a_out)[3])
		{
			auto slot = [](const RE::TESForm* a_f) -> int {
				const auto* p = a_f ? a_f->As<RE::AlchemyItem>() : nullptr;
				if (!p || p->IsFood() || p->IsPoison()) { return -1; }
				const auto* eff = p->GetCostliestEffectItem();
				const auto* mgef = eff ? eff->baseEffect : nullptr;
				if (!mgef || mgef->IsDetrimental()) { return -1; }
				switch (mgef->data.primaryAV) {
				case RE::ActorValue::kHealth:  return 0;
				case RE::ActorValue::kMagicka: return 1;
				case RE::ActorValue::kStamina: return 2;
				default:                       return -1;
				}
			};
			if (a_base && a_base->containerObjects) {
				for (std::uint32_t i = 0; i < a_base->numContainerObjects; ++i) {
					auto* co = a_base->containerObjects[i];
					if (const int s = co ? slot(co->obj) : -1; s >= 0) { a_out[s] += co->count; }
				}
			}
			if (a_changes && a_changes->entryList) {
				for (auto* entry : *a_changes->entryList) {
					if (const int s = entry ? slot(entry->object) : -1; s >= 0) { a_out[s] += entry->countDelta; }
				}
			}
		}

		bool CountPotionsGuarded(RE::TESContainer* a_base, RE::InventoryChanges* a_changes, std::int32_t (&a_out)[3])
		{
			__try {
				CountPotions(a_base, a_changes, a_out);
				return true;
			} __except (1 /* EXCEPTION_EXECUTE_HANDLER */) {
				return false;
			}
		}

		// Phase 4 build 1 - HPM's own player bars ([PlayerBars]). The settings for this pass (Tick copies them once a read)
		// and the side values the bar readers hand to Write: the Survival penalty and the phantom's linger.
		settings::PlayerBars g_pb;
		settings::BossBars   g_bb;
		bool                 g_bossLogged = false;

		// a boss: a dragon (its race's ActorTypeDragon), or an actor placed as its location's boss (the vanilla Boss
		// location ref type, Skyrim.esm 0x130F7 - dungeon bosses, dragon priests, named chiefs; Dragonborn's DLC2Boss1 too)
		bool IsBoss(RE::Actor* a_actor)
		{
			static RE::BGSLocationRefType* boss = RE::TESForm::LookupByID<RE::BGSLocationRefType>(0x000130F7);
			static RE::BGSLocationRefType* boss2 = [] {
				auto* dh = RE::TESDataHandler::GetSingleton();
				return dh ? dh->LookupForm<RE::BGSLocationRefType>(0x0206B5, "Dragonborn.esm") : nullptr;
			}();
			if (!g_bossLogged) {
				g_bossLogged = true;
				logger::info("widgets: boss rules - Boss location ref type {}, DLC2Boss1 {}", boss != nullptr, boss2 != nullptr);
			}
			if (const auto* race = a_actor->GetRace(); race && race->HasKeywordString("ActorTypeDragon")) { return true; }
			if (const auto* x = a_actor->extraList.GetByType<RE::ExtraLocationRefType>(); x && x->locRefType) {
				return x->locRefType == boss || (boss2 && x->locRefType == boss2);
			}
			return false;
		}

		// the boss fighting you: alive, in combat with the player as its target, within fMaxDistance; the nearest. The high
		// process actors are walked twice a second, the chosen boss's health every read.
		bool ReadBoss(float& a_value, std::string& a_text)
		{
			static RE::ActorHandle chosen;
			static int             tick = 0;
			if (!g_bb.enabled) {
				chosen = {};
				return false;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* lists = RE::ProcessLists::GetSingleton();
			if (!player || !lists) { return false; }
			if (++tick >= 5 || !chosen) {
				tick = 0;
				chosen = {};
				float best = g_bb.maxDistance;
				for (auto& handle : lists->highActorHandles) {
					auto actor = handle.get();
					if (!actor || actor.get() == player || actor->IsDead() || !actor->IsInCombat()) { continue; }
					if (actor->GetActorRuntimeData().currentCombatTarget.get().get() != player) { continue; }
					const float d = player->GetPosition().GetDistance(actor->GetPosition());
					if (d >= best || !IsBoss(actor.get())) { continue; }
					best = d;
					chosen = handle;
				}
			}
			auto boss = chosen.get();
			if (!boss || boss->IsDead()) { return false; }
			auto* avo = boss->AsActorValueOwner();
			const float mx = avo ? avo->GetPermanentActorValue(RE::ActorValue::kHealth) + boss->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth) : 0.0F;
			if (!(mx > 0.0F)) { return false; }
			a_value = std::clamp(avo->GetActorValue(RE::ActorValue::kHealth) / mx, 0.0F, 1.0F);
			const char* n = boss->GetDisplayFullName();
			a_text = std::string(n && *n ? n : " ") + '\x1f' + (g_bb.showLevel ? std::to_string(boss->GetLevel()) : std::string(" "));
			return true;
		}
		float                g_penaltyOut = -1.0F;

		struct BarTrack
		{
			float last = -1.0F;
			std::chrono::steady_clock::time_point changed{};
			bool  loggedSurvival = false;
		};
		BarTrack g_bars[3];

		// a bar's value against its UNPENALISED maximum, and the Survival penalty as a fraction of it. The maximum the game
		// shows is base + permanent + temporary modifiers; Survival Mode's needs are (to be measured, rule 30) a negative
		// temporary modifier, so a negative temporary part is the penalty and the bar runs against the max without it.
		bool BarFill(RE::Actor* a_actor, RE::ActorValue a_av, int a_index, bool a_survival, float& a_fill, float& a_penalty, float& a_cur, float& a_max)
		{
			auto* avo = a_actor ? a_actor->AsActorValueOwner() : nullptr;
			if (!avo) { return false; }
			const float cur = avo->GetActorValue(a_av);
			const float perm = avo->GetPermanentActorValue(a_av);
			const float temp = a_actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, a_av);
			const float full = perm + std::max(temp, 0.0F);
			if (!(full > 0.0F)) { return false; }
			a_penalty = a_survival ? std::clamp(-std::min(temp, 0.0F) / full, 0.0F, 1.0F) : 0.0F;
			const float shownMax = a_survival ? full : perm + temp;
			a_fill = std::clamp(cur / (a_survival ? full : std::max(shownMax, 0.001F)), 0.0F, 1.0F);
			a_cur = cur;
			a_max = a_survival ? full : shownMax;
			if (a_survival && !g_bars[a_index].loggedSurvival) {
				g_bars[a_index].loggedSurvival = true;
				logger::info("widgets: player bar {} under Survival - base {:.1f}, permanent {:.1f}, temporary {:.1f}, current {:.1f}",
					a_index, avo->GetBaseActorValue(a_av), perm, temp, cur);
			}
			return true;
		}

		bool SurvivalOn();   // below, with the Survival widgets

		bool ReadPlayerBar(int a_index, float& a_value, std::string& a_text)
		{
			if (!g_pb.enabled) { return false; }
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) { return false; }
			static constexpr RE::ActorValue kAv[3]{ RE::ActorValue::kHealth, RE::ActorValue::kMagicka, RE::ActorValue::kStamina };
			const bool survival = g_pb.survivalPenalty && SurvivalOn();
			// the three bars' state, read together: "when another bar shows" needs them all
			float fill[3]{}, pen[3]{}, cur[3]{}, mx[3]{};
			bool  ok[3]{};
			const auto now = std::chrono::steady_clock::now();
			for (int i = 0; i < 3; ++i) {
				RE::Actor* who = player;
				RE::NiPointer<RE::Actor> mount;
				if (i == 2 && g_pb.mountStamina && player->GetMount(mount) && mount) { who = mount.get(); }   // riding: the horse's stamina
				ok[i] = BarFill(who, kAv[i], i, survival, fill[i], pen[i], cur[i], mx[i]);
				if (ok[i] && std::abs(fill[i] - g_bars[i].last) > 0.0005F) {
					g_bars[i].last = fill[i];
					g_bars[i].changed = now;
				}
			}
			auto dynamic = [&](int i) {
				return ok[i] && (fill[i] < 0.999F || pen[i] > 0.0F || now - g_bars[i].changed < std::chrono::seconds(3));
			};
			const int  modes[3]{ g_pb.healthMode, g_pb.magickaMode, g_pb.staminaMode };
			const int  i = a_index;
			bool       shown = false;
			switch (modes[i]) {
			case 0: shown = false; break;
			case 1: shown = dynamic(i); break;
			case 2: shown = player->IsInCombat(); break;
			case 3: shown = dynamic(0) || dynamic(1) || dynamic(2); break;
			default: shown = true; break;
			}
			if (!ok[i] || !shown) { return false; }
			a_value = fill[i];
			g_penaltyOut = pen[i];
			a_text = g_pb.showValues ? std::format("{:.0f} / {:.0f}", std::max(cur[i], 0.0F), mx[i]) : std::string(" ");
			return true;
		}

		constexpr char kSep = '\x1f';   // between a multi-line widget's fields: Value, Value2, Value3 ...

		// Resistances: fire, frost, shock, magic, poison, disease as whole percents, then the armor rating and the speed
		// (%) - STB Widgets' resist widget's eight, in its order.
		bool ReadResist(float& a_value, std::string& a_text)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* avo = player ? player->AsActorValueOwner() : nullptr;
			if (!avo) { return false; }
			constexpr RE::ActorValue kAvs[6]{ RE::ActorValue::kResistFire, RE::ActorValue::kResistFrost, RE::ActorValue::kResistShock,
				RE::ActorValue::kResistMagic, RE::ActorValue::kPoisonResist, RE::ActorValue::kResistDisease };
			a_text.clear();
			for (int i = 0; i < 6; ++i) {
				if (i) { a_text += kSep; }
				a_text += std::format("{:.0f}%", avo->GetActorValue(kAvs[i]));
			}
			a_text += kSep + std::format("{:.0f}", avo->GetActorValue(RE::ActorValue::kDamageResist));
			a_text += kSep + std::format("{:.0f}%", avo->GetActorValue(RE::ActorValue::kSpeedMult));
			a_value = 0.0F;
			return true;
		}

		// Equipped: right hand, left hand, the shout or power, the ammo with its count ("-" for an empty slot).
		// An item's type as an icon frame (1-based) - the frames of the STB equip widget's icon sprite, which Norden UI
		// reskins (read from its art, 2026-10-04): 1 fist, 2 dagger, 4 sword, 6 war axe, 7 mace, 10 greatsword,
		// 11 battleaxe, 12 warhammer, 16 bow, 18 crossbow, 20 heavy shield, 21 light shield, 22..26 the schools
		// (alteration, conjuration, destruction, illusion, restoration), 27 scroll, 28 staff. 0 = no icon.
		int IconFrame(const RE::TESForm* a_f)
		{
			if (!a_f) { return 0; }
			if (const auto* w = a_f->As<RE::TESObjectWEAP>()) {
				switch (w->GetWeaponType()) {
				case RE::WEAPON_TYPE::kHandToHandMelee: return 1;
				case RE::WEAPON_TYPE::kOneHandDagger:   return 2;
				case RE::WEAPON_TYPE::kOneHandSword:    return 4;
				case RE::WEAPON_TYPE::kOneHandAxe:      return 6;
				case RE::WEAPON_TYPE::kOneHandMace:     return 7;
				case RE::WEAPON_TYPE::kTwoHandSword:    return 10;
				case RE::WEAPON_TYPE::kTwoHandAxe:      return w->HasKeywordString("WeapTypeWarhammer") ? 12 : 11;
				case RE::WEAPON_TYPE::kBow:             return 16;
				case RE::WEAPON_TYPE::kCrossbow:        return 18;
				case RE::WEAPON_TYPE::kStaff:           return 28;
				default:                                return 0;
				}
			}
			if (const auto* a = a_f->As<RE::TESObjectARMO>()) { return a->IsHeavyArmor() ? 20 : 21; }
			if (a_f->GetFormType() == RE::FormType::Scroll) { return 27; }
			if (const auto* sp = a_f->As<RE::SpellItem>()) {
				const auto* eff = sp->GetCostliestEffectItem();
				const auto* mgef = eff ? eff->baseEffect : nullptr;
				switch (mgef ? mgef->GetMagickSkill() : RE::ActorValue::kNone) {
				case RE::ActorValue::kAlteration:  return 22;
				case RE::ActorValue::kConjuration: return 23;
				case RE::ActorValue::kDestruction: return 24;
				case RE::ActorValue::kIllusion:    return 25;
				case RE::ActorValue::kRestoration: return 26;
				default:                           return 24;
				}
			}
			return 0;   // a torch, or anything else: its name, no icon
		}

		// One field of a multi-line widget with its icon's frame: "\x1d<frame>\x1d<text>" - Write steps IconN to that frame
		// (a multi-frame icon) and hides it at 0.
		std::string WithIcon(int a_frame, const std::string& a_text) { return "\x1d" + std::to_string(a_frame) + "\x1d" + a_text; }

		// Equipped: right hand, left hand, the shout or power, the ammo with its count - each with its icon frame; an empty
		// slot is empty (no text, no icon), and the ammo shows only with a bow or crossbow in hand, as STB Widgets does.
		// Then the restoring potions carried: health, magicka, stamina ("x3"; STB's potion icon frames 1, 3, 2).
		bool ReadEquip(float& a_value, std::string& a_text)
		{
			static int         tick = 0;
			static std::string last;
			auto*              player = RE::PlayerCharacter::GetSingleton();
			if (!player) { return false; }
			auto name = [](const RE::TESForm* a_f) -> std::string {
				const char* n = a_f ? a_f->GetName() : nullptr;
				return n && *n ? n : std::string{};
			};
			if (last.empty() || ++tick >= 5) {   // twice a second: the ammo count walks the inventory
				tick = 0;
				auto*       right = player->GetEquippedObject(false);
				auto*       left = player->GetEquippedObject(true);
				const int   rightFrame = right ? IconFrame(right) : 0;
				std::string ammo = WithIcon(0, {});
				auto*       a = player->GetCurrentAmmo();
				if (a && (rightFrame == 16 || rightFrame == 18)) {
					RE::TESNPC* npc = player->GetActorBase();
					const auto n = CountItemGuarded(npc ? static_cast<RE::TESContainer*>(npc) : nullptr, player->GetInventoryChanges(), a);
					ammo = WithIcon(1, n >= 0 ? std::format("{} x{}", name(a), n) : name(a));
				}
				std::int32_t pots[3]{};
				RE::TESNPC*  base = player->GetActorBase();
				if (!CountPotionsGuarded(base ? static_cast<RE::TESContainer*>(base) : nullptr, player->GetInventoryChanges(), pots)) {
					pots[0] = pots[1] = pots[2] = 0;
				}
				auto pot = [](std::int32_t a_n, int a_frame) { return a_n > 0 ? WithIcon(a_frame, std::format("x{}", a_n)) : WithIcon(0, {}); };
				auto* power = player->GetActorRuntimeData().selectedPower;
				const int powerFrame = !power ? 0 : (power->Is(RE::FormType::Shout) ? 1 : 2);
				last = WithIcon(rightFrame, name(right)) + kSep + WithIcon(left && left != right ? IconFrame(left) : 0, left != right ? name(left) : std::string{}) +
				       kSep + WithIcon(powerFrame, name(power)) + kSep + ammo + kSep + pot(pots[0], 1) + kSep + pot(pots[1], 3) + kSep +
				       pot(pots[2], 2);
			}
			a_text = last;
			a_value = 0.0F;
			return true;
		}

		// Play time: the game's own count of real hours played (the GetRealHoursPassed condition function - saved with the
		// game, nothing of ours to store).
		bool ReadPlayTime(float& a_value, std::string& a_text)
		{
			static RE::SCRIPT_FUNCTION* fn = RE::SCRIPT_FUNCTION::LocateScriptCommand("GetRealHoursPassed");
			auto*                       player = RE::PlayerCharacter::GetSingleton();
			if (!fn || !fn->conditionFunction || !player) { return false; }
			double hours = 0.0;
			if (!fn->conditionFunction(player, nullptr, nullptr, hours) || hours < 0.0) { return false; }
			const auto mins = static_cast<long long>(hours * 60.0);
			a_text = std::format("{}h {:02}m", mins / 60, mins % 60);
			a_value = 0.0F;
			return true;
		}

		// Active effects: the player's timed effects (not hidden in the UI, not dispelled or inactive), soonest to end first,
		// up to six lines "Name  m:ss". Shown only while there is one.
		bool ReadEffects(float& a_value, std::string& a_text)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* target = player ? player->AsMagicTarget() : nullptr;
			auto* list = target ? target->GetActiveEffectList() : nullptr;
			if (!list) { return false; }
			std::vector<std::pair<float, std::string>> rows;
			for (auto* ae : *list) {
				if (!ae || ae->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled)) { continue; }
				const auto* mgef = ae->GetBaseObject();
				if (!mgef || mgef->data.flags.all(RE::EffectSetting::EffectSettingData::Flag::kHideInUI)) { continue; }
				if (!(ae->duration > 0.0F)) { continue; }
				const float left = std::max(0.0F, ae->duration - ae->elapsedSeconds);
				const char* n = mgef->GetFullName();
				const int   s = static_cast<int>(std::ceil(left));
				rows.emplace_back(left, std::format("{}  {}:{:02}", n && *n ? n : "?", s / 60, s % 60));
			}
			if (rows.empty()) { return false; }
			std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
			a_text.clear();
			for (std::size_t i = 0; i < 6; ++i) {
				if (i) { a_text += kSep; }
				a_text += i < rows.size() ? rows[i].second : std::string{};
			}
			a_value = 0.0F;
			return true;
		}

		// Survival Mode's needs - hunger, fatigue (exhaustion) and cold - as bars, shown only while Survival Mode is on
		// (its Survival_ModeEnabled global). The globals are found by form ID in its plugin; what was found is logged once.
		// The fill is how far the need has gone: the value against its largest (Survival Mode's top stage).
		// by Survival Mode's own form ID (ccQDRSSE001-SurvivalMode.esl - the editor IDs are not kept on 1.5.97: every
		// lookup by them came back empty with the plugin loaded, 2026-10-04), else by editor ID
		RE::TESGlobal* FindGlobal(RE::FormID a_local, std::initializer_list<const char*> a_ids)
		{
			if (auto* dh = RE::TESDataHandler::GetSingleton()) {
				if (auto* g = dh->LookupForm<RE::TESGlobal>(a_local, "ccQDRSSE001-SurvivalMode.esl")) { return g; }
			}
			for (const char* id : a_ids) {
				if (auto* g = RE::TESForm::LookupByEditorID<RE::TESGlobal>(id)) { return g; }
			}
			return nullptr;
		}

		RE::TESGlobal* SurvivalEnabledGlobal();

		RE::TESGlobal* SurvivalEnabledGlobal()
		{
			static bool           looked = false;
			static RE::TESGlobal* g = nullptr;
			if (!looked) {
				looked = true;
				g = FindGlobal(0x826, { "Survival_ModeEnabled" });
			}
			return g;
		}

		bool SurvivalOn()
		{
			auto* g = SurvivalEnabledGlobal();
			return g && g->value >= 0.5F;
		}

		bool ReadSurvival(int a_which, float& a_value)
		{
			static bool           looked = false;
			RE::TESGlobal*        enabled = SurvivalEnabledGlobal();
			static RE::TESGlobal* need[3]{};
			static RE::TESGlobal* most[3]{};
			if (!looked) {
				looked = true;
				need[0] = FindGlobal(0x81A, { "Survival_HungerNeedValue" });
				need[1] = FindGlobal(0x816, { "Survival_ExhaustionNeedValue" });
				need[2] = FindGlobal(0x81B, { "Survival_ColdNeedValue" });
				most[0] = FindGlobal(0x80C, { "Survival_HungerNeedMaxValue" });
				most[1] = FindGlobal(0x84A, { "Survival_ExhaustionNeedMaxValue" });
				most[2] = FindGlobal(0x84B, { "Survival_ColdNeedMaxValue" });
				logger::info("widgets: survival - enabled global {}, hunger {}/{}, fatigue {}/{}, cold {}/{}", enabled != nullptr,
							 need[0] != nullptr, most[0] != nullptr, need[1] != nullptr, most[1] != nullptr, need[2] != nullptr, most[2] != nullptr);
			}
			if (!enabled || enabled->value < 0.5F || !need[a_which]) { return false; }
			const float top = most[a_which] && most[a_which]->value > 0.0F ? most[a_which]->value : 1000.0F;
			a_value = std::clamp(need[a_which]->value / top, 0.0F, 1.0F);
			return true;
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
			if (a_key == "PlayerHealth") { return ReadPlayerBar(0, a_value, a_text); }
			if (a_key == "BossBars") { return ReadBoss(a_value, a_text); }
			if (a_key == "PlayerMagicka") { return ReadPlayerBar(1, a_value, a_text); }
			if (a_key == "PlayerStamina") { return ReadPlayerBar(2, a_value, a_text); }
			if (a_key == "BowDraw") { return ReadBow(a_value); }
			if (a_key == "ShoutCharge") { return ReadShoutCharge(a_value); }
			if (a_key == "InfoResist") { return ReadResist(a_value, a_text); }
			if (a_key == "InfoEquip") { return ReadEquip(a_value, a_text); }
			if (a_key == "InfoPlayTime") { return ReadPlayTime(a_value, a_text); }
			if (a_key == "InfoEffects") { return ReadEffects(a_value, a_text); }
			if (a_key == "SurvHunger") { return ReadSurvival(0, a_value); }
			if (a_key == "SurvFatigue") { return ReadSurvival(1, a_value); }
			if (a_key == "SurvCold") { return ReadSurvival(2, a_value); }
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
			// the Phantom: holds the old value for fPhantomSeconds after a drop, then eases down to the fill (a full bar a
			// second); a rise takes it up at once
			if (a_shown && a_b.phantomState != 0) {
				RE::GFxValue ph;
				RE::GFxValue::DisplayInfo pi;
				if (widget.GetMember("Phantom", &ph) && ph.IsDisplayObject() && ph.GetDisplayInfo(&pi)) {
					a_b.phantomState = 1;
					const auto  now = std::chrono::steady_clock::now();
					const float dt = a_b.lastWrite.time_since_epoch().count() ? std::clamp(std::chrono::duration<float>(now - a_b.lastWrite).count(), 0.0F, 0.1F) : 0.0F;
					const float v = std::clamp(a_value, 0.0F, 1.0F);
					if (!g_pb.phantom || a_b.phantom < 0.0F || v >= a_b.phantom) {
						a_b.phantom = v;
						a_b.phantomHold = now + std::chrono::milliseconds(static_cast<int>(g_pb.phantomSeconds * 1000.0F));
					} else if (now >= a_b.phantomHold) {
						a_b.phantom = std::max(v, a_b.phantom - dt);
					}
					if (std::abs(pi.GetXScale() - a_b.phantom * 100.0) > 0.05) {
						pi.SetScale(a_b.phantom * 100.0, pi.GetYScale());
						ph.SetDisplayInfo(pi);
					}
					a_b.lastWrite = now;
				} else {
					a_b.phantomState = 0;
				}
			}
			// the Penalty (Survival's reduction): registered on its RIGHT edge, so its _xscale grows from the bar's end
			if (a_shown && g_penaltyOut >= 0.0F && std::abs(g_penaltyOut - a_b.penalty) > 0.001F) {
				RE::GFxValue pen;
				RE::GFxValue::DisplayInfo di;
				if (widget.GetMember("Penalty", &pen) && pen.IsDisplayObject() && pen.GetDisplayInfo(&di)) {
					di.SetScale(g_penaltyOut * 100.0, di.GetYScale());
					pen.SetDisplayInfo(di);
				}
				a_b.penalty = g_penaltyOut;
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
			if (a_shown && !a_text.empty() && a_text != a_b.text) {   // Value (Value2, Value3 ... for a multi-line widget), only on change
				std::size_t start = 0;
				int         fields = 0, used = 0;
				for (int n = 1;; ++n) {
					const auto end = a_text.find(kSep, start);
					std::string part = a_text.substr(start, end == std::string::npos ? std::string::npos : end - start);
					const std::string suffix = n == 1 ? "" : std::to_string(n);
					if (part.size() > 2 && part[0] == '\x1d') {   // the field's icon frame (WithIcon)
						const auto close = part.find('\x1d', 1);
						const int  frame = close == std::string::npos ? 0 : std::atoi(part.substr(1, close - 1).c_str());
						part = close == std::string::npos ? std::string{} : part.substr(close + 1);
						RE::GFxValue icon, total;
						RE::GFxValue::DisplayInfo ii;
						if (widget.GetMember(("Icon" + suffix).c_str(), &icon) && icon.IsDisplayObject() && icon.GetDisplayInfo(&ii)) {
							ii.SetVisible(frame > 0);
							icon.SetDisplayInfo(ii);
							if (frame > 0 && icon.GetMember("_totalframes", &total) && total.IsNumber() && total.GetNumber() > 1) {
								RE::GFxValue arg{ static_cast<double>(frame) };
								icon.Invoke("gotoAndStop", nullptr, &arg, 1);
							}
						}
					}
					RE::GFxValue field;
					const std::string name = "Value" + suffix;
					if (widget.GetMember(name.c_str(), &field) && field.IsDisplayObject()) { field.SetText(part.c_str()); }
					fields = n;
					if (!part.empty()) { used = n; }
					if (end == std::string::npos) { break; }
					start = end + 1;
				}
				// a list (active effects) whose last rows are empty - one effect of six - has its Frame cut to the rows in use,
				// from the top down - the widget was centred on the full Frame, so its top row stays put
				if (fields > 1 && std::string_view(hud::Elements()[a_b.element].key) == "InfoEffects") {
					RE::GFxValue frame;
					RE::GFxValue::DisplayInfo fi;
					if (widget.GetMember("Frame", &frame) && frame.IsDisplayObject() && frame.GetDisplayInfo(&fi)) {
						fi.SetScale(fi.GetXScale(), 100.0 * std::max(used, 1) / fields);
						frame.SetDisplayInfo(fi);
					}
				}
				a_b.text = a_text;
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
		if (readNow) {   // the player bars' and the boss bar's settings for this pass
			const auto snap = settings::Get();
			g_pb = snap.pb;
			g_bb = snap.bb;
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
				g_penaltyOut = -1.0F;
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
		return out + R"(],"casters":)" + g_casters + R"(,"detect":)" + g_detect + R"(,"breath":)" + g_breath + R"(,"bow":)" + g_bow +
		       R"(,"shout":)" + g_shout;
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
