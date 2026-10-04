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
    charge time, so the bar fills as it runs out.
  - Detection meter (detection.swf): while sneaking, the highest Actor::RequestDetectionLevel of the player among the
    high-process actors; nobody is asked while you are not sneaking. The scale (-100 empty .. 0 detected full) is
    PROVISIONAL until the raw levels are measured in game (DevBench widgets op, "detect").
  - The default art is generated (tools/swfgen.py, tools/gen-widgets.py): a dark plate, a thin border, a flat fill in
    muted colours - the HUD draws about twice as bright as a source colour, so light colours went white.
  - DevBench hud.position: widgets (state, plus the casting and detection readouts), forceWidget {element, value},
    loadWidget {element, url}.

### Fixed
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
