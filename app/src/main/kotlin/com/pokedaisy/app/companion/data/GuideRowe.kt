package com.pokedaisy.app.companion.data

/**
 * Pokémon R.O.W.E.'s guide: HERE's wild Pokémon, read from its ROM (GUIDE_TABLES_ROWE). No NEXT BOSS: its gyms come in
 * any order, and its badge flags are renumbered and haven't been found.
 */
internal val GUIDE_ROWE = GameGuide(
    verified = false,
    pages = listOf(
        page(
            "TIPS",
            section(
                "THIS GUIDE",
                entry(
                    "Where does this guide come from?",
                    "HERE lists the wild POKéMON of the place you're in, read from the game itself.",
                ),
                entry(
                    "Why are the levels so low?",
                    "HERE shows the levels the game's tables hold. The game raises wild levels as you earn badges.",
                ),
                entry(
                    "Why is there no NEXT BOSS page?",
                    "You can take the gyms in any order, so there's no one boss to show next.",
                ),
            ),
        ),
    ),
)
