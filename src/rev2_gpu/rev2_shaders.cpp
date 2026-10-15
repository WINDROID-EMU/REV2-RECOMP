#include "rev2_shaders.h"
#include "rev2_dispatch.h"

#include <fstream>
#include <iostream>
#include <cstring>

#include <rex/ui/vulkan/device.h>

namespace rev2::gpu {

bool ShaderManager::LoadPrebakedCache(const std::string& path) {
  if (loaded_) return true;

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    std::fprintf(stderr, "[REV2_SHADERS] Falha ao abrir cache prebaked: %s\n", path.c_str());
    return false;
  }

  uint32_t magic = 0, version = 0, title_id = 0, count = 0;
  file.read(reinterpret_cast<char*>(&magic), 4);
  file.read(reinterpret_cast<char*>(&version), 4);
  file.read(reinterpret_cast<char*>(&title_id), 4);
  file.read(reinterpret_cast<char*>(&count), 4);

  if (magic != 0x53584552) { // "REXS"
    std::fprintf(stderr, "[REV2_SHADERS] Formato de magic incorreto: 0x%08X\n", magic);
    return false;
  }

  shaders_.reserve(count);

  for (uint32_t i = 0; i < count; ++i) {
    PrebakedShader s;
    file.read(reinterpret_cast<char*>(&s.ucode_hash), 8);
    file.read(reinterpret_cast<char*>(&s.shader_type), 4);

    uint32_t tex_count = 0;
    file.read(reinterpret_cast<char*>(&tex_count), 4);
    if (tex_count > 0) {
      s.tex_bindings.resize(tex_count);
      file.read(reinterpret_cast<char*>(s.tex_bindings.data()), tex_count * sizeof(uint32_t));
    }

    uint32_t sam_count = 0;
    file.read(reinterpret_cast<char*>(&sam_count), 4);
    if (sam_count > 0) {
      s.sam_bindings.resize(sam_count);
      file.read(reinterpret_cast<char*>(s.sam_bindings.data()), sam_count * sizeof(uint32_t));
    }

    uint32_t spv_bytes = 0;
    file.read(reinterpret_cast<char*>(&spv_bytes), 4);
    if (spv_bytes > 0) {
      s.spirv.resize(spv_bytes / sizeof(uint32_t));
      file.read(reinterpret_cast<char*>(s.spirv.data()), spv_bytes);
    }

    shaders_[s.ucode_hash] = std::move(s);
  }

  loaded_ = true;
  std::printf("[REV2_SHADERS] Sucesso ao carregar %zu shaders prebaked SPIR-V de %s\n",
              shaders_.size(), path.c_str());
  std::fflush(stdout);
  return true;
}

const PrebakedShader* ShaderManager::FindShader(uint64_t ucode_hash) const {
  auto it = shaders_.find(ucode_hash);
  if (it != shaders_.end()) {
    return &it->second;
  }
  return nullptr;
}

VkShaderModule ShaderManager::GetOrCreateModule(VkDevice device, uint64_t ucode_hash) {
  auto it = shaders_.find(ucode_hash);
  if (it == shaders_.end()) {
    return VK_NULL_HANDLE;
  }

  auto& s = it->second;
  if (s.module != VK_NULL_HANDLE) {
    return s.module;
  }

  if (s.spirv.empty() || device == VK_NULL_HANDLE) {
    return VK_NULL_HANDLE;
  }

  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = s.spirv.size() * sizeof(uint32_t);
  create_info.pCode = s.spirv.data();

  VkShaderModule module = VK_NULL_HANDLE;
  auto& dfn = GetVulkanDispatch();
  if (!dfn.initialized) {
    dfn.Load(device);
  }
  if (dfn.vkCreateShaderModule) {
    if (dfn.vkCreateShaderModule(device, &create_info, nullptr, &module) == VK_SUCCESS) {
      s.module = module;
      return module;
    }
  }

  return VK_NULL_HANDLE;
}

