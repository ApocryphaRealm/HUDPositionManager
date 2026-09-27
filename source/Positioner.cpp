#include "Positioner.h"

#include "Elements.h"
#include "Settings.h"
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
		// One clip of an element. "base" is where the HUD itself puts the clip; our offset and scale ride
		// on top of it. When the HUD moves the clip on its own (a charge meter appearing, the compass
		// shifting for the shout meter), the value we read differs from the one we last wrote, and that
		// value becomes the new base - so we follow the HUD instead of fighting it.
		struct Part
		{
			std::string  path;         // full path from _root
			RE::GFxValue obj;          // cached handle, re-resolved regularly
			bool         found = false;
			bool         haveBase = false;
			double       baseX = 0, baseY = 0, baseXS = 100, baseYS = 100;
			double       centerX = 0, centerY = 0;  // the clip's own centre, in its local units (scale about it)
			bool         touched = false;            // our values are on it now
			double       lastX = 0, lastY = 0, lastXS = 100, lastYS = 100;
			bool         hiddenByUs = false;
			bool         visibleBefore = true;       // what the HUD had when we hid it
		};

		std::vector<std::vector<Part>> g_parts;       // [element][part]
		RE::GFxMovieView*              g_movie = nullptr;
		unsigned long long             g_frame = 0;
		bool                           g_wasEnabled = true;

		std::mutex g_stateLock;
		State      g_state;
		std::atomic<int> g_selected{ -1 };

		// The clip-listing request, answered on the main thread.
		std::mutex              g_listLock;
		std::condition_variable g_listCv;
		int                     g_listDepth = 0;   // 0 = no request pending
		std::string             g_listResult;
		bool                    g_listDone = false;

		constexpr double kEpsPos = 0.2;    // Scaleform stores positions in twips (1/20 px): a read-back can differ by that much
		constexpr double kEpsScale = 0.05;

		void BuildParts()
		{
			g_parts.clear();
			for (const auto& el : hud::Elements()) {
				std::vector<Part> parts;
				for (const char* p : el.parts) {
					Part part;
					part.path = std::string("_root.HUDMovieBaseInstance.") + p;
					parts.push_back(std::move(part));
				}
				g_parts.push_back(std::move(parts));
			}
		}

		bool Resolve(Part& a_part)
		{
			if (!g_movie) { return false; }
			RE::GFxValue v;
			if (g_movie->GetVariable(&v, a_part.path.c_str()) && v.IsDisplayObject()) {
				a_part.obj = v;
				if (!a_part.found) {
					logger::debug("found {} in this HUD", a_part.path);
				}
				a_part.found = true;
				return true;
			}
			if (a_part.found) {
				logger::debug("{} is no longer in the HUD", a_part.path);
			}
			a_part.found = false;
			a_part.haveBase = false;
			a_part.touched = false;
			return false;
		}

		// The clip's own centre in its local coordinates, so a scale change keeps the element centred.
		void MeasureCenter(Part& a_part)
		{
			RE::GFxValue bounds;
			RE::GFxValue self = a_part.obj;
			if (a_part.obj.Invoke("getBounds", &bounds, &self, 1) && bounds.IsObject()) {
				RE::GFxValue a, b, c, d;
				if (bounds.GetMember("xMin", &a) && bounds.GetMember("xMax", &b) && bounds.GetMember("yMin", &c) && bounds.GetMember("yMax", &d) &&
					a.IsNumber() && b.IsNumber() && c.IsNumber() && d.IsNumber()) {
					a_part.centerX = (a.GetNumber() + b.GetNumber()) * 0.5;
					a_part.centerY = (c.GetNumber() + d.GetNumber()) * 0.5;
					return;
				}
			}
			a_part.centerX = a_part.centerY = 0.0;  // no bounds: scale about the clip's registration point instead
		}

		void Restore(Part& a_part, RE::GFxValue::DisplayInfo& a_info)
		{
			a_info.SetPosition(a_part.baseX, a_part.baseY);
			a_info.SetScale(a_part.baseXS, a_part.baseYS);
			if (a_part.hiddenByUs) {
				a_info.SetVisible(a_part.visibleBefore);
				a_part.hiddenByUs = false;
			}
			a_part.obj.SetDisplayInfo(a_info);
			a_part.touched = false;
		}

		void ApplyPart(Part& a_part, const settings::ElementSetting& a_s, bool a_active)
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
				logger::debug("{}: the HUD has it at ({:.1f}, {:.1f}) scale {:.1f}/{:.1f}, centre ({:.1f}, {:.1f})",
							  a_part.path, x, y, xs, ys, a_part.centerX, a_part.centerY);
			} else if (!a_part.touched) {
				a_part.baseX = x; a_part.baseY = y; a_part.baseXS = xs; a_part.baseYS = ys;  // follow the HUD while we are idle
			} else {
				// the HUD moved it since our last write: its new value is the new base
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
			const double s = a_s.scale;
			const double txs = a_part.baseXS * s, tys = a_part.baseYS * s;
			// keep the clip's centre where it was: move by the centre's growth, in the parent's units
			const double tx = a_part.baseX + a_s.offsetX + a_part.centerX * a_part.baseXS / 100.0 * (1.0 - s);
			const double ty = a_part.baseY + a_s.offsetY + a_part.centerY * a_part.baseYS / 100.0 * (1.0 - s);
			bool write = std::abs(x - tx) > kEpsPos || std::abs(y - ty) > kEpsPos || std::abs(xs - txs) > kEpsScale || std::abs(ys - tys) > kEpsScale;
			if (write) {
				info.SetPosition(tx, ty);
				info.SetScale(txs, tys);
			}
			if (a_s.hide && info.GetVisible()) {
				if (!a_part.hiddenByUs) {
					a_part.visibleBefore = true;
				}
				a_part.hiddenByUs = true;
				info.SetVisible(false);
				write = true;
			} else if (!a_s.hide && a_part.hiddenByUs) {
				info.SetVisible(a_part.visibleBefore);
				a_part.hiddenByUs = false;
				write = true;
			}
			if (write) {
				a_part.obj.SetDisplayInfo(info);
			}
			a_part.touched = true;
			a_part.lastX = tx; a_part.lastY = ty; a_part.lastXS = txs; a_part.lastYS = tys;
		}

		bool RootBounds(Part& a_part, RE::GFxValue& a_root, float& a_l, float& a_t, float& a_r, float& a_b)
		{
			RE::GFxValue bounds;
			if (!a_part.obj.Invoke("getBounds", &bounds, &a_root, 1) || !bounds.IsObject()) { return false; }
			RE::GFxValue a, b, c, d;
			if (!(bounds.GetMember("xMin", &a) && bounds.GetMember("xMax", &b) && bounds.GetMember("yMin", &c) && bounds.GetMember("yMax", &d))) { return false; }
			if (!(a.IsNumber() && b.IsNumber() && c.IsNumber() && d.IsNumber())) { return false; }
			a_l = static_cast<float>(a.GetNumber()); a_r = static_cast<float>(b.GetNumber());
			a_t = static_cast<float>(c.GetNumber()); a_b = static_cast<float>(d.GetNumber());
			return a_r > a_l && a_b > a_t && a_r - a_l < 100000.0F;   // an empty clip reports a huge inverted box
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
				const bool  gotInfo = val.GetDisplayInfo(&info);
				Part        tmp;
				tmp.obj = val;
				float l = 0, t = 0, r = 0, b = 0;
				const bool hasBox = RootBounds(tmp, a_root, l, t, r, b);
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
			int depth = 0;
			{
				std::lock_guard lk(g_listLock);
				depth = g_listDepth;
			}
			if (depth <= 0) { return; }
			std::string out = "[";
			RE::GFxValue base, root;
			if (g_movie && g_movie->GetVariable(&base, "_root.HUDMovieBaseInstance") && base.IsObject() && g_movie->GetVariable(&root, "_root")) {
				bool first = true;
				ListInto(out, base, root, "", depth, first);
			}
			out += "]";
			{
				std::lock_guard lk(g_listLock);
				g_listResult = std::move(out);
				g_listDone = true;
				g_listDepth = 0;
			}
			g_listCv.notify_all();
		}
	}

	void Tick(RE::HUDMenu* a_hud)
	{
		++g_frame;
		auto* movie = (a_hud && a_hud->uiMovie) ? a_hud->uiMovie.get() : nullptr;
		if (!movie) {
			static bool logged = false;
			if (!logged) { logger::debug("HUD advanced without a movie; nothing to position yet"); logged = true; }
			return;
		}
		if (movie != g_movie) {
			// a new HUD movie (a load, a new game): everything is found and measured again
			logger::debug("HUD movie {} (was {}); finding the elements again", fmt::ptr(movie), fmt::ptr(g_movie));
			g_movie = movie;
			BuildParts();
		}
		if (g_parts.empty()) { BuildParts(); }

		const settings::Snapshot s = settings::Get();
		if (s.enabled != g_wasEnabled) {
			logger::info("HUD Position Manager {}", s.enabled ? "enabled: the saved layout is applied" : "disabled: every element back where the HUD puts it");
			g_wasEnabled = s.enabled;
		}
		const bool resolveNow = (g_frame % 60) == 1;   // re-resolve handles about once a second (rule 17: a clip can appear later)
		const auto& els = hud::Elements();
		const int   selected = g_selected.load();
		RE::GFxValue root;
		const bool  haveRoot = movie->GetVariable(&root, "_root");
		State       st;
		st.hudSeen = true;
		st.frames = g_frame;
		st.elements.resize(els.size());
		const bool measureAll = (g_frame % 30) == 0;   // bounds for the tool twice a second; the selected one every 6 frames
		for (std::size_t i = 0; i < g_parts.size() && i < els.size(); ++i) {
			const settings::ElementSetting es = i < s.elements.size() ? s.elements[i] : settings::ElementSetting{};
			const bool active = s.enabled && !es.IsDefault();
			auto& es2 = st.elements[i];
			es2.partsTotal = static_cast<int>(g_parts[i].size());
			const bool measure = haveRoot && ((static_cast<int>(i) == selected && g_frame % 6 == 0) || measureAll);
			for (auto& part : g_parts[i]) {
				if ((!part.found && resolveNow) || (part.found && resolveNow)) {
					Resolve(part);
				}
				if (!part.found) { continue; }
				++es2.partsFound;
				ApplyPart(part, es, active);
				float l, t, r, b;
				if (measure && RootBounds(part, root, l, t, r, b)) {
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

	State GetState()
	{
		std::lock_guard lk(g_stateLock);
		return g_state;
	}

	void SetSelected(int a_index) { g_selected.store(a_index); }

	std::string ListClips(int a_depth, int a_timeoutMs)
	{
		std::unique_lock lk(g_listLock);
		g_listDone = false;
		g_listResult.clear();
		g_listDepth = a_depth < 1 ? 1 : (a_depth > 3 ? 3 : a_depth);
		if (!g_listCv.wait_for(lk, std::chrono::milliseconds(a_timeoutMs), [] { return g_listDone; })) {
			g_listDepth = 0;
			return {};
		}
		return g_listResult;
	}
}
