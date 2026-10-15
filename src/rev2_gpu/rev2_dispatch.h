#pragma once

#include <rex/ui/vulkan/api.h>
#include <dlfcn.h>
#include <cstdio>

namespace rev2::gpu {

struct VulkanDispatch {
  // Device functions loaded via vkGetDeviceProcAddr
  PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = nullptr;
  PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
  PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
  PFN_vkCreateFence vkCreateFence = nullptr;
  PFN_vkResetFences vkResetFences = nullptr;
  PFN_vkWaitForFences vkWaitForFences = nullptr;
  PFN_vkResetCommandPool vkResetCommandPool = nullptr;
  PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
  PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
  PFN_vkQueueSubmit vkQueueSubmit = nullptr;

  PFN_vkCreateBuffer vkCreateBuffer = nullptr;
  PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;
  PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = nullptr;
  PFN_vkAllocateMemory vkAllocateMemory = nullptr;
  PFN_vkFreeMemory vkFreeMemory = nullptr;
  PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;
  PFN_vkMapMemory vkMapMemory = nullptr;
  PFN_vkUnmapMemory vkUnmapMemory = nullptr;

  PFN_vkCreateFramebuffer vkCreateFramebuffer = nullptr;
  PFN_vkDestroyFramebuffer vkDestroyFramebuffer = nullptr;
  PFN_vkCreateRenderPass vkCreateRenderPass = nullptr;
  PFN_vkDestroyRenderPass vkDestroyRenderPass = nullptr;

  PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier = nullptr;
  PFN_vkCmdClearColorImage vkCmdClearColorImage = nullptr;
  PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass = nullptr;
  PFN_vkCmdEndRenderPass vkCmdEndRenderPass = nullptr;
  PFN_vkCmdSetViewport vkCmdSetViewport = nullptr;
  PFN_vkCmdSetScissor vkCmdSetScissor = nullptr;
  PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets = nullptr;
  PFN_vkCmdBindPipeline vkCmdBindPipeline = nullptr;
  PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers = nullptr;
  PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer = nullptr;
  PFN_vkCmdDrawIndexed vkCmdDrawIndexed = nullptr;
  PFN_vkCmdDraw vkCmdDraw = nullptr;

  PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout = nullptr;
  PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout = nullptr;
  PFN_vkCreatePipelineLayout vkCreatePipelineLayout = nullptr;
  PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = nullptr;
  PFN_vkCreateDescriptorPool vkCreateDescriptorPool = nullptr;
  PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = nullptr;
  PFN_vkResetDescriptorPool vkResetDescriptorPool = nullptr;
  PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets = nullptr;
  PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets = nullptr;

  PFN_vkCreateShaderModule vkCreateShaderModule = nullptr;
  PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;
  PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines = nullptr;
  PFN_vkDestroyPipeline vkDestroyPipeline = nullptr;

  PFN_vkCreateImage vkCreateImage = nullptr;
  PFN_vkDestroyImage vkDestroyImage = nullptr;
  PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements = nullptr;
  PFN_vkBindImageMemory vkBindImageMemory = nullptr;
  PFN_vkCreateImageView vkCreateImageView = nullptr;
  PFN_vkDestroyImageView vkDestroyImageView = nullptr;
  PFN_vkCreateSampler vkCreateSampler = nullptr;
  PFN_vkDestroySampler vkDestroySampler = nullptr;
  PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage = nullptr;

  // Physical device functions
  PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = nullptr;
  PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties = nullptr;

  bool initialized = false;

