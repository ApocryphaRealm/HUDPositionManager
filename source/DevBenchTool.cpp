#include "DevBenchTool.h"

#include "DevBench/DevBenchAPI.h"
#include "Elements.h"
#include "Immersive.h"
#include "ActorBars.h"
#include "Page.h"
#include "Positioner.h"
#include "Settings.h"
#include "Widgets.h"
#include "utils/Logger.h"
#include "utils/Strings.h"

#include <format>
#include <string>
#include <string_view>

// "hud.position": observe and DRIVE HUD Position Manager without a keypress (rules 31 and 64). The
// handler runs on DevBench's own thread, so it only reads the snapshots the HUD hook leaves and
// publishes settings the hook picks up; the clip listing is answered on the main thread.
namespace DevBenchTool
{
	namespace
	{
		// Minimal JSON field reader (no parser dependency): "key":"value" or "key":value.
		std::string Field(std::string_view a_json, std::string_view a_key)
		{
			const std::string needle = std::string("\"") + std::string(a_key) + "\"";
			auto pos = a_json.find(needle);
			if (pos == std::string_view::npos) { return {}; }
			pos = a_json.find(':', pos + needle.size());
			if (pos == std::string_view::npos) { return {}; }
			++pos;
			while (pos < a_json.size() && (a_json[pos] == ' ' || a_json[pos] == '\t')) { ++pos; }
			if (pos < a_json.size() && a_json[pos] == '"') {
				const auto end = a_json.find('"', pos + 1);
				return std::string(a_json.substr(pos + 1, end - pos - 1));
			}
			const auto end = a_json.find_first_of(",}", pos);
			std::string raw{ a_json.substr(pos, end - pos) };
			while (!raw.empty() && (raw.back() == ' ' || raw.back() == '\t')) { raw.pop_back(); }
			return raw;
		}

		std::string StateJson()
		{
			const auto s = settings::Get();
			const auto st = positioner::GetState();
			const auto& els = hud::Elements();
			std::string members;
			for (const int m : s.group.members) {
				if (m >= 0 && static_cast<std::size_t>(m) < els.size()) { members += (members.empty() ? "" : ",") + std::string(els[static_cast<std::size_t>(m)].key); }
			}
			std::string out = std::format(R"({{"ok":true,"op":"state","enabled":{},"linkBars":{},"linkWidgets":{},"alwaysVisible":{},"unlocked":{},"fade":{{"on":{},"in":{},"out":{},"min":{},"max":{}}},"immersive":{},"toggleShown":{},"context":{{"interior":{},"weapon":{},"sneak":{}}},"inCombat":{},"group":{{"members":"{}","x":{:.2f},"y":{:.2f}}},"stage":[{:.1f},{:.1f},{:.1f},{:.1f}],"hudSeen":{},"frames":{},"elements":[)",
										  s.enabled, s.linkBars, s.linkWidgets, s.alwaysVisible, s.unlocked, s.fade, s.fadeIn, s.fadeOut, s.opacityMin,
										  s.opacityMax, immersive::StateJson(), st.toggleShown, st.interior, st.weaponDrawn, st.sneaking, st.inCombat, members,
										  s.group.x, s.group.y,
										  st.stageLeft, st.stageTop, st.stageW, st.stageH, st.hudSeen, st.frames);
			for (std::size_t i = 0; i < els.size(); ++i) {
				const auto e = i < s.elements.size() ? s.elements[i] : settings::ElementSetting{};
				const auto x = i < st.elements.size() ? st.elements[i] : positioner::ElementState{};
				out += std::format(R"({}{{"key":"{}","menu":"{}","open":{},"found":{},"parts":{},"x":{:.2f},"y":{:.2f},"scale":{:.2f},"length":{:.2f},"height":{:.2f},"hide":{},"show":{},"alwaysVisible":{},"follow":"{}","applied":[{:.1f},{:.1f}],"hiddenByShow":{},"alphaHeld":{},"fade":{:.3f},"alphaMul":{:.3f},"box":{}}})",
								   i ? "," : "", els[i].key, els[i].menu ? els[i].menu : "HUD Menu", x.menuOpen, x.partsFound, x.partsTotal, e.x, e.y, e.scale, e.stretchX, e.stretchY, e.hide,
								   e.show, e.alwaysVisible,
								   (e.follow >= 0 && static_cast<std::size_t>(e.follow) < els.size()) ? els[static_cast<std::size_t>(e.follow)].key : "",
								   x.appliedX, x.appliedY, x.hiddenByShow, x.alphaHeld, x.fade, x.alphaMul,
								   x.hasBounds ? std::format("[{:.1f},{:.1f},{:.1f},{:.1f}]", x.xMin, x.yMin, x.xMax, x.yMax) : std::string("null"));
			}
			return out + "]}";
		}

