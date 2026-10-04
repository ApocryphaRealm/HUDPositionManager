# Changelog - HUD Position Manager

Every version, beside the code it describes. Status is the version ledger's word for the build.

## Unreleased

The owner, 2026-10-04: "update the Skyrim HUD position manager to the same controls and HUD widgets as the Oblivion
version", then: HPM is to build the widgets vanilla Skyrim lacks itself (reskinnable SWFs), find other mods' widgets the
way ImmersiveHUD does, and replace ImmersiveHUD, TrueHUD and moreHUD. Plan: 4. plans\hud-position-manager-skyrim-parity.
Phase 1 below (the Oblivion version's controls); the widgets follow.

### Added
- The page is the Oblivion version's: Presets / Layout / Combined widgets tabs, on the Apocrypha Menu Framework's own
  Dear ImGui (AMF.h; built against the framework's 1.90.8 docking, cmake/ports*/imgui), the bumpers walking the tabs.
- Presets: whole layouts in SKSE\Plugins\HUDPositionManager\presets - load, save to a new preset, update one in place,
  delete (a second press confirms).
- Precise sliders (rule 68): position in 0.1 % of the screen, size in hundredths; a move slider's range is where the
  element's art meets the edge of the screen (measured), frozen while the slider is held.
- Free placement ([General] bUnlocked): the move sliders go past the edges of the screen.
- Length and Height (fLength / fHeight) on the bars, the meters, the compass and the temperature meter.
- Show, per element (iShow): always, only in combat, only out of combat - the player's combat flag, read only while an
  element uses it, held for 3 s after a fight.
- Always visible ([General] bAlwaysVisible, and per element on the bars): the bar's alpha held at 100 while you play.
- Combined widgets ([Group] sMembers / fX / fY): any set of elements moves as one; its members are pinned first in the
  element tabs, marked "+".
- Two more HUD elements, the clips ImmersiveHUD SKSE reaches that 1.0 did not: the floating quest marker
  (FloatingQuestMarkerInstance) and survival mode's temperature meter (TemperatureMeter_mc).
- DevBench hud.position: x/y in percent, length, height, show, alwaysVisible, the switches and the group; forceCombat,
  range (the open tab's slider range), presets / savePreset / loadPreset / deletePreset.
- Phase 2 - built widgets: HPM makes its own clips in the HUD movie (HPM_<key> under HUDMovieBaseInstance, registered
  in the HUD's HudElements so the HUD's modes show and hide them), loads each one's art from
  Interface\HUDPositionManager\widgets\<widget>.swf, and drives the clip contract (Frame / Fill / Icon / Value, README
  "Widget art for reskins"). Each is an ordinary element tab.
  - Breath meter (breath.swf): under water, the air left - underWaterTimer against fActorSwimBreathBase; hidden with
    water breathing.
  - Casting bar (casting.swf): a charging spell in either hand - the caster's castingTimer counts DOWN from the spell's
    charge time, so the bar fills as it runs out. Charged and still held (caster state 3), it stays full until released.
  - Detection meter (detection.swf): while sneaking, the game's own sneak eye as a bar - its animation frame, 1 hidden
    .. 101 detected (StealthMeterInstance.SneakAnimInstance). Measured 2026-10-04: raw detection levels are noisy (a
    hunting bandit went 161 then -2 while the eye stayed open), so they are only the fallback for a HUD with no eye
    (highest RequestDetectionLevel, about -5 .. 100). Nothing is read while you are not sneaking.
  - The default art is generated (tools/swfgen.py, tools/gen-widgets.py): a dark plate, a thin border, a flat fill in
    muted colours - the HUD draws about twice as bright as a source colour, so light colours went white.
  - DevBench hud.position: widgets (state, plus the casting and detection readouts), forceWidget {element, value},
    loadWidget {element, url}.

### Tested (2026-10-04, SE 1.5.97, Njordlinger Test, Norden UI)
- Casting bar on a held Firebolt (0.833 s charge): 0 -> 0.48 -> 0.96 while charging (caster state 2), full once
  charged and held (state 3) until released; no crash through the long hold that crashed the earlier build.
- Detection meter while sneaking with a bandit spawned: eye frame 1 -> 26 -> 101, bar 0 -> 0.25 -> 1.0; hidden when
  not sneaking. Default spot (0.42 of the screen) overlaps the notifications in this layout - to revisit.
- Default colours: dark #2050E0 drew as (66,170,251), and the encoding was checked with six SWF variants, so the art is
  fine; the HUD simply draws brighter.

- Info widgets (gold.swf, weight.swf, level.swf - Frame / Icon / Value, the level a meter with Value): gold carried,
  carried / maximum weight, the level with its progress to the next. Text fields import $EverywhereFont from
  gfxfontlib.swf, as the game's hudmenu.swf does. Tested 2026-10-04: "1", "72", "2 / 300" drawn bottom right, equal to
  the HUD's own figures.

- Game time (time.swf, hh:mm from Calendar::GetHour) and shout cooldown (shout.swf, a draining bar with the seconds
  left, Actor::GetVoiceRecoveryTime). Tested 2026-10-04: the clock matched the HUD's (07:13, 07:38); a 12 s cooldown
  (Papyrus SetVoiceRecoveryTime) drained about 1/12 a second and hid at zero; 30 s drew a violet bar with "27".
- Breath tested under water at Dawnstar (held under by Papyrus SetPosition): the meter shows while kUnderwater, and the
  allowance is now fActorSwimBreathBase + fActorSwimBreathMult x 50 - drowning damage began at 20.5 s (base 10, mult
  0.2), 10.4 s with mult 0, and still 20 s with stamina doubled. The base alone had emptied the meter at half time.

### Tested on a minimal profile (2026-10-04, "HPM Minimal": SKSE, Address Library, Engine Fixes, SkyUI, AMF, TestBench,
HPM - none of CastingBar, oxygenMeter2, STB Widgets, TrueHUD, ImmersiveHUD, moreHUD; a FRESH game, coc Riverwood from the
main menu)
- HPM's own widgets stand alone: all eight load and draw on the vanilla HUD; gold 140 then 390 after AddItem 250, equal
  to the game's GetGoldAmount; carry weight 96 / 300; the clock 08:04.
- With Norden UI, Norden UI - Black and Norden Black's optional HPM widget page added (still none of the replaced mods),
  every widget wears Norden Black's art.

- Level has two styles, picked on its tab (Style; [InfoLevel] iStyle): Bar - the number in front of an XP bar
  (level.swf) - or Badge - the number in a badge with the XP shown round it (level_badge.swf). The owner, 2026-10-04:
  "they do the same thing, so it should be one or the other, but we should let them choose". Switching loads the other
  art into the same holder at once. The default badge is a round plate with a 36-segment ring.
- Two more optional parts in the clip contract: Ring (Seg0..SegN-1, the first value x N shown) and Meter (a multi-frame
  sprite stood on frame 1 + value x (frames - 1)) - Norden UI's own level badge is a 141-frame meter and works as it is.
- The widgets now sit on Norden UI's layout (measured against Norden's own CastingBar, oxygen meter and STB widgets),
  saved as the preset "Norden UI".
- Tested 2026-10-04 (HPM Minimal + Norden UI + Norden UI - Black's HPM option, fresh coc Riverwood): Badge -> Bar ->
  Badge swaps live; the badge reads "1" and fills from the bottom with the XP (0.43 after AdvanceSkill); the bar reads
  "1" with its XP bar.

- Seven more built widgets (the owner, 2026-10-04: "Check what Norden UI patches for and see if there's any other
  mod that adds a HUD widget that we can add", then "go on the 3"): Resistances (STB Widgets' eight values - fire,
  frost, shock, magic, poison, disease, armor rating, speed), Equipped items, Play time (GetRealHoursPassed, the
  game's own count), Active effects (timed, not hidden, soonest to end first, up to six) and Survival Mode's hunger,
  fatigue and cold (read from ccQDRSSE001-SurvivalMode.esl's globals by form ID - the editor IDs are not kept on
  1.5.97; shown only while its Survival_ModeEnabled is on). 11-language names for each.
- Resistances are one combined widget under the health / magicka / stamina bars, their width, moving with Health;
  Equipped is the Tween menu's four-way cross at the bottom right (the owner: "the STB widget skin in Norden UI is
  four directional and shaped like the tween menu" ... "place them and size them under the health bar").
- Clip contract: Value2..ValueN for several texts; Icon2..IconN, a multi-frame icon stood on the item's type frame
  (STB's 28 equip-icon frames, read from its art); an empty slot hides its icon. Active effects' Frame is cut to the
  rows in use.
- The Norden UI preset sizes Norden's resist row to the bars (0.43) and its equip cross (0.6).
- Tested 2026-10-04 (HPM Minimal, Norden UI + Norden UI - Black's HPM option, fresh coc Riverwood): resist 0% /
  50% frost (a Nord) / armor 65 / speed 100% under the bars; equip: Iron War Axe with Norden's battleaxe icon,
  Flames in the left hand (destruction icon), Unrelenting Force (shout glyph), Hunting Bow + "Iron Arrow x47" with
  the bow and arrow icons; play time "0h 01m"; active effects "Armor - Oak 0:43" counting down after Oakflesh.
- Survival bars tested 2026-10-04 once Survival Mode could load in HPM Minimal (the profile had "Base Game DLC
  Content" off, so every .esl with a DLC master was dropped - found by the powershell agent): all six globals found by
  form ID; hidden until Survival Mode is accepted, then hunger 0.145, fatigue 0.146, cold 0.049 -> 0.055 (rain), equal
  to the globals over their maxima. Each bar now has its own icon (a drumstick, a crescent moon, a snowflake) - no
  words, nothing to translate.
- Bow draw and Shout charge, two more built widgets beside the casting bar (the Casting Bar mod ships the same three
  bars, CastingBar_Spell / _Bow / _Shout, and Norden UI - Black skins all three):
  - Bow draw (bowdraw.swf): from the draw starting (attack state kBowDraw / kBowAttached) to fully drawn (kBowDrawn),
    full until loosed. SE keeps no draw amount (currentBowDrawAmount is a VR field), so the bar runs against the time
    the last full draw took, measured every draw - Quick Shot or a slower bow are followed after one shot.
  - Shout charge (shoutcharge.swf): while the shout button is held, toward the game's own three-word time
    (fShoutTime2, 0.9 s; fShoutTime1 0.2 s for the second word).
  - Tested 2026-10-04 (HPM Minimal + Norden Black art, fresh coc Riverwood): Hunting Bow drawn - 0 while drawing,
    full and held while drawn, in Norden's green bow bar; Unrelenting Force held - 0.8 at the poll, then the cooldown
    (36 s) began. The Norden UI preset puts both on the casting bar's spot and size.
  - DevBench widgets also reports "bow" (attack state, elapsed, last full draw) and "shout" (both voice casters, held,
    the word times).

- Equipped also shows the restoring potions carried - health, magicka, stamina (Value5..7 with Icon5..7; STB's own
  potion icon frames 1, 3, 2 in Norden's art, its left back): drinkable, not food or poison, the costliest effect
  beneficial and on that value; counted with the ammo, twice a second, behind the same fault guard. Tested 2026-10-04:
  x8 / x7 / x10, equal to GetItemCount of the minor health / magicka / stamina potions after AddItem.
- Phase 3 (ImmersiveHUD parity, PHASE3-IHUD-PLAN.md) build 1: Show (in / out of combat) can FADE instead of hiding at
  once - [General] bFade (off by default), iFadeInSpeed / iFadeOutSpeed 1..20 (ImmersiveHUD's scale: half a full fade a
  second per step), iOpacityMin / iOpacityMax (percent). One alpha path for every part: the owner's alpha (the game's
  bar fade, TrueHUD, a built widget's 0/100) is followed as positions are and stays the base; ours is base x the fade,
  written only when it differs, given back when nothing applies. Presets carry the fade. Page: under HUD visibility.
  DevBench: set {fade, fadeIn, fadeOut, opacityMin, opacityMax}; state reports them and each element's fade and
  alphaMul. 11 languages.
  - Tested 2026-10-04 (HPM Minimal, Norden UI): Compass on "only in combat", fade on, out 2 / in 10, opacity 20-100:
    forced out of combat its alpha went 98 -> 90 -> 20 and held at 20; forced in, 28 -> 44 -> 100 in a fifth of a
    second; with the fade off again it hid at once (_visible false) with its alpha given back at 100.

### Fixed
- A crash as the widgets' art loaded into the HUD (crash-2026-10-04-22-17-54: a null write at SkyrimSE+0FFD2A4 inside
  HUDMenu::AdvanceMovie, the same second the 19 loadMovie calls were made). The only new art was the survival icons,
  whose drumstick and snowflake were polygons OVERLAPPING inside one DefineShape - nothing else we ship does that. Each
  polygon is now its own shape and the next load ran clean; one crash, one clean run, so this is the likely cause, not
  a proven one (FFDec parsed and rendered the old file without complaint).
- After a style change the old art stayed until the new one arrived, and was taken for it: the text went into the old
  field and the new one kept its sample text ("100"). A widget counts as loaded only once its _url names the art asked
  for - and _url comes back percent-encoded (level%5Fbadge.swf), so it is decoded first.
- A widget is centred on its Frame's bounds (the contract's size), so art outside the Frame (Norden's level flash)
  no longer pulls it off its spot.
- Scaling a built widget slid it right by half its width (its centre was measured before the art was centred).
- Gold read only the InventoryChanges deltas, which are changes FROM the base container: a fresh character (base 140
  gold) read 0 - 140 = -140 in an old save. Gold is now the base container's gold plus the deltas, under the same guard.
- Nothing read after coc from the main menu: that starts play with neither kPostLoadGame nor kNewGame, so the load gate
  never opened. The gate now has three states - no load seen yet (read once the player stands in a loaded world), a
  save loading (kPreLoadGame: no reads), loaded - and still never reads under the Loading Menu.
- Gold: CommonLib's Actor::GetGoldAmount faulted on SE 1.5.97 every time, after the save had loaded too (a garbage
  object pointer from GetInventory's base-container walk, crash-2026-10-04-12-12-14). Gold is now the sum of the gold
  entries of the player's InventoryChanges, read behind an SEH guard that turns the widget off (logged) on a fault.
- Built widgets read nothing before kPostLoadGame / kNewGame, after kPreLoadGame, or while the Loading Menu is open
  (crash-2026-10-04-11-46-31, found by the primary session: the HUD advances during the load screen).
- A crash in the HUD hook (ApplyPart's GetDisplayInfo) when a widget mod swapped or recreated its menu's movie between
  the once-a-second checks - seen during a held spell cast with the CastingBar menu. The movie is now held alongside
  the menu, and every frame checks that the menu still shows it.

### Changed
- Positions are a percentage of the screen ([<element>] fX / fY). A 1.0 INI's fOffsetX / fOffsetY (HUD units of the
  1280x720 stage) are converted on the first load and the old keys dropped at the next save.
- "Move with" also carries the Combined widgets offset (once, when the element or one it follows is a member).

### Tested (2026-10-04, SE 1.5.97, Njordlinger Test, Norden UI + TrueHUD, DevBench + gamelink frames)
- Migration: Health fOffsetX=64 loaded as fX=5.0, applied as 64 HUD units; the move-with chain carried it to Magicka,
  Stamina and TrueHUD's three bars. The page registered through AMF.h (AMF 2.0.6.0).
- Show "Only in combat" on the compass: forced out of combat -> _visible false; forced in -> true. Length 1.5 ->
  _xscale 150. Combined widgets Compass + Crosshair at +2 % / -1 %: both applied 25.6 / -9.6 units (the visible stage
  was 1280x960 with the monitor off - percent follows the real screen). A saved preset loaded back the layout; deleted.
- The page: Presets and Layout tabs drawn (switches, wrapped hints); Health's slider range x 5.0..66.72, y
  -79.97..18.6; Free placement on -> -100..100.
- NOT yet seen: Always visible on a HUD that fades its bars (Norden + TrueHUD draws the bars through TrueHUD; the vanilla
  Health clip's alpha stays 100 here), the 1.7 build line, the 3 s combat linger in a real fight.

## 1.0.1 - 2026-09-27 - working

- The on-screen outline of the element being edited is gone, with its switch and its INI key (bHighlight). The
  element itself moves on screen as the sliders move, so the outline only covered what it was showing (the owner:
  "There isn't even a need for visual ghosts in this mod because it does it in real time"). An old bHighlight line
  left in an INI is ignored.

## 1.0.0 - 2026-09-27 - working

First version. The owner asked for a settings menu that drives the HUD widgets at runtime, replacing SkyHUD's
restart-to-apply config file. The design follows the clean-room plan in 4. plans\skyhud-conversion: no SkyHUD file
is read, copied or decompiled, and element names come from the vanilla HUD and from inspecting the running movie.
- HUDMenu::AdvanceMovie hook (vtable slot 5, the Dragon's Eye Minimap pattern). After the HUD's own ActionScript has
  run, it applies each element's offset, scale (about the element's centre) and hide to its clips, so the HUD's
  values are replaced before the frame is drawn, on whichever HUD movie is loaded.
- The offset follows the HUD. The value the HUD itself sets is the base, and a change the HUD makes is taken as the
  new base, not overwritten back.
- 20 HUD elements, named from the vanilla HUD and from the running Norden UI HUD's own clip listing:
  - the bars, the charge meters (left, right, combined) and the compass;
  - the shout meter, as its own element, so a HUD that separates it from the compass (Dragonborn UI) is covered;
  - the crosshair, enemy health, stealth meter, subtitles, ammo count, notifications and quest updates;
  - the activate prompt, location name, level-up meter, word wall letters and clock.
- 10 widgets from other mods, each its own menu and moved by its movie's _root: STB Widgets' gold, carry weight, level,
  resistances, equipment, shout, game time and play time, plus an oxygen meter and a casting bar. They are not part of
  the HUD movie, so moving a bar never moved them. "Move with" lets a widget, or any element, take another element's
  offset, so a widget beside a bar follows it.
- TrueHUD's player widget has its own elements (health, magicka, stamina, and its special bars, enchantment charge
  and shout indicator). Norden UI draws the visible bars through it, so moving only the HUD's Health left the bar on
  screen behind; each follows the matching HUD bar.
- Offsets are in HUD units and converted to each movie's own units (from its visible frame), so a widget movie drawn
  at a different size moves the same distance on screen as the bar beside it. The scale of the clips an element sits
  inside is divided out, so a nested clip moves by the requested amount too.
- "Move with" chains (a widget that moves with Magicka, which moves with Health, takes both), with a guard against
  a loop.
- Two settings-page switches for UIs that link the bars and the widgets around them:
  - "Move the three bars together" makes Magicka and Stamina move with Health by default.
  - "Widgets around the bars move with them" makes STB Widgets' gold, carry weight, level, resistances and game time
    move with Health by default.
  Switching one off sets those elements to move on their own. An element the INI gives no sMoveWith takes the
  default of the INI's switches.
- Each switch's explanation sits on its own line under it; beside the switch it read as part of the label.
- Two names that reach the same clip (a HUD keeps extra references) are used once, so an offset can never be added
  twice.
- A widget menu that closes is held until its clip handles are released.
- Settings page (AMF): a tab per element with sliders, hide, per-element and global reset, an on-screen outline of the
  element being edited, and a found / not-found line per element. The HUD movie is only ever read on the main thread;
  the page draws from a snapshot.
- HUDPositionManager.ini is written with ordinary file I/O, rewriting only this mod's keys in place, a moment after
  the last edit. It ships at log level info.
- The DevBench tool "hud.position" offers state, set, reset, save, strings, and clips, a live listing of the running
  HUD's clips used to map element names.
- All eleven languages.
- Builds for SE/AE (CommonLibSSE-NG 3.7) and 1.7 (CommonLibSSE-NG 7.2).
