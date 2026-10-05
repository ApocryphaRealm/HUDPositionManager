HUD Position Manager
====================
Version 1.0.1

Move, resize and hide the parts of Skyrim's HUD - health, magicka and stamina, the charge meters, the compass, the
crosshair, enemy health, the stealth meter, subtitles, the ammo count, notifications, quest updates, the activate
prompt and the location name, TrueHUD's player bars and widgets such as STB Widgets - from a settings page in the
Apocrypha Menu Framework. Every change shows in the HUD at once, while you play, and is saved automatically. No
restart, and no editing of config files.

It works on the HUD you already use. It does not replace the HUD's art or its layout file: it takes each element
where the HUD puts it and moves it from there, every frame, so a UI overhaul's own styling stays and your layout is
laid on top of it.


WHAT IT DOES
------------
- One tab per HUD element: move it left/right and up/down, change its size (it grows and shrinks about its own
  centre), or hide it. A reset button per element, and one for everything.
- Changes apply the moment a slider moves. Turn "Apply my layout" off to see the HUD as it was, and on again.
- Each tab says whether your HUD has that element. An element your HUD lacks, or names differently, is reported and
  skipped; its settings are kept and do nothing.
- When the HUD moves an element itself (a charge meter appearing, the compass making room for the shout meter), your
  offset follows it rather than fighting it.
- Widgets from other mods (STB Widgets' gold, carry weight, level, resistances, equipment, shout, game time and play
  time, an oxygen meter, a casting bar) and TrueHUD's player bars each have their own tab.
- "Move with" makes any element take another element's movement as well as its own, so a widget beside a bar
  follows the bar.
- Two switches for how your UI arranges things:
  - "Move the three bars together": Magicka and Stamina move with Health. On by default.
  - "Widgets around the bars move with them": the widgets a UI such as Norden UI places around the bars move with
    Health, so the whole block moves as one. On by default.
  Switch either off to move those parts one by one.
- Settings live in Data/SKSE/Plugins/HUDPositionManager.ini, written for you by the page.


BUILT WIDGETS
-------------
Widgets vanilla Skyrim lacks are built in, so no separate widget mod is needed for them. Each has the same tab as any
HUD element (move, size, length, show, hide) and appears only when it has something to show:
- Breath meter: under water, the air you have left (hidden with water breathing).
- Casting bar: while a spell charges, how far the charge has got.
- Bow draw: while a bow draws, how far - full once drawn. Shout charge: while a shout is held, toward its third word.
- Detection meter: while you sneak, the game's own sneak eye as a bar - how close anyone is to seeing you.
- Gold, carry weight, level (a bar or a badge), game time, shout cooldown.
- Resistances: fire, frost, shock, magic, poison, disease, armor rating and speed, one combined widget under the
  bars (it moves with them).
- Equipped items: the Tween menu's four-way cross - right hand, left hand, shout or power, and the ammo with its
  count while a bow or crossbow is in hand - each with its item-type icon.
- Play time: the real hours played on this character (the game's own count, kept in the save).
- Active effects: up to six timed effects on you and the time each has left, soonest first; shown while any are.
- Survival Mode's hunger, fatigue and cold, as bars, shown only while Survival Mode is on.


WIDGET ART FOR RESKINS
----------------------
Every built widget is a plain SWF (version 8, ActionScript 2, no script inside) in
Data/Interface/HUDPositionManager/widgets/ - breath, casting, detection, shout, level (the Bar style), level_badge (the
Badge style), bowdraw, shoutcharge, gold, weight, time, playtime, resist, equip, effects, hunger, fatigue and cold, each <name>.swf. The plugin drives named clips on
the movie's main timeline; a reskin replaces the file and keeps these instance names:
- Frame: the art behind everything. Its size sets the widget's size; the widget is centred on its spot.
- Fill: a meter's filled part, its registration point on its LEFT edge. The plugin sets its _xscale from 0 to 100.
- Icon (optional): a symbol, left as drawn.
- Value (optional): a dynamic text field for a number.
- Value2, Value3 ... (optional): a widget with several texts writes them in order - resist: fire, frost, shock,
  magic, poison, disease, armor, speed; equip: right hand, left hand, shout / power, ammo; effects: one row each.
- Icon2, Icon3 ... (equip): each field's icon. A multi-frame icon is stood on the item's type frame - the frames of
  STB Widgets' equip icon: 1 fist, 2 dagger, 4 sword, 6 war axe, 7 mace, 10 greatsword, 11 battleaxe, 12 warhammer,
  16 bow, 18 crossbow, 20 heavy shield, 21 light shield, 22-26 the schools (alteration, conjuration, destruction,
  illusion, restoration), 27 scroll, 28 staff; the shout icon 1 a shout, 2 a power. An empty slot hides its icon.
- Active effects only: the Frame is cut from the bottom to the rows in use (one effect of six: a sixth of it).
- Ring (optional): a sprite holding Seg0, Seg1, ... SegN-1; HPM shows the first value x N of them (the Level badge's XP
  ring - any number of segments, any shape).
- Meter (optional): a sprite with several frames; HPM stands it on frame 1 + value x (frames - 1), so a frame-animated
  meter works as it is (the game's own level meter is one).
- infobar.swf (the bars over characters) also has Frame2 / Fill2 (magicka, under the bar on the left; Fill2 registered
  on its LEFT edge) and Frame3 / Fill3 (stamina, on the right; Fill3 registered on its RIGHT edge, so it empties toward
  the bar's end). The plugin shows or hides each pair per [InfoBars] uResources*.
  Value3 is the damage counter at the bar's right end; Value2 (the level) is written as HTML in its difficulty colour.
- bossbar.swf also has Boss2 and Boss3: sprites with their own Frame / Fill / Value / Value2 for a second and a third
  boss ([BossBars] uMaxCount). The plugin moves them under or over the first bar by uSpacing and hides an unused one.
- floattext.swf (floating text, the damage numbers): one Value field CENTRED on the movie's origin - the plugin puts the
  origin on the point over the character. Text with no plate behind it should carry an outline (a Glow filter on the
  field) to stay readable over a bright sky; the defaults of floattext.swf and loot.swf do.
[Colors] (the page's Colours section) recolours the Fill, Fill2, Fill3 and Phantom clips of HPM's own bars with a colour
transform, only once the player picks a colour; with every colour left as the art's own, a reskin is drawn as made.
Do not import fonts_en.swf into a widget (ImportAssets): Skyrim refuses to load a child SWF that does. Any SWF editor
(JPEXS FFDec) opens the defaults; tools/gen-widgets.py regenerates them.


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
