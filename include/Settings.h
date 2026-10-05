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
		bool  crossHideAiming = false;    // [Crosshair] bHideWhileAiming - the crosshair hides while a bow is drawn or an aimed spell charges
		bool  crossHideSneaking = false;  // [Crosshair] bHideWhileSneaking - ... and while sneaking (the sneak eye sits over it)
	};

	// [PlayerBars] - phase 4 build 1 (TrueHUD parity, PHASE4-TRUEHUD-PLAN.md): HPM's own Health / Magicka / Stamina bars,
	// with the recent loss (phantom) and the Survival penalty. Off by default.
	struct PlayerBars
	{
		bool  enabled = false;       // bEnabled
		int   healthMode = 1;        // uHealthMode - 0 never, 1 when it changes, 2 in combat, 3 when another bar shows, 4 always
		int   magickaMode = 1;       // uMagickaMode
		int   staminaMode = 1;       // uStaminaMode
		bool  phantom = true;        // bPhantom - the recent loss lingers behind the fill
		float phantomSeconds = 0.75F;   // fPhantomSeconds 0..3
		bool  mountStamina = true;   // bMountStamina - the mount's stamina while riding
		bool  survivalPenalty = true;   // bSurvivalPenalty - Survival Mode's reduction drawn at the bar's end
		bool  showValues = false;    // bShowValues - "120 / 150" on the bar
	};

	// [InfoBars] - phase 4 build 2: health bars over the characters around you (TrueHUD's info bars). Off by default.
	struct InfoBars
	{
		bool  enabled = false;          // bEnabled
		int   hostiles = 1;             // uDisplayHostiles - 0 never, 1 in a fight or when hit, 2 always
		int   teammates = 1;            // uDisplayTeammates - 0 never, 1 while you fight, 2 always
		int   others = 1;               // uDisplayOthers - 0 never, 1 when hit, 2 always
		bool  showName = true;          // bShowName
		bool  showLevel = true;         // bShowLevel
		int   maxCount = 10;            // uMaxCount 1..20
		float maxDistance = 2048.0F;    // fMaxDistance - game units
		float offsetZ = 20.0F;          // fOffsetZ - above the head
		float fScale = 1.0F;            // fScale
		bool  scaleWithDistance = true; // bScaleWithDistance
		// B2 (TrueHUD's resource bars): magicka and stamina under the bar - 0 never, 1 when not full, 2 always (TrueHUD's defaults)
		int   resHostiles = 0;          // uResourcesHostiles
		int   resTeammates = 1;         // uResourcesTeammates
		int   resOthers = 0;            // uResourcesOthers
		// B3 (TrueHUD's info-bar options)
		int   anchor = 1;               // uAnchor - 0 the chest, 1 over the head
		bool  levelColors = true;       // bLevelColors - the level number red 10+ levels above the player, grey 10+ below
		bool  damageCounter = true;     // bDamageCounter - the health lost in the last seconds, at the bar's end
		float damageSeconds = 2.0F;     // fDamageCounterSeconds 0.5..10
	};

	// [BossBars] - phase 4 build 3: a large bar for the boss you are fighting (TrueHUD's boss bar). Off by default.
	struct BossBars
	{
		bool  enabled = false;          // bEnabled
		float maxDistance = 4096.0F;    // fMaxDistance - game units
		bool  showLevel = true;         // bShowLevel
		int   modifyHud = 0;            // uModifyHUD - while a boss bar shows: 0 nothing, 1 the subtitles move up, 2 the compass hides
		int   maxCount = 1;             // uMaxCount 1..3 - B4b: more bosses at once, the nearest on the first bar
		bool  stackUp = false;          // bStackUp - the second and third bars over the first instead of under it
		int   spacing = 50;             // uSpacing 20..150 - between two bars, in HUD units
	};

	// [RecentLoot] - phase 4 build 4: what you just picked up (TrueHUD's recent loot). Off by default.
	struct RecentLoot
	{
		bool  enabled = false;          // bEnabled
		bool  hideVanilla = false;      // bHideVanillaMessage - the game's own "X added" notification hidden (off: both show)
		float seconds = 5.0F;           // fSeconds - how long an entry stays
		int   maxCount = 6;             // uMaxCount 1..6
		bool  hideInInventory = true;   // bHideInInventoryMenus - hidden while trading, looting a container or giving (TrueHUD's default)
		bool  hideInCrafting = false;   // bHideInCraftingMenus - hidden at a forge, a workbench, an alchemy or enchanting table
		int   direction = 0;            // uDirection - 0 the newest on top, 1 the newest at the bottom (the list grows up)
	};

	// [FloatingText] - phase 4 build 4 (PHASE4-TRUEHUD-PLAN.md 2.5): short text that rises over a character and fades - the
	// damage you deal, and anything another mod sends (the ModEvent HPM_FloatingText). Off by default.
	struct FloatingText
	{
		bool  enabled = false;          // bEnabled
		bool  damageNumbers = true;     // bDamageNumbers - the damage you deal, over the one you hit
		float seconds = 1.5F;           // fSeconds 0.5..5 - how long a text stays
		int   rise = 40;                // iRise 0..120 - how far it rises a second (HUD units)
		float fScale = 1.0F;            // fScale
		bool  scaleWithDistance = true; // bScaleWithDistance
	};

	// [Colors] - B7 (TrueHUD's Colors page): HPM's own bars recoloured, "RRGGBB"; empty = the art's own colour (so a reskin
	// keeps its look until the player picks one). Health also colours the boss bars and the bars over characters.
	struct Colors
	{
		std::string health;             // sHealth
		std::string magicka;            // sMagicka
		std::string stamina;            // sStamina
		std::string phantom;            // sPhantom - the recent loss behind the fill
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
		PlayerBars                  pb;
		InfoBars                    ib;
		BossBars                    bb;
		RecentLoot                  rl;
		FloatingText                ft;
		Colors                      col;
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

	// B7: the player's TrueHUD colours - its MCM Helper overrides (Data\MCM\Settings\TrueHUD.ini) over its shipped defaults
	// (Data\MCM\Config\TrueHUD\settings.ini). False when neither file is there; a_from names the file(s) read.
	bool     ImportTrueHUDColors(Colors& a_out, std::string& a_from);
	// TrueHUD's shipped colours, as a palette (its settings.ini [Colors] defaults)
	Colors   TrueHUDPalette();

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
