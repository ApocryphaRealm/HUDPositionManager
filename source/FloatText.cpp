#include "FloatText.h"

#include "Settings.h"
#include "utils/Logger.h"

#include <SKSE/SKSE.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <format>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace floattext
{
	namespace
	{
		using clock = std::chrono::steady_clock;
		constexpr int         kPool = 16;
		constexpr const char* kArt = "HUDPositionManager/widgets/floattext.swf";
		constexpr const char* kMarker = "hpmInHudElements";
		// the HUD's modes a clip in HudElements must claim, or the first mode change hides it for good (review H1)
		constexpr std::array  kModes{ "All", "StealthMode", "Swimming", "HorseMode", "WarHorseMode" };
		constexpr float       kLift = 12.0F;   // world units above the head the text starts at

		// 0 no load message yet (coc from the main menu sends none), 1 a save loading, 2 loaded - the built widgets' states
		std::atomic<int> g_ready{ 0 };

		// what the sinks hand the main thread: a text over a form (0 = the player), or a hit to watch
		struct Pending
		{
			RE::FormID  form = 0;
			std::string text;
			float       seconds = 0.0F;
		};
		std::mutex                 g_queueLock;
		std::vector<Pending>       g_pending;
		std::vector<RE::FormID>    g_hitQueue;   // targets the player just hit
		std::atomic<bool>          g_watchHits{ false };   // set by Tick from the settings: the hit sink queues only then

		// the damage watch: a target's health followed after each hit; a loss within two seconds of the player's last hit on
		// it becomes "-N" over it. The health is kept 30 s, so the next blow has a baseline; a target seen for the first time
		// starts from its maximum - whether the game sends the hit event before or after it applies the damage, the first
		// blow on a fresh foe is counted (a foe already hurt by something else reads high on that first blow only)
		struct Watch
		{
			RE::ObjectRefHandle ref;
			float               health = -1.0F;
			clock::time_point   until{};    // numbers are made until then (2 s after the last hit)
			clock::time_point   keep{};     // the baseline is kept until then (30 s)
		};
		std::unordered_map<RE::FormID, Watch> g_watch;   // main thread only

		struct Text
		{
			RE::GFxValue        holder;
			bool                created = false;
			bool                loaded = false;
			bool                failed = false;
			unsigned long long  createdFrame = 0;
			bool                used = false;
			RE::ObjectRefHandle ref;               // the character it rises over (empty: its anchor stays where it was)
			RE::FormID          formId = 0;
			RE::NiPoint3        anchor{};          // the last world point it was drawn over
			std::string         text;
			bool                written = false;   // the text is in the clip
			float               damage = 0.0F;     // a damage number's amount (merged while it is young)
			float               age = 0.0F, life = 1.5F;
			float               alpha = 0.0F;
			float               sx = 0, sy = 0, scale = -1.0F;
		};

		// the pool's clips point into the HUD movie, so the movie is held as long as they are (review M1): declared before
		// the pool, and a swap releases the clips first, then the old movie
		RE::GPtr<RE::GFxMovieView> g_hudRef;
		std::array<Text, kPool>    g_pool;
		std::mutex                 g_lock;   // DevBench reads the pool on its own thread

		class HitSink final : public RE::BSTEventSink<RE::TESHitEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
			{
				if (!a_event || !g_watchHits.load()) { return RE::BSEventNotifyControl::kContinue; }
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!player || a_event->cause.get() != player) { return RE::BSEventNotifyControl::kContinue; }
				const auto* target = a_event->target.get();
				if (target && target != player && target->As<RE::Actor>()) {
					logger::debug("floating text: the player hit {:08X}", target->GetFormID());
					std::lock_guard l(g_queueLock);
					if (g_hitQueue.size() < 64) { g_hitQueue.push_back(target->GetFormID()); }
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// the author API: <form>.SendModEvent("HPM_FloatingText", "text", seconds)
		class ModEventSink final : public RE::BSTEventSink<SKSE::ModCallbackEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
			{
				if (!a_event || std::string_view(a_event->eventName.c_str()) != "HPM_FloatingText") { return RE::BSEventNotifyControl::kContinue; }
				const char* t = a_event->strArg.c_str();
				if (!t || !*t) { return RE::BSEventNotifyControl::kContinue; }
				Add(a_event->sender ? a_event->sender->GetFormID() : 0, t, a_event->numArg);
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		float HealthOf(RE::Actor* a_actor)
		{
			auto* avo = a_actor->AsActorValueOwner();
			return avo ? avo->GetActorValue(RE::ActorValue::kHealth) : -1.0F;
		}

		bool Create(Text& a_t, int a_index, RE::GFxValue& a_base)
		{
			const std::string name = std::format("HPM_FT{}", a_index);
			RE::GFxValue existing;
			if (a_base.GetMember(name.c_str(), &existing) && existing.IsDisplayObject()) {
				a_t.holder = existing;
			} else {
				RE::GFxValue depth;
				double       d = 17000.0 + a_index;
				if (a_base.Invoke("getNextHighestDepth", &depth) && depth.IsNumber()) { d = std::max(d, depth.GetNumber()); }
				std::array<RE::GFxValue, 2> args{ RE::GFxValue{ name.c_str() }, RE::GFxValue{ d } };
				if (!a_base.Invoke("createEmptyMovieClip", &a_t.holder, args.data(), args.size()) || !a_t.holder.IsDisplayObject()) { return false; }
				RE::GFxValue::DisplayInfo info;
				if (a_t.holder.GetDisplayInfo(&info)) {
					info.SetAlpha(0.0);
					a_t.holder.SetDisplayInfo(info);
				}
				RE::GFxValue child;
				std::array<RE::GFxValue, 2> childArgs{ RE::GFxValue{ "widget" }, RE::GFxValue{ 1.0 } };
				if (!a_t.holder.Invoke("createEmptyMovieClip", &child, childArgs.data(), childArgs.size()) || !child.IsDisplayObject()) { return false; }
				RE::GFxValue url{ kArt };
				child.Invoke("loadMovie", nullptr, &url, 1);
			}
			for (const char* mode : kModes) { a_t.holder.SetMember(mode, RE::GFxValue{ true }); }
			RE::GFxValue elements;
			if (a_base.GetMember("HudElements", &elements) && elements.IsArray() && !a_t.holder.HasMember(kMarker)) {
				elements.PushBack(a_t.holder);
				a_t.holder.SetMember(kMarker, RE::GFxValue{ true });
			}
			a_t.created = true;
			return true;
		}

		// a free slot, or the oldest text's when every slot is in use
		Text* Slot()
		{
			Text* oldest = nullptr;
			for (auto& t : g_pool) {
				if (t.failed) { continue; }
				if (!t.used && t.alpha <= 0.0F) { return &t; }
				if (!oldest || t.age > oldest->age) { oldest = &t; }
			}
			return oldest;
		}

		void Start(Text& a_t, RE::TESObjectREFR* a_ref, const std::string& a_text, float a_life, float a_damage)
		{
			a_t.used = true;
			a_t.ref = a_ref ? a_ref->GetHandle() : RE::ObjectRefHandle{};
			a_t.formId = a_ref ? a_ref->GetFormID() : 0;
			if (a_ref) {
				a_t.anchor = a_ref->GetPosition();
				a_t.anchor.z += a_ref->GetHeight() + kLift;
			}
			a_t.text = a_text;
			a_t.written = false;
			a_t.damage = a_damage;
			a_t.age = 0.0F;
			a_t.life = a_life;
		}

		// the damage watch, at the read pass: new hits start (or extend) a watch; each watched target's health loss is "-N"
		void WatchDamage(float a_life)
		{
			std::vector<RE::FormID> hits;
			{
				std::lock_guard l(g_queueLock);
				hits.swap(g_hitQueue);
			}
			const auto now = clock::now();
			for (const auto id : hits) {
				auto* ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(id);
				auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
				if (!actor) { continue; }
				auto& w = g_watch[id];
				if (!w.ref) {
					w.ref = actor->GetHandle();
					auto* avo = actor->AsActorValueOwner();
					w.health = avo ? avo->GetPermanentActorValue(RE::ActorValue::kHealth) +
					                     actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth) :
					                 -1.0F;
				}
				w.until = now + std::chrono::seconds(2);
				w.keep = now + std::chrono::seconds(30);
			}
			for (auto it = g_watch.begin(); it != g_watch.end();) {
				auto ref = it->second.ref.get();
				auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
				if (!actor || now > it->second.keep) {
					it = g_watch.erase(it);
					continue;
				}
				const float h = HealthOf(actor);
				if (now <= it->second.until && it->second.health >= 0.0F && h < it->second.health - 0.5F) {
					const float lost = it->second.health - h;
					// a young damage number over the same character takes this loss too (one number per blow, not per tick)
					auto young = std::ranges::find_if(g_pool, [&](const Text& t) { return t.used && t.formId == it->first && t.damage > 0.0F && t.age < 0.3F; });
					if (young != g_pool.end()) {
						young->damage += lost;
						young->text = std::format("-{:.0f}", young->damage);
						young->written = false;
					} else if (auto* t = Slot()) {
						Start(*t, actor, std::format("-{:.0f}", lost), a_life, lost);
					}
				}
				it->second.health = h;
				++it;
			}
		}
	}

	void Register()
	{
		static HitSink      hits;
		static ModEventSink events;
		if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton()) {
			holder->AddEventSink<RE::TESHitEvent>(&hits);
		}
		if (auto* src = SKSE::GetModCallbackEventSource()) {
			src->AddEventSink(&events);
		}
		logger::info("floating text: hit sink and the ModEvent HPM_FloatingText registered");
	}

	void SetReady(bool a_ready)
	{
		g_ready = a_ready ? 2 : 1;
		if (a_ready) { return; }
		{
			std::lock_guard l(g_queueLock);
			g_pending.clear();
			g_hitQueue.clear();
		}
		std::lock_guard l(g_lock);
		for (auto& t : g_pool) {
			t.used = false;
			t.ref = {};   // a handle from the old game could resolve to another reference in the new one
		}
		g_watch.clear();
	}

	void SimulateHit(RE::FormID a_formId)
	{
		std::lock_guard l(g_queueLock);
		g_hitQueue.push_back(a_formId);
		logger::info("floating text: a hit on {:08X} simulated (test)", a_formId);
	}

	void Add(RE::FormID a_formId, const std::string& a_text, float a_seconds)
	{
		std::lock_guard l(g_queueLock);
		if (g_pending.size() < 32) { g_pending.push_back({ a_formId, a_text, a_seconds }); }
	}

	void Tick(RE::GFxMovieView* a_hud, unsigned long long a_frame, float a_left, float a_top, float a_width, float a_height, bool a_read)
	{
		static settings::FloatingText s;
		if (a_read || a_frame % 60 == 1) { s = settings::Get().ft; }
		g_watchHits = s.enabled && s.damageNumbers;
		std::lock_guard l(g_lock);
		if (!a_hud) { return; }
		if (a_hud != g_hudRef.get()) {
			for (auto& t : g_pool) { t = Text{}; }              // the old movie's clips first ...
			g_hudRef = RE::GPtr<RE::GFxMovieView>{ a_hud };   // ... then the old movie
		}
		auto* ui = RE::UI::GetSingleton();
		const bool loading = !ui || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) || g_ready.load() == 1;
		if (loading) { return; }
		const bool anyUsed = std::ranges::any_of(g_pool, [](const Text& t) { return t.used || t.alpha > 0.0F; });
		if (!s.enabled && !anyUsed) {
			std::lock_guard q(g_queueLock);
			g_pending.clear();
			g_hitQueue.clear();
			return;   // off: nothing read, nothing written (the pool is made only once used)
		}

		// the queue, at the read pass: texts from other mods (and DevBench), and the damage watch
		if (a_read && s.enabled) {
			std::vector<Pending> pending;
			{
				std::lock_guard q(g_queueLock);
				pending.swap(g_pending);
			}
			for (const auto& p : pending) {
				RE::TESObjectREFR* ref = p.form ? RE::TESForm::LookupByID<RE::TESObjectREFR>(p.form) : nullptr;
				if (!ref) { ref = RE::PlayerCharacter::GetSingleton(); }
				if (auto* t = Slot()) { Start(*t, ref, p.text, p.seconds > 0.0F ? std::clamp(p.seconds, 0.5F, 10.0F) : s.seconds, 0.0F); }
			}
			if (s.damageNumbers) { WatchDamage(s.seconds); }
			else { g_watch.clear(); }
		}

		RE::GFxValue base;
		if (!a_hud->GetVariable(&base, "_root.HUDMovieBaseInstance") || !base.IsObject()) { return; }
		RE::GFxValue::DisplayInfo baseInfo;
		const bool   haveBase = base.GetDisplayInfo(&baseInfo);
		const double ox = haveBase ? baseInfo.GetX() : 0.0, oy = haveBase ? baseInfo.GetY() : 0.0;
		const double bsx = haveBase && std::abs(baseInfo.GetXScale()) > 1.0 ? baseInfo.GetXScale() / 100.0 : 1.0;
		const double bsy = haveBase && std::abs(baseInfo.GetYScale()) > 1.0 ? baseInfo.GetYScale() / 100.0 : 1.0;
		auto* cam = RE::Main::WorldRootCamera();
		auto* player = RE::PlayerCharacter::GetSingleton();
		static auto last = clock::now();
		const auto  now = clock::now();
		const float dt = std::clamp(std::chrono::duration<float>(now - last).count(), 0.0F, 0.1F);
		last = now;

		for (int i = 0; i < kPool; ++i) {
			auto& t = g_pool[i];
			if (!t.used && t.alpha <= 0.0F) { continue; }
			if (t.failed) { t.used = false; continue; }
			if (!t.created) {
				if (!Create(t, i, base)) { continue; }
				t.createdFrame = a_frame;
			}
			RE::GFxValue widget;
			const bool   haveWidget = t.holder.GetMember("widget", &widget) && widget.IsDisplayObject();
			if (!t.loaded) {
				RE::GFxValue value;
				t.loaded = haveWidget && widget.GetMember("Value", &value) && value.IsDisplayObject();
				if (!t.loaded) {
					if (a_frame - t.createdFrame > 300) {   // art that never arrives would hold the slot: given up after ~5 s
						t.failed = true;
						t.used = false;
						logger::warn("floating text: text {} - {} never loaded; that slot is not used this session", i, haveWidget ? kArt : "its widget clip");
					}
					continue;
				}
			}
			if (!haveWidget) { continue; }
			if (t.used && !t.written) {
				RE::GFxValue value;
				if (widget.GetMember("Value", &value) && value.IsDisplayObject()) { value.SetText(t.text.c_str()); }
				t.written = true;
			}
			// it follows its character while there is one, then stays where it was last drawn; it rises as it ages
			if (t.used) {
				t.age += dt;
				if (auto ref = t.ref.get(); ref && !ref->IsDisabled()) {
					t.anchor = ref->GetPosition();
					t.anchor.z += ref->GetHeight() + kLift;
				}
				if (t.age >= t.life) { t.used = false; }
			}
			bool onScreen = false;
			// over the player in first person the point above the head is behind the camera: such a text rises from a fixed
			// spot above the crosshair instead (measured 2026-10-04 in HPM Minimal: a text "over the player" never showed)
			auto* pcam = RE::PlayerCamera::GetSingleton();
			const bool overPlayerFP = player && t.formId == player->GetFormID() && pcam && pcam->IsInFirstPerson();
			if (overPlayerFP) {
				onScreen = true;
				const float sx = a_left + 0.5F * a_width;
				const float sy = a_top + 0.40F * a_height - static_cast<float>(s.rise) * t.age;
				const float sc = s.fScale;
				RE::GFxValue::DisplayInfo info;
				if (t.holder.GetDisplayInfo(&info) && (std::abs(sx - t.sx) > 0.1F || std::abs(sy - t.sy) > 0.1F || std::abs(sc - t.scale) > 0.005F)) {
					info.SetPosition((sx - ox) / bsx, (sy - oy) / bsy);
					info.SetScale(sc * 100.0, sc * 100.0);
					t.holder.SetDisplayInfo(info);
					t.sx = sx; t.sy = sy; t.scale = sc;
				}
			} else if (cam && player) {
				float x = 0, y = 0, z = 0;
				if (cam->WorldPtToScreenPt3(t.anchor, x, y, z, 1e-5F) && z > 0.0F && x > -0.05F && x < 1.05F && y > -0.05F && y < 1.05F) {
					onScreen = true;
					const float sx = a_left + x * a_width;
					const float sy = a_top + (1.0F - y) * a_height - static_cast<float>(s.rise) * t.age;
					const float d = player->GetPosition().GetDistance(t.anchor);
					const float sc = s.fScale * (s.scaleWithDistance ? std::clamp(1.0F - d / 4096.0F * 0.5F, 0.5F, 1.0F) : 1.0F);
					RE::GFxValue::DisplayInfo info;
					if (t.holder.GetDisplayInfo(&info) && (std::abs(sx - t.sx) > 0.1F || std::abs(sy - t.sy) > 0.1F || std::abs(sc - t.scale) > 0.005F)) {
						info.SetPosition((sx - ox) / bsx, (sy - oy) / bsy);
						info.SetScale(sc * 100.0, sc * 100.0);
						t.holder.SetDisplayInfo(info);
						t.sx = sx; t.sy = sy; t.scale = sc;
					}
				}
			}
			// full for the first 60 % of its life, then fading to nothing; off screen it is hidden at once
			const float life = std::max(t.life, 0.1F);
			const float target = (t.used && onScreen) ? 100.0F * std::clamp((1.0F - t.age / life) / 0.4F, 0.0F, 1.0F) : 0.0F;
			const float a = t.used ? target : std::max(0.0F, t.alpha - dt * 400.0F);
			if (std::abs(a - t.alpha) > 0.5F || (a == 0.0F && t.alpha != 0.0F)) {
				RE::GFxValue::DisplayInfo info;
				if (t.holder.GetDisplayInfo(&info)) {
					info.SetAlpha(a);
					t.holder.SetDisplayInfo(info);
				}
				t.alpha = a;
			}
		}
	}

	std::string StateJson()
	{
		std::lock_guard l(g_lock);
		std::string out = "[";
		for (int i = 0; i < kPool; ++i) {
			const auto& t = g_pool[i];
			if (!t.used && t.alpha <= 0.0F && !t.failed) { continue; }
			std::string text;
			for (const char c : t.text) { if (c != '"' && c != '\\' && static_cast<unsigned char>(c) >= 0x20) { text += c; } }
			// the FormID stored on the main thread: this (DevBench's) thread never resolves a handle
			out += std::format(R"({}{{"slot":{},"ref":"{:08X}","text":"{}","age":{:.2f},"life":{:.2f},"alpha":{:.0f},"x":{:.1f},"y":{:.1f},"scale":{:.2f},"loaded":{},"failed":{}}})",
				out.size() > 1 ? "," : "", i, t.formId, text, t.age, t.life, t.alpha, t.sx, t.sy, t.scale, t.loaded, t.failed);
		}
		return out + "]";
	}
}
