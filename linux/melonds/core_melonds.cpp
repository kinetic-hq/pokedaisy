// GPL-3.0. DsCore over melonDS (third_party/melonds, pinned to 1.1 with the app's patch), set up the way the Android
// app's DS core (app/src/main/cpp/nds/pk_nds.cpp) does: FreeBIOS and generated firmware, direct boot, the software
// renderer on the emulation thread, the JIT without fast memory, melonDS's Platform layer from pk_nds_platform.cpp.
#include "ds_core.h"
#include "pk_nds_platform.h"

#include <Args.h>
#include <GPU.h>
#include <GPU3D_Soft.h>
#include <NDS.h>
#include <NDSCart.h>
#include <RTC.h>
#include <SPU.h>
#include <Savestate.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

using namespace melonDS;

namespace {
constexpr double SAMPLE_RATE = 48000.0;

void onLog(int level, const char* message) {
  // Debug and info are melonDS's chatter; warnings and errors go to the host's log.
  if (level >= (int) Platform::LogLevel::Warn) std::fprintf(stderr, "melonDS: %s", message);
}

struct MelonCore final : DsCore {
  PkNdsHost host{};
  std::unique_ptr<NDS> nds;
  // The save memory as the game last wrote it, kept by WriteNDSSave: what the frontend flushes.
  std::vector<uint8_t> save;
  bool dirty = false;
  uint32_t writes = 0;
  std::vector<uint32_t> screens[2] = {std::vector<uint32_t>(256 * 192), std::vector<uint32_t>(256 * 192)};

  MelonCore() {
    host.onSave = [](PkNdsHost* h, const uint8_t* data, uint32_t len, uint32_t, uint32_t) {
      auto* self = static_cast<MelonCore*>(h->user);
      self->save.assign(data, data + len);
      self->dirty = true;
      self->writes++;
    };
    // 1 = External: our own Stop() on the way out, not worth a line.
    host.onStop = [](PkNdsHost*, int reason) { if (reason != 1) std::fprintf(stderr, "melonDS stopped (%d)\n", reason); };
    host.user = this;
    pkNdsSetLog(onLog);
  }
  ~MelonCore() override {
    if (nds) nds->Stop();
  }
  const char* name() const override { return "melonDS"; }

  void setClock() {
    time_t now = std::time(nullptr);
    struct tm t{};
#ifdef _WIN32
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    nds->RTC.SetDateTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
  }

  bool boot(const std::string&, const std::vector<uint8_t>& rom, std::string& err) override {
    NDSArgs args;
    args.OutputSampleRate = SAMPLE_RATE;
#ifdef JIT_ENABLED
    // Fast memory catches faults with a signal handler; without it the JIT needs nothing from the host.
    args.JIT = JITArgs{32, true, true, false};
#endif
    nds = std::make_unique<NDS>(std::move(args), &host);
    nds->Reset();
    auto copy = std::make_unique<u8[]>(rom.size());
    std::memcpy(copy.get(), rom.data(), rom.size());
    auto cart = NDSCart::ParseROM(std::move(copy), (u32) rom.size(), &host, NDSCart::NDSCartArgs{});
    if (!cart) { err = "not a DS ROM melonDS can run"; return false; }
    nds->SetNDSCart(std::move(cart));
    nds->Reset();
    nds->SetupDirectBoot("rom.nds");
    static_cast<SoftRenderer&>(nds->GPU.GetRenderer3D()).SetThreaded(false, nds->GPU);
    setClock();
    nds->Start();
    if (const u8* mem = nds->GetNDSSave()) save.assign(mem, mem + nds->GetNDSSaveLength());
    dirty = false;
    return true;
  }

  uint32_t read32(uint32_t a) override {
    if (a < 0x02000000 || a > 0x023ffffc) return 0;
    uint32_t v;
    std::memcpy(&v, nds->MainRAM + (a & nds->MainRAMMask & ~3u), 4);
    return v;
  }
  void write32(uint32_t a, uint32_t v) override {
    if (a >= 0x02000000 && a <= 0x023ffffc) std::memcpy(nds->MainRAM + (a & nds->MainRAMMask & ~3u), &v, 4);
  }
  const uint8_t* mainRam() const override { return nds->MainRAM; }
  size_t mainRamSize() const override { return (size_t) nds->MainRAMMask + 1; }
  void setButtons(uint32_t held) override { nds->SetKeyMask(~held & 0xFFF); }
  void setTouch(int x, int y, bool down) override {
    if (down) nds->TouchScreen((u16) std::clamp(x, 0, 255), (u16) std::clamp(y, 0, 191));
    else nds->ReleaseScreen();
  }
  void runFrame() override {
    nds->RunFrame();
    int fb = nds->GPU.FrontBuffer;
    for (int s = 0; s < 2; s++) {
      const u32* src = nds->GPU.Framebuffer[fb][s].get();
      if (!src) continue;
      // melonDS draws xRGB; SDL's ARGB8888 wants the alpha set.
      for (int i = 0; i < 256 * 192; i++) screens[s][i] = src[i] | 0xFF000000u;
    }
  }
  const uint32_t* framebuffer(int screen) override { return screens[screen ? 1 : 0].data(); }
  size_t takeAudio(int16_t* out, size_t maxFrames) override {
    return (size_t) std::max(0, nds->SPU.ReadOutput(out, (int) maxFrames));
  }
  double audioRate() const override { return SAMPLE_RATE; }

  std::vector<uint8_t> battery() const override { return save; }
  bool loadBattery(const std::vector<uint8_t>& data) override {
    // Only a save of this cart's size: a smaller or bigger file would be cut or padded and then written back over.
    if (data.empty() || data.size() != nds->GetNDSSaveLength()) return false;
    nds->SetNDSSave(data.data(), (u32) data.size());
    save = data;
    dirty = false;
    return true;
  }
  void restoreBattery(const std::vector<uint8_t>& data, bool wasDirty) override {
    // Straight into the cart's memory: SetNDSSave would count as the game writing it.
    if (u8* mem = nds->GetNDSSave())
      std::memcpy(mem, data.data(), std::min<size_t>(data.size(), nds->GetNDSSaveLength()));
    save = data;
    dirty = wasDirty;
  }
  bool batteryDirty() const override { return dirty; }
  void clearBatteryDirty() override { dirty = false; }
  uint32_t batteryWrites() const override { return writes; }

  bool saveState(std::vector<uint8_t>& out, std::string& err) override {
    Savestate st;
    if (!nds->DoSavestate(&st) || st.Error) { err = "melonDS couldn't take a state"; return false; }
    st.Finish();
    auto* p = static_cast<const uint8_t*>(st.Buffer());
    out.assign(p, p + st.Length());
    return true;
  }
  bool loadState(const std::vector<uint8_t>& data, bool keepBattery, std::string& err) override {
    if (data.size() < 16) { err = "truncated state"; return false; }
    std::vector<uint8_t> before = save;
    bool wasDirty = dirty;
    std::vector<uint8_t> copy = data;
    Savestate st(copy.data(), (u32) copy.size(), false);
    if (st.Error || !nds->DoSavestate(&st) || st.Error) { err = "not a melonDS state for this cart"; return false; }
    // The state's clock is the moment it was taken: the DS keeps the real time, as at boot.
    setClock();
    if (keepBattery) restoreBattery(before, wasDirty);
    return true;
  }
  uint64_t frameCount() const override { return nds->NumFrames; }
  const char* stateTag() const override { return ".melonds"; }
  bool saveBesideRom() const override { return true; }
};
}

std::unique_ptr<DsCore> ds_core_create() { return std::make_unique<MelonCore>(); }