static constexpr uint32_t kFallbackVertexSpv[288] = {
  0x07230203, 0x00010000, 0x0008000B, 0x00000025, 0x00000000, 0x00020011, 0x00000001, 0x0006000B,
  0x00000001, 0x4C534C47, 0x6474732E, 0x3035342E, 0x00000000, 0x0003000E, 0x00000000, 0x00000001,
  0x000B000F, 0x00000000, 0x00000004, 0x6E69616D, 0x00000000, 0x0000000D, 0x00000012, 0x0000001B,
  0x0000001D, 0x00000021, 0x00000023, 0x00030003, 0x00000002, 0x000001C2, 0x00040005, 0x00000004,
  0x6E69616D, 0x00000000, 0x00060005, 0x0000000B, 0x505F6C67, 0x65567265, 0x78657472, 0x00000000,
  0x00060006, 0x0000000B, 0x00000000, 0x505F6C67, 0x7469736F, 0x006E6F69, 0x00070006, 0x0000000B,
  0x00000001, 0x505F6C67, 0x746E696F, 0x657A6953, 0x00000000, 0x00070006, 0x0000000B, 0x00000002,
  0x435F6C67, 0x4470696C, 0x61747369, 0x0065636E, 0x00070006, 0x0000000B, 0x00000003, 0x435F6C67,
  0x446C6C75, 0x61747369, 0x0065636E, 0x00030005, 0x0000000D, 0x00000000, 0x00040005, 0x00000012,
  0x705F6E69, 0x0000736F, 0x00040005, 0x0000001B, 0x5F74756F, 0x006C6F63, 0x00040005, 0x0000001D,
  0x635F6E69, 0x00006C6F, 0x00040005, 0x00000021, 0x5F74756F, 0x00007675, 0x00040005, 0x00000023,
  0x755F6E69, 0x00000076, 0x00030047, 0x0000000B, 0x00000002, 0x00050048, 0x0000000B, 0x00000000,
  0x0000000B, 0x00000000, 0x00050048, 0x0000000B, 0x00000001, 0x0000000B, 0x00000001, 0x00050048,
  0x0000000B, 0x00000002, 0x0000000B, 0x00000003, 0x00050048, 0x0000000B, 0x00000003, 0x0000000B,
  0x00000004, 0x00040047, 0x00000012, 0x0000001E, 0x00000000, 0x00040047, 0x0000001B, 0x0000001E,
  0x00000000, 0x00040047, 0x0000001D, 0x0000001E, 0x00000001, 0x00040047, 0x00000021, 0x0000001E,
  0x00000001, 0x00040047, 0x00000023, 0x0000001E, 0x00000002, 0x00020013, 0x00000002, 0x00030021,
  0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006,
  0x00000004, 0x00040015, 0x00000008, 0x00000020, 0x00000000, 0x0004002B, 0x00000008, 0x00000009,
  0x00000001, 0x0004001C, 0x0000000A, 0x00000006, 0x00000009, 0x0006001E, 0x0000000B, 0x00000007,
  0x00000006, 0x0000000A, 0x0000000A, 0x00040020, 0x0000000C, 0x00000003, 0x0000000B, 0x0004003B,
  0x0000000C, 0x0000000D, 0x00000003, 0x00040015, 0x0000000E, 0x00000020, 0x00000001, 0x0004002B,
  0x0000000E, 0x0000000F, 0x00000000, 0x00040017, 0x00000010, 0x00000006, 0x00000003, 0x00040020,
  0x00000011, 0x00000001, 0x00000010, 0x0004003B, 0x00000011, 0x00000012, 0x00000001, 0x0004002B,
  0x00000006, 0x00000014, 0x3F800000, 0x00040020, 0x00000019, 0x00000003, 0x00000007, 0x0004003B,
  0x00000019, 0x0000001B, 0x00000003, 0x00040020, 0x0000001C, 0x00000001, 0x00000007, 0x0004003B,
  0x0000001C, 0x0000001D, 0x00000001, 0x00040017, 0x0000001F, 0x00000006, 0x00000002, 0x00040020,
  0x00000020, 0x00000003, 0x0000001F, 0x0004003B, 0x00000020, 0x00000021, 0x00000003, 0x00040020,
  0x00000022, 0x00000001, 0x0000001F, 0x0004003B, 0x00000022, 0x00000023, 0x00000001, 0x00050036,
  0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200F8, 0x00000005, 0x0004003D, 0x00000010,
  0x00000013, 0x00000012, 0x00050051, 0x00000006, 0x00000015, 0x00000013, 0x00000000, 0x00050051,
  0x00000006, 0x00000016, 0x00000013, 0x00000001, 0x00050051, 0x00000006, 0x00000017, 0x00000013,
  0x00000002, 0x00070050, 0x00000007, 0x00000018, 0x00000015, 0x00000016, 0x00000017, 0x00000014,
  0x00050041, 0x00000019, 0x0000001A, 0x0000000D, 0x0000000F, 0x0003003E, 0x0000001A, 0x00000018,
  0x0004003D, 0x00000007, 0x0000001E, 0x0000001D, 0x0003003E, 0x0000001B, 0x0000001E, 0x0004003D,
  0x0000001F, 0x00000024, 0x00000023, 0x0003003E, 0x00000021, 0x00000024, 0x000100FD, 0x00010038,
};

