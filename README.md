HUD Position Manager
====================
Version 1.0.0

Move, resize and hide the parts of Skyrim's HUD - health, magicka and stamina, the charge meters, the compass, the
crosshair, enemy health, the stealth meter, subtitles, the ammo count, notifications, quest updates, the activate
prompt and the location name - from a settings page in the Apocrypha Menu Framework. Every change shows in the HUD
at once, while you play, and is saved automatically. No restart, and no editing of config files.

It works on the HUD you already use. It does not replace the HUD's art or its layout file: it takes each element
where the HUD puts it and moves it from there, every frame, so a UI overhaul's own styling stays and your layout is
laid on top of it.


WHAT IT DOES
------------
- One tab per HUD element: move it left/right and up/down, change its size (it grows and shrinks about its own
  centre), or hide it. A reset button per element, and one for everything.
- Changes apply the moment a slider moves. Turn "Apply my layout" off to see the HUD as it was, and on again.
- The element you are editing is outlined on screen, so you can find a small one (the stealth meter, the ammo count).
- Each tab says whether your HUD has that element. An element your HUD lacks, or names differently, is reported and
  skipped; its settings are kept and do nothing.
- When the HUD moves an element itself (a charge meter appearing, the compass making room for the shout meter), your
  offset follows it rather than fighting it.
- Settings live in Data/SKSE/Plugins/HUDPositionManager.ini, written for you by the page.


REQUIREMENTS
------------
- SKSE64
- Address Library for SKSE Plugins
- Apocrypha Menu Framework (the settings page; without it the INI still applies)
- DevBench is optional (the hud.position testing tool)

Skyrim SE 1.5.97, AE 1.6.x, and 1.7.x (a separate 1.7 build).


BUILDING
--------
Visual Studio 2022 with the C++ workload, and VCPKG_ROOT pointing at a vcpkg checkout.
- SE/AE line: configure.bat, then build.bat (CommonLibSSE-NG 3.7).
- 1.7 line: configure17.bat, then build17.bat (CommonLibSSE-NG 7.2).
tools/gen-dist.py writes the shipped INI and the eleven translation files.


LICENCE
-------
GPL-3.0-or-later (LICENSE, NOTICE.md). Components under other licences: THIRD_PARTY_NOTICES.md.
