#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>

#include <rex/ui/vulkan/api.h>

namespace rev2::gpu {

struct PrebakedShader {
  uint64_t ucode_hash = 0;
  uint32_t shader_type = 0; // 0 = Vertex, 1 = Pixel
  std::vector<uint32_t> tex_bindings;
  std::vector<uint32_t> sam_bindings;
  std::vector<uint32_t> spirv; // SPIR-V bytecode in 32-bit words
  VkShaderModule module = VK_NULL_HANDLE;
};

class ShaderManager {
 public:
  static ShaderManager& Instance() {
    static ShaderManager s_instance;
    return s_instance;
  }

  bool LoadPrebakedCache(const std::string& path);
  const PrebakedShader* FindShader(uint64_t ucode_hash) const;
  VkShaderModule GetOrCreateModule(VkDevice device, uint64_t ucode_hash);
  VkShaderModule GetFallbackVertexModule(VkDevice device);
  VkShaderModule GetFallbackPixelModule(VkDevice device);

  size_t ShaderCount() const { return shaders_.size(); }
  bool IsLoaded() const { return loaded_; }

 private:
  ShaderManager() = default;
  std::unordered_map<uint64_t, PrebakedShader> shaders_;
  bool loaded_ = false;
  VkShaderModule fallback_vs_ = VK_NULL_HANDLE;
  VkShaderModule fallback_ps_ = VK_NULL_HANDLE;
};

} // namespace rev2::gpu
