captured: 2026-11-12 (headless native-capture/nds_dump, by hand; mainram.bin = the DS's 4 MB main RAM at 0x02000000,
  zeroed but for what the desktop's Johto reader reads: 0x021D1000-0x021D3FFF (sSaveDataPtr 0x021D2228, sFieldSysPtr
  0x021D4158, as retail), the SaveData at 0x0227C254 through its Pokedex (+16 +5288 +0x800), the FieldSystem,
  the PlayerAvatar and the player's map object) - the rest is unrelated memory, and zeros keep the file tiny in git.
rom: Pokemon - HeartFelt Gold (~/Downloads/Game ROMs & Emulation/nds/, sha1 59f5d4f8..., IPKE rev 0, ARM9 0x111EF8 bytes,
  an hg-engine build of the US HeartGold)
save: none exists for it. A new game played headless to the player's room, then field scripts run with nds_dump's
  `script`: GiveMon (137) x6 - 155 Lv12, 750 Lv20, 25 Lv9 holding 234, 937 Lv35, 445 Lv40, 6 Lv30 (species ids are the
  build's: Gen 5 on sit at national + 50) -, GivePokedex (291), GiveRunningShoes (293), GiveItem (125): 17 x5, 4 x10,
  155 x3, 79 x2, 328, 450, 420; then SaveGameNormal (254). dump: in the player's room (map header 64), money 3000.
layout found: party at body+144 and the trainer at body+100 as retail; the bag at body+1604 with pockets
  {245, 92, 101, 12, 40, 64, 24, 30} (items and key items grown), so everything after it sits 496 bytes later: the
  location at body+5156 (64, as a SoulSilver new game made the same way), the event flags at body+4788, the Pokedex
  (magic 0xBEEFCAFE) at body+5288. The repel counter, SAVE_MISC_DATA and the Pokegear word aren't mapped.
pokedex: 1534 species ids; caught bits at dex+4, seen bits at dex+0x400 (the six given creatures in both).
