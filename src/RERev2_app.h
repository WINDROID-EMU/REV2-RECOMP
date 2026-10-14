// RERev2 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>

class Rerev2App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Rerev2App>(new Rerev2App(ctx, "RERev2",
        PPCImageConfig));
  }

  void OnConfigurePaths(rex::PathConfig& paths) override {
    auto current_dir = std::filesystem::current_path();
    if (paths.cache_root.empty() || paths.cache_root.string().find(".local/share") != std::string::npos) {
      paths.cache_root = current_dir / "cache";
    }
    if (paths.user_data_root.empty() || paths.user_data_root.string().find(".local/share") != std::string::npos) {
      paths.user_data_root = current_dir / "user_data";
    }
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
    // High performance defaults: eliminates micro-stutters and PCIe readback stalls
    rex::cvar::SetFlagByName("vulkan_async_skip_incomplete_frames", "true");
    rex::cvar::SetFlagByName("async_shader_compilation", "true");
    rex::cvar::SetFlagByName("readback_resolve", "none");
    rex::cvar::SetFlagByName("texture_cache_memory_limit_soft", "1024");
    rex::cvar::SetFlagByName("texture_cache_memory_limit_hard", "2048");
    rex::cvar::SetFlagByName("gpu_3d_to_2d_texture", "true");
    rex::cvar::SetFlagByName("vulkan_pipeline_creation_threads", "6");
    rex::cvar::SetFlagByName("log_level", "warning");
    rex::cvar::SetFlagByName("log_noisy", "false");
    rex::cvar::SetFlagByName("log_verbose", "false");
  }
};
