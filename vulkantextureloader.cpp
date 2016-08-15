#include "vulkantextureloader.hpp"

VulkanTextureLoader::VulkanTextureLoader(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, VkCommandPool cmdPool)
{
    this->physicalDevice = physicalDevice;
    this->device         = device;
    this->queue          = queue;
    this->cmdPool        = cmdPool;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);
    vulkan_helper = new VulkanHelper(physicalDevice, device, deviceMemoryProperties);

    // Create command buffer for submitting image barriers
    // and converting tilings
    VkCommandBufferAllocateInfo cmdBufInfo = {};
    cmdBufInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufInfo.commandPool        = cmdPool;
    cmdBufInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufInfo.commandBufferCount = 1;

    HANDLE_VK_RESULT(vkAllocateCommandBuffers(device, &cmdBufInfo, &command_buffer));
}


VulkanTextureLoader::~VulkanTextureLoader()
{
    vkFreeCommandBuffers(device, cmdPool, 1, &command_buffer);
    delete vulkan_helper;
}

void VulkanTextureLoader::setImageLayout(
    VkCommandBuffer    cmdbuffer,
    VkImage            image,
    VkImageAspectFlags aspectMask,
    VkImageLayout      oldImageLayout,
    VkImageLayout      newImageLayout)
{
    VkImageSubresourceRange subresourceRange = {};

    subresourceRange.aspectMask   = aspectMask;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount   = 1;
    subresourceRange.layerCount   = 1;
    setImageLayout(cmdbuffer, image, oldImageLayout, newImageLayout, subresourceRange);
}


