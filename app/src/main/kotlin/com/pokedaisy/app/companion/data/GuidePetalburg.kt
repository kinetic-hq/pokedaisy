package com.pokedaisy.app.companion.data

/**
 * Pokémon Petalburg Dynasty's guide: HERE's wild Pokémon, read from its ROM (GUIDE_TABLES_PETALBURG). No NEXT BOSS or
 * area data yet: you play PETALBURG's own Gym Leader, and its story's order and its maps' scripts haven't been read.
 */
internal val GUIDE_PETALBURG = GameGuide(
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
                    "Why is there no NEXT BOSS page?",
                    "This game tells its own story in its own order, so there's no one boss to show next yet.",
                ),
            ),
        ),
    ),
)
