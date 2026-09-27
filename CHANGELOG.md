# Changelog - HUD Position Manager

Every version, beside the code it describes. Status is the version ledger's word for the build.

## 1.0.0 - 2026-09-27 - untested

First version. The owner asked for a settings menu that drives the HUD widgets at runtime, replacing SkyHUD's
restart-to-apply config file. The design follows the clean-room plan in 4. plans\skyhud-conversion: no SkyHUD file
is read, copied or decompiled, and element names come from the vanilla HUD and from inspecting the running movie.
- HUDMenu::AdvanceMovie hook (vtable slot 5, the Dragon's Eye Minimap pattern). After the HUD's own ActionScript has
  run, it applies each element's offset, scale (about the element's centre) and hide to its clips, so the HUD's
  values are replaced before the frame is drawn, on whichever HUD movie is loaded.
- The offset follows the HUD. The value the HUD itself sets is the base, and a change the HUD makes is taken as the
  new base, not overwritten back.
- 15 elements from the vanilla HUD's instance names. The activate prompt and location name use candidate names that
  still need confirming in game.
- Settings page (AMF): a tab per element with sliders, hide, per-element and global reset, an on-screen outline of the
  element being edited, and a found / not-found line per element. The HUD movie is only ever read on the main thread;
  the page draws from a snapshot.
- HUDPositionManager.ini is written with ordinary file I/O, rewriting only this mod's keys in place, a moment after
  the last edit. It ships at log level info.
- The DevBench tool "hud.position" offers state, set, reset, save, strings, and clips, a live listing of the running
  HUD's clips used to map element names.
- All eleven languages.
- Builds for SE/AE (CommonLibSSE-NG 3.7) and 1.7 (CommonLibSSE-NG 7.2).
