# Feature tips

> Moved verbatim from CLAUDE.md on 2026-11-12 (lines 119-132). The guidance is Lidor Itzhari's, PokeDaisy's author (lidor30/pokedaisy), with this fork's additions. Code comments and the add-rom skill that say "CLAUDE.md" mean these notes.

## Feature tips (APP > TIPS, ON)

A TIP card (the PokeDaisy flower, NO THANKS / TRY IT, a bar that runs out = later) introduces a feature the player hasn't
found (the user's picks, 2026-10-21): ZOOM / look-around, EXPAND, QUICK MENU, HUD taps, PC, AUTO-RUN, WILD ENCOUNTERS,
REUSABLE TMS, FLY, and the DS triggers (2026-11-03: `TipId.DS_TRIGGERS`, `early`: its moment - a DS game on one screen - skips the half
hour and the gap, `EARLY_SETTLE` into a session) (`TipId`, `companion/FeatureTips.kt`; card + texts `companion/ui/FeatureTip.kt`). `TipScheduler`'s rules
(`TipSchedulerTest`): 30 min of field play first, **at least 5 min of play between two cards** (the user's rule: never one
after the other), only on a calm field (no battle, menu, text, HUD panel, QUICK MENU or companion window), a moment's tip
first (`FeatureTips.moment`: B held while walking, 3 wild battles in 3 min, a TM used up, the game's region map closed),
never for a feature the player used (`FeatureTips.used`, called where each feature runs) or turned down; an ignored one
comes back once, an hour later. Two screens: the card is on the companion (`CompanionSettings.showsTips`); one screen: top
centre of the HUD's view. `PokeDaisyActivity.tickTips` / `tipAvailable` / `tryTip`. A new tip: a `TipId`, its text, where
it's used, its availability and its TRY IT, the manual's TIPS section.
