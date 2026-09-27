#include "Settings.h"

#include "Elements.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <sstream>

namespace settings
{
	namespace
	{
		std::string g_iniPath;
		std::mutex  g_lock;
		Snapshot    g_snap;
		bool        g_dirty = false;
		std::chrono::steady_clock::time_point g_lastEdit{};

		logger::level ClampLevel(long a_raw)
		{
			return (a_raw >= 0 && a_raw <= static_cast<long>(logger::level::off)) ? static_cast<logger::level>(a_raw)
																				   : logger::level::info;
		}

		std::string Trim(std::string a_s)
		{
			while (!a_s.empty() && (a_s.back() == ' ' || a_s.back() == '\t' || a_s.back() == '\r')) { a_s.pop_back(); }
			std::size_t i = 0;
			while (i < a_s.size() && (a_s[i] == ' ' || a_s[i] == '\t')) { ++i; }
			return a_s.substr(i);
		}

		std::string Num(float a_v)
		{
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.2f", a_v);
			std::string s{ buf };
			while (!s.empty() && s.back() == '0') { s.pop_back(); }
			if (!s.empty() && s.back() == '.') { s.pop_back(); }
			return s.empty() ? "0" : s;
		}

		// Every (section, key, value) this mod owns, from a snapshot.
		std::vector<std::tuple<std::string, std::string, std::string>> Rows(const Snapshot& a_s)
		{
			std::vector<std::tuple<std::string, std::string, std::string>> rows;
			rows.emplace_back("General", "bEnabled", a_s.enabled ? "1" : "0");
			rows.emplace_back("General", "bHighlight", a_s.highlight ? "1" : "0");
			const auto& els = hud::Elements();
			for (std::size_t i = 0; i < els.size() && i < a_s.elements.size(); ++i) {
				const auto& e = a_s.elements[i];
				rows.emplace_back(els[i].key, "fOffsetX", Num(e.offsetX));
				rows.emplace_back(els[i].key, "fOffsetY", Num(e.offsetY));
				rows.emplace_back(els[i].key, "fScale", Num(e.scale));
				rows.emplace_back(els[i].key, "bHide", e.hide ? "1" : "0");
				rows.emplace_back(els[i].key, "sMoveWith", (e.follow >= 0 && static_cast<std::size_t>(e.follow) < els.size()) ? els[static_cast<std::size_t>(e.follow)].key : "");
			}
			return rows;
		}

		void Load()
		{
			Snapshot s;
			s.elements.assign(hud::Elements().size(), {});
			std::ifstream in(g_iniPath);
			if (!in) {
				logger::warn("{} not found; every element starts where the HUD puts it", g_iniPath);
			}
			std::string line, section;
			while (std::getline(in, line)) {
				line = Trim(line);
				if (line.empty() || line[0] == ';' || line[0] == '#') { continue; }
				if (line.front() == '[' && line.back() == ']') { section = line.substr(1, line.size() - 2); continue; }
				const auto eq = line.find('=');
				if (eq == std::string::npos) { continue; }
				const std::string key = Trim(line.substr(0, eq));
				std::string       val = Trim(line.substr(eq + 1));
				if (const auto sc = val.find(';'); sc != std::string::npos) { val = Trim(val.substr(0, sc)); }
				try {
					if (section == "General") {
						if (key == "bEnabled") { s.enabled = std::stol(val) != 0; }
						else if (key == "bHighlight") { s.highlight = std::stol(val) != 0; }
						else if (key == "uLogLevel") { debug::logLevel = ClampLevel(std::stol(val)); }
						continue;
					}
					const int idx = hud::IndexOf(section);
					if (idx < 0) { continue; }
					auto& e = s.elements[static_cast<std::size_t>(idx)];
					if (key == "fOffsetX") { e.offsetX = std::stof(val); }
					else if (key == "fOffsetY") { e.offsetY = std::stof(val); }
					else if (key == "fScale") { e.scale = std::stof(val); }
					else if (key == "bHide") { e.hide = std::stol(val) != 0; }
					else if (key == "sMoveWith") { e.follow = val.empty() ? -1 : hud::IndexOf(val); if (e.follow == idx) { e.follow = -1; } }
				} catch (...) {
					logger::warn("{}: [{}] {}={} is not a number; kept the default", g_iniPath, section, key, val);
				}
			}
			for (auto& e : s.elements) {
				if (!(e.scale > 0.05F && e.scale < 10.0F)) {
					e.scale = 1.0F;  // a scale of 0 or garbage would make the element vanish; 1 is the HUD's own size
				}
			}
			std::lock_guard lk(g_lock);
			g_snap = std::move(s);
			g_dirty = false;
			logger::debug("settings loaded from {}: enabled={}, highlight={}, {} elements", g_iniPath, g_snap.enabled, g_snap.highlight, g_snap.elements.size());
		}
	}

