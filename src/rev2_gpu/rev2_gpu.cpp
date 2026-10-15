#include "rev2_gpu.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <vector>

#include <rex/graphics/pipeline/texture/info.h>
#include <rex/graphics/pipeline/texture/util.h>
#include <rex/runtime.h>
#include <rex/graphics/graphics_system.h>
#include <rex/graphics/command_processor.h>
#include <rex/ui/vulkan/api.h>
#include <rex/ui/presenter.h>
#include <rex/ui/vulkan/presenter.h>
#include <rex/ui/vulkan/provider.h>
#include <rex/ui/vulkan/device.h>
#include "rev2_shaders.h"
#include "rev2_pipeline.h"
#include "rev2_dispatch.h"
#include <filesystem>

namespace rev2::gpu {

static bool s_initialized = false;
static std::unordered_map<uint32_t, CachedTexture> s_texture_cache;

static void UntileTexture2D(
    const uint8_t* src_tiled,
    uint8_t* dst_linear,
    uint32_t width,
    uint32_t height,
    uint32_t format
) {
  const rex::graphics::FormatInfo* fmt_info = rex::graphics::FormatInfo::Get(format);
  if (!fmt_info || !src_tiled || !dst_linear) return;

  uint32_t block_w = fmt_info->block_width ? fmt_info->block_width : 1;
  uint32_t block_h = fmt_info->block_height ? fmt_info->block_height : 1;
  uint32_t block_bytes = fmt_info->bytes_per_block();
  uint32_t bpb_log2 = rex::log2_floor(block_bytes);

  uint32_t blocks_x = (width + block_w - 1) / block_w;
  uint32_t blocks_y = (height + block_h - 1) / block_h;
  uint32_t pitch_blocks = (blocks_x + 31) & ~31;

  for (uint32_t by = 0; by < blocks_y; ++by) {
    for (uint32_t bx = 0; bx < blocks_x; ++bx) {
      int32_t tiled_offset = rex::graphics::texture_util::GetTiledOffset2D(
          bx, by, pitch_blocks, bpb_log2);
      
      const uint8_t* src = src_tiled + tiled_offset;
      uint8_t* dst = dst_linear + (by * blocks_x + bx) * block_bytes;

      std::memcpy(dst, src, block_bytes);
    }
  }
}

static VkCommandPool s_clear_cmd_pool = VK_NULL_HANDLE;
static VkCommandBuffer s_clear_cmd_buf = VK_NULL_HANDLE;
static VkFence s_clear_fence = VK_NULL_HANDLE;

static VkBuffer s_vk_index_buf = VK_NULL_HANDLE;
static VkDeviceMemory s_vk_index_mem = VK_NULL_HANDLE;
static size_t s_vk_index_cap = 0;

static VkBuffer s_vk_vertex_buf = VK_NULL_HANDLE;
static VkDeviceMemory s_vk_vertex_mem = VK_NULL_HANDLE;
static size_t s_vk_vertex_cap = 0;

static VkBuffer s_vk_uniform_buf = VK_NULL_HANDLE;
static VkDeviceMemory s_vk_uniform_mem = VK_NULL_HANDLE;
static size_t s_vk_uniform_cap = 0;

static uint32_t FindMemoryType(rex::ui::vulkan::VulkanDevice* vk_dev, uint32_t type_filter, VkMemoryPropertyFlags properties) {
  if (!vk_dev) return 0;
  auto& dfn = GetVulkanDispatch();

  VkPhysicalDevice phys = *reinterpret_cast<VkPhysicalDevice*>(reinterpret_cast<uint8_t*>(vk_dev) + 8);
  std::fprintf(stderr, "[REV2_GPU] FindMemoryType: vk_dev=%p, phys=%p, filter=0x%x, props=0x%x\n",
               (void*)vk_dev, (void*)phys, type_filter, properties);

  if (dfn.vkGetPhysicalDeviceMemoryProperties && phys) {
    VkPhysicalDeviceMemoryProperties mem_props{};
    dfn.vkGetPhysicalDeviceMemoryProperties(phys, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
      if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
        std::fprintf(stderr, "[REV2_GPU] FindMemoryType -> selected type %u\n", i);
        return i;
      }
    }
    // Fallback: any memory type that matches the filter
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
      if (type_filter & (1 << i)) {
        std::fprintf(stderr, "[REV2_GPU] FindMemoryType fallback -> selected type %u\n", i);
        return i;
      }
    }
  }
  std::fprintf(stderr, "[REV2_GPU] FindMemoryType failed, defaulting to 0\n");
  return 0;
}

