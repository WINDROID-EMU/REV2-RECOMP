#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <atomic>
#include <rex/ui/vulkan/api.h>

namespace rev2::gpu {

struct Viewport {
  float x = 0.0f;
  float y = 0.0f;
  float width = 960.0f;
  float height = 540.0f;
  float min_z = 0.0f;
  float max_z = 1.0f;
};

struct ScissorRect {
  int32_t left = 0;
  int32_t top = 0;
  int32_t right = 960;
  int32_t bottom = 540;
};

struct Vector4f {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float w = 0.0f;
};

struct VertexStream {
  uint32_t buffer_guest_addr = 0;
  uint32_t offset = 0;
  uint32_t stride = 0;
};

struct IndexBufferBinding {
  uint32_t buffer_guest_addr = 0;
  uint32_t format = 0; // 0 = 16-bit, 1 = 32-bit
  uint32_t count = 0;
};

struct TextureBinding {
  uint32_t guest_addr = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t depth = 0;
  uint32_t format = 0;
  uint32_t mip_levels = 0;
  bool is_tiled = false;
};

struct SamplerBinding {
  uint32_t min_filter = 0;
  uint32_t mag_filter = 0;
  uint32_t mip_filter = 0;
  uint32_t address_u = 0;
  uint32_t address_v = 0;
  uint32_t address_w = 0;
  VkSampler vk_sampler = VK_NULL_HANDLE;
};

struct RenderState {
  bool blend_enable = false;
  uint32_t src_blend = 1;
  uint32_t dst_blend = 0;
  uint32_t blend_op = 1;

  bool z_enable = true;
  bool z_write_enable = true;
  uint32_t z_func = 4; // D3DCMP_LESSEQUAL

  uint32_t cull_mode = 3; // D3DCULL_CCW
  uint32_t color_write_mask = 0x0F; // RGBA

  bool alpha_test_enable = false;
  uint32_t alpha_ref = 0;
  uint32_t alpha_func = 8; // D3DCMP_ALWAYS
};

struct VkDrawIndexedCommand {
  uint32_t primitive_type = 0;
  int32_t base_vertex = 0;
  uint32_t min_index = 0;
  uint32_t num_vertices = 0;
  uint32_t start_index = 0;
  uint32_t primitive_count = 0;
  uint32_t index_count = 0;
  uint32_t instance_count = 1;
  uint32_t first_instance = 0;

  // Offsets e metadados no buffer de geometria do quadro
  uint32_t index_data_offset = 0;
  uint32_t index_data_size = 0;
  uint32_t index_format = 0; // 0 = 16-bit, 1 = 32-bit
  uint32_t vertex_data_offset = 0;
  uint32_t vertex_data_size = 0;
  uint32_t vertex_stride = 32;

  // Snapshot de estado do pipeline para este draw
  uint32_t vs_handle = 0;
  uint32_t ps_handle = 0;
  RenderState state;
};

struct FrameStats {
  std::atomic<uint64_t> total_frames{0};
  std::atomic<uint64_t> draw_indexed_calls{0};
  std::atomic<uint64_t> draw_primitive_calls{0};
  std::atomic<uint64_t> vk_draws_emitted{0};
  std::atomic<uint64_t> vs_constant_updates{0};
  std::atomic<uint64_t> ps_constant_updates{0};
  std::atomic<uint64_t> texture_binds{0};
  std::atomic<uint64_t> render_state_changes{0};
  std::atomic<uint64_t> clears{0};
  std::atomic<uint64_t> resolves{0};
  // Texture Cache & Untiling
  std::atomic<uint64_t> texture_cache_hits{0};
  std::atomic<uint64_t> texture_cache_misses{0};
  std::atomic<uint64_t> textures_untiled{0};
  std::atomic<uint64_t> bytes_untiled{0};

  // Tiling & Resolve
  std::atomic<uint64_t> tiling_begin_calls{0};
  std::atomic<uint64_t> tiling_end_calls{0};

