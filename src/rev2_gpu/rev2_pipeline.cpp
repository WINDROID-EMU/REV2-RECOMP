#include "rev2_pipeline.h"
#include "rev2_shaders.h"
#include "rev2_dispatch.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace rev2::gpu {

static VkBlendFactor MapD3DBlendFactor(uint32_t d3d_factor) {
  switch (d3d_factor) {
    case 1:  return VK_BLEND_FACTOR_ZERO;
    case 2:  return VK_BLEND_FACTOR_ONE;
    case 3:  return VK_BLEND_FACTOR_SRC_COLOR;
    case 4:  return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case 5:  return VK_BLEND_FACTOR_SRC_ALPHA;
    case 6:  return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case 7:  return VK_BLEND_FACTOR_DST_ALPHA;
    case 8:  return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case 9:  return VK_BLEND_FACTOR_DST_COLOR;
    case 10: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    default: return VK_BLEND_FACTOR_ONE;
  }
}

static VkCompareOp MapD3DCompareOp(uint32_t d3d_cmp) {
  switch (d3d_cmp) {
    case 1: return VK_COMPARE_OP_NEVER;
    case 2: return VK_COMPARE_OP_LESS;
    case 3: return VK_COMPARE_OP_EQUAL;
    case 4: return VK_COMPARE_OP_LESS_OR_EQUAL;
    case 5: return VK_COMPARE_OP_GREATER;
    case 6: return VK_COMPARE_OP_NOT_EQUAL;
    case 7: return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case 8: return VK_COMPARE_OP_ALWAYS;
    default: return VK_COMPARE_OP_LESS_OR_EQUAL;
  }
}

static VkPrimitiveTopology MapD3DTopology(uint32_t d3d_prim) {
  switch (d3d_prim) {
    case 1: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case 2: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case 3: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case 4: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case 5: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case 6: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    default: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  }
}

