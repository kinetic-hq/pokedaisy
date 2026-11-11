// GPL-3.0. DsCore over DSperate (third_party/dsperate, unmodified): the Linux DS backend it always was.
#include "ds_core.h"
#include "core/nds.h"
#include "core/state/state.h"
#if DSPERATE_JIT
#include "core/cpu/jit/jit.h"
#endif
#include <cstdio>

namespace {
struct DsperateCore final : DsCore {
  std::unique_ptr<ds::NDS> nds = std::make_unique<ds::NDS>();
  bool jit = false;
  ~DsperateCore() override {
#if DSPERATE_JIT
    if (jit) ds::jit::detach(*nds);
#endif
  }
  const char* name() const override { return "DSperate"; }
  bool boot(const std::string& path, const std::vector<uint8_t>&, std::string& err) override {
    if (!nds->load_bios("", "", "", {}, &err) || !nds->load_rom(path)) {
      if (err.empty()) err = nds->rom_error;
      return false;
    }
    nds->setup_direct_boot();
    std::fprintf(stderr, "DSperate %s\n", path.c_str());  // also the marker linux/package.py --dsperate looks for
    nds->io.start_rtc_clock();
    return true;
  }
  // The JIT goes on once the console the frontend keeps is settled (after a resume state's load), as it always did.
  bool start(std::string& err) override {
#if DSPERATE_JIT
    if (!ds::jit::attach(*nds, true, true)) { err = "JIT attach failed"; return false; }
    jit = true;
#endif
    (void) err;
    return true;
  }
  uint32_t read32(uint32_t a) override { return a >= 0x02000000 && a <= 0x023ffffc ? nds->bus.dma_read32(ds::Cpu::ARM9, a) : 0; }
  void write32(uint32_t a, uint32_t v) override { if (a >= 0x02000000 && a <= 0x023ffffc) nds->bus.dma_write32(ds::Cpu::ARM9, a, v); }
  const uint8_t* mainRam() const override { return nds->bus.main_ram.get(); }
  size_t mainRamSize() const override { return nds->bus.main_ram_size(); }
  void setButtons(uint32_t held) override { nds->io.set_buttons(held); }
  void setTouch(int x, int y, bool down) override { nds->io.set_touch(x, y, down); }
  void runFrame() override { nds->run_frame(); nds->gpu.quiesce(); }
  const uint32_t* framebuffer(int screen) override { return nds->gpu.framebuffer(screen); }
  size_t takeAudio(int16_t* out, size_t maxFrames) override { return nds->spu.take(out, maxFrames); }
  double audioRate() const override { return nds->spu.output_rate_hz(); }
  std::vector<uint8_t> battery() const override { return nds->cart->sram(); }
  bool loadBattery(const std::vector<uint8_t>& data) override { return !data.empty() && nds->cart->load_save(data.data(), data.size()).fitted; }
  void restoreBattery(const std::vector<uint8_t>& data, bool dirty) override {
    nds->cart->sram() = data;
    if (!dirty) nds->cart->clear_sram_dirty();
  }
  bool batteryDirty() const override { return nds->cart->sram_dirty(); }
  void clearBatteryDirty() override { nds->cart->clear_sram_dirty(); }
  uint32_t batteryWrites() const override { return nds->cart->sram_writes(); }
  bool saveState(std::vector<uint8_t>& out, std::string& err) override {
    ds::state::Writer w;
    if (!nds->save_state(w, err)) return false;
    out = w.data();
    return true;
  }
  bool loadState(const std::vector<uint8_t>& data, bool keepBattery, std::string& err) override {
    if (data.size() < 8) { err = "truncated state"; return false; }
    std::vector<uint8_t> before = nds->cart->sram();
    bool dirty = nds->cart->sram_dirty();
    ds::state::Reader reader(data.data(), data.size());
    if (!nds->load_state(reader, err)) return false;
    if (keepBattery) restoreBattery(before, dirty);
    return true;
  }
  uint64_t frameCount() const override { return nds->frame_count; }
  const char* stateTag() const override { return ".dsperate"; }
  bool saveBesideRom() const override { return false; }
};
}

std::unique_ptr<DsCore> ds_core_create() { return std::make_unique<DsperateCore>(); }