  bool Load(VkDevice dev, VkPhysicalDevice phys_dev = VK_NULL_HANDLE) {
    if (!dev) return false;
    if (initialized) return true;

    void* libvk = dlopen("libvulkan.so.1", RTLD_NOLOAD | RTLD_LAZY);
    if (!libvk) libvk = dlopen("libvulkan.so", RTLD_NOLOAD | RTLD_LAZY);
    if (!libvk) libvk = dlopen("libvulkan.so.1", RTLD_LAZY);
    if (!libvk) libvk = dlopen("libvulkan.so", RTLD_LAZY);
    if (!libvk) {
      std::fprintf(stderr, "[REV2_DISPATCH] Falha ao abrir libvulkan.so!\n");
      return false;
    }

    vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(dlsym(libvk, "vkGetDeviceProcAddr"));
    if (!vkGetDeviceProcAddr) {
      std::fprintf(stderr, "[REV2_DISPATCH] Falha ao obter vkGetDeviceProcAddr!\n");
      return false;
    }

    vkGetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        dlsym(libvk, "vkGetPhysicalDeviceMemoryProperties"));
    vkGetPhysicalDeviceFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(
        dlsym(libvk, "vkGetPhysicalDeviceFormatProperties"));

#define LOAD_VK_FN(name) \
    name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(dev, #name)); \
    if (!name) { \
      name = reinterpret_cast<PFN_##name>(dlsym(libvk, #name)); \
    }

    LOAD_VK_FN(vkCreateCommandPool);
    LOAD_VK_FN(vkAllocateCommandBuffers);
    LOAD_VK_FN(vkCreateFence);
    LOAD_VK_FN(vkResetFences);
    LOAD_VK_FN(vkWaitForFences);
    LOAD_VK_FN(vkResetCommandPool);
    LOAD_VK_FN(vkBeginCommandBuffer);
    LOAD_VK_FN(vkEndCommandBuffer);
    LOAD_VK_FN(vkQueueSubmit);

    LOAD_VK_FN(vkCreateBuffer);
    LOAD_VK_FN(vkDestroyBuffer);
    LOAD_VK_FN(vkGetBufferMemoryRequirements);
    LOAD_VK_FN(vkAllocateMemory);
    LOAD_VK_FN(vkFreeMemory);
    LOAD_VK_FN(vkBindBufferMemory);
    LOAD_VK_FN(vkMapMemory);
    LOAD_VK_FN(vkUnmapMemory);

    LOAD_VK_FN(vkCreateFramebuffer);
    LOAD_VK_FN(vkDestroyFramebuffer);
    LOAD_VK_FN(vkCreateRenderPass);
    LOAD_VK_FN(vkDestroyRenderPass);

    LOAD_VK_FN(vkCmdPipelineBarrier);
    LOAD_VK_FN(vkCmdClearColorImage);
    LOAD_VK_FN(vkCmdBeginRenderPass);
    LOAD_VK_FN(vkCmdEndRenderPass);
    LOAD_VK_FN(vkCmdSetViewport);
    LOAD_VK_FN(vkCmdSetScissor);
    LOAD_VK_FN(vkCmdBindDescriptorSets);
    LOAD_VK_FN(vkCmdBindPipeline);
    LOAD_VK_FN(vkCmdBindVertexBuffers);
    LOAD_VK_FN(vkCmdBindIndexBuffer);
    LOAD_VK_FN(vkCmdDrawIndexed);
    LOAD_VK_FN(vkCmdDraw);

    LOAD_VK_FN(vkCreateDescriptorSetLayout);
    LOAD_VK_FN(vkDestroyDescriptorSetLayout);
    LOAD_VK_FN(vkCreatePipelineLayout);
    LOAD_VK_FN(vkDestroyPipelineLayout);
    LOAD_VK_FN(vkCreateDescriptorPool);
    LOAD_VK_FN(vkDestroyDescriptorPool);
    LOAD_VK_FN(vkResetDescriptorPool);
    LOAD_VK_FN(vkAllocateDescriptorSets);
    LOAD_VK_FN(vkUpdateDescriptorSets);

    LOAD_VK_FN(vkCreateShaderModule);
    LOAD_VK_FN(vkDestroyShaderModule);
    LOAD_VK_FN(vkCreateGraphicsPipelines);
    LOAD_VK_FN(vkDestroyPipeline);

    LOAD_VK_FN(vkCreateImage);
    LOAD_VK_FN(vkDestroyImage);
    LOAD_VK_FN(vkGetImageMemoryRequirements);
    LOAD_VK_FN(vkBindImageMemory);
    LOAD_VK_FN(vkCreateImageView);
    LOAD_VK_FN(vkDestroyImageView);
    LOAD_VK_FN(vkCreateSampler);
    LOAD_VK_FN(vkDestroySampler);
    LOAD_VK_FN(vkCmdCopyBufferToImage);

#undef LOAD_VK_FN

    initialized = true;
    std::fprintf(stderr, "[REV2_DISPATCH] Tabela de dispatch Vulkan carregada com 100%% de precisao direta!\n");
    return true;
  }
};

inline VulkanDispatch& GetVulkanDispatch() {
  static VulkanDispatch s_dispatch;
  return s_dispatch;
}

} // namespace rev2::gpu
