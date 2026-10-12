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
