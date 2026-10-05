#pragma once

#include <RE/Skyrim.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

// B7 ([Colors]): HPM's own bars recoloured on the player's word. A colour is "RRGGBB" (TrueHUD's notation); an empty
// string leaves the art's own colour, so a reskin (Norden UI) keeps its look until the player picks a colour.
namespace tint
{
	inline std::optional<std::uint32_t> Parse(const std::string& a_hex)
	{
		std::string h = a_hex;
		if (!h.empty() && h.front() == '#') { h.erase(0, 1); }
		if (h.size() != 6) { return std::nullopt; }
		try { return static_cast<std::uint32_t>(std::stoul(h, nullptr, 16)); } catch (...) { return std::nullopt; }
	}

	// a solid colour (multiply 0, add 0..255 - the pattern Wheeler Refined's icon tint uses), or the art's own (identity)
	inline void Apply(RE::GFxValue& a_clip, const std::string& a_hex)
	{
		if (!a_clip.IsDisplayObject()) { return; }
		RE::GRenderer::Cxform cx{};
		std::memset(&cx, 0, sizeof(cx));
		cx.matrix[RE::GRenderer::Cxform::kA][RE::GRenderer::Cxform::kMult] = 1.0F;
		if (const auto rgb = Parse(a_hex)) {
			cx.matrix[RE::GRenderer::Cxform::kR][RE::GRenderer::Cxform::kAdd] = static_cast<float>((*rgb >> 16) & 0xFF);
			cx.matrix[RE::GRenderer::Cxform::kG][RE::GRenderer::Cxform::kAdd] = static_cast<float>((*rgb >> 8) & 0xFF);
			cx.matrix[RE::GRenderer::Cxform::kB][RE::GRenderer::Cxform::kAdd] = static_cast<float>(*rgb & 0xFF);
		} else {
			for (int c = 0; c < 3; ++c) { cx.matrix[c][RE::GRenderer::Cxform::kMult] = 1.0F; }
		}
		a_clip.SetCxform(cx);
	}

	// a named child of a widget
	inline void ApplyTo(RE::GFxValue& a_widget, const char* a_child, const std::string& a_hex)
	{
		RE::GFxValue c;
		if (a_widget.GetMember(a_child, &c)) { Apply(c, a_hex); }
	}
}
