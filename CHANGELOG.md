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

- Phase 3 build 2 - the HUD toggle ([Immersive], off by default): a key (X, DEFAULT-KEYS.md; ImmersiveHUD's own) and
  a controller button (unbound) show and hide every element whose Show is the new "Follow the HUD toggle" (iShow 3) -
  press to toggle, hold mode, or a display duration (fDisplaySeconds 0..10, frozen in menus); shown after loading
  (bStartVisible); while shown, the bars the game fades stay up (bHoldBarsWhenShown). The toggle's elements always
  fade (the fade engine). Never in a menu that pauses the game. Bound on the page with press-to-bind rows (Dragon's
  Eye Minimap's pattern: the next press on that side; AMF's reserved keys and Esc refused; an arm with the page
  closed is dropped and never eats a key). "Put the whole HUD on the toggle" / "Take it all off" - everything but the
  crosshair, the sneak eye, notifications, subtitles, prompts, the location name, the enemy's health and the widgets
  that already show only when they matter. ImmersiveHUD.dll present is logged and named on the page.
- Phase 3 build 3 - ImmersiveHUD's global rules: the toggle's elements also show in combat (bShowInCombat) and with a
  weapon or spell drawn (bShowWeaponDrawn), both on; and four context modes per element: only indoors (4), only
  outdoors (5), only with a weapon drawn (6), only while sneaking (8); 7 is kept for a lock-on target (True
  Directional Movement). Each context is read only while an element uses it.
- DevBench: toggle {down?}, immersiveAll {on}, forceContext {interior, weapon, sneak}; set takes immersive,
  toggleKey, toggleButton, hold, displaySeconds, startVisible, holdBars, showInCombat, showWeaponDrawn; state reports
  the toggle, toggleShown and the context. 28 new strings in 11 languages (the powershell agent's translations).
- Tested 2026-10-04 (HPM Minimal, Norden UI, fresh coc Riverwood): immersive on + the whole HUD on the toggle (35
  elements) -> the HUD faded out of sight; the real X key (bench.input) -> it faded back with the bars up; X again ->
  out; display duration 2 s -> one press showed it and it was gone with 0 s left; drawing the axe (R) -> shown, sheathed
  -> out; Compass "only indoors" with the interior forced 0 / 1 -> _visible false / true.

- Phase 3 build 4: "When it matters" (iShow 9) on the crosshair - shown while a weapon is out and you attack, draw a
  bow or charge a spell, faded otherwise - and on the sneak eye - while sneaking, as strong as the detection (a quarter
  at the first notice, full when seen; its alpha set outright, because the game tweens the eye's alpha by reading it
  back and a multiplier on top compounded to 0). Relinquish: bleeding out or dead, the whole HUD comes back and nothing
  fades it. The author API: the ModEvent HPM_SetElementHidden (strArg the element key, numArg 1 hide / 0 give back)
  hides an element for another mod; not saved. DevBench forceContext takes aim and eye too.
  - Tested 2026-10-04 (HPM Minimal): crosshair on 9 with aiming forced off -> its alpha 85 -> 0, forced on -> back up;
    the sneak eye on 9, sneaking, the eye forced to half detection -> alpha 62.5 (0.25 + 0.75 x 0.5). NOT yet proven: the
    author API - the sink receives ModEvents (SkyUI's were logged), but the test SendModEvent's arrival was not, as the
    log stopped after the first five; every HPM_ event is now logged.

- Phase 4 (TrueHUD parity, PHASE4-TRUEHUD-PLAN.md) build 1 - HPM's own player bars ([PlayerBars], off by default):
  Health, Magicka and Stamina as built widgets (playerhealth / playermagicka / playerstamina.swf) where the game's bars
  are (hide those on their tabs to use these), each shown Never / When it changes / In combat / When another bar shows /
  Always (TrueHUD's five modes); the recent loss lingering behind the fill (bPhantom, fPhantomSeconds, then easing a
  full bar a second); the Survival penalty drawn from the bar's end; the mount's stamina while riding; the numbers on
  the bar (bShowValues). Two more optional clips in the contract: Phantom (left registered, behind Fill) and Penalty
  (registered on its RIGHT edge). Page: Bars and loot, under the HUD toggle. 19 strings in 11 languages.
  - Measured (rule 30): Survival Mode's need penalty is a NEGATIVE TEMPORARY actor-value modifier. At hunger 600
    stamina's max read 46.67 (GetActorValueMax) while GetActorValuePercentage read 0.4667 against 100; the bar runs
    against the unpenalised max (permanent + positive temporary), so Fill 46.67 and Penalty 53.33 - exactly the game's.
  - Tested 2026-10-04 (HPM Minimal, fresh coc Riverwood): the three bars drawn with "100 / 100"; console damageav
    health 40 -> "60 / 100", the phantom held the lost 40 for 1.5 s and then eased away; the penalty above.
  - The author API is still unproven: TestBench's papyrus call of Form.SendModEvent (and of Actor.DamageActorValue)
    returned "called" without effect, while console damageav worked - a TestBench question, being looked into.

- Phase 4 build 2 - bars over characters ([InfoBars], off by default; TrueHUD's info bars): a pool of up to 20 clips
  in the HUD movie (HPM_IB0..19, infobar.swf: Frame, Phantom, Fill, Value the name, Value2 the level), registered in
  HudElements with the HUD's mode flags. Chosen about four times a second from the high-process actors: enemies in a
  fight or hit (uDisplayHostiles), followers while you fight (uDisplayTeammates), anyone you hit (uDisplayOthers; a hit
  sink records who you hit and who hit you), each 0 never / 1 / 2 always; nearest first, at most uMaxCount, within
  fMaxDistance, only in the player's line of sight. Each frame each bar is projected through the world camera
  (NiCamera::WorldPtToScreenPt3, above the head by fOffsetZ), sized with distance, faded in and out; health with the
  recent loss, name and level ten times a second. Nothing is read between kPreLoadGame and the game being loaded or
  under a load screen; the pool lets every character go on a load. DevBench: bars, pinNearest {on}. Norden UI - Black
  grafts infobar.swf from Norden's own TrueHUD art (HealthBar*), in Norden's TrueHUD colours.
  - Tested 2026-10-04 (HPM Minimal, Riverwood): with everyone shown, bars over Faendal, Gerdur, Hilde, Sven, Hod, Alvor and
    a chicken, named, sized by distance and following them; Alvor damaged by 60 -> fill 0.54 with the loss held behind it;
    with line of sight on, the ones behind the tree and the buildings dropped (7 -> 4); opening and closing the Tween
    menu hid them and gave them back.
- The author API proven (2026-10-04, the powershell agent, once TestBench passed a numArg as a Float): SendModEvent
  ("HPM_SetElementHidden", "Compass", 1) hid the compass, 0 gave it back. Author hides are dropped on a load.
- Review fixes (REVIEW-2026-10-04.md): the info bars claim the HUD's modes (H1), no actor read outside the game-ready
  window and the pool cleared on a load (H2), the HUD toggle key is the menu's while AMF is open (M2), author hides
  cleared on a load (M3); the info bars' name / level follow their switches; the scan is every twelfth frame.

- Phase 4 build 3 - the boss bar ([BossBars], off by default; TrueHUD's boss bar): a built widget (bossbar.swf: Frame,
  Phantom, Fill, Value the name above, Value2 the level) at the top centre, an ordinary element tab. The boss: alive, in
  combat with the player as its target, within fMaxDistance, the nearest - a dragon (its race's ActorTypeDragon) or a
  reference placed as its location's boss (the Boss location ref type, Skyrim.esm 0x130F7; Dragonborn's DLC2Boss1
  0x0206B5, both read from the masters). Norden UI - Black grafts it from Norden's own BossBar* art.
  - Tested 2026-10-04 (HPM Minimal, Riverwood, god mode): a fire dragon placed beside the player -> "Elder Dragon" in a
    wide bar at the top, Norden's art; both boss rules resolved.

- Phase 4 build 4 - recent loot ([RecentLoot], off by default; TrueHUD's recent loot): a list widget (loot.swf, six
  rows, its plate cut to the rows in use) of what came into the player's inventory from anywhere else - a
  TESContainerChangedEvent sink, nameless forms skipped, the same item merged while it is up, each row gone fSeconds after
  it last changed, at most uMaxCount, newest first. A second hook, HUDMenu::ProcessMessage (vtable slot 4), matches the
  game's own "<item> added" notifications against its GMSTs (sAddItemtoInventory / sAddItemsToInventory); it hides them
  only with bHideVanillaMessage=1, which ships off - the owner, 2026-10-04: "add the hook but dont hide anything".
  DevBench: loot {name?, count?} (the list, an injected entry, and the hook's seen / hidden counts).
  - Tested 2026-10-04 (HPM Minimal, a New Game through Alternate Perspective - its start room): AddItem 25 gold and an
    iron dagger -> the list "Iron Dagger" / "Gold x25"; the game's "Gold (25) Added" still shown; the hook saw both
    lines, hid none.

- Phase 4 build 4 - floating text ([FloatingText], off by default; TrueHUD's floating text, PHASE4-TRUEHUD-PLAN.md 2.5):
  a short text that rises over a character and fades. Two sources:
  - Damage numbers (bDamageNumbers, on within the feature): a TESHitEvent sink notes each character the player hits; the
    read pass follows its health, and each loss within 2 s of the player's last hit on it rises over it as "-N" (losses in
    the same 0.3 s merge into one number). The health is kept 30 s; a character seen for the first time starts from its
    maximum, so the first blow counts whether the game sends the hit before or after applying the damage.
  - Other mods: <form>.SendModEvent("HPM_FloatingText", "text", seconds) - the form is the character it rises over (the
    player when it is not one), 0 seconds takes fSeconds.
  - A pool of 16 HUD clips (HPM_FT0..15) loading floattext.swf - a Value field centred on the point, no plate, an outline
    (a Glow filter on the text: swfgen.place_glow, PlaceObject3) - registered in HudElements with the HUD's modes, read
    only in the game-ready window, the HUD movie held (as the info bars). Over the player in first person the text rises
    from a spot above the crosshair (the point above the head is behind the camera there).
  - Settings fSeconds 0.5..5, iRise 0..120 (HUD units a second), fScale, bScaleWithDistance; the page's Bars and loot
    section; 11 languages. DevBench: floatText {text, ref, seconds} (as the ModEvent; lists the texts up), ftHit {ref}
    (a player hit queued as the sink would - the damage watch).
  - Tested 2026-10-04 (HPM Minimal, New Game, Alternate Perspective's start room, then Riverwood): a text over the player
    rose from above the crosshair (the first build put it over the head, off screen in first person - fixed); a wolf
    placed beside the player, a hit queued (ftHit) and 8 health taken (Papyrus DamageActorValue, 22 -> 14) -> "-8" over
    the wolf, then "-5", beside its info bar; wolf.SendModEvent("HPM_FloatingText", "Over the wolf", 3) -> the text over
    the wolf; a text over the bright mountain read with its outline.
  - A real blow, proven 2026-10-04 (Njordlinger Test, Main Agent's combined run, IED unticked for the run so F10 reached
    AutoCombat's doctrine box; .MD\handoffs\results\hpm-real-blow-*): AutoCombat's iron-sword hits on a wolf -> "-4", then
    "-10" over it (alpha 100), white over its info bar in the frame. The same blow also drew TrueHUD's own damage counter
    on its bar and Modern Floating Damage's red "10": with HPM's damage numbers on and Modern_Floating_Damage.dll loaded,
    the page now says so ("Use one or the other", 11 languages), as for TrueHUD.
- tools/gen-dist.py: the gate's source scan read keys as HPM_ + letters only, so a key with a second underscore (HPM_FT_*,
  HPM_PB_*, HPM_IB_*, HPM_BB_*, HPM_RL_*) was never compared with the table. It now reads letters, digits and
  underscores: 150 keys checked instead of 116, all matching.

- Phase 4 (ImmersiveHUD side), build D1 - discovering other mods' HUD widgets, read-only (PHASE4-DISCOVERY-PLAN.md):
  every 5 s in play, SkyUI widgets (_root.WidgetContainer.<n>, keyed by the SWF they loaded), clips another mod added
  under HUDMovieBaseInstance (loaded from a file of their own), and overlay menus (open in play, with none of a real
  menu's flags - pausing, the cursor, a menu context, modal, freeze frame - and never a vanilla, AMF, TestBench or HPM
  menu; one already a hand-named element is marked "known"). Listed and logged on a change, moved by nothing yet.
  DevBench: discovered {rescan?}.
  - Tested 2026-10-04 (HPM Minimal): one found - SkyUI's activeeffects.swf at _root.WidgetContainer.0; no vanilla menu
    taken (only the HUD was open).
  - Tested 2026-10-04 (Njordlinger Test, New Game, Norden UI + InfinityUI + TrueHUD + STB): 22 found. Overlay menus:
    STB's eight (equip, game time, gold, level, play time, resist, shout, weight), oxygenMeter2 and TrueHUD all marked
    "known" (already elements); new - DurabilityMenu, BTPS Menu, BTPS Ovelay Menu. HUD clips: InfinityUI's Minimap and
    QuestItemList, FollowerStats, B612's spin icon. No vanilla menu taken (Fader, Main, Mist, RaceSex and MessageBox were
    open at times). One flaw, fixed: InfinityUI re-parents the vanilla compass's pieces under HUDMovieBaseInstance from
    its own compass.swf, and CompassCard / CompassFrame / their Alts / CompassRect were listed as widgets - the vanilla
    HUD's alternate and inner clip names are now never discoveries.
  - Re-run 2026-10-04 (Njordlinger Test, Main Agent's combined run under rule 69; .MD\handoffs\results\hpm-d1b-*.txt):
    all pass - no compass piece listed; the ten hand-named widget menus "known"; DurabilityMenu, both BTPS menus,
    Minimap, QuestItemList, FollowerStats and B612's spin icon listed; no vanilla menu; SkyUI's container gave
    b612_announcement.swf and activeeffects.swf - in slots 0 and 1 this time, the reverse of the first run, which is why
    a SkyUI widget is keyed by its SWF, not its slot.

- Phase 4, build D2 - a discovered widget gets its own tab and moves like any element. The scan writes what it finds to
  Data\SKSE\Plugins\HUDPositionManager\discovered.ini (one section per widget: kind, where, SWF, name, last seen; written
  only when a widget is new or first seen that day). At plugin load, hud::Elements() reads it and appends each widget to
  the fixed table as an element: W_<kind>_<name> (its INI section, so presets carry it), at most 32, a widget not seen for
  30 days left out. The table is built once, before any thread or hook, and never changes during a session - no race with
  the page's render thread - so a widget first found mid-session gets its tab from the next start, as ImmersiveHUD's
  "may need relaunch"; the Layout tab says so. Kinds: a clip under HUDMovieBaseInstance (its path), an overlay menu (its
  _root), a SkyUI widget (its slot of WidgetContainer, looked up by its SWF at each resolve - the slot shifts between
  sessions). The fixed table's own elements never become discoveries. Each discovered tab says "Found in your game:" and
  its SWF. 11 languages. DevBench discovered: a "tab" flag per entry.
  - Tested 2026-10-04 (Njordlinger Test, Main Agent's combined run under rule 69; .MD\handoffs\results\hpm-d2-*): with a
    seed cache of D1's five finds, all five tabs present and found (Minimap, FollowerStats, QuestItemList, DurabilityMenu,
    SkyUI activeeffects). Moved: Minimap -10 % -> applied -128 and its box 1024.5 -> 896.5, the InfinityUI minimap
    visibly further left in the frame with its location label; DurabilityMenu +10 % -> 32.4 -> 160.4, the durability
    widget further right; activeeffects +10 % down -> applied 72. All back to 0 after. The scan rewrote the cache with the
    seed's five plus three more (B612's spin icon, both BTPS menus) for the next start. QuestItemList and activeeffects
    report no box while they draw nothing (no quest items, no effects). Each discovered element writes its [W_*] section
    to the INI like any element. sLastSeen is a UTC day, as the pruning's own clock.

- Phase 4, build D3 - the discovery cache keeps itself clean:
  - Once a session, at the first scan (the archives are loaded by then), a cached widget whose SWF the game can no longer
    open is dropped - its mod is gone. The game's own resource lookup (BSResourceNiBinaryStream, as AMF reads its
    translation files) sees loose files and archives alike, so a widget whose art lives in a BSA is kept.
  - "Forget this widget" on each discovered tab (DevBench forget {element}): the widget stays in the cache marked
    bForgotten=1, so no scan adds it back, and it gets no tab from the next start; deleting discovered.ini brings every
    widget back. The page and DevBench queue the request; the main thread writes it at the next scan in play. 11 languages.
  - Tested 2026-10-04 (Njordlinger Test, Main Agent's combined run; .MD\handoffs\results\hpm-d3-*): a ghost widget with
    no SWF had its tab that session and was pruned from the cache at the first scan; SkyUI's activeeffects (its SWF in
    SkyUI's BSA) kept; BTPS Menu forgotten -> bForgotten=1. The damage-number check from a real blow did not run: TestBench
    sets AutoCombat's doctrine with F10, which in Njordlinger Test opens Immersive Equipment Displays' editor instead.

- Recent loot's rows carry an outline (a Glow filter on each text, gen-widgets grid_widget(outline=True)) - still no
  plate (the owner: "You dont need the outline box for the recent loot"; if the text did not stay readable on bright
  scenes, an outline or a shadow on the text, never a box). Checked 2026-10-04 in Riverwood: the plain rows read against
  the dark bridge but washed out moved over the bright mountain ("Gold x25" / "Iron Sword" barely legible). With the
  outline, the same rows in the same spot read clearly (2026-10-04, a second run).

- The parity gap list (4. plans\...\GAPS-2026-10-04.md), the cheap wins first (the owner, 2026-10-05, "do it with your
  suggestion"; moreHUD dropped; the TrueHUD API parked on his packaging call - its licence check: B1-TRUEHUD-API-LICENCE.md):
  - A1: Show "Only while locked on to a target" (iShow 7, kept free since build 3) - True Directional Movement's target
    lock, through its API (TrueDirectionalMovementAPI.h from TDM 2.2.6, MIT, vendored unmodified; THIRD_PARTY_NOTICES),
    obtained at kPostPostLoad and read only while an element uses it. Offered on the Show list only with TDM installed
    (or the INI already holding 7); without TDM nothing can lock, so the element stays shown, with a hint.
  - A2: the crosshair's two ImmersiveHUD options, [Crosshair] bHideWhileAiming (a bow from the draw to the release, or
    an aimed spell charging) and bHideWhileSneaking, whatever its Show.
  - A4: a killcam, the free camera or the vanity camera stands the clock still (ImmersiveHUD 3.0.0) - no fade runs and
    the toggle's display time does not run out behind a camera the game drives.
  - DevBench: forceContext lock / bowAim / camera; state context lockedOn / tdm / bowAim / cameraFrozen; set
    crossHideAiming / crossHideSneaking. 5 new strings, 11 languages.
  - Tested 2026-10-05 (Njordlinger Test, Main Agent, .MD\handoffs\results\hpm-a1/a2/a4-*): A1 4/4 - TDM's API obtained;
    the compass on Show 7 hidden without a lock, shown after a real middle click locked a wolf (frame hpm-a1-tdm-lock),
    hidden again when a second click released it. A2 - sneaking (Actor.StartSneaking) hid the crosshair, standing showed
    it; a bow drawn by a real left button held 1.5 s hid it (bowAim true, frame hpm-a2-bow-drawn-no-crosshair), loosing
    showed it. A4 5/5 - in tfc the 3 s display time stood still; it ran out 4 s after leaving tfc.
  - B5: [BossBars] uModifyHUD - while the boss bar shows, 0 nothing, 1 the subtitles lift 10 % of the screen out of its
    way (a bar at the bottom), 2 the compass hides (a bar at the top, in its place). TrueHUD's option; off by default.
  - B8: [RecentLoot] bHideInInventoryMenus (trading, a container, giving; on, as TrueHUD), bHideInCraftingMenus (off),
    uDirection (0 the newest on top, 1 the newest in the bottom row - the list grows up).
  - D1: "Show every element" on the Layout tab, while the page is open (never saved): every element shows whatever its
    Show, the bars the game fades are held up, and HPM's widgets that wait for something show sample content - names
    from the game's own records (Alduin on the boss bar; an iron sword, gold and a sweet roll in recent loot), so they
    come in the player's language. Show's hint now points at it (it promised the switch before it existed). The game's
    own event-only parts (subtitles, the enemy's bar) are not given sample text yet. DevBench preview {seconds}.
  - 14 new strings, 11 languages.
  - Tested 2026-10-05 (Njordlinger Test, Main Agent, .MD\handoffs\results\hpm-b5-*, hpm-b8-*, hpm-d1-*): B5 - with the bar
    held up, bossShown true and the subtitles applied at -72 (10 % of the 720 stage) in mode 1 (frame hpm-b5-mode1); in
    mode 2 the compass hid and the subtitles went back (hpm-b5-mode2); the bar gone, the compass came back. B8 3/3 - a
    sword picked up showed the list, looting a wolf (ContainerMenu) hid it, closing the menu showed it again; uDirection 1
    put the list on its bottom rows, the newest lowest (hpm-b8-grows-up). D1 3/3 - the preview showed Alduin's boss bar,
    an "Oakflesh 1:00" effects sample, the bow meter and the held bars, and a compass on "Only in combat" out of combat
    (hpm-d1-preview); it lapsed cleanly. Not yet seen: the page's own switch (the run used DevBench preview), a real boss
    driving B5, bHideInCraftingMenus.
- B2 - magicka and stamina under the bars over characters (TrueHUD's resource bars): two thin bars, magicka on the left,
  stamina on the right emptying toward the bar's end (infobar.swf Frame2 / Fill2 / Frame3 / Fill3). [InfoBars]
  uResourcesHostiles / uResourcesTeammates / uResourcesOthers: 0 never, 1 when not full, 2 always (TrueHUD's defaults 0 / 1
  / 0); the values are read only for a group whose mode is not Never. 3 new strings, 11 languages.
- B3 - more on those bars (TrueHUD's info-bar options):
  - a damage counter (bDamageCounter, fDamageCounterSeconds 2): the health lost in the last seconds at the bar's right
    end (Value3), summed while the hits keep coming;
  - the level coloured by difficulty (bLevelColors): red 10 or more levels above the player, grey 10 or more below;
  - uAnchor: over the head (as before) or on the chest.
  - 8 new strings, 11 languages. Not built: TrueHUD's soul-gem / square level icons and the wider bar for a target.
- Tested 2026-10-05 (Njordlinger Test, Main Agent, one launch, .MD\handoffs\results\hpm-run2-*, hpm-b2r-*, hpm-b3-*): B2
  4/4 - Never no sub-bars; Always both, stamina ~1.0 (frame hpm-b2r-1); When not full on a rested wolf hid stamina; 40
  stamina damage showed stamina 0.467 alone (hpm-b2r-2). B3 6/6 - the counter read -12, then -17 for 5 more within 2 s,
  blank 3 s later (hpm-b3-1); the wolf's level grey with the player at level 40; on the chest the bar sat at y 518.8
  against 378.9 over the head (hpm-b3-2). Not seen: the red level colour. DevBench save (the resets) wrote the INI.
- B4a - boss rule files: besides its own rules (dragons, a location's boss), the boss bar takes TrueHUD's rule syntax -
  [BossRecognition] Race / NPC / LocRefType / NPCBlacklist = Plugin:0xID - from HPM's own SKSE\Plugins\HUDPositionManager\
  Bosses\*.ini (HPM_bosses.ini ships as a commented template, no active rules) and from the TrueHUD_*.ini files mods ship
  for TrueHUD (SKSE\Plugins\TrueHUD), so their bosses keep their bar without TrueHUD. Read once, at the first boss check;
  a line whose plugin is not loaded is skipped; the blacklist wins.
  - Tested 2026-10-05 (Njordlinger Test, Main Agent, .MD\handoffs\results\hpm-b4a-*): a test rule file naming the wolf's
    base (Skyrim.esm 0x023ABE) gave a fighting wolf the boss bar (frame hpm-b4a-1, "Wolf", level 5); the log read 3 files
    - 14 races, 65 characters, 2 location types, 2 never a boss (TrueHUD's own TrueHUD_base.ini among them); the wolf
    gone, the bar went.
- B4b - more boss bars at once: [BossBars] uMaxCount 1..3 (default 1), the nearest boss on the first bar and the next
  ones on bossbar.swf's Boss2 / Boss3 rows; bStackUp puts them over the first bar instead of under it, uSpacing (50)
  apart. Over the first bar only where there is room: when the top row's name would leave the visible screen (the bar
  at its default spot by the top edge), they stay under it, and the page says so by the control. 6 new strings, 11
  languages. Not on rows 2 and 3: the phantom, resource sub-bars, a damage counter.
  - Tested 2026-10-05 (Njordlinger Test, Main Agent, .MD\handoffs\results\hpm-b4b-*, hpm-b4b2-*): three wolves made
    bosses by a rule file - uMaxCount 1 gave no extra rows, 3 gave two (frame hpm-b4b-1, under the first); one wolf gone,
    one row; all gone, no bar. The first try at "over the first" pushed rows 2 and 3 off the top of the screen; with the
    room check, at the default spot they stayed under it (hpm-b4b2-2), and with the bar moved 40 % down they stacked over
    it, all on screen (hpm-b4b2-3).
- B7 - the colours of HPM's own bars ([Colors] sHealth / sMagicka / sStamina / sPhantom, RRGGBB; TrueHUD's Colors page):
  each the art's own until the player picks one, so a reskin keeps its look. A colour transform on the Fill, Fill2,
  Fill3 and Phantom clips (multiply 0, add 0..255), written once per change; health also colours the boss bars (every
  row) and the bars over characters. The page: a picker per colour with an "art's own" switch, "All the art's own",
  "TrueHUD's colours" (its shipped DF2020 / 284BD7 / 007E00 / CBCBCB) and "Import from TrueHUD" (the player's MCM Helper
  TrueHUD.ini over TrueHUD's shipped settings.ini). 9 new strings, 11 languages. Not built: TrueHUD's other 24 colours
  (backgrounds, penalties, flashes, the difficulty outlines).
  - Tested 2026-10-05 (Njordlinger Test, Main Agent, judged by eye, .MD\handoffs\results\hpm-b7-*): the art's own red /
    blue / green; picked gold / cyan / magenta on the player bars, the wolf's bar and sub-bars and the boss bar (so the
    0..255 add scale is right); Import read both TrueHUD files and turned every bar TrueHUD's darker red / blue / green;
    "art's own" put the art back.
- Settings are also saved while HPM's page draws. Saving ran only in the HUD hook, which a menu holding the game stops, so
  a change made on the page and then a quit from the menu was lost; found when a test's reset, made just before the game
  closed, never reached the INI (2026-10-05). The two savers take turns. Not yet seen in game.

### Fixed
- Review follow-ups (REVIEW-2026-10-04.md, the items still open):
  - M1: the info bars and the built widgets hold the HUD movie (a GPtr, the positioner's Tracked pattern) for as long as
    their clips point into it, and release the clips first on a swap - they were safe only because the positioner ticked
    them before dropping its own reference. Holding it also closes the address-reuse hole in the swap test.
  - M4: a mod's hide (the author API) while the player's layout is not applied hides the element where the HUD puts it;
    it was also moved, sized and faded by the layout.
  - M5 (noted, not changed): the Survival penalty is Survival's negative temporary modifier (measured), and a disease or
    a poison that lowers the max works the same way, so with Survival on its reduction is drawn in the Penalty too.
  - L2: hits older than the 10 s window are pruned at each scan (the map grew forever; a recycled 0xFF.. id could
    inherit "hit recently").
  - L3: an info bar whose art never loads (missing infobar.swf, or a holder without its child) gives its slot up after
    ~5 s, logged once; it held its character and the slot for the session.
  - L5: the info bars and the built widgets' first spot divide by HUDMovieBaseInstance's own scale (a HUD-scale mod drew
    them off their point); its display info is read once a frame.
  - L6: DevBench's bars list reports a FormID stored on the main thread instead of resolving handles on its own thread.
  - The boss bar lets its boss go when a save starts loading (a handle from the old game could resolve to another
    reference), and recent loot drops what a load or a new game put in the inventory.
  - Tested 2026-10-04 (HPM Minimal, New Game, Alternate Perspective's start room): a wolf placed beside the player got
    its info bar (projected at 604,382, alpha 100) with the M1 / L5 changes in.
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
