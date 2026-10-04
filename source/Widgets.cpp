#include "Widgets.h"

#include "Elements.h"
#include "utils/Logger.h"

#include <RE/Skyrim.h>

#include <algorithm>
#include <array>
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
			unsigned long long createdFrame = 0;
		};

		std::vector<Built>    g_built;
		RE::GFxMovieView*     g_hud = nullptr;
		std::mutex            g_lock;      // the DevBench tool reads g_built on its own thread

		// where a widget first sits, as a fraction of the HUD's VISIBLE stage (its centre); the positioner's offset rides on
		// top. HUDMovieBaseInstance's origin is near the stage's centre (measured 2026-10-04: 616.65, 475.45 on a 1280x960
		// stage), so a position in its space is the stage point less that origin - a fixed local point drew off screen.
		struct Spot { float fx, fy; };
		Spot DefaultSpot(const std::string& a_key)
		{
			if (a_key == "Breath") { return { 0.5F, 0.80F }; }       // centred, above the bars' row
			if (a_key == "CastingBar") { return { 0.5F, 0.58F }; }   // centred, under the crosshair
			if (a_key == "Detection") { return { 0.5F, 0.42F }; }    // centred, over the crosshair (the sneak eye's place)
			return { 0.5F, 0.5F };
		}

		// ------------------------------------------------------------------ the values (game data, main thread)

		// Breath: the time the player has been under water against the game's breath allowance. Shown only under water,
		// and never with water breathing. The allowance is the fActorSwimBreathBase game setting (20 s when unread).
		bool ReadBreath(float& a_value)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) { return false; }
			const auto& rt = player->GetActorRuntimeData();
			const bool under = rt.boolFlags.all(RE::Actor::BOOL_FLAGS::kUnderwater);
			if (!under) { return false; }
			if (auto* avo = player->AsActorValueOwner(); avo && avo->GetActorValue(RE::ActorValue::kWaterBreathing) > 0.0F) { return false; }
			float allowance = 20.0F;
			if (auto* gs = RE::GameSettingCollection::GetSingleton()) {
				if (auto* s = gs->GetSetting("fActorSwimBreathBase"); s && s->GetFloat() > 0.0F) { allowance = s->GetFloat(); }
			}
			a_value = std::clamp(1.0F - rt.underWaterTimer / allowance, 0.0F, 1.0F);
			static bool logged = false;
			if (!logged) {
				logged = true;
				logger::info("widgets: breath first read under water - underWaterTimer {:.2f} s of fActorSwimBreathBase {:.2f} s", rt.underWaterTimer, allowance);
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

		bool ReadValue(const std::string& a_key, float& a_value, RE::GFxValue& a_base)
		{
			if (a_key == "Breath") { return ReadBreath(a_value); }
			if (a_key == "CastingBar") { return ReadCasting(a_value); }
			if (a_key == "Detection") { return ReadDetection(a_value, a_base); }
			return false;
		}

		// ------------------------------------------------------------------ the clips

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
				RE::GFxValue url{ el.swf };
				const bool called = child.Invoke("loadMovie", nullptr, &url, 1);
				logger::info("widgets: {} created at depth {:.0f}, loadMovie(\"{}\") {}", el.key, d, el.swf, called ? "called" : "FAILED");
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

		void Write(Built& a_b, float a_value, bool a_shown)
		{
			RE::GFxValue widget;
			if (!a_b.holder.GetMember("widget", &widget) || !widget.IsDisplayObject()) { return; }
			if (!a_b.loaded) {
				RE::GFxValue fill;
				a_b.loaded = widget.GetMember("Fill", &fill) && fill.IsDisplayObject();
				if (!a_b.loaded) { return; }   // the SWF is still loading
				// centre the art on its spot: the holder's origin was the spot, the art's is its top-left corner
				RE::GFxValue::DisplayInfo w, h;
				if (widget.GetDisplayInfo(&w) && a_b.holder.GetDisplayInfo(&h)) {
					RE::GFxValue width, height;
					if (widget.GetMember("_width", &width) && widget.GetMember("_height", &height) && width.IsNumber() && height.IsNumber()) {
						w.SetPosition(-width.GetNumber() / 2.0, -height.GetNumber() / 2.0);
						widget.SetDisplayInfo(w);
					}
				}
				logger::info("widgets: {} art loaded", hud::Elements()[a_b.element].key);
			}
			if (a_shown && std::abs(a_value - a_b.value) > 0.002F) {
				RE::GFxValue fill;
				RE::GFxValue::DisplayInfo info;
				if (widget.GetMember("Fill", &fill) && fill.GetDisplayInfo(&info)) {
					info.SetScale(std::clamp(a_value, 0.0F, 1.0F) * 100.0, info.GetYScale());
					fill.SetDisplayInfo(info);
				}
				a_b.value = a_value;
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
		const bool readNow = (a_frame % 6) == 0;   // the values ten times a second at 60 fps
		for (auto& b : g_built) {
			if (!b.created) {
				if ((a_frame % 30) != 0 || !Create(b, a_hud, base, a_frame)) { continue; }
			}
			Register(b, base);
			if (!readNow) { continue; }
			float v = 0.0F;
			bool  shown = false;
			if (b.forced >= 0.0F) {
				v = b.forced;
				shown = true;
			} else {
				shown = ReadValue(els[b.element].key, v, base);
			}
			Write(b, v, shown);
		}
	}

	std::string StateJson()
	{
		std::scoped_lock l(g_lock);
		std::string out = "[";
		for (const auto& b : g_built) {
			out += std::format(R"({}{{"key":"{}","created":{},"registered":{},"loaded":{},"shown":{},"value":{:.3f},"forced":{:.3f}}})",
				out.size() > 1 ? "," : "", hud::Elements()[b.element].key, b.created, b.registered, b.loaded, b.shown, b.value, b.forced);
		}
		return out + R"(],"casters":)" + g_casters + R"(,"detect":)" + g_detect;
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
