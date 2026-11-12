package com.pokedaisy.app.companion.data

import org.junit.Assert.assertEquals
import org.junit.Test

/**
 * pokeemerald-expansion's Tera type: the 5 bits above the 11-bit species in the growth substruct (gen3TeraType),
 * read for a Terastallized battler (Petalburg Dynasty's activeGimmick). Built by hand: personality and OT ID 0, so
 * the key is 0 and the substructs sit in GAEM order, growth first.
 */
class TeraTypeTest {
    private fun mon(speciesWord: Int): ByteArray {
        val raw = ByteArray(100)
        raw[0x20] = (speciesWord and 0xFF).toByte()
        raw[0x21] = (speciesWord ushr 8).toByte()
        raw[0x22] = 1 // a held item, so the checksum is never the species word alone
        var sum = 0
        for (i in 0 until 24) sum += (raw[0x20 + i * 2].toInt() and 0xFF) or ((raw[0x21 + i * 2].toInt() and 0xFF) shl 8)
        raw[28] = (sum and 0xFF).toByte()
        raw[29] = ((sum ushr 8) and 0xFF).toByte()
        return raw
    }

    @Test
    fun `the Tera type is the five bits above the species`() {
        // The TERA career's foe: species 1336, Tera type 5 (Ground), as its party record read.
        assertEquals(5, gen3TeraType(mon(1336 or (5 shl 11)), 0))
        assertEquals(1336, decodePartyMon(mon(1336 or (5 shl 11)), 0)?.species?.and(0x7FF))
    }

    @Test
    fun `no Tera type reads zero`() {
        assertEquals(0, gen3TeraType(mon(987), 0))
        assertEquals(0, gen3TeraType(ByteArray(100), 0)) // an empty slot fails the checksum
    }
}