bool PipelineManager::Initialize(rex::ui::vulkan::VulkanDevice* device) {
  if (!device) return false;
  device_ = device;
  VkDevice dev = device_->device();
  auto& dfn = GetVulkanDispatch();
  dfn.Load(dev);

  // 1. Cria Descriptor Set Layouts (Sets 0, 1, 2, 3)
  // Set 0: Shared Memory Buffer (SSBO para vfetch)
  VkDescriptorSetLayoutBinding set0_binding{};
  set0_binding.binding = 0;
  set0_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  set0_binding.descriptorCount = 1;
  set0_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo set0_ci{};
  set0_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  set0_ci.bindingCount = 1;
  set0_ci.pBindings = &set0_binding;
  if (dfn.vkCreateDescriptorSetLayout(dev, &set0_ci, nullptr, &descriptor_set_layouts_[0]) != VK_SUCCESS) {
    std::fprintf(stderr, "[REV2_PIPELINE] Falha ao criar VkDescriptorSetLayout Set 0\n");
    return false;
  }

  // Set 1: Uniform Buffers (System constants, VS float, PS float, bool/loop, fetch)
  std::vector<VkDescriptorSetLayoutBinding> set1_bindings(5);
  // Binding 0: system constants
  set1_bindings[0].binding = 0;
  set1_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  set1_bindings[0].descriptorCount = 1;
  set1_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  // Binding 1: VS float constants (Dynamic UBO per-draw)
  set1_bindings[1].binding = 1;
  set1_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  set1_bindings[1].descriptorCount = 1;
  set1_bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  // Binding 2: PS float constants (Dynamic UBO per-draw)
  set1_bindings[2].binding = 2;
  set1_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  set1_bindings[2].descriptorCount = 1;
  set1_bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  // Binding 3: bool/loop constants
  set1_bindings[3].binding = 3;
  set1_bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  set1_bindings[3].descriptorCount = 1;
  set1_bindings[3].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  // Binding 4: fetch constants
  set1_bindings[4].binding = 4;
  set1_bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  set1_bindings[4].descriptorCount = 1;
  set1_bindings[4].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo set1_ci{};
  set1_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  set1_ci.bindingCount = static_cast<uint32_t>(set1_bindings.size());
  set1_ci.pBindings = set1_bindings.data();
  if (dfn.vkCreateDescriptorSetLayout(dev, &set1_ci, nullptr, &descriptor_set_layouts_[1]) != VK_SUCCESS) {
    std::fprintf(stderr, "[REV2_PIPELINE] Falha ao criar VkDescriptorSetLayout Set 1\n");
    return false;
  }

  // Set 2: Layout vazio
  VkDescriptorSetLayoutCreateInfo set2_ci{};
  set2_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  set2_ci.bindingCount = 0;
  set2_ci.pBindings = nullptr;
  dfn.vkCreateDescriptorSetLayout(dev, &set2_ci, nullptr, &descriptor_set_layouts_[2]);

  // Set 3: Texturas (0..15) e Samplers (16..31)
  std::vector<VkDescriptorSetLayoutBinding> set3_bindings(32);
  for (uint32_t i = 0; i < 16; ++i) {
    set3_bindings[i].binding = i;
    set3_bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    set3_bindings[i].descriptorCount = 1;
    set3_bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  }
  for (uint32_t i = 0; i < 16; ++i) {
    set3_bindings[16 + i].binding = 16 + i;
    set3_bindings[16 + i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    set3_bindings[16 + i].descriptorCount = 1;
    set3_bindings[16 + i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  }

  VkDescriptorSetLayoutCreateInfo set3_ci{};
  set3_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  set3_ci.bindingCount = static_cast<uint32_t>(set3_bindings.size());
  set3_ci.pBindings = set3_bindings.data();
  dfn.vkCreateDescriptorSetLayout(dev, &set3_ci, nullptr, &descriptor_set_layouts_[3]);

  // 2. Cria Pipeline Layout com todos os 4 descriptor set layouts
  VkPipelineLayoutCreateInfo pipeline_layout_ci{};
  pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_ci.setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts_.size());
  pipeline_layout_ci.pSetLayouts = descriptor_set_layouts_.data();

  if (dfn.vkCreatePipelineLayout(dev, &pipeline_layout_ci, nullptr, &pipeline_layout_) != VK_SUCCESS) {
    std::fprintf(stderr, "[REV2_PIPELINE] Falha ao criar VkPipelineLayout\n");
    return false;
  }

  // 3. Cria Descriptor Pool
  std::array<VkDescriptorPoolSize, 5> pool_sizes{};
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  pool_sizes[0].descriptorCount = 32;
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_sizes[1].descriptorCount = 128;
  pool_sizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  pool_sizes[2].descriptorCount = 128;
  pool_sizes[3].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  pool_sizes[3].descriptorCount = 128;
  pool_sizes[4].type = VK_DESCRIPTOR_TYPE_SAMPLER;
  pool_sizes[4].descriptorCount = 128;

  VkDescriptorPoolCreateInfo pool_ci{};
  pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_ci.maxSets = 64;
  pool_ci.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
  pool_ci.pPoolSizes = pool_sizes.data();
  pool_ci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

  VkResult pool_res = dfn.vkCreateDescriptorPool(dev, &pool_ci, nullptr, &descriptor_pool_);
  std::fprintf(stderr, "[REV2_PIPELINE] vkCreateDescriptorPool res=%d, pool=%p\n", pool_res, (void*)descriptor_pool_);
  if (pool_res == VK_SUCCESS) {
    for (size_t s = 0; s < descriptor_set_layouts_.size(); ++s) {
      if (descriptor_set_layouts_[s] != VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = descriptor_pool_;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &descriptor_set_layouts_[s];
        VkResult alloc_res = dfn.vkAllocateDescriptorSets(dev, &alloc_info, &descriptor_sets_[s]);
        std::fprintf(stderr, "[REV2_PIPELINE] Set %zu: layout=%p, alloc_res=%d, set=%p\n",
                     s, (void*)descriptor_set_layouts_[s], alloc_res, (void*)descriptor_sets_[s]);
      }
    }
  }
  std::fflush(stderr);

  std::printf("[REV2_PIPELINE] Gerenciador de Pipelines Vulkan inicializado com sucesso (4 Descriptor Sets).\n");
  std::fflush(stdout);
  return true;
}

void PipelineManager::Shutdown() {
  if (!device_) return;
  auto& dfn = GetVulkanDispatch();
  VkDevice dev = device_->device();

  for (auto& [key, pipe] : pipelines_) {
    if (pipe != VK_NULL_HANDLE) {
      dfn.vkDestroyPipeline(dev, pipe, nullptr);
    }
  }
  pipelines_.clear();

  if (render_pass_ != VK_NULL_HANDLE) {
    dfn.vkDestroyRenderPass(dev, render_pass_, nullptr);
    render_pass_ = VK_NULL_HANDLE;
  }

  if (pipeline_layout_ != VK_NULL_HANDLE) {
    dfn.vkDestroyPipelineLayout(dev, pipeline_layout_, nullptr);
    pipeline_layout_ = VK_NULL_HANDLE;
  }

  if (descriptor_pool_ != VK_NULL_HANDLE) {
    dfn.vkDestroyDescriptorPool(dev, descriptor_pool_, nullptr);
    descriptor_pool_ = VK_NULL_HANDLE;
  }

  for (auto& layout : descriptor_set_layouts_) {
    if (layout != VK_NULL_HANDLE) {
      dfn.vkDestroyDescriptorSetLayout(dev, layout, nullptr);
      layout = VK_NULL_HANDLE;
    }
  }

  device_ = nullptr;
}

VkRenderPass PipelineManager::GetOrCreateRenderPass(VkFormat color_format, VkFormat depth_format) {
  if (render_pass_ != VK_NULL_HANDLE &&
      render_pass_format_ == color_format &&
      render_pass_depth_format_ == depth_format) {
    return render_pass_;
  }
  if (!device_) return VK_NULL_HANDLE;
  auto& dfn = GetVulkanDispatch();
  VkDevice dev = device_->device();

  if (render_pass_ != VK_NULL_HANDLE) {
    dfn.vkDestroyRenderPass(dev, render_pass_, nullptr);
    render_pass_ = VK_NULL_HANDLE;
  }

  std::vector<VkAttachmentDescription> attachments;

  VkAttachmentDescription color_attachment{};
  color_attachment.format = color_format;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  attachments.push_back(color_attachment);

  VkAttachmentReference color_ref{};
  color_ref.attachment = 0;
  color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_ref{};
  bool has_depth = (depth_format != VK_FORMAT_UNDEFINED);
  if (has_depth) {
    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = depth_format;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments.push_back(depth_attachment);

    depth_ref.attachment = 1;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  }

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref;
  if (has_depth) {
    subpass.pDepthStencilAttachment = &depth_ref;
  }

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  if (has_depth) {
    dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  }

  VkRenderPassCreateInfo rp_ci{};
  rp_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rp_ci.attachmentCount = static_cast<uint32_t>(attachments.size());
  rp_ci.pAttachments = attachments.data();
  rp_ci.subpassCount = 1;
  rp_ci.pSubpasses = &subpass;
  rp_ci.dependencyCount = 1;
  rp_ci.pDependencies = &dependency;

  if (dfn.vkCreateRenderPass(dev, &rp_ci, nullptr, &render_pass_) == VK_SUCCESS) {
    render_pass_format_ = color_format;
    render_pass_depth_format_ = depth_format;
    return render_pass_;
  }

  return VK_NULL_HANDLE;
}

VkPipeline PipelineManager::GetOrCreatePipeline(const PipelineKey& key, VkRenderPass render_pass) {
  auto it = pipelines_.find(key);
  if (it != pipelines_.end()) {
    return it->second;
  }
  if (!device_ || render_pass == VK_NULL_HANDLE) {
    return VK_NULL_HANDLE;
  }

  auto& dfn = GetVulkanDispatch();
  VkDevice dev = device_->device();

  // Obtém os Shader Modules SPIR-V prebaked ou recorre ao shader de fallback nativo
  VkShaderModule vs_mod = ShaderManager::Instance().GetOrCreateModule(dev, key.vs_hash);
  VkShaderModule ps_mod = ShaderManager::Instance().GetOrCreateModule(dev, key.ps_hash);

  if (vs_mod == VK_NULL_HANDLE) {
    vs_mod = ShaderManager::Instance().GetFallbackVertexModule(dev);
  }
  if (ps_mod == VK_NULL_HANDLE) {
    ps_mod = ShaderManager::Instance().GetFallbackPixelModule(dev);
  }

  if (vs_mod == VK_NULL_HANDLE || ps_mod == VK_NULL_HANDLE) {
    return VK_NULL_HANDLE;
  }

  // 1. Shader Stages
  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vs_mod;
  stages[0].pName = "main";

  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = ps_mod;
  stages[1].pName = "main";

  // 2. Vertex Input State
  VkVertexInputBindingDescription binding_desc{};
  binding_desc.binding = 0;
  binding_desc.stride = 32; // Default vertex stride para geometria MT Framework
  binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription attr_descs[3]{};
  // Pos: float3
  attr_descs[0].location = 0;
  attr_descs[0].binding = 0;
  attr_descs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attr_descs[0].offset = 0;
  // Normal/Color: uint32 ou float4
  attr_descs[1].location = 1;
  attr_descs[1].binding = 0;
  attr_descs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
  attr_descs[1].offset = 12;
  // TexCoord: float2
  attr_descs[2].location = 2;
  attr_descs[2].binding = 0;
  attr_descs[2].format = VK_FORMAT_R32G32_SFLOAT;
  attr_descs[2].offset = 24;

  VkPipelineVertexInputStateCreateInfo vtx_ci{};
  vtx_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vtx_ci.vertexBindingDescriptionCount = 1;
  vtx_ci.pVertexBindingDescriptions = &binding_desc;
  vtx_ci.vertexAttributeDescriptionCount = 3;
  vtx_ci.pVertexAttributeDescriptions = attr_descs;

  // 3. Input Assembly State
  VkPipelineInputAssemblyStateCreateInfo ia_ci{};
  ia_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  ia_ci.topology = MapD3DTopology(key.primitive_type);
  ia_ci.primitiveRestartEnable = VK_FALSE;

  // 4. Viewport & Scissor (Dynamic)
  VkPipelineViewportStateCreateInfo vp_ci{};
  vp_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  vp_ci.viewportCount = 1;
  vp_ci.scissorCount = 1;

  VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dyn_ci{};
  dyn_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dyn_ci.dynamicStateCount = 2;
  dyn_ci.pDynamicStates = dynamic_states;

  // 5. Rasterization State
  VkPipelineRasterizationStateCreateInfo rast_ci{};
  rast_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rast_ci.depthClampEnable = VK_FALSE;
  rast_ci.rasterizerDiscardEnable = VK_FALSE;
  rast_ci.polygonMode = VK_POLYGON_MODE_FILL;
  rast_ci.lineWidth = 1.0f;
  rast_ci.cullMode = (key.cull_mode == 1) ? VK_CULL_MODE_NONE :
                     ((key.cull_mode == 2) ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT);
  rast_ci.frontFace = VK_FRONT_FACE_CLOCKWISE;

  // 6. Multisample State
  VkPipelineMultisampleStateCreateInfo ms_ci{};
  ms_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  ms_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // 7. Depth Stencil State
  VkPipelineDepthStencilStateCreateInfo ds_ci{};
  ds_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  ds_ci.depthTestEnable = key.z_enable ? VK_TRUE : VK_FALSE;
  ds_ci.depthWriteEnable = key.z_write_enable ? VK_TRUE : VK_FALSE;
  ds_ci.depthCompareOp = MapD3DCompareOp(key.z_func);

  // 8. Color Blend State
  VkPipelineColorBlendAttachmentState blend_att{};
  blend_att.blendEnable = key.blend_enable ? VK_TRUE : VK_FALSE;
  blend_att.srcColorBlendFactor = MapD3DBlendFactor(key.src_blend);
  blend_att.dstColorBlendFactor = MapD3DBlendFactor(key.dst_blend);
  blend_att.colorBlendOp = VK_BLEND_OP_ADD;
  blend_att.srcAlphaBlendFactor = MapD3DBlendFactor(key.src_blend);
  blend_att.dstAlphaBlendFactor = MapD3DBlendFactor(key.dst_blend);
  blend_att.alphaBlendOp = VK_BLEND_OP_ADD;
  blend_att.colorWriteMask = key.color_write_mask & 0xF;

  VkPipelineColorBlendStateCreateInfo cb_ci{};
  cb_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  cb_ci.attachmentCount = 1;
  cb_ci.pAttachments = &blend_att;

  // 9. Criação do Graphics Pipeline
  VkGraphicsPipelineCreateInfo pipe_ci{};
  pipe_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipe_ci.stageCount = 2;
  pipe_ci.pStages = stages;
  pipe_ci.pVertexInputState = &vtx_ci;
  pipe_ci.pInputAssemblyState = &ia_ci;
  pipe_ci.pViewportState = &vp_ci;
  pipe_ci.pRasterizationState = &rast_ci;
  pipe_ci.pMultisampleState = &ms_ci;
  pipe_ci.pDepthStencilState = &ds_ci;
  pipe_ci.pColorBlendState = &cb_ci;
  pipe_ci.pDynamicState = &dyn_ci;
  pipe_ci.layout = pipeline_layout_;
  pipe_ci.renderPass = render_pass;
  pipe_ci.subpass = 0;

  VkPipeline pipeline = VK_NULL_HANDLE;
  VkResult res = dfn.vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipe_ci, nullptr, &pipeline);
  if (res == VK_SUCCESS) {
    pipelines_[key] = pipeline;
    std::printf("[REV2_PIPELINE] Criado novo VkPipeline (VS: 0x%016lX, PS: 0x%016lX) -> %p\n",
                (unsigned long)key.vs_hash, (unsigned long)key.ps_hash, (void*)pipeline);
    std::fflush(stdout);
    return pipeline;
  } else {
    std::fprintf(stderr, "[REV2_PIPELINE] vkCreateGraphicsPipelines falhou: res=%d (VS: 0x%016lX, PS: 0x%016lX, vs_mod=%p, ps_mod=%p)\n",
                 res, (unsigned long)key.vs_hash, (unsigned long)key.ps_hash, (void*)vs_mod, (void*)ps_mod);
  }

  return VK_NULL_HANDLE;
}

} // namespace rev2::gpu
