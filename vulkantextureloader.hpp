#ifndef VULKANTEXTURELOADER_H
#define VULKANTEXTURELOADER_H

/*
 * Texture loader for Vulkan
 *
 * Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include <khronos/vulkan/vulkan.h>
#include <gli/gli.hpp>
#include "common.hpp"

struct VulkanTexture
{
    VkSampler             sampler;
    VkImage               image;
    VkImageLayout         imageLayout;
    VkDeviceMemory        deviceMemory;
    VkImageView           view;
    uint32_t              width, height;
    uint32_t              mipLevels;
    uint32_t              layerCount;
    VkDescriptorImageInfo descriptor;
};

class VulkanTextureLoader
{
public:
    VulkanTextureLoader(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, VkCommandPool cmdPool);
    ~VulkanTextureLoader();

    // Load a 2D texture
    void loadTexture(std::string filename, VkFormat format, VulkanTexture *texture, VkImageUsageFlags imageUsageFlags, VkSamplerAddressMode sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    // Change layout of image
    void setImageLayout(
        VkCommandBuffer    cmdbuffer,
        VkImage            image,
        VkImageAspectFlags aspectMask,
        VkImageLayout      oldImageLayout,
        VkImageLayout      newImageLayout);

    void setImageLayout(VkCommandBuffer         cmdbuffer,
                        VkImage                 image,
                        VkImageLayout           oldImageLayout,
                        VkImageLayout           newImageLayout,
                        VkImageSubresourceRange subresourceRange);

    // Clean up vulkan resources used by a texture object
    void destroyTexture(VulkanTexture texture);

private:
    VulkanHelper *vulkan_helper;

    VkPhysicalDevice                 physicalDevice;
    VkDevice                         device;
    VkQueue                          queue;
    VkCommandBuffer                  command_buffer;
    VkCommandPool                    cmdPool;
    VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
};

#endif // VULKANTEXTURELOADER_H
