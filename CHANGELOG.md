# Changelog - HUD Position Manager

Every version, beside the code it describes. Status is the version ledger's word for the build.

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
