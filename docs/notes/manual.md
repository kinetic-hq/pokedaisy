# The user manual

> Moved verbatim from CLAUDE.md on 2026-11-12 (lines 72-92). The guidance is Lidor Itzhari's, PokeDaisy's author (lidor30/pokedaisy), with this fork's additions. Code comments and the add-rom skill that say "CLAUDE.md" mean these notes.

## The user manual (`docs/manual/`): keep it current

**Every change a player can notice updates the manual in the same change**: a new feature, setting, tab,
button or supported game, a changed default, a renamed label, a removed option (if unsure, it counts). The
manual is Markdown in `docs/manual/` (`README.md` there has the format and the writing rules) and the *only*
place it's written: the website renders it as `/docs/` (static pages, for search engines) and as the docs
API `/api/docs/v1/docs.json` (`website/src/lib/docs.js`), which the app's HELP reader (`DocsApi.kt`,
`companion/ui/DocsScreen.kt`: top-screen Settings > HELP, the library's `?`, companion SETTINGS > HELP)
fetches and keeps for offline reading. Both deploy with an app release, like the rest of the site.
Writing rules (the user's): human and plain, second person, **no em / en dashes** (nor a spaced hyphen for
one), **no real Pokémon / move / item names** in text or pictures (the invented creatures EMBERPUP & co. in
`website/src/lib/creatures.js` stand in, also on the homepage demos). Reading text is Atkinson Hyperlegible
(`readingFontFamily`), headings and labels stay pixel. Screenshots: `make docs-shots` (ui-preview's
`docsShots`, `ui-preview/src/render/DocsShots.kt`: the real screens over an invented save, no ROM art; add a
`Shot` there for a new screen). `make docs-check` / CI's Docs job fails on a page that won't parse, a broken
link or image, a dash, or a `SETTING_KEYWORDS` label no page names in backticks (so a new settings row needs
its keywords *and* its docs). Search is one engine in two languages, `website/src/lib/docsearch.js` and
`DocsSearch.kt`, kept in step by `website/test/docsearch-cases.json` (`docsearch.test.js`, `DocsSearchTest`,
ui-preview's `docsSearchCheck`); words people search for go in `docs/manual/synonyms.yml` or a page's
`keywords`. A block type the Markdown gains must be drawn by `DocBlocks.astro` *and* `DocsScreen.kt`.