static bool EnsureBuffer(rex::ui::vulkan::VulkanDevice* vk_dev,
                         VkBuffer& buffer,
                         VkDeviceMemory& memory,
                         size_t& current_capacity,
                         size_t required_size,
                         VkBufferUsageFlags usage) {
  std::fprintf(stderr, "[REV2_GPU] EnsureBuffer entry: req_size=%zu, cur_cap=%zu, buf=%p\n",
               required_size, current_capacity, (void*)buffer);
  if (buffer != VK_NULL_HANDLE && current_capacity >= required_size) {
    return true;
  }
  VkDevice dev = vk_dev->device();
  auto& dfn = GetVulkanDispatch();
  dfn.Load(dev);
  std::fprintf(stderr, "[REV2_GPU] EnsureBuffer: dev=%p\n", (void*)dev);

  if (buffer != VK_NULL_HANDLE) {
    dfn.vkDestroyBuffer(dev, buffer, nullptr);
    buffer = VK_NULL_HANDLE;
  }
  if (memory != VK_NULL_HANDLE) {
    dfn.vkFreeMemory(dev, memory, nullptr);
    memory = VK_NULL_HANDLE;
  }

  size_t alloc_size = std::max(required_size, current_capacity * 2);
  alloc_size = std::max(alloc_size, size_t(64 * 1024));

  VkBufferCreateInfo buf_ci{};
  buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buf_ci.size = alloc_size;
  buf_ci.usage = usage;
  buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  std::fprintf(stderr, "[REV2_GPU] EnsureBuffer calling vkCreateBuffer size=%zu...\n", alloc_size);
  VkResult res = dfn.vkCreateBuffer(dev, &buf_ci, nullptr, &buffer);
  std::fprintf(stderr, "[REV2_GPU] EnsureBuffer vkCreateBuffer res=%d, buf=%p\n", res, (void*)buffer);
  if (res != VK_SUCCESS) {
    return false;
  }

  VkMemoryRequirements mem_reqs{};
  dfn.vkGetBufferMemoryRequirements(dev, buffer, &mem_reqs);
  std::fprintf(stderr, "[REV2_GPU] EnsureBuffer mem_reqs: size=%llu, typeBits=0x%x\n",
               (unsigned long long)mem_reqs.size, mem_reqs.memoryTypeBits);

  uint32_t mem_type_idx = FindMemoryType(vk_dev, mem_reqs.memoryTypeBits,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = mem_reqs.size;
  alloc_info.memoryTypeIndex = mem_type_idx;

  std::fprintf(stderr, "[REV2_GPU] EnsureBuffer calling vkAllocateMemory typeIdx=%u...\n", mem_type_idx);
  res = dfn.vkAllocateMemory(dev, &alloc_info, nullptr, &memory);
  std::fprintf(stderr, "[REV2_GPU] EnsureBuffer vkAllocateMemory res=%d, mem=%p\n", res, (void*)memory);
  if (res != VK_SUCCESS) {
    dfn.vkDestroyBuffer(dev, buffer, nullptr);
    buffer = VK_NULL_HANDLE;
    return false;
  }

  res = dfn.vkBindBufferMemory(dev, buffer, memory, 0);
  std::fprintf(stderr, "[REV2_GPU] EnsureBuffer vkBindBufferMemory res=%d\n", res);
  if (res != VK_SUCCESS) {
    dfn.vkDestroyBuffer(dev, buffer, nullptr);
    dfn.vkFreeMemory(dev, memory, nullptr);
    buffer = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
    return false;
  }

  current_capacity = alloc_size;
  std::fprintf(stderr, "[REV2_GPU] EnsureBuffer success!\n");
  return true;
}

static bool UploadBufferData(rex::ui::vulkan::VulkanDevice* vk_dev,
                            VkDeviceMemory memory,
                            const void* data,
                            size_t size) {
  if (!memory || !data || size == 0) return false;
  auto& dfn = GetVulkanDispatch();
  void* mapped = nullptr;
  if (dfn.vkMapMemory(vk_dev->device(), memory, 0, size, 0, &mapped) == VK_SUCCESS) {
    std::memcpy(mapped, data, size);
    dfn.vkUnmapMemory(vk_dev->device(), memory);
    return true;
  }
  return false;
}

static bool EnsureVulkanClearContext(rex::ui::vulkan::VulkanDevice* vk_dev) {
  if (!vk_dev) {
    std::fprintf(stderr, "[REV2_GPU] EnsureVulkanClearContext: vk_dev is null!\n");
    return false;
  }
  VkDevice dev = vk_dev->device();
  auto& dfn = GetVulkanDispatch();
  dfn.Load(dev);

  static bool s_pipe_init = false;
  if (!s_pipe_init) {
    s_pipe_init = true;
    PipelineManager::Instance().Initialize(vk_dev);
  }
  if (s_clear_cmd_buf != VK_NULL_HANDLE) return true;

  uint32_t qf = vk_dev->queue_family_graphics_compute();
  std::fprintf(stderr, "[REV2_GPU] EnsureVulkanClearContext: dev=%p, qf=%u\n", (void*)dev, qf);

  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.queueFamilyIndex = qf;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  VkResult res = dfn.vkCreateCommandPool(dev, &pool_info, nullptr, &s_clear_cmd_pool);
  std::fprintf(stderr, "[REV2_GPU] vkCreateCommandPool res=%d, pool=%p\n", res, (void*)s_clear_cmd_pool);
  if (res != VK_SUCCESS) {
    return false;
  }

  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = s_clear_cmd_pool;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = 1;

  res = dfn.vkAllocateCommandBuffers(dev, &alloc_info, &s_clear_cmd_buf);
  std::fprintf(stderr, "[REV2_GPU] vkAllocateCommandBuffers res=%d, cmd=%p\n", res, (void*)s_clear_cmd_buf);
  if (res != VK_SUCCESS) {
    return false;
  }

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  res = dfn.vkCreateFence(dev, &fence_info, nullptr, &s_clear_fence);
  std::fprintf(stderr, "[REV2_GPU] vkCreateFence res=%d, fence=%p\n", res, (void*)s_clear_fence);
  if (res != VK_SUCCESS) {
    return false;
  }

  return true;
}

void Initialize() {
  if (s_initialized) return;
  s_initialized = true;

  auto& ps = PipelineState::Instance();
  const char* native_env = std::getenv("REV2_NATIVE_GPU");
  if (native_env && std::strcmp(native_env, "0") == 0) {
    ps.native_path_enabled = false;
  } else {
    ps.native_path_enabled = true;
  }

  const char* fallback_env = std::getenv("REV2_FALLBACK_XENOS");
  if (fallback_env && std::strcmp(fallback_env, "1") == 0) {
    ps.fallback_to_xenos = true;
  } else {
    ps.fallback_to_xenos = false;
  }

  // Carrega shaders pré-compilados SPIR-V para o Marco M6
  std::string spv_path = "cache/shaders/shareable/5841144C_prebaked.spvcache";
  if (!std::filesystem::exists(spv_path)) {
    spv_path = "recomp_project/cache/shaders/shareable/5841144C_prebaked.spvcache";
  }
  if (!std::filesystem::exists(spv_path)) {
    spv_path = "../cache/shaders/shareable/5841144C_prebaked.spvcache";
  }
  if (!std::filesystem::exists(spv_path)) {
    spv_path = "android/app/src/main/assets/shaders/shareable/5841144C_prebaked.spvcache";
  }
  ShaderManager::Instance().LoadPrebakedCache(spv_path);

  std::printf("\n=======================================================\n");
  std::printf("  [REV2_GPU] Camada D3D + Plume/Vulkan Inicializada\n");
  std::printf("  Status: %s\n", ps.native_path_enabled ? "ATIVA (Nativa D3D)" : "DESATIVADA (Modo Legado)");
  std::printf("  Fallback Xenos: %s\n", ps.fallback_to_xenos ? "HABILITADO" : "DESATIVADO (100%% Nativo Plume)");
  std::printf("  Texture Cache: ATIVO (D3D Texture Manager)\n");
  std::printf("  eDRAM Bypass: ATIVO (Direct Resolve)\n");
  std::printf("  Plume Present/Clear: ATIVO (Marco M5)\n");
  std::printf("  Shaders Prebaked: %zu módulos SPIR-V (Marco M6)\n", ShaderManager::Instance().ShaderCount());
  std::printf("=======================================================\n\n");
  std::fflush(stdout);
}

void OnPresent(uint32_t flags) {
  auto& ps = PipelineState::Instance();
  uint64_t frame = ps.stats.total_frames.fetch_add(1, std::memory_order_relaxed);
  std::fprintf(stderr, "[REV2_GPU] >>> OnPresent entered! frame=%lu, draws=%zu, clears=%d\n",
               frame + 1, ps.frame_draw_list.size(), ps.clear_state.pending);

  if (frame < 5 || (frame % 300) == 0) {
    DumpFrameMetrics();
  }

  // Executa apresentação e Clear/Draw nativo pela Plume (Marco M5 e M6)
  if (ps.native_path_enabled) {
    auto* rt = rex::Runtime::instance();
    if (rt && rt->graphics_system()) {
      auto* presenter = rt->graphics_system()->presenter();
      auto* vk_presenter = dynamic_cast<rex::ui::vulkan::VulkanPresenter*>(presenter);
      const rex::ui::vulkan::VulkanDevice* vk_dev_const = vk_presenter ? vk_presenter->vulkan_device() : nullptr;
      auto* provider = dynamic_cast<rex::ui::vulkan::VulkanProvider*>(rt->graphics_system()->provider());
      if (!vk_dev_const && provider) {
        vk_dev_const = provider->vulkan_device();
      }
      auto* vk_dev = const_cast<rex::ui::vulkan::VulkanDevice*>(vk_dev_const);
      std::fprintf(stderr, "[REV2_GPU] Presenter=%p, vk_presenter=%p, vk_dev=%p\n",
                   (void*)presenter, (void*)vk_presenter, (void*)vk_dev);

      if (presenter && vk_dev && EnsureVulkanClearContext(vk_dev)) {
        auto& dfn = GetVulkanDispatch();
        uint32_t w = static_cast<uint32_t>(ps.viewport.width > 0.0f ? ps.viewport.width : 960.0f);
        uint32_t h = static_cast<uint32_t>(ps.viewport.height > 0.0f ? ps.viewport.height : 540.0f);

        // Upload de buffers de geometria do quadro
        if (!ps.frame_index_data.empty()) {
          EnsureBuffer(vk_dev, s_vk_index_buf, s_vk_index_mem, s_vk_index_cap,
                       ps.frame_index_data.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
          UploadBufferData(vk_dev, s_vk_index_mem, ps.frame_index_data.data(), ps.frame_index_data.size());
        }

        if (!ps.frame_vertex_data.empty()) {
          EnsureBuffer(vk_dev, s_vk_vertex_buf, s_vk_vertex_mem, s_vk_vertex_cap,
                       ps.frame_vertex_data.size(),
                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
          UploadBufferData(vk_dev, s_vk_vertex_mem, ps.frame_vertex_data.data(), ps.frame_vertex_data.size());
        }

        // Upload de constantes para Uniform Buffer (Set 1)
        EnsureBuffer(vk_dev, s_vk_uniform_buf, s_vk_uniform_mem, s_vk_uniform_cap,
                     65536, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        static std::vector<uint8_t> s_ubo_staging(65536, 0);
        std::memcpy(s_ubo_staging.data() + 256, ps.vs_constants.data(), sizeof(ps.vs_constants));
        std::memcpy(s_ubo_staging.data() + 4608, ps.ps_constants.data(), sizeof(ps.ps_constants));
        UploadBufferData(vk_dev, s_vk_uniform_mem, s_ubo_staging.data(), 9472);

        // Atualização dos conjuntos de descritores (Set 0 e Set 1)
        VkDevice dev = vk_dev->device();
        auto& pm = PipelineManager::Instance();
        VkDescriptorSet set0 = pm.GetDescriptorSet(0);
        VkDescriptorSet set1 = pm.GetDescriptorSet(1);

        if (set0 != VK_NULL_HANDLE && s_vk_vertex_buf != VK_NULL_HANDLE) {
          VkDescriptorBufferInfo ssbo_info{};
          ssbo_info.buffer = s_vk_vertex_buf;
          ssbo_info.offset = 0;
          ssbo_info.range = VK_WHOLE_SIZE;

          VkWriteDescriptorSet write0{};
          write0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
          write0.dstSet = set0;
          write0.dstBinding = 0;
          write0.descriptorCount = 1;
          write0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
          write0.pBufferInfo = &ssbo_info;
          dfn.vkUpdateDescriptorSets(dev, 1, &write0, 0, nullptr);
        }

        if (set1 != VK_NULL_HANDLE && s_vk_uniform_buf != VK_NULL_HANDLE) {
          VkDescriptorBufferInfo ubo_sys_info{s_vk_uniform_buf, 0, 256};
          VkDescriptorBufferInfo ubo_vs_info{s_vk_uniform_buf, 256, sizeof(ps.vs_constants)};
          VkDescriptorBufferInfo ubo_ps_info{s_vk_uniform_buf, 4608, sizeof(ps.ps_constants)};
          VkDescriptorBufferInfo ubo_bool_info{s_vk_uniform_buf, 8960, 256};
          VkDescriptorBufferInfo ubo_fetch_info{s_vk_uniform_buf, 9216, 256};

          std::array<VkWriteDescriptorSet, 5> writes1{};
          writes1[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
          writes1[0].dstSet = set1;
          writes1[0].dstBinding = 0;
          writes1[0].descriptorCount = 1;
          writes1[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
          writes1[0].pBufferInfo = &ubo_sys_info;

          writes1[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
          writes1[1].dstSet = set1;
          writes1[1].dstBinding = 1;
          writes1[1].descriptorCount = 1;
          writes1[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
          writes1[1].pBufferInfo = &ubo_vs_info;

          writes1[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
          writes1[2].dstSet = set1;
          writes1[2].dstBinding = 2;
          writes1[2].descriptorCount = 1;
          writes1[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
          writes1[2].pBufferInfo = &ubo_ps_info;

          writes1[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
          writes1[3].dstSet = set1;
          writes1[3].dstBinding = 3;
          writes1[3].descriptorCount = 1;
          writes1[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
          writes1[3].pBufferInfo = &ubo_bool_info;

          writes1[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
          writes1[4].dstSet = set1;
          writes1[4].dstBinding = 4;
          writes1[4].descriptorCount = 1;
          writes1[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
          writes1[4].pBufferInfo = &ubo_fetch_info;
          std::fprintf(stderr, "[REV2_GPU] Calling vkUpdateDescriptorSets for set1=%p, writes=%zu\n", (void*)set1, writes1.size());
          std::fflush(stderr);
          dfn.vkUpdateDescriptorSets(dev, static_cast<uint32_t>(writes1.size()), writes1.data(), 0, nullptr);
          std::fprintf(stderr, "[REV2_GPU] vkUpdateDescriptorSets done\n");
          std::fflush(stderr);
        }

        std::fprintf(stderr, "[REV2_GPU] Calling RefreshGuestOutput w=%u, h=%u\n", w, h);
        presenter->RefreshGuestOutput(
            w, h, 16, 9,
            [&](rex::ui::Presenter::GuestOutputRefreshContext& context) -> bool {
              auto& vk_ctx = static_cast<rex::ui::vulkan::VulkanPresenter::VulkanGuestOutputRefreshContext&>(context);
              VkImage dst_image = vk_ctx.image();
              std::fprintf(stderr, "[REV2_GPU] RefreshGuestOutput refresher: dst_image=%p\n", (void*)dst_image);
              if (dst_image == VK_NULL_HANDLE) return false;

              dfn.vkResetCommandPool(vk_dev->device(), s_clear_cmd_pool, 0);

              VkCommandBufferBeginInfo begin_info{};
              begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
              begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
              dfn.vkBeginCommandBuffer(s_clear_cmd_buf, &begin_info);
              std::fprintf(stderr, "[REV2_GPU] vkBeginCommandBuffer done, draws=%zu\n", ps.frame_draw_list.size());

              if (!ps.frame_draw_list.empty()) {
                // Marco M6: RenderPass com Geometria
                VkImageMemoryBarrier barrier_to_color{};
                barrier_to_color.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier_to_color.oldLayout = vk_ctx.image_ever_written_previously()
                    ? rex::ui::vulkan::VulkanPresenter::kGuestOutputInternalLayout
                    : VK_IMAGE_LAYOUT_UNDEFINED;
                barrier_to_color.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                barrier_to_color.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier_to_color.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier_to_color.image = dst_image;
                barrier_to_color.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier_to_color.subresourceRange.baseMipLevel = 0;
                barrier_to_color.subresourceRange.levelCount = 1;
                barrier_to_color.subresourceRange.baseArrayLayer = 0;
                barrier_to_color.subresourceRange.layerCount = 1;
                barrier_to_color.srcAccessMask = vk_ctx.image_ever_written_previously()
                    ? rex::ui::vulkan::VulkanPresenter::kGuestOutputInternalAccessMask
                    : 0;
                barrier_to_color.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                dfn.vkCmdPipelineBarrier(
                    s_clear_cmd_buf,
                    vk_ctx.image_ever_written_previously()
                        ? rex::ui::vulkan::VulkanPresenter::kGuestOutputInternalStageMask
                        : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &barrier_to_color
                );

                VkRenderPass rp = PipelineManager::Instance().GetOrCreateRenderPass(rex::ui::vulkan::VulkanPresenter::kGuestOutputFormat);
                if (rp != VK_NULL_HANDLE) {
                  VkImageView view = vk_ctx.image_view();
                  VkFramebufferCreateInfo fb_ci{};
                  fb_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                  fb_ci.renderPass = rp;
                  fb_ci.attachmentCount = 1;
                  fb_ci.pAttachments = &view;
                  fb_ci.width = w;
                  fb_ci.height = h;
                  fb_ci.layers = 1;

                  VkFramebuffer fb = VK_NULL_HANDLE;
                  if (dfn.vkCreateFramebuffer(vk_dev->device(), &fb_ci, nullptr, &fb) == VK_SUCCESS) {
                    VkRenderPassBeginInfo rp_bi{};
                    rp_bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                    rp_bi.renderPass = rp;
                    rp_bi.framebuffer = fb;
                    rp_bi.renderArea.offset = {0, 0};
                    rp_bi.renderArea.extent = {w, h};

                    VkClearValue cv{};
                    uint32_t c = ps.clear_state.color;
                    cv.color.float32[0] = ((c >> 16) & 0xFF) / 255.0f;
                    cv.color.float32[1] = ((c >> 8) & 0xFF) / 255.0f;
                    cv.color.float32[2] = (c & 0xFF) / 255.0f;
                    cv.color.float32[3] = ((c >> 24) & 0xFF) / 255.0f;

                    rp_bi.clearValueCount = 1;
                    rp_bi.pClearValues = &cv;

                    dfn.vkCmdBeginRenderPass(s_clear_cmd_buf, &rp_bi, VK_SUBPASS_CONTENTS_INLINE);

                    VkViewport vp{0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f};
                    dfn.vkCmdSetViewport(s_clear_cmd_buf, 0, 1, &vp);

                    VkRect2D sc{{0, 0}, {w, h}};
                    dfn.vkCmdSetScissor(s_clear_cmd_buf, 0, 1, &sc);

                    // Vincula conjuntos de descritores (Sets 0..3)
                    std::array<VkDescriptorSet, 4> desc_sets = {
                      pm.GetDescriptorSet(0),
                      pm.GetDescriptorSet(1),
                      pm.GetDescriptorSet(2),
                      pm.GetDescriptorSet(3)
                    };
                    dfn.vkCmdBindDescriptorSets(
                      s_clear_cmd_buf,
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pm.GetPipelineLayout(),
                      0, static_cast<uint32_t>(desc_sets.size()),
                      desc_sets.data(),
                      0, nullptr
                    );

                    for (const auto& cmd : ps.frame_draw_list) {
                      PipelineKey key;
                      key.primitive_type = cmd.primitive_type;
                      key.vs_hash = cmd.vs_handle ? cmd.vs_handle : ps.current_vs_handle;
                      key.ps_hash = cmd.ps_handle ? cmd.ps_handle : ps.current_ps_handle;
                      key.z_enable = cmd.state.z_enable;
                      key.z_write_enable = cmd.state.z_write_enable;
                      key.z_func = cmd.state.z_func;
                      key.blend_enable = cmd.state.blend_enable;
                      key.src_blend = cmd.state.src_blend;
                      key.dst_blend = cmd.state.dst_blend;
                      key.cull_mode = cmd.state.cull_mode;
                      key.color_write_mask = cmd.state.color_write_mask;

                      VkPipeline pipeline = pm.GetOrCreatePipeline(key, rp);
                      if (pipeline != VK_NULL_HANDLE) {
                        dfn.vkCmdBindPipeline(s_clear_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

                        if (s_vk_vertex_buf != VK_NULL_HANDLE && cmd.vertex_data_size > 0) {
                          VkDeviceSize vtx_offset = cmd.vertex_data_offset;
                          dfn.vkCmdBindVertexBuffers(s_clear_cmd_buf, 0, 1, &s_vk_vertex_buf, &vtx_offset);
                        }

                        if (s_vk_index_buf != VK_NULL_HANDLE && cmd.index_count > 0 && cmd.index_data_size > 0) {
                          dfn.vkCmdBindIndexBuffer(s_clear_cmd_buf, s_vk_index_buf, cmd.index_data_offset,
                                                   (cmd.index_format == 0) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
                          dfn.vkCmdDrawIndexed(s_clear_cmd_buf, cmd.index_count, cmd.instance_count, 0, cmd.base_vertex, cmd.first_instance);
                        } else if (cmd.num_vertices > 0) {
                          dfn.vkCmdDraw(s_clear_cmd_buf, cmd.num_vertices, cmd.instance_count, 0, cmd.first_instance);
                        }
                      }
                    }

                    dfn.vkCmdEndRenderPass(s_clear_cmd_buf);
                    dfn.vkDestroyFramebuffer(vk_dev->device(), fb, nullptr);
                  }
                }

                VkImageMemoryBarrier barrier_to_read{};
                barrier_to_read.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier_to_read.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                barrier_to_read.newLayout = rex::ui::vulkan::VulkanPresenter::kGuestOutputInternalLayout;
                barrier_to_read.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier_to_read.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier_to_read.image = dst_image;
                barrier_to_read.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier_to_read.subresourceRange.baseMipLevel = 0;
                barrier_to_read.subresourceRange.levelCount = 1;
                barrier_to_read.subresourceRange.baseArrayLayer = 0;
                barrier_to_read.subresourceRange.layerCount = 1;
                barrier_to_read.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier_to_read.dstAccessMask = rex::ui::vulkan::VulkanPresenter::kGuestOutputInternalAccessMask;

                dfn.vkCmdPipelineBarrier(
                    s_clear_cmd_buf,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    rex::ui::vulkan::VulkanPresenter::kGuestOutputInternalStageMask,
                    0, 0, nullptr, 0, nullptr, 1, &barrier_to_read
                );
              } else {
                // Marco M5: Fast Clear
                VkImageMemoryBarrier barrier_to_dst{};
                barrier_to_dst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier_to_dst.oldLayout = vk_ctx.image_ever_written_previously()
                    ? rex::ui::vulkan::VulkanPresenter::kGuestOutputInternalLayout
                    : VK_IMAGE_LAYOUT_UNDEFINED;
                barrier_to_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier_to_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier_to_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier_to_dst.image = dst_image;
                barrier_to_dst.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier_to_dst.subresourceRange.baseMipLevel = 0;
                barrier_to_dst.subresourceRange.levelCount = 1;
                barrier_to_dst.subresourceRange.baseArrayLayer = 0;
                barrier_to_dst.subresourceRange.layerCount = 1;
                barrier_to_dst.srcAccessMask = vk_ctx.image_ever_written_previously()
                    ? rex::ui::vulkan::VulkanPresenter::kGuestOutputInternalAccessMask
                    : 0;
                barrier_to_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                dfn.vkCmdPipelineBarrier(
                    s_clear_cmd_buf,
                    vk_ctx.image_ever_written_previously()
                        ? rex::ui::vulkan::VulkanPresenter::kGuestOutputInternalStageMask
                        : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &barrier_to_dst
                );

                VkClearColorValue clear_color{};
                uint32_t c = ps.clear_state.color;
                clear_color.float32[0] = ((c >> 16) & 0xFF) / 255.0f;
                clear_color.float32[1] = ((c >> 8) & 0xFF) / 255.0f;
                clear_color.float32[2] = (c & 0xFF) / 255.0f;
                clear_color.float32[3] = ((c >> 24) & 0xFF) / 255.0f;

                VkImageSubresourceRange range{};
                range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                range.baseMipLevel = 0;
                range.levelCount = 1;
                range.baseArrayLayer = 0;
                range.layerCount = 1;

                dfn.vkCmdClearColorImage(s_clear_cmd_buf, dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &range);

                VkImageMemoryBarrier barrier_to_read{};
                barrier_to_read.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier_to_read.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier_to_read.newLayout = rex::ui::vulkan::VulkanPresenter::kGuestOutputInternalLayout;
                barrier_to_read.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier_to_read.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier_to_read.image = dst_image;
                barrier_to_read.subresourceRange = range;
                barrier_to_read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier_to_read.dstAccessMask = rex::ui::vulkan::VulkanPresenter::kGuestOutputInternalAccessMask;

                dfn.vkCmdPipelineBarrier(
                    s_clear_cmd_buf,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    rex::ui::vulkan::VulkanPresenter::kGuestOutputInternalStageMask,
                    0, 0, nullptr, 0, nullptr, 1, &barrier_to_read
                );
              }

              dfn.vkEndCommandBuffer(s_clear_cmd_buf);

              dfn.vkResetFences(vk_dev->device(), 1, &s_clear_fence);
              VkSubmitInfo submit_info{};
              submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
              submit_info.commandBufferCount = 1;
              submit_info.pCommandBuffers = &s_clear_cmd_buf;

              {
                auto queue_acq = vk_dev->AcquireQueue(vk_dev->queue_family_graphics_compute(), 0);
                dfn.vkQueueSubmit(queue_acq.queue(), 1, &submit_info, s_clear_fence);
              }
              dfn.vkWaitForFences(vk_dev->device(), 1, &s_clear_fence, VK_TRUE, UINT64_MAX);
              std::fprintf(stderr, "[REV2_GPU] refresher submitted and fence waited successfully!\n");

              return true;
            }
        );
        std::fprintf(stderr, "[REV2_GPU] RefreshGuestOutput returned to OnPresent\n");
      }
    }
    ps.clear_state.pending = false;
    ps.frame_draw_list.clear();
    ps.frame_index_data.clear();
    ps.frame_vertex_data.clear();
  }
}

void OnClear(uint32_t count, uint32_t flags, uint32_t color, float z, uint32_t stencil) {
  auto& ps = PipelineState::Instance();
  ps.stats.clears.fetch_add(1, std::memory_order_relaxed);
  ps.clear_state.flags = flags;
  ps.clear_state.color = color;
  ps.clear_state.z = z;
  ps.clear_state.stencil = stencil;
  ps.clear_state.pending = true;
}

void OnSetRenderTarget(uint32_t index, uint32_t surface_addr) {
  auto& ps = PipelineState::Instance();
  if (index < 4) {
    ps.render_targets[index] = surface_addr;
  }
}

void OnSetDepthStencilSurface(uint32_t surface_addr) {
  auto& ps = PipelineState::Instance();
  ps.depth_stencil_surface = surface_addr;
}

void OnSetViewport(float x, float y, float w, float h, float min_z, float max_z) {
  auto& ps = PipelineState::Instance();
  ps.viewport = {x, y, w, h, min_z, max_z};
}

void OnSetScissorRect(int32_t l, int32_t t, int32_t r, int32_t b) {
  auto& ps = PipelineState::Instance();
  ps.scissor = {l, t, r, b};
}

void OnSetVertexShader(uint32_t shader_addr) {
  auto& ps = PipelineState::Instance();
  ps.current_vs_handle = shader_addr;
}

void OnSetPixelShader(uint32_t shader_addr) {
  auto& ps = PipelineState::Instance();
  ps.current_ps_handle = shader_addr;
}

void OnSetVSConstantF(uint32_t start_reg, const float* data, uint32_t count) {
  auto& ps = PipelineState::Instance();
  ps.stats.vs_constant_updates.fetch_add(count, std::memory_order_relaxed);
  if (data && start_reg + count <= ps.vs_constants.size()) {
    std::memcpy(&ps.vs_constants[start_reg], data, count * sizeof(Vector4f));
  }
}

void OnSetPSConstantF(uint32_t start_reg, const float* data, uint32_t count) {
  auto& ps = PipelineState::Instance();
  ps.stats.ps_constant_updates.fetch_add(count, std::memory_order_relaxed);
  if (data && start_reg + count <= ps.ps_constants.size()) {
    std::memcpy(&ps.ps_constants[start_reg], data, count * sizeof(Vector4f));
  }
}

void OnSetTexture(uint32_t sampler, uint32_t texture_addr, const uint8_t* base) {
  auto& ps = PipelineState::Instance();
  ps.stats.texture_binds.fetch_add(1, std::memory_order_relaxed);
  if (sampler >= ps.textures.size()) return;

  if (texture_addr == 0 || base == nullptr) {
    ps.textures[sampler] = {};
    return;
  }

  // Decodifica descritor da textura do Xbox 360 no endereço guest
  const uint8_t* tex_desc = base + texture_addr;
  uint32_t dword0 = __builtin_bswap32(*(reinterpret_cast<const uint32_t*>(tex_desc + 28)));
  uint32_t dword1 = __builtin_bswap32(*(reinterpret_cast<const uint32_t*>(tex_desc + 32)));
  uint32_t base_ptr = __builtin_bswap32(*(reinterpret_cast<const uint32_t*>(tex_desc + 36)));

  TextureBinding tb;
  tb.guest_addr = base_ptr;
  tb.width = (dword0 & 0x1FFF) + 1;
  tb.height = ((dword0 >> 13) & 0x1FFF) + 1;
  tb.depth = ((dword0 >> 26) & 0x3F) + 1;
  tb.is_tiled = (dword1 & 0x01) != 0;
  tb.format = (dword1 >> 24) & 0x3F;
  tb.mip_levels = (dword1 >> 1) & 0x0F;

  ps.textures[sampler] = tb;

  // Gerenciamento e Cache de Texturas (Fase 7)
  if (base_ptr != 0) {
    auto it = s_texture_cache.find(base_ptr);
    if (it != s_texture_cache.end() &&
        it->second.width == tb.width &&
        it->second.height == tb.height &&
        it->second.format == tb.format) {
      ps.stats.texture_cache_hits.fetch_add(1, std::memory_order_relaxed);
      it->second.last_frame_used = ps.stats.total_frames.load(std::memory_order_relaxed);
    } else {
      ps.stats.texture_cache_misses.fetch_add(1, std::memory_order_relaxed);

      const rex::graphics::FormatInfo* fmt_info = rex::graphics::FormatInfo::Get(tb.format);
      uint32_t block_w = (fmt_info && fmt_info->block_width) ? fmt_info->block_width : 4;
      uint32_t block_h = (fmt_info && fmt_info->block_height) ? fmt_info->block_height : 4;
      uint32_t block_bytes = fmt_info ? fmt_info->bytes_per_block() : 16;
      uint32_t blocks_x = (tb.width + block_w - 1) / block_w;
      uint32_t blocks_y = (tb.height + block_h - 1) / block_h;
      uint32_t total_size = blocks_x * blocks_y * block_bytes;

      CachedTexture entry;
      entry.guest_addr = base_ptr;
      entry.width = tb.width;
      entry.height = tb.height;
      entry.depth = tb.depth;
      entry.format = tb.format;
      entry.is_tiled = tb.is_tiled;
      entry.data_size = total_size;
      entry.last_frame_used = ps.stats.total_frames.load(std::memory_order_relaxed);

      if (total_size > 0 && base) {
        entry.data.resize(total_size);
        const uint8_t* src_ptr = base + base_ptr;
        if (tb.is_tiled) {
          UntileTexture2D(src_ptr, entry.data.data(), tb.width, tb.height, tb.format);
          ps.stats.textures_untiled.fetch_add(1, std::memory_order_relaxed);
          ps.stats.bytes_untiled.fetch_add(total_size, std::memory_order_relaxed);
        } else {
          std::memcpy(entry.data.data(), src_ptr, total_size);
        }
      }

      s_texture_cache[base_ptr] = std::move(entry);
    }
  }
}

void OnSetSamplerState(uint32_t sampler, uint32_t type, uint32_t value) {
  auto& ps = PipelineState::Instance();
  ps.stats.sampler_state_changes.fetch_add(1, std::memory_order_relaxed);
  if (sampler < ps.samplers.size()) {
    switch (type) {
      case 1: ps.samplers[sampler].address_u = value; break;
      case 2: ps.samplers[sampler].address_v = value; break;
      case 3: ps.samplers[sampler].address_w = value; break;
      case 4: ps.samplers[sampler].mag_filter = value; break;
      case 5: ps.samplers[sampler].min_filter = value; break;
      case 6: ps.samplers[sampler].mip_filter = value; break;
      default: break;
    }
  }
}

void OnSetRenderState(uint32_t state, uint32_t value) {
  auto& ps = PipelineState::Instance();
  ps.stats.render_state_changes.fetch_add(1, std::memory_order_relaxed);
  switch (state) {
    case 7:   ps.state.z_enable = (value != 0); break;
    case 14:  ps.state.z_write_enable = (value != 0); break;
    case 23:  ps.state.z_func = value; break;
    case 27:  ps.state.blend_enable = (value != 0); break;
    case 19:  ps.state.src_blend = value; break;
    case 20:  ps.state.dst_blend = value; break;
    case 25:  ps.state.cull_mode = value; break;
    case 168: ps.state.color_write_mask = value; break;
    default: break;
  }
}

void OnSetStreamSource(uint32_t stream, uint32_t buf_addr, uint32_t offset, uint32_t stride) {
  auto& ps = PipelineState::Instance();
  ps.stats.stream_source_changes.fetch_add(1, std::memory_order_relaxed);
  if (stream < ps.vertex_streams.size()) {
    ps.vertex_streams[stream] = {buf_addr, offset, stride};
  }
}

void OnSetIndices(uint32_t idx_buf_addr) {
  auto& ps = PipelineState::Instance();
  ps.stats.indices_changes.fetch_add(1, std::memory_order_relaxed);
  ps.index_buffer.buffer_guest_addr = idx_buf_addr;
}

void OnSetVertexDeclaration(uint32_t decl_addr) {
  auto& ps = PipelineState::Instance();
  ps.stats.vertex_decl_changes.fetch_add(1, std::memory_order_relaxed);
  ps.vertex_declaration = decl_addr;
}

void OnDrawIndexedPrimitive(uint32_t type, int32_t base_vtx, uint32_t min_idx, uint32_t num_vtx, uint32_t start_idx, uint32_t prim_count) {
  auto& ps = PipelineState::Instance();
  ps.stats.draw_indexed_calls.fetch_add(1, std::memory_order_relaxed);
}

#include <dlfcn.h>

void EmitVkDrawIndexed(uint32_t type, int32_t base_vtx, uint32_t min_idx, uint32_t num_vtx, uint32_t start_idx, uint32_t prim_count, const uint8_t* base) {
  auto& ps = PipelineState::Instance();

  // 1. Calcula contagem real de índices conforme a topologia de primitiva D3D
  uint32_t index_count = 0;
  switch (type) {
    case 1: // D3DPT_POINTLIST
      index_count = prim_count;
      break;
    case 2: // D3DPT_LINELIST
      index_count = prim_count * 2;
      break;
    case 3: // D3DPT_LINESTRIP
      index_count = prim_count + 1;
      break;
    case 4: // D3DPT_TRIANGLELIST
      index_count = prim_count * 3;
      break;
    case 5: // D3DPT_TRIANGLESTRIP
      index_count = prim_count + 2;
      break;
    case 6: // D3DPT_TRIANGLEFAN
      index_count = prim_count + 2;
      break;
    case 8: // D3DPT_RECTLIST
      index_count = prim_count * 3;
      break;
    default:
      index_count = prim_count ? (prim_count * 3) : num_vtx;
      break;
  }
  if (index_count == 0 && num_vtx > 0) {
    index_count = num_vtx;
  }
  if (index_count > 500000) {
    index_count = 500000;
  }

  // 2. Incrementa contador atômico de emissão Vulkan/Plume
  ps.stats.vk_draws_emitted.fetch_add(1, std::memory_order_relaxed);

  // 3. Monta comando VkDrawIndexedCommand estruturado
  VkDrawIndexedCommand cmd{};
  cmd.primitive_type = type;
  cmd.base_vertex = base_vtx;
  cmd.min_index = min_idx;
  cmd.num_vertices = num_vtx;
  cmd.start_index = start_idx;
  cmd.primitive_count = prim_count;
  cmd.index_count = index_count;
  cmd.instance_count = 1;
  cmd.first_instance = 0;

  // 4. Conversão e Validação de Geometria de Índices (Big-Endian para Host)
  uint32_t index_offset = static_cast<uint32_t>(ps.frame_index_data.size());
  uint32_t index_bytes = 0;
  uint32_t index_format = ps.index_buffer.format;

  if (base && ps.index_buffer.buffer_guest_addr && index_count > 0 && ps.frame_index_data.size() < 64 * 1024 * 1024) {
    if (index_format == 0) {
      // 16-bit indices (Big-Endian Xenos kInt16)
      index_bytes = index_count * sizeof(uint16_t);
      const uint16_t* src_idx = reinterpret_cast<const uint16_t*>(base + ps.index_buffer.buffer_guest_addr) + start_idx;
      size_t old_size = ps.frame_index_data.size();
      ps.frame_index_data.resize(old_size + index_bytes);
      uint16_t* dst_idx = reinterpret_cast<uint16_t*>(ps.frame_index_data.data() + old_size);
      for (uint32_t i = 0; i < index_count; ++i) {
        dst_idx[i] = __builtin_bswap16(src_idx[i]);
      }
    } else {
      // 32-bit indices (Big-Endian Xenos kInt32)
      index_bytes = index_count * sizeof(uint32_t);
      const uint32_t* src_idx = reinterpret_cast<const uint32_t*>(base + ps.index_buffer.buffer_guest_addr) + start_idx;
      size_t old_size = ps.frame_index_data.size();
      ps.frame_index_data.resize(old_size + index_bytes);
      uint32_t* dst_idx = reinterpret_cast<uint32_t*>(ps.frame_index_data.data() + old_size);
      for (uint32_t i = 0; i < index_count; ++i) {
        dst_idx[i] = __builtin_bswap32(src_idx[i]);
      }
    }
  }

  cmd.index_data_offset = index_offset;
  cmd.index_data_size = index_bytes;
  cmd.index_format = index_format;

  // 5. Copia e valida Stream de Vértices para frame_vertex_data
  uint32_t vtx_stride = ps.vertex_streams[0].stride ? ps.vertex_streams[0].stride : 32;
  uint32_t vtx_offset = static_cast<uint32_t>(ps.frame_vertex_data.size());
  uint32_t vtx_count = (num_vtx > 0) ? num_vtx : (prim_count * 3);
  uint32_t vtx_bytes = 0;

  if (base && ps.vertex_streams[0].buffer_guest_addr && vtx_count > 0) {
    uint32_t total_vtx = min_idx + vtx_count;
    vtx_bytes = total_vtx * vtx_stride;
    const uint8_t* src_vtx = base + ps.vertex_streams[0].buffer_guest_addr + ps.vertex_streams[0].offset;
    size_t old_size = ps.frame_vertex_data.size();
    ps.frame_vertex_data.resize(old_size + vtx_bytes);
    uint8_t* dst_vtx = ps.frame_vertex_data.data() + old_size;

    const uint32_t* src_dw = reinterpret_cast<const uint32_t*>(src_vtx);
    uint32_t* dst_dw = reinterpret_cast<uint32_t*>(dst_vtx);
    size_t dword_count = vtx_bytes / 4;
    for (size_t i = 0; i < dword_count; ++i) {
      dst_dw[i] = __builtin_bswap32(src_dw[i]);
    }
  }

  cmd.vertex_data_offset = vtx_offset;
  cmd.vertex_data_size = vtx_bytes;
  cmd.vertex_stride = vtx_stride;

  // 6. Snapshot de shaders e render states ativos
  cmd.vs_handle = ps.current_vs_handle;
  cmd.ps_handle = ps.current_ps_handle;
  cmd.state = ps.state;

  // 7. Registra no buffer de quadro da Plume
  ps.frame_draw_list.push_back(cmd);
}

void OnDrawPrimitive(uint32_t type, uint32_t start_vtx, uint32_t prim_count) {
  auto& ps = PipelineState::Instance();
  ps.stats.draw_primitive_calls.fetch_add(1, std::memory_order_relaxed);
}

void OnSetBlendState(uint32_t src_blend, uint32_t dst_blend, uint32_t op) {
  auto& ps = PipelineState::Instance();
  ps.stats.blend_state_changes.fetch_add(1, std::memory_order_relaxed);
  ps.state.blend_enable = (src_blend != 1 || dst_blend != 0);
  ps.state.src_blend = src_blend;
  ps.state.dst_blend = dst_blend;
  ps.state.blend_op = op;
}

void OnSetDepthStencilState(uint32_t depth_enable, uint32_t z_func) {
  auto& ps = PipelineState::Instance();
  ps.stats.depth_stencil_state_changes.fetch_add(1, std::memory_order_relaxed);
  ps.state.z_enable = (depth_enable != 0);
  if (z_func != 0) {
    ps.state.z_func = z_func;
  }
}

void OnSetAlphaTestState(uint32_t alpha_ref, uint32_t alpha_func) {
  auto& ps = PipelineState::Instance();
  ps.stats.alpha_test_state_changes.fetch_add(1, std::memory_order_relaxed);
  ps.state.alpha_test_enable = (alpha_func != 8);
  ps.state.alpha_ref = alpha_ref;
  ps.state.alpha_func = alpha_func;
}

void OnSetColorWriteMask(uint32_t rt_index, uint32_t mask) {
  auto& ps = PipelineState::Instance();
  ps.stats.color_write_mask_changes.fetch_add(1, std::memory_order_relaxed);
  ps.state.color_write_mask = mask;
}

void OnResolveParameters(uint32_t config) {
  auto& ps = PipelineState::Instance();
  ps.stats.resolve_parameter_changes.fetch_add(1, std::memory_order_relaxed);
}

void OnResolve(uint32_t flags, uint32_t src_surface, uint32_t dst_texture) {
  auto& ps = PipelineState::Instance();
  ps.stats.resolves.fetch_add(1, std::memory_order_relaxed);
  ps.last_resolve_src = src_surface;
  ps.last_resolve_dst = dst_texture;
}

void OnBeginTiling(uint32_t tile_count) {
  auto& ps = PipelineState::Instance();
  ps.stats.tiling_begin_calls.fetch_add(1, std::memory_order_relaxed);
  ps.tiling_active = true;
  ps.active_tiles = tile_count;
}

void OnEndTiling() {
  auto& ps = PipelineState::Instance();
  ps.stats.tiling_end_calls.fetch_add(1, std::memory_order_relaxed);
  ps.tiling_active = false;
  ps.active_tiles = 0;
}

void DumpFrameMetrics() {
  auto& ps = PipelineState::Instance();
  uint64_t f = ps.stats.total_frames.load(std::memory_order_relaxed);
  uint64_t di = ps.stats.draw_indexed_calls.load(std::memory_order_relaxed);
  uint64_t vk_d = ps.stats.vk_draws_emitted.load(std::memory_order_relaxed);
  uint64_t dp = ps.stats.draw_primitive_calls.load(std::memory_order_relaxed);
  uint64_t vs = ps.stats.vs_constant_updates.load(std::memory_order_relaxed);
  uint64_t ps_c = ps.stats.ps_constant_updates.load(std::memory_order_relaxed);
  uint64_t tb = ps.stats.texture_binds.load(std::memory_order_relaxed);
  uint64_t th = ps.stats.texture_cache_hits.load(std::memory_order_relaxed);
  uint64_t tm = ps.stats.texture_cache_misses.load(std::memory_order_relaxed);
  uint64_t tu = ps.stats.textures_untiled.load(std::memory_order_relaxed);
  uint64_t bu = ps.stats.bytes_untiled.load(std::memory_order_relaxed);
  uint64_t rs = ps.stats.render_state_changes.load(std::memory_order_relaxed);
  uint64_t cl = ps.stats.clears.load(std::memory_order_relaxed);
  uint64_t res = ps.stats.resolves.load(std::memory_order_relaxed);
  uint64_t ss = ps.stats.stream_source_changes.load(std::memory_order_relaxed);
  uint64_t idx = ps.stats.indices_changes.load(std::memory_order_relaxed);
  uint64_t bs = ps.stats.blend_state_changes.load(std::memory_order_relaxed);
  uint64_t ds = ps.stats.depth_stencil_state_changes.load(std::memory_order_relaxed);
  uint64_t at = ps.stats.alpha_test_state_changes.load(std::memory_order_relaxed);
  uint64_t cwm = ps.stats.color_write_mask_changes.load(std::memory_order_relaxed);

  std::printf("\n=======================================================\n");
  std::printf("  [REV2_GPU] Metricas da Camada D3D + Plume (Frame %lu)\n", (unsigned long)f);
  std::printf("  - Modo de Execucao:       %s\n", ps.fallback_to_xenos ? "Fallback Xenos Ativo" : "100%% NATIVO PLUME/VULKAN");
  std::printf("  - Draws Indexados (D3D):  %lu\n", (unsigned long)di);
  std::printf("  - vkCmdDrawIndexed Plume: %lu\n", (unsigned long)vk_d);
  std::printf("  - Draws Primitivos:       %lu\n", (unsigned long)dp);
  std::printf("  - Streams de Vertices:    %lu (Buffer Cap: %.2f KB)\n", (unsigned long)ss, (double)s_vk_vertex_cap / 1024.0);
  std::printf("  - Indices Configurados:   %lu (Buffer Cap: %.2f KB)\n", (unsigned long)idx, (double)s_vk_index_cap / 1024.0);
  std::printf("  - Atualizacoes Const VS:  %lu\n", (unsigned long)vs);
  std::printf("  - Atualizacoes Const PS:  %lu\n", (unsigned long)ps_c);
  std::printf("  - Vinculacoes Textura:    %lu (Hits: %lu, Misses: %lu)\n", (unsigned long)tb, (unsigned long)th, (unsigned long)tm);
  std::printf("  - Texturas Tiled Detect:  %lu (%.2f MB est.)\n", (unsigned long)tu, (double)bu / (1024.0 * 1024.0));
  std::printf("  - Cache Texturas Vivas:   %zu\n", s_texture_cache.size());
  std::printf("  - Shaders Prebaked Vivos: %zu\n", ShaderManager::Instance().ShaderCount());
  std::printf("  - Pipelines Vulkan Vivos: %zu\n", PipelineManager::Instance().PipelineCount());
  std::printf("  - Mudancas Render State:  %lu (Blend: %lu, Depth: %lu, Alpha: %lu, Mask: %lu)\n",
              (unsigned long)rs, (unsigned long)bs, (unsigned long)ds, (unsigned long)at, (unsigned long)cwm);
  std::printf("  - Operacoes Clear:        %lu\n", (unsigned long)cl);
  std::printf("  - Operacoes Resolve:      %lu\n", (unsigned long)res);
  std::printf("=======================================================\n\n");
  std::fflush(stdout);
}

} // namespace rev2::gpu
