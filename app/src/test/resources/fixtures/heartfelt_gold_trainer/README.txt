captured: 2026-11-12 (headless native-capture/nds_dump, by hand; mainram.bin = the DS's 4 MB main RAM at 0x02000000,
  zeroed but for what the desktop's Johto reader and Gen4Reader read: 0x021D1000-0x021D3FFF, sFieldSysPtr, the SaveData through its
  Pokedex, the FieldSystem, its player avatar and map object, gOverlayTemplate_Battle's init (0x020FA484), the process
  manager and its child, the BattleSystem, its BattleContext through the battle mons, and the parties).
rom: Pokemon - HeartFelt Gold (~/Downloads/Game ROMs & Emulation/nds/, sha1 59f5d4f8..., IPKE rev 0, ARM9 0x111EF8 bytes)
save: heartfelt_gold's new game (its README), from its state after the six creatures were given: then a field script,
  TrainerBattle (213) trainer 20, wait 600. dump: at the commands, CYNDAQUIL Lv12 32/32 (TACKLE 35, LEER 30,
  SMOKESCREEN 20, EMBER 25) against the gym leader's first of six, species id 570 Lv14 (45 HP), as the screen showed.