	void Init(const std::string& a_iniFileName)
	{
		g_iniPath = "Data/SKSE/Plugins/" + a_iniFileName;
		Load();
	}

	const std::string& GetIniPath() { return g_iniPath; }

	Snapshot Get()
	{
		std::lock_guard lk(g_lock);
		return g_snap;
	}

	void Publish(const Snapshot& a_s)
	{
		std::lock_guard lk(g_lock);
		g_snap = a_s;
		g_dirty = true;
		g_lastEdit = std::chrono::steady_clock::now();
	}

	void MaybeSave()
	{
		{
			std::lock_guard lk(g_lock);
			if (!g_dirty || std::chrono::steady_clock::now() - g_lastEdit < std::chrono::milliseconds(1500)) {
				return;
			}
		}
		Save();
	}

	bool Save()
	{
		Snapshot s;
		{
			std::lock_guard lk(g_lock);
			s = g_snap;
			g_dirty = false;
		}
		// Read the file, replace each of our keys in place (comments and unknown keys stay), add what
		// is missing at the end of its section, and write it back with ordinary file I/O - usvfs
		// redirects that correctly, where the Win32 profile API silently writes nowhere (rule 16).
		std::vector<std::string> lines;
		{
			std::ifstream in(g_iniPath);
			std::string   l;
			while (std::getline(in, l)) {
				if (!l.empty() && l.back() == '\r') { l.pop_back(); }
				lines.push_back(l);
			}
		}
		for (const auto& [sec, key, val] : Rows(s)) {
			std::string current;
			int         sectionEnd = -1;
			bool        inSec = false, done = false;
			for (std::size_t i = 0; i < lines.size(); ++i) {
				const std::string t = Trim(lines[i]);
				if (!t.empty() && t.front() == '[' && t.back() == ']') {
					inSec = (t.substr(1, t.size() - 2) == sec);
					if (inSec) { sectionEnd = static_cast<int>(i); }
					continue;
				}
				if (!inSec) { continue; }
				sectionEnd = static_cast<int>(i);
				const auto eq = t.find('=');
				if (eq != std::string::npos && Trim(t.substr(0, eq)) == key) {
					lines[i] = key + "=" + val;
					done = true;
					break;
				}
			}
			if (done) { continue; }
			if (sectionEnd < 0) {
				if (!lines.empty() && !Trim(lines.back()).empty()) { lines.emplace_back(); }
				lines.push_back("[" + sec + "]");
				lines.push_back(key + "=" + val);
			} else {
				lines.insert(lines.begin() + sectionEnd + 1, key + "=" + val);
			}
		}
		std::ofstream out(g_iniPath, std::ios::binary | std::ios::trunc);
		if (!out) {
			logger::error("Could not write {}; the settings stay for this session only", g_iniPath);
			return false;
		}
		for (const auto& l : lines) { out << l << "\r\n"; }
		logger::debug("settings saved to {}", g_iniPath);
		return true;
	}
}
