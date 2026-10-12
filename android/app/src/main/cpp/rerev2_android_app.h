#pragma once

#if __has_include("generated/default/RERev2_init.h")
#include "generated/default/RERev2_init.h"
#elif __has_include("RERev2_init.h")
#include "RERev2_init.h"
#else
#include <rex/ppc/function.h>
extern const rex::PPCImageInfo PPCImageConfig;
#endif

#include "src/RERev2_app.h"
#include "audio/aaudio_driver.h"
#include "input/android_input_driver.h"
#include "vfs/android_storage.h"

#include <rex/system/kernel_state.h>
#include <rex/system/function_dispatcher.h>
#include <rex/system/xmemory.h>

namespace rex::android {

class Rerev2AndroidApp final : public Rerev2App {
 public:
  using Rerev2App::Rerev2App;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Rerev2AndroidApp>(new Rerev2AndroidApp(ctx, "RERev2", PPCImageConfig));
  }

 protected:
  void OnPreSetup(rex::RuntimeConfig& config) override {
    Rerev2App::OnPreSetup(config);
    // Override audio and input factories to use pure Android NDK backends
    config.audio_factory = REX_AUDIO_BACKEND(rex::audio::android::AndroidAAudioSystem);
    config.input_factory = REX_INPUT_BACKEND(rex::input::android::AndroidInputSystem::Create);
  }

  void OnConfigurePaths(rex::PathConfig& paths) override {
    Rerev2App::OnConfigurePaths(paths);
    rex::vfs::android::AndroidStorage::ConfigureAppPaths(paths);
  }
};

}  // namespace rex::android

#include "src/ppc_thunk_dispatcher.h"

// Fast native path for MT Framework inline getter 0x82F57288
inline void sub_82F57288(PPCContext& ctx, uint8_t* base) {
  uint32_t r11 = 0x83AC0000;
  r11 = REX_LOAD_U32(r11 + 0x6C20);
  ctx.r11.u64 = r11;
  ctx.r3.u64 = REX_LOAD_U64(r11 + 0x28);
}

namespace rex::runtime {
inline PPCFunc* ResolveIndirectFunction(uint32_t guest_address) {
  if (guest_address == 0x82F57288) {
    return &sub_82F57288;
  }

  if (auto* rt = rex::Runtime::instance()) {
    if (auto* fd = rt->function_dispatcher()) {
      if (auto* fn = fd->GetFunction(guest_address)) {
        return fn;
      }
    }
  }

  return &ExecutePpcThunk;
}
}  // namespace rex::runtime