		void Tool(void*, const char* a_argsJson, void* a_sink, DevBenchAPI::WriteFn a_write)
		{
			const std::string_view json = a_argsJson ? a_argsJson : "";
			const std::string      op = Field(json, "op");

			if (op.empty() || op == "state") {
				a_write(a_sink, StateJson().c_str());
				return;
			}
			if (op == "set") {
				// {element, x?, y?, scale?, length?, height?, hide?, show?, alwaysVisible?, follow?} - any subset; or the switches
				auto s = settings::Get();
				const std::string key = Field(json, "element");
				try {
					if (const auto v = Field(json, "enabled"); !v.empty()) { s.enabled = (v == "true" || v == "1"); }
					if (const auto v = Field(json, "linkBars"); !v.empty()) { s.linkBars = (v == "true" || v == "1"); settings::ApplyLink(s, false, s.linkBars); }
					if (const auto v = Field(json, "linkWidgets"); !v.empty()) { s.linkWidgets = (v == "true" || v == "1"); settings::ApplyLink(s, true, s.linkWidgets); }
					if (const auto v = Field(json, "alwaysVisible"); !v.empty() && key.empty()) { s.alwaysVisible = (v == "true" || v == "1"); }
					if (const auto v = Field(json, "unlocked"); !v.empty()) { s.unlocked = (v == "true" || v == "1"); }
					if (const auto v = Field(json, "fade"); !v.empty()) { s.fade = (v == "true" || v == "1"); }
					if (const auto v = Field(json, "immersive"); !v.empty()) { s.imm.enabled = (v == "true" || v == "1"); }
					if (const auto v = Field(json, "toggleKey"); !v.empty()) { s.imm.key = std::stoi(v); }
					if (const auto v = Field(json, "toggleButton"); !v.empty()) { s.imm.button = std::stoi(v); }
					if (const auto v = Field(json, "hold"); !v.empty()) { s.imm.hold = (v == "true" || v == "1"); }
					if (const auto v = Field(json, "displaySeconds"); !v.empty()) { s.imm.seconds = std::stof(v); }
					if (const auto v = Field(json, "startVisible"); !v.empty()) { s.imm.startVisible = (v == "true" || v == "1"); }
					if (const auto v = Field(json, "holdBars"); !v.empty()) { s.imm.holdBars = (v == "true" || v == "1"); }
					if (const auto v = Field(json, "showInCombat"); !v.empty()) { s.imm.inCombat = (v == "true" || v == "1"); }
					if (const auto v = Field(json, "playerBars"); !v.empty()) { s.pb.enabled = (v == "true" || v == "1"); }
					if (const auto v = Field(json, "infoBars"); !v.empty()) { s.ib.enabled = (v == "true" || v == "1"); }
					if (const auto v = Field(json, "bossBars"); !v.empty()) { s.bb.enabled = (v == "true" || v == "1"); }
					if (const auto v = Field(json, "ibOthers"); !v.empty()) { s.ib.others = std::stoi(v); }
					if (const auto v = Field(json, "ibMaxDistance"); !v.empty()) { s.ib.maxDistance = std::stof(v); }
					if (const auto v = Field(json, "pbMode"); !v.empty()) { s.pb.healthMode = s.pb.magickaMode = s.pb.staminaMode = std::stoi(v); }
					if (const auto v = Field(json, "pbValues"); !v.empty()) { s.pb.showValues = (v == "true" || v == "1"); }
					if (const auto v = Field(json, "pbPhantomSeconds"); !v.empty()) { s.pb.phantomSeconds = std::stof(v); }
					if (const auto v = Field(json, "showWeaponDrawn"); !v.empty()) { s.imm.weaponDrawn = (v == "true" || v == "1"); }
					if (const auto v = Field(json, "fadeIn"); !v.empty()) { s.fadeIn = std::stoi(v); }
					if (const auto v = Field(json, "fadeOut"); !v.empty()) { s.fadeOut = std::stoi(v); }
					if (const auto v = Field(json, "opacityMin"); !v.empty()) { s.opacityMin = std::stoi(v); }
					if (const auto v = Field(json, "opacityMax"); !v.empty()) { s.opacityMax = std::stoi(v); }
					if (const auto v = Field(json, "groupX"); !v.empty()) { s.group.x = std::stof(v); }
					if (const auto v = Field(json, "groupY"); !v.empty()) { s.group.y = std::stof(v); }
					if (json.find("\"groupMembers\"") != std::string_view::npos) {
						s.group.members.clear();
						std::string list = Field(json, "groupMembers");
						for (std::size_t p = 0; p <= list.size();) {
							const auto c = list.find(',', p);
							const std::string k = list.substr(p, (c == std::string::npos ? list.size() : c) - p);
							if (const int idx = hud::IndexOf(k); idx >= 0) { s.group.members.push_back(idx); }
							if (c == std::string::npos) { break; }
							p = c + 1;
						}
					}
					if (!key.empty()) {
						const int idx = hud::IndexOf(key);
						if (idx < 0 || static_cast<std::size_t>(idx) >= s.elements.size()) {
							a_write(a_sink, R"({"ok":false,"error":"unknown element"})");
							return;
						}
						auto& e = s.elements[static_cast<std::size_t>(idx)];
						if (const auto v = Field(json, "x"); !v.empty()) { e.x = std::stof(v); }
						if (const auto v = Field(json, "y"); !v.empty()) { e.y = std::stof(v); }
						if (const auto v = Field(json, "scale"); !v.empty()) { e.scale = std::stof(v); }
						if (const auto v = Field(json, "length"); !v.empty()) { e.stretchX = std::stof(v); }
						if (const auto v = Field(json, "height"); !v.empty()) { e.stretchY = std::stof(v); }
						if (const auto v = Field(json, "hide"); !v.empty()) { e.hide = (v == "true" || v == "1"); }
						if (const auto v = Field(json, "show"); !v.empty()) { e.show = std::stoi(v); }
						if (const auto v = Field(json, "alwaysVisible"); !v.empty()) { e.alwaysVisible = (v == "true" || v == "1"); }
						if (const auto v = Field(json, "style"); !v.empty()) { e.style = (v == "1") ? 1 : 0; }   // a two-style widget (Level: 0 Bar, 1 Badge)
						if (json.find("\"follow\"") != std::string_view::npos) {
							const auto v = Field(json, "follow");
							e.follow = v.empty() ? -1 : hud::IndexOf(v);
						}
					}
				} catch (...) {
					a_write(a_sink, R"({"ok":false,"error":"a value is not a number"})");
					return;
				}
				settings::Publish(s);
				a_write(a_sink, R"({"ok":true,"op":"set"})");
				return;
			}
			if (op == "reset") {
				auto s = settings::Get();
				for (std::size_t i = 0; i < s.elements.size(); ++i) { s.elements[i] = settings::DefaultFor(i, s.linkBars, s.linkWidgets); }
				settings::Publish(s);
				a_write(a_sink, R"({"ok":true,"op":"reset"})");
				return;
			}
			if (op == "widgets") {
				a_write(a_sink, (std::string(R"({"ok":true,"op":"widgets","widgets":)") + widgets::StateJson() + "}").c_str());
				return;
			}
			if (op == "loadWidget") {   // {element, url} - test: load another SWF into a built widget
				const auto r = widgets::LoadUrl(Field(json, "element"), Field(json, "url"));
				a_write(a_sink, std::format(R"({{"ok":true,"op":"loadWidget","result":"{}"}})", r).c_str());
				return;
			}
			if (op == "forceWidget") {   // {element, value 0..1 held and shown, -1 live}
				float v = -1.0F;
				try { if (const auto f = Field(json, "value"); !f.empty()) { v = std::stof(f); } } catch (...) {}
				const bool ok = widgets::Force(Field(json, "element"), v);
				a_write(a_sink, std::format(R"({{"ok":{},"op":"forceWidget"}})", ok).c_str());
				return;
			}
			if (op == "toggle") {   // the HUD toggle as its key would: {} a press and release, {down:1} / {down:0} for hold mode
				const auto d = Field(json, "down");
				if (d.empty()) {
					immersive::Simulate(true);
					immersive::Simulate(false);
				} else {
					immersive::Simulate(d == "1" || d == "true");
				}
				a_write(a_sink, (std::string(R"({"ok":true,"op":"toggle","immersive":)") + immersive::StateJson() + "}").c_str());
				return;
			}
			if (op == "immersiveAll") {   // {on} - every element on "Always" goes on the toggle (or every toggle element back)
				const auto v = Field(json, "on");
				const bool on = v.empty() || v == "1" || v == "true";
				const int  n = page::PutAllOnToggle(on);
				a_write(a_sink, std::format(R"({{"ok":true,"op":"immersiveAll","on":{},"changed":{}}})", on, n).c_str());
				return;
			}
			if (op == "bars") {   // the info bars in use: which character, its fill, where on the screen
				a_write(a_sink, (std::string(R"({"ok":true,"op":"bars","bars":)") + actorbars::StateJson() + "}").c_str());
				return;
			}
			if (op == "pinNearest") {   // {on} - a test: the nearest character always gets a bar
				const auto v = Field(json, "on");
				actorbars::PinNearest(v.empty() || v == "1" || v == "true");
				a_write(a_sink, R"({"ok":true,"op":"pinNearest"})");
				return;
			}
			if (op == "forceContext") {   // {interior, weapon, sneak, aim, eye}: -1 the game's own, 0 / 1 forced (eye: a frame 1..101)
				auto f = [&](const char* k) { int v = -1; try { if (const auto s = Field(json, k); !s.empty()) { v = std::stoi(s); } } catch (...) {} return v; };
				positioner::ForceContext(f("interior"), f("weapon"), f("sneak"));
				positioner::ForceAimEye(f("aim"), f("eye"));
				a_write(a_sink, R"({"ok":true,"op":"forceContext"})");
				return;
			}
			if (op == "forceCombat") {
				int v = -1;
				try { if (const auto f = Field(json, "value"); !f.empty()) { v = std::stoi(f); } } catch (...) {}
				positioner::ForceCombat(v);
				a_write(a_sink, std::format(R"({{"ok":true,"op":"forceCombat","value":{}}})", v).c_str());
				return;
			}
			if (op == "range") {   // the move sliders' range on the element tab the page last drew (Free placement widens it)
				const auto r = page::LastOpenRange();
				a_write(a_sink, r.valid ? std::format(R"({{"ok":true,"op":"range","element":"{}","unlocked":{},"minX":{:.2f},"maxX":{:.2f},"minY":{:.2f},"maxY":{:.2f}}})",
					r.element, r.unlocked, r.minX, r.maxX, r.minY, r.maxY).c_str()
					: R"({"ok":false,"error":"no element tab drawn yet - open the page's Layout tab"})");
				return;
			}
			if (op == "presets") {
				std::string list;
				for (const auto& p : settings::ListPresets()) {
					list += std::format(R"({}{{"name":"{}","author":"{}","path":"{}"}})", list.empty() ? "" : ",", p.name, p.author, p.path.generic_string());
				}
				a_write(a_sink, (std::string(R"({"ok":true,"op":"presets","presets":[)") + list + "]}").c_str());
				return;
			}
			if (op == "savePreset") {
				const auto path = settings::SavePreset(Field(json, "name"), Field(json, "author"), Field(json, "note"));
				a_write(a_sink, std::format(R"({{"ok":{},"op":"savePreset","path":"{}"}})", !path.empty(), path.generic_string()).c_str());
				return;
			}
			if (op == "loadPreset" || op == "deletePreset") {
				const std::filesystem::path path = Field(json, "path");
				const bool ok = op == "loadPreset" ? settings::LoadPreset(path) : settings::DeletePreset(path);
				a_write(a_sink, std::format(R"({{"ok":{},"op":"{}"}})", ok, op).c_str());
				return;
			}
			if (op == "save") {
				a_write(a_sink, std::format(R"({{"ok":{},"op":"save","path":"{}"}})", settings::Save(), settings::GetIniPath()).c_str());
				return;
			}
			if (op == "clips") {
				int depth = 1;
				try { if (const auto v = Field(json, "depth"); !v.empty()) { depth = std::stoi(v); } } catch (...) {}
				const std::string list = positioner::ListClips(Field(json, "menu"), depth, 2000);
				if (list.empty()) {
					a_write(a_sink, R"j({"ok":false,"op":"clips","error":"the HUD did not advance within 2 s (no game loaded, or the game is paused)"})j");
				} else {
					a_write(a_sink, (std::string(R"({"ok":true,"op":"clips","clips":)") + list + "}").c_str());
				}
				return;
			}
			if (op == "strings") {
				a_write(a_sink, std::format(R"({{"ok":true,"op":"strings","strings":{}}})", strings::StatusJson()).c_str());
				return;
			}
			a_write(a_sink, R"({"ok":false,"error":"unknown op"})");
		}
	}