  // State changes
  std::atomic<uint64_t> sampler_state_changes{0};
  std::atomic<uint64_t> stream_source_changes{0};
  std::atomic<uint64_t> indices_changes{0};
  std::atomic<uint64_t> vertex_decl_changes{0};
  std::atomic<uint64_t> blend_state_changes{0};
  std::atomic<uint64_t> depth_stencil_state_changes{0};
  std::atomic<uint64_t> alpha_test_state_changes{0};
  std::atomic<uint64_t> color_write_mask_changes{0};
  std::atomic<uint64_t> resolve_parameter_changes{0};
};

struct CachedTexture {
  uint32_t guest_addr = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t depth = 0;
  uint32_t format = 0;
  bool is_tiled = false;
  uint32_t data_size = 0;
  uint64_t last_frame_used = 0;
  std::vector<uint8_t> data;
  VkImage vk_image = VK_NULL_HANDLE;
  VkDeviceMemory vk_mem = VK_NULL_HANDLE;
  VkImageView vk_view = VK_NULL_HANDLE;
  VkFormat vk_format = VK_FORMAT_UNDEFINED;
};

class PipelineState {
 public:
  static PipelineState& Instance() {
    static PipelineState s_instance;
    return s_instance;
  }

  // Viewport / Scissor
  Viewport viewport;
  ScissorRect scissor;

  // Render Targets & Depth
  uint32_t render_targets[4] = {0, 0, 0, 0};
  uint32_t depth_stencil_surface = 0;

  // Shaders
  uint32_t current_vs_handle = 0;
  uint32_t current_ps_handle = 0;

  // Constants
  std::array<Vector4f, 256> vs_constants;
  std::array<Vector4f, 64> ps_constants;

  // Streams & Indices
  std::array<VertexStream, 16> vertex_streams;
  IndexBufferBinding index_buffer;
  uint32_t vertex_declaration = 0;

  // Textures & Samplers
  std::array<TextureBinding, 16> textures;
  std::array<SamplerBinding, 16> samplers;

  // Tiling state (eDRAM bypass)
  bool tiling_active = false;
  uint32_t active_tiles = 0;

  // Resolve state
  uint32_t last_resolve_src = 0;
  uint32_t last_resolve_dst = 0;

  // Render State
  RenderState state;

  // Clear State
  struct ClearState {
    uint32_t flags = 0;
    uint32_t color = 0xFF000000;
    float z = 1.0f;
    uint32_t stencil = 0;
    bool pending = false;
  } clear_state;

  // Buffer de emissão nativa Plume / Vulkan
  std::vector<VkDrawIndexedCommand> frame_draw_list;

  // Buffers de geometria dinâmica acumulados no quadro
  std::vector<uint8_t> frame_index_data;
  std::vector<uint8_t> frame_vertex_data;

  // Métricas
  FrameStats stats;

  // Controle de modo
  bool native_path_enabled = true;
  bool fallback_to_xenos = false;

  void ResetFrameCounters() {
    stats.draw_indexed_calls = 0;
    stats.draw_primitive_calls = 0;
    stats.vk_draws_emitted = 0;
    stats.vs_constant_updates = 0;
    stats.ps_constant_updates = 0;
    stats.texture_binds = 0;
    stats.render_state_changes = 0;
    stats.clears = 0;
    stats.resolves = 0;
    stats.texture_cache_hits = 0;
    stats.texture_cache_misses = 0;
    stats.textures_untiled = 0;
    stats.bytes_untiled = 0;
    stats.tiling_begin_calls = 0;
    stats.tiling_end_calls = 0;
    stats.sampler_state_changes = 0;
    stats.stream_source_changes = 0;
    stats.indices_changes = 0;
    stats.vertex_decl_changes = 0;
    stats.blend_state_changes = 0;
    stats.depth_stencil_state_changes = 0;
    stats.alpha_test_state_changes = 0;
    stats.color_write_mask_changes = 0;
    stats.resolve_parameter_changes = 0;
    frame_draw_list.clear();
    frame_index_data.clear();
    frame_vertex_data.clear();
  }
};

} // namespace rev2::gpu

