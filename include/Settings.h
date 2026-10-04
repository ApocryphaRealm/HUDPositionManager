#pragma once

#include "utils/Logger.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

// HUDPositionManager.ini. The settings page (render thread) and the HUD hook (main thread) share
// them through a snapshot under a lock: the page edits a copy and publishes it, the hook reads a
// copy each frame. Saving is debounced - a change is written a moment after the last edit - with
// ordinary file I/O that rewrites only this mod's keys in place, so comments survive (rule 16).
//
// 1.1 (2026-10-04, the owner: "the same controls ... as the Oblivion version"): positions are a PERCENTAGE of the
// screen (fX / fY, as the Oblivion version), Length / Height, Show (in / out of combat), Always visible, Free placement,
// the Combined widgets group and presets. A 1.0 INI's fOffsetX / fOffsetY (HUD units of the 1280x720 stage) are
// converted on load and dropped on the next save.
namespace settings
{
	inline constexpr float kMoveX = 100.0F, kMoveY = 100.0F;   // percent of the screen (the page narrows it to what fits)
	inline constexpr float kScaleMin = 0.25F, kScaleMax = 3.0F;
	inline constexpr float kStageW = 1280.0F, kStageH = 720.0F; // the HUD movie's stage: 1.0's offsets were in these units

	struct ElementSetting
	{
		float x = 0.0F;          // [<key>] fX - right is positive, percent of the screen's width
		float y = 0.0F;          // [<key>] fY - down is positive, percent of the screen's height
		float scale = 1.0F;      // [<key>] fScale - times the size the HUD gives it, about the element's centre
		float stretchX = 1.0F;   // [<key>] fLength - along its width, on top of fScale
		float stretchY = 1.0F;   // [<key>] fHeight - along its height
		bool  hide = false;      // [<key>] bHide
		int   show = 0;          // [<key>] iShow - 0 always (as the game decides), 1 only in combat, 2 only out of combat,
		                         // 3 follow the HUD toggle ([Immersive], phase 3 build 2); build 3: 4 only indoors, 5 only
		                         // outdoors, 6 only with a weapon drawn, 8 only while sneaking (7 is kept for a lock-on target);
		                         // build 4: 9 "when it matters" - the crosshair while aiming / attacking / casting, the sneak eye
		                         // as strong as the detection (Crosshair and StealthMeter only)
		bool  alwaysVisible = false;   // [<key>] bAlwaysVisible - only the elements the game fades on its own
		int   follow = -1;       // [<key>] sMoveWith - moves with another element (its index; -1 = on its own)
		int   style = 0;         // [<key>] iStyle - a built widget with two art styles: 0 the first (Bar), 1 the second (Badge)

		bool IsDefault() const
		{
			return x == 0.0F && y == 0.0F && scale == 1.0F && stretchX == 1.0F && stretchY == 1.0F && !hide && show == 0 && !alwaysVisible && style == 0;
		}
	};

	// the Combined widgets tab: any set of elements moves as one on shared sliders, on top of each one's own
	struct Group
	{
		std::vector<int> members;   // [Group] sMembers - element keys in the INI, indices here
		float            x = 0.0F;  // [Group] fX / fY - percent of the screen
		float            y = 0.0F;
		bool Has(int a_i) const
		{
			for (const int m : members) {
				if (m == a_i) { return true; }
			}
			return false;
		}
	};

	// [Immersive] - the HUD toggle (ImmersiveHUD parity, PHASE3-IHUD-PLAN.md build 2). Off by default.
	struct Immersive
	{
		bool  enabled = false;      // bEnabled - the toggle works (elements on "Follow the HUD toggle" fade with it)
		int   key = 45;             // iToggleKey - DirectInput scan code, X (DEFAULT-KEYS.md); 0 none
		int   button = 0;           // iToggleButton - XInput button mask; 0 none (the controller ships unbound)
		bool  hold = false;         // bHoldMode - shown only while the key is held
		float seconds = 0.0F;       // fDisplaySeconds 0..10 - a press shows the HUD this long; 0 = the press toggles
		bool  startVisible = false; // bStartVisible - shown after a load
		bool  holdBars = true;      // bHoldBarsWhenShown - while shown, the bars the game fades stay up (ImmersiveHUD's "full control")
		bool  inCombat = true;      // bShowInCombat - the toggle's elements also show in combat (build 3)
		bool  weaponDrawn = true;   // bShowWeaponDrawn - ... and while a weapon or spell is drawn
	};