static constexpr uint32_t kFallbackPixelSpv[175] = {
  0x07230203, 0x00010000, 0x0008000B, 0x0000001D, 0x00000000, 0x00020011, 0x00000001, 0x0006000B,
  0x00000001, 0x4C534C47, 0x6474732E, 0x3035342E, 0x00000000, 0x0003000E, 0x00000000, 0x00000001,
  0x0008000F, 0x00000004, 0x00000004, 0x6E69616D, 0x00000000, 0x00000009, 0x0000000B, 0x0000001C,
  0x00030010, 0x00000004, 0x00000007, 0x00030003, 0x00000002, 0x000001C2, 0x00040005, 0x00000004,
  0x6E69616D, 0x00000000, 0x00050005, 0x00000009, 0x5F74756F, 0x6F6C6F63, 0x00000072, 0x00040005,
  0x0000000B, 0x635F6E69, 0x00006C6F, 0x00040005, 0x0000001C, 0x755F6E69, 0x00000076, 0x00040047,
  0x00000009, 0x0000001E, 0x00000000, 0x00040047, 0x0000000B, 0x0000001E, 0x00000000, 0x00040047,
  0x0000001C, 0x0000001E, 0x00000001, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002,
  0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020,
  0x00000008, 0x00000003, 0x00000007, 0x0004003B, 0x00000008, 0x00000009, 0x00000003, 0x00040020,
  0x0000000A, 0x00000001, 0x00000007, 0x0004003B, 0x0000000A, 0x0000000B, 0x00000001, 0x00040015,
  0x0000000C, 0x00000020, 0x00000000, 0x0004002B, 0x0000000C, 0x0000000D, 0x00000003, 0x00040020,
  0x0000000E, 0x00000001, 0x00000006, 0x0004002B, 0x00000006, 0x00000011, 0x00000000, 0x00020014,
  0x00000012, 0x0004002B, 0x00000006, 0x00000015, 0x3F800000, 0x0007002C, 0x00000007, 0x00000016,
  0x00000015, 0x00000015, 0x00000015, 0x00000015, 0x00040017, 0x00000017, 0x00000012, 0x00040004,
  0x00040017, 0x0000001A, 0x00000006, 0x00000002, 0x00040020, 0x0000001B, 0x00000001, 0x0000001A,
  0x0004003B, 0x0000001B, 0x0000001C, 0x00000001, 0x00050036, 0x00000002, 0x00000004, 0x00000000,
  0x00000003, 0x000200F8, 0x00000005, 0x00050041, 0x0000000E, 0x0000000F, 0x0000000B, 0x0000000D,
  0x0004003D, 0x00000006, 0x00000010, 0x0000000F, 0x000500BA, 0x00000012, 0x00000013, 0x00000010,
  0x00000011, 0x0004003D, 0x00000007, 0x00000014, 0x0000000B, 0x00070050, 0x00000017, 0x00000018,
  0x00000013, 0x00000013, 0x00000013, 0x00000013, 0x000600A9, 0x00000007, 0x00000019, 0x00000018,
  0x00000014, 0x00000016, 0x0003003E, 0x00000009, 0x00000019, 0x000100FD, 0x00010038,
};

VkShaderModule ShaderManager::GetFallbackVertexModule(VkDevice device) {
  if (fallback_vs_ != VK_NULL_HANDLE) return fallback_vs_;
  if (device == VK_NULL_HANDLE) return VK_NULL_HANDLE;

  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = sizeof(kFallbackVertexSpv);
  create_info.pCode = kFallbackVertexSpv;

  auto& dfn = GetVulkanDispatch();
  if (!dfn.initialized) {
    dfn.Load(device);
  }
  if (dfn.vkCreateShaderModule && dfn.vkCreateShaderModule(device, &create_info, nullptr, &fallback_vs_) == VK_SUCCESS) {
    std::fprintf(stderr, "[REV2_SHADERS] Fallback Vertex Shader criado com sucesso! module=%p\n", (void*)fallback_vs_);
    return fallback_vs_;
  }
  std::fprintf(stderr, "[REV2_SHADERS] Falha ao criar Fallback Vertex Shader!\n");
  return VK_NULL_HANDLE;
}

VkShaderModule ShaderManager::GetFallbackPixelModule(VkDevice device) {
  if (fallback_ps_ != VK_NULL_HANDLE) return fallback_ps_;
  if (device == VK_NULL_HANDLE) return VK_NULL_HANDLE;

  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = sizeof(kFallbackPixelSpv);
  create_info.pCode = kFallbackPixelSpv;

  auto& dfn = GetVulkanDispatch();
  if (!dfn.initialized) {
    dfn.Load(device);
  }
  if (dfn.vkCreateShaderModule && dfn.vkCreateShaderModule(device, &create_info, nullptr, &fallback_ps_) == VK_SUCCESS) {
    std::fprintf(stderr, "[REV2_SHADERS] Fallback Pixel Shader criado com sucesso! module=%p\n", (void*)fallback_ps_);
    return fallback_ps_;
  }
  std::fprintf(stderr, "[REV2_SHADERS] Falha ao criar Fallback Pixel Shader!\n");
  return VK_NULL_HANDLE;
}

VkShaderModule ShaderManager::GetDefaultPrebakedVertexModule(VkDevice device) {
  for (auto& [hash, s] : shaders_) {
    if (s.shader_type == 0 && !s.spirv.empty()) {
      VkShaderModule mod = GetOrCreateModule(device, hash);
      if (mod != VK_NULL_HANDLE) return mod;
    }
  }
  return GetFallbackVertexModule(device);
}

VkShaderModule ShaderManager::GetDefaultPrebakedPixelModule(VkDevice device) {
  for (auto& [hash, s] : shaders_) {
    if (s.shader_type == 1 && !s.spirv.empty()) {
      VkShaderModule mod = GetOrCreateModule(device, hash);
      if (mod != VK_NULL_HANDLE) return mod;
    }
  }
  return GetFallbackPixelModule(device);
}

} // namespace rev2::gpu
