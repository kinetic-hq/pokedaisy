# NEW tags

> Moved verbatim from CLAUDE.md on 2026-11-12 (lines 93-99). The guidance is Lidor Itzhari's, PokeDaisy's author (lidor30/pokedaisy), with this fork's additions. Code comments and the add-rom skill that say "CLAUDE.md" mean these notes.

## NEW tags on new features

**Every new player-facing feature gets a NEW tag** (the user's rule, from the HUD on): its settings row on both screens
carries `labelBadge = NewFeatures.badge(NewFeatures.<ID>)` (green, like the Library's NEW), and its id goes into
`NewFeatures.ALL` (`companion/NewFeatures.kt`). The tag shows for 7 days from the first launch of a build that has the
feature (dated at app start, `Prefs.newFeatureSince`), then goes by itself.
