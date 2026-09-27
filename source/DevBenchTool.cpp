#include "DevBenchTool.h"

#include "DevBench/DevBenchAPI.h"
#include "Elements.h"
#include "Positioner.h"
#include "Settings.h"
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
			std::string out = std::format(R"({{"ok":true,"op":"state","enabled":{},"highlight":{},"linkBars":{},"linkWidgets":{},"hudSeen":{},"frames":{},"elements":[)",
										  s.enabled, s.highlight, s.linkBars, s.linkWidgets, st.hudSeen, st.frames);
			for (std::size_t i = 0; i < els.size(); ++i) {
				const auto e = i < s.elements.size() ? s.elements[i] : settings::ElementSetting{};
				const auto x = i < st.elements.size() ? st.elements[i] : positioner::ElementState{};
				out += std::format(R"({}{{"key":"{}","menu":"{}","open":{},"found":{},"parts":{},"offsetX":{:.1f},"offsetY":{:.1f},"scale":{:.2f},"hide":{},"follow":"{}","box":{}}})",
								   i ? "," : "", els[i].key, els[i].menu ? els[i].menu : "HUD Menu", x.menuOpen, x.partsFound, x.partsTotal, e.offsetX, e.offsetY, e.scale, e.hide,
								   (e.follow >= 0 && static_cast<std::size_t>(e.follow) < els.size()) ? els[static_cast<std::size_t>(e.follow)].key : "",
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
				// {element, offsetX?, offsetY?, scale?, hide?} - any subset; or {enabled} / {highlight}
				auto s = settings::Get();
				const std::string key = Field(json, "element");
				try {
					if (const auto v = Field(json, "enabled"); !v.empty()) { s.enabled = (v == "true" || v == "1"); }
					if (const auto v = Field(json, "highlight"); !v.empty()) { s.highlight = (v == "true" || v == "1"); }
					if (const auto v = Field(json, "linkBars"); !v.empty()) { s.linkBars = (v == "true" || v == "1"); settings::ApplyLink(s, false, s.linkBars); }
					if (const auto v = Field(json, "linkWidgets"); !v.empty()) { s.linkWidgets = (v == "true" || v == "1"); settings::ApplyLink(s, true, s.linkWidgets); }
					if (!key.empty()) {
						const int idx = hud::IndexOf(key);
						if (idx < 0 || static_cast<std::size_t>(idx) >= s.elements.size()) {
							a_write(a_sink, R"({"ok":false,"error":"unknown element"})");
							return;
						}
						auto& e = s.elements[static_cast<std::size_t>(idx)];
						if (const auto v = Field(json, "offsetX"); !v.empty()) { e.offsetX = std::stof(v); }
						if (const auto v = Field(json, "offsetY"); !v.empty()) { e.offsetY = std::stof(v); }
						if (const auto v = Field(json, "scale"); !v.empty()) { e.scale = std::stof(v); }
						if (const auto v = Field(json, "hide"); !v.empty()) { e.hide = (v == "true" || v == "1"); }
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
			"HUD has it and its box in stage units. op=set {element, offsetX, offsetY, scale, hide} changes an element "
			"(any subset), follow (another element's key, \\\"\\\" for none), or {enabled}/{highlight}; applied on the next HUD frame and saved once edits settle. op=reset puts "
			"every element back. op=save writes the INI now. op=clips {depth 1-3, menu} lists the running HUD movie's clips under "
			"_root.HUDMovieBaseInstance - or, with menu, that open menu's clips under _root - with position, scale, visibility and "
			"box: the research op for mapping element names. op=strings reports the active language.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"},\"element\":{\"type\":\"string\"},"
			"\"offsetX\":{\"type\":\"number\"},\"offsetY\":{\"type\":\"number\"},\"scale\":{\"type\":\"number\"},\"hide\":{\"type\":\"boolean\"},"
			"\"enabled\":{\"type\":\"boolean\"},\"highlight\":{\"type\":\"boolean\"},\"depth\":{\"type\":\"number\"},"
			"\"follow\":{\"type\":\"string\"},\"menu\":{\"type\":\"string\"}}},"
			"\"readOnly\":false"
			"}";
		if (devBench->RegisterTool("hud.position", descriptor, &Tool, nullptr)) {
			logger::info("Registered \"hud.position\" with DevBench (build {})", devBench->GetBuildNumber());
			registered = true;
		}
	}
}