	struct Snapshot
	{
		bool                        enabled = true;        // [General] bEnabled - "Apply my layout"
		// The owner, 2026-09-27: "toggles ... to account for whether they have a UI mod that links all of these bars
		// and HUD widgets together so that they all move as one or separately". Each applies (on) or removes (off)
		// the element table's default "Move with" for its group; the per-tab "Move with" stays for anything custom.
		bool                        linkBars = true;       // [General] bLinkBars - Magicka and Stamina move with Health
		bool                        linkWidgets = true;    // [General] bLinkWidgets - widgets placed around the bars move with them
		bool                        alwaysVisible = false; // [General] bAlwaysVisible - the bars stay shown in play instead of fading
		bool                        unlocked = false;      // [General] bUnlocked - "Free placement": the move sliders go past the screen's edges
		// Phase 3, ImmersiveHUD parity (PHASE3-IHUD-PLAN.md, build 1): Show (in / out of combat) fades instead of hiding at
		// once. Off by default - nothing changes until the player turns it on.
		bool                        fade = false;          // [General] bFade
		int                         fadeIn = 10;           // [General] iFadeInSpeed 1..20 - ImmersiveHUD's scale: half a full fade a second per step
		int                         fadeOut = 5;           // [General] iFadeOutSpeed 1..20
		int                         opacityMin = 0;        // [General] iOpacityMin - percent, an element "hidden" by Show
		int                         opacityMax = 100;      // [General] iOpacityMax - percent, an element shown by Show
		std::vector<ElementSetting> elements;              // in hud::Elements() order
		Group                       group;
		Immersive                   imm;
	};

	// An element's shipped defaults (its "Move with" comes from the element table, when its group's link is on).
	ElementSetting DefaultFor(std::size_t a_index, bool a_linkBars = true, bool a_linkWidgets = true);

	// Apply a group link to a snapshot: every element of the group (HUD bars / widgets with a default "Move with")
	// gets its table default when a_on, or moves on its own when off.
	void ApplyLink(Snapshot& a_s, bool a_widgets, bool a_on);

	void Init(const std::string& a_iniFileName);
	const std::string& GetIniPath();

	Snapshot Get();                       // a copy, any thread
	void     Publish(const Snapshot& a);  // any thread; marks the settings for a save
	void     Update(const std::function<void(Snapshot&)>& a_change);   // any thread: Get, change, clamp, Publish
	void     MaybeSave();                 // main thread, every frame: writes once the edits have settled
	bool     Save();                      // writes now

	// Presets: whole layouts as INI files in Data\SKSE\Plugins\HUDPositionManager\presets\<name>.ini - the element
	// sections plus [General] bAlwaysVisible and the [Group] section, under a [Preset] header (sName, sAuthor, sNote).
	// Other authors ship theirs into that folder; the page lists, loads, saves, updates and deletes them.
	struct PresetInfo
	{
		std::filesystem::path path;
		std::string           name, author, note;
	};
	std::filesystem::path   PresetsFolder();
	std::vector<PresetInfo> ListPresets();                                  // by name, case-insensitively
	bool                    LoadPreset(const std::filesystem::path& a_path);  // applies its layout and saves
	std::filesystem::path   SavePreset(std::string a_name, const std::string& a_author, const std::string& a_note);   // empty on failure
	bool                    UpdatePreset(const std::filesystem::path& a_path);   // the current layout into an existing preset (its header kept)
	bool                    DeletePreset(const std::filesystem::path& a_path);

	namespace debug
	{
		inline logger::level logLevel = logger::level::info;  // ships at info (uLogLevel=2), matching the INI
	}
}
