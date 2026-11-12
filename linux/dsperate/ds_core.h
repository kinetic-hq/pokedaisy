// GPL-3.0. The DS core the SDL host's DS frontend (frontend.cpp) runs: DSperate on Linux (POCKET_DSPERATE) or
// melonDS on Windows and Linux (POCKET_MELONDS, linux/melonds/). One of them is built; the frontend, the companion
// and the app's field read (pk_nds_field.cpp) only see this.
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct DsCore {
  virtual ~DsCore() {}
  // "DSperate" / "melonDS": the log's first word, which the tests read.
  virtual const char* name() const = 0;
  // A cart from its bytes, booted straight into the game (no firmware menu). path names it in the core's logs.
  virtual bool boot(const std::string& path, const std::vector<uint8_t>& rom, std::string& err) = 0;
  // Called once on the console that will run, after any resume state is in.
  virtual bool start(std::string& err) { (void) err; return true; }
  // Main RAM (0x02000000-0x023FFFFF) only: what the companion and the field read touch.
  virtual uint32_t read32(uint32_t addr) = 0;
  virtual void write32(uint32_t addr, uint32_t value) = 0;
  virtual const uint8_t* mainRam() const = 0;
  virtual size_t mainRamSize() const = 0;
  // The DS buttons as DSperate numbers them (bit 0 A, 1 B, 2 SELECT, 3 START, 4-7 RIGHT LEFT UP DOWN, 8 R, 9 L,
  // 10 X, 11 Y), and the stylus on the bottom screen (0-255, 0-191).
  virtual void setButtons(uint32_t held) = 0;
  virtual void setTouch(int x, int y, bool down) = 0;
  virtual void runFrame() = 0;
  // 256x192 ARGB, screen 0 the top, 1 the bottom: valid until the next frame.
  virtual const uint32_t* framebuffer(int screen) = 0;
  // Interleaved stereo s16 frames, up to max; 0 when there are none left.
  virtual size_t takeAudio(int16_t* out, size_t maxFrames) = 0;
  virtual double audioRate() const = 0;
  // The cart's battery save, as standalone emulators keep it (the raw save memory, no footer).
  virtual std::vector<uint8_t> battery() const = 0;
  // Put a save file in: false (and nothing changed) when it isn't this cart's size.
  virtual bool loadBattery(const std::vector<uint8_t>& data) = 0;
  // Replace the save memory as it is now, without marking it written (a state's copy put back).
  virtual void restoreBattery(const std::vector<uint8_t>& data, bool dirty) = 0;
  virtual bool batteryDirty() const = 0;
  virtual void clearBatteryDirty() = 0;
  // Counts the game's writes, so the frontend flushes only once they've stopped.
  virtual uint32_t batteryWrites() const = 0;
  virtual bool saveState(std::vector<uint8_t>& out, std::string& err) = 0;
  // A state carries the save memory: keepBattery puts the console's back afterwards (never write a state's older
  // save over the file), still waiting for its flush if it was.
  virtual bool loadState(const std::vector<uint8_t>& data, bool keepBattery, std::string& err) = 0;
  virtual uint64_t frameCount() const = 0;
  // A state file's tag (".dsperate" / ".melonds"): one core's states mean nothing to the other.
  virtual const char* stateTag() const = 0;
  // Where the battery save lives: DATA/saves/<file>.sav (false) or beside the ROM as <name>.sav (true), where
  // standalone melonDS and the Android app keep it.
  virtual bool saveBesideRom() const = 0;
  // 59.8261 for both.
  virtual double fps() const { return 59.8261; }
  // EXPAND (melonDS only: the app's pk_nds_wide.cpp, the game's 3D drawn again past the top screen's edges).
  virtual bool wideAvailable() const { return false; }
  virtual void wideEnable(bool on) { (void) on; }
  // What the next frames' margins show: the overworld's 3D, a battle's background (Gen 4), or nothing (black).
  virtual void wideState(bool overworld, bool battle) { (void) overworld; (void) battle; }
  // The window shown, in the top screen's pixels (its top-left (0, 0)): the margins drawn after each frame.
  virtual void wideWindow(int x, int y, int w, int h) { (void) x; (void) y; (void) w; (void) h; }
  // The last frame's window as ARGB (w x h): false when EXPAND drew nothing.
  virtual bool wideView(uint32_t* out, int x, int y, int w, int h) { (void) out; (void) x; (void) y; (void) w; (void) h; return false; }
  // The top screen faded all the way to black or white (a door, a warp): the camera's FADE.
  virtual bool topFadedOut() const { return false; }
};

std::unique_ptr<DsCore> ds_core_create();