	void Init(bool a_lastAttempt)
	{
		static bool registered = false;
		if (registered) { return; }
		DevBenchAPI::IDevBenchInterface001* devBench = DevBenchAPI::GetDevBenchInterface001();
		if (!devBench) {
			if (a_lastAttempt) {
				logger::info("DevBench not detected; skipping the \"hud.position\" tool");
			}
			return;
		}
		constexpr const char* descriptor =
			"{"
			"\"description\":\"Observe and drive HUD Position Manager. op=state: settings, and per element whether the running "
			"HUD has it, its box in stage units, what Show / Always visible are doing. op=set {element, x, y (percent of the screen), "
			"scale, length, height, hide, show (0 always, 1 only in combat, 2 only out of combat), alwaysVisible, follow (another "
			"element's key, empty for none)} changes an element (any subset), or the switches {enabled, linkBars, linkWidgets, "
			"alwaysVisible, unlocked, groupMembers (comma-separated keys), groupX, groupY}; applied on the next HUD frame and saved "
			"once edits settle. op=forceCombat {value -1 the game's, 0 out, 1 in} (test). op=range: the move sliders' range on the "
			"element tab the page last drew. op=presets / savePreset {name} / loadPreset {path} / deletePreset {path}. op=reset puts "
			"every element back. op=save writes the INI now. op=clips {depth 1-3, menu} lists the running HUD movie's clips under "
			"_root.HUDMovieBaseInstance - or, with menu, that open menu's clips under _root - with position, scale, visibility and "
			"box: the research op for mapping element names. op=strings reports the active language.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"},\"element\":{\"type\":\"string\"},"
			"\"x\":{\"type\":\"number\"},\"y\":{\"type\":\"number\"},\"scale\":{\"type\":\"number\"},\"hide\":{\"type\":\"boolean\"},"
			"\"length\":{\"type\":\"number\"},\"height\":{\"type\":\"number\"},\"show\":{\"type\":\"number\"},\"value\":{\"type\":\"number\"},"
			"\"name\":{\"type\":\"string\"},\"path\":{\"type\":\"string\"},\"groupMembers\":{\"type\":\"string\"},"
			"\"enabled\":{\"type\":\"boolean\"},\"depth\":{\"type\":\"number\"},"
			"\"follow\":{\"type\":\"string\"},\"menu\":{\"type\":\"string\"}}},"
			"\"readOnly\":false"
			"}";
		if (devBench->RegisterTool("hud.position", descriptor, &Tool, nullptr)) {
			logger::info("Registered \"hud.position\" with DevBench (build {})", devBench->GetBuildNumber());
			registered = true;
		}
	}
}