void VulkanTextureLoader::setImageLayout(
    VkCommandBuffer         cmdbuffer,
    VkImage                 image,
    VkImageLayout           oldImageLayout,
    VkImageLayout           newImageLayout,
    VkImageSubresourceRange subresourceRange)
{
    // Create an image barrier object
    VkImageMemoryBarrier imageMemoryBarrier = {};

    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.pNext = nullptr;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.oldLayout           = oldImageLayout;
    imageMemoryBarrier.newLayout           = newImageLayout;
    imageMemoryBarrier.image            = image;
    imageMemoryBarrier.subresourceRange = subresourceRange;

    switch (oldImageLayout)
    {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        imageMemoryBarrier.srcAccessMask = 0;
        break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    }

    switch (newImageLayout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        imageMemoryBarrier.srcAccessMask = imageMemoryBarrier.srcAccessMask | VK_ACCESS_TRANSFER_READ_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        if (imageMemoryBarrier.srcAccessMask == 0)
        {
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    }

    vkCmdPipelineBarrier(
        cmdbuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);
}

// Load a 2D texture
void VulkanTextureLoader::loadTexture(std::string filename, VkFormat format, VulkanTexture *texture, VkImageUsageFlags imageUsageFlags, VkSamplerAddressMode sampler_address_mode)
{
    gli::texture2D tex2D(gli::load(filename.c_str()));
    if (tex2D.empty())
    {
        qFatal("Could not load texture file");
    }

    texture->width     = static_cast<uint32_t>(tex2D[0].dimensions().x);
    texture->height    = static_cast<uint32_t>(tex2D[0].dimensions().y);
    texture->mipLevels = static_cast<uint32_t>(tex2D.levels());

    // Get device properites for the requested texture format
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);

    // Only use linear tiling if requested (and supported by the device)
    // Support for linear tiling is mostly limited, so prefer to use
    // optimal tiling instead
    // On most implementations linear tiling will only support a very
    // limited amount of formats and features (mip maps, cubemaps, arrays, etc.)
    VkBool32 useStaging = true;

    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.pNext           = nullptr;
    memAllocInfo.allocationSize  = 0;
    memAllocInfo.memoryTypeIndex = 0;
    VkMemoryRequirements memReqs;

    // Use a separate command buffer for texture loading
    VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
    cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufferBeginInfo.pNext = nullptr;
    HANDLE_VK_RESULT(vkBeginCommandBuffer(command_buffer, &cmdBufferBeginInfo));

    {
        // Create a host-visible staging buffer that contains the raw image data
        VkBuffer       stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size  = tex2D.size();
        // This buffer is used as a transfer source for the buffer copy
        bufferCreateInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        HANDLE_VK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &stagingBuffer));

        // Get memory requirements for the staging buffer (alignment, memory type bits)
        vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;
        // Get memory type index for a host visible buffer
        memAllocInfo.memoryTypeIndex = vulkan_helper->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        HANDLE_VK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &stagingMemory));
        HANDLE_VK_RESULT(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0));

        // Copy texture data into staging buffer
        uint8_t *data;
        HANDLE_VK_RESULT(vkMapMemory(device, stagingMemory, 0, memReqs.size, 0, (void **)&data));
        memcpy(data, tex2D.data(), tex2D.size());
        vkUnmapMemory(device, stagingMemory);

        // Setup buffer copy regions for each mip level
        std::vector<VkBufferImageCopy> bufferCopyRegions;
        uint32_t offset = 0;

        for (uint32_t i = 0; i < texture->mipLevels; i++)
        {
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel       = i;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount     = 1;
            bufferCopyRegion.imageExtent.width  = static_cast<uint32_t>(tex2D[i].dimensions().x);
            bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(tex2D[i].dimensions().y);
            bufferCopyRegion.imageExtent.depth  = 1;
            bufferCopyRegion.bufferOffset       = offset;

            bufferCopyRegions.push_back(bufferCopyRegion);

            offset += static_cast<uint32_t>(tex2D[i].size());
        }

        // Create optimal tiled target image
        VkImageCreateInfo imageCreateInfo = {};
        imageCreateInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.pNext         = nullptr;
        imageCreateInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format        = format;
        imageCreateInfo.mipLevels     = texture->mipLevels;
        imageCreateInfo.arrayLayers   = 1;
        imageCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage         = imageUsageFlags;
        imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent        = { texture->width, texture->height, 1 };
        imageCreateInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // Eeeh, override?

        HANDLE_VK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &texture->image));

        vkGetImageMemoryRequirements(device, texture->image, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;

        memAllocInfo.memoryTypeIndex = vulkan_helper->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        HANDLE_VK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &texture->deviceMemory));
        HANDLE_VK_RESULT(vkBindImageMemory(device, texture->image, texture->deviceMemory, 0));

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount   = texture->mipLevels;
        subresourceRange.layerCount   = 1;

        // Image barrier for optimal image (target)
        // Optimal image will be used as destination for the copy
        setImageLayout(
            command_buffer,
            texture->image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);

        // Copy mip levels from staging buffer
        vkCmdCopyBufferToImage(
            command_buffer,
            stagingBuffer,
            texture->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(bufferCopyRegions.size()),
            bufferCopyRegions.data()
            );

        // Change texture image layout to shader read after all mip levels have been copied
        texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        setImageLayout(
            command_buffer,
            texture->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            texture->imageLayout,
            subresourceRange);

        // Submit command buffer containing copy and image layout commands
        HANDLE_VK_RESULT(vkEndCommandBuffer(command_buffer));

        // Create a fence to make sure that the copies have finished before continuing
        VkFence           copyFence;
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FLAGS_NONE;
        HANDLE_VK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &copyFence));

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &command_buffer;

        HANDLE_VK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, copyFence));

        HANDLE_VK_RESULT(vkWaitForFences(device, 1, &copyFence, VK_TRUE, VK_DEFAULT_FENCE_TIMEOUT));

        vkDestroyFence(device, copyFence, nullptr);

        // Clean up staging resources
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
    }

    // Create sampler
    VkSamplerCreateInfo sampler = {};
    sampler.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter    = VK_FILTER_LINEAR;
    sampler.minFilter    = VK_FILTER_LINEAR;
    sampler.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU = sampler_address_mode;
    sampler.addressModeV = sampler_address_mode;
    sampler.addressModeW = sampler_address_mode;
    sampler.mipLodBias   = 0.0f;
    sampler.compareOp    = VK_COMPARE_OP_NEVER;
    sampler.minLod       = 0.0f;
    // Max level-of-detail should match mip level count
    sampler.maxLod = (useStaging) ? (float)texture->mipLevels : 0.0f;
    // Enable anisotropic filtering
    sampler.maxAnisotropy    = 8;
    sampler.anisotropyEnable = VK_TRUE;
    sampler.borderColor      = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    HANDLE_VK_RESULT(vkCreateSampler(device, &sampler, nullptr, &texture->sampler));

    // Create image view
    // Textures are not directly accessed by the shaders and
    // are abstracted by image views containing additional
    // information and sub resource ranges
    VkImageViewCreateInfo view = {};
    view.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext            = nullptr;
    view.image            = VK_NULL_HANDLE;
    view.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    view.format           = format;
    view.components       = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    // Linear tiling usually won't support mip maps
    // Only set mip map count if optimal tiling is used
    view.subresourceRange.levelCount = (useStaging) ? texture->mipLevels : 1;
    view.image = texture->image;
    HANDLE_VK_RESULT(vkCreateImageView(device, &view, nullptr, &texture->view));

    // Fill descriptor image info that can be used for setting up descriptor sets
    texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    texture->descriptor.imageView   = texture->view;
    texture->descriptor.sampler     = texture->sampler;
}


// Clean up vulkan resources used by a texture object
void VulkanTextureLoader::destroyTexture(VulkanTexture texture)
{
    vkDestroyImageView(device, texture.view, nullptr);
    vkDestroyImage(device, texture.image, nullptr);
    vkDestroySampler(device, texture.sampler, nullptr);
    vkFreeMemory(device, texture.deviceMemory, nullptr);
}
