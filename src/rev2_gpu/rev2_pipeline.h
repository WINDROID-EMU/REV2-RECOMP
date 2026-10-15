#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>

#include <rex/ui/vulkan/api.h>
#include <rex/ui/vulkan/device.h>
#include "rev2_state.h"

namespace rev2::gpu {

struct PipelineKey {
  uint64_t vs_hash = 0;
  uint64_t ps_hash = 0;
  uint32_t primitive_type = 4; // D3DPT_TRIANGLELIST default
  bool z_enable = false;
  bool z_write_enable = false;
  uint32_t z_func = 4; // D3DCMP_LESSEQUAL
  bool blend_enable = false;
  uint32_t src_blend = 5; // D3DBLEND_SRCALPHA
  uint32_t dst_blend = 6; // D3DBLEND_INVSRCALPHA
  uint32_t cull_mode = 1; // D3DCULL_NONE
  uint32_t color_write_mask = 0xF; // D3DCOLORWRITEENABLE_ALL

  bool operator==(const PipelineKey& other) const {
    return vs_hash == other.vs_hash &&
           ps_hash == other.ps_hash &&
           primitive_type == other.primitive_type &&
           z_enable == other.z_enable &&
           z_write_enable == other.z_write_enable &&
           z_func == other.z_func &&
           blend_enable == other.blend_enable &&
           src_blend == other.src_blend &&
           dst_blend == other.dst_blend &&
           cull_mode == other.cull_mode &&
           color_write_mask == other.color_write_mask;
  }
};

struct PipelineKeyHasher {
  std::size_t operator()(const PipelineKey& k) const noexcept {
    std::size_t h = std::hash<uint64_t>{}(k.vs_hash) ^ (std::hash<uint64_t>{}(k.ps_hash) << 1);
    h ^= std::hash<uint32_t>{}(k.primitive_type) << 2;
    h ^= (k.z_enable ? 1 : 0) << 3;
    h ^= (k.z_write_enable ? 1 : 0) << 4;
    h ^= std::hash<uint32_t>{}(k.z_func) << 5;
    h ^= (k.blend_enable ? 1 : 0) << 6;
    h ^= std::hash<uint32_t>{}(k.src_blend) << 7;
    h ^= std::hash<uint32_t>{}(k.dst_blend) << 8;
    h ^= std::hash<uint32_t>{}(k.cull_mode) << 9;
    h ^= std::hash<uint32_t>{}(k.color_write_mask) << 10;
    return h;
  }
};

class PipelineManager {
 public:
  static PipelineManager& Instance() {
    static PipelineManager s_instance;
    return s_instance;
  }

  bool Initialize(rex::ui::vulkan::VulkanDevice* device);
  void Shutdown();

  VkRenderPass GetOrCreateRenderPass(VkFormat color_format, VkFormat depth_format = VK_FORMAT_UNDEFINED);
  VkPipeline GetOrCreatePipeline(const PipelineKey& key, VkRenderPass render_pass);
  VkPipelineLayout GetPipelineLayout() const { return pipeline_layout_; }

  VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t set) const {
    return (set < descriptor_set_layouts_.size()) ? descriptor_set_layouts_[set] : VK_NULL_HANDLE;
  }
  VkDescriptorSet GetDescriptorSet(uint32_t set) const {
    return (set < descriptor_sets_.size()) ? descriptor_sets_[set] : VK_NULL_HANDLE;
  }
  VkDescriptorPool GetDescriptorPool() const { return descriptor_pool_; }

  size_t PipelineCount() const { return pipelines_.size(); }

 private:
  PipelineManager() = default;

  rex::ui::vulkan::VulkanDevice* device_ = nullptr;
  std::array<VkDescriptorSetLayout, 4> descriptor_set_layouts_{};
  VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
  std::array<VkDescriptorSet, 4> descriptor_sets_{};
  VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
  VkRenderPass render_pass_ = VK_NULL_HANDLE;
  VkFormat render_pass_format_ = VK_FORMAT_UNDEFINED;
  VkFormat render_pass_depth_format_ = VK_FORMAT_UNDEFINED;

  std::unordered_map<PipelineKey, VkPipeline, PipelineKeyHasher> pipelines_;
};

} // namespace rev2::gpu
