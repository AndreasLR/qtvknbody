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
        vulkan_helper->setImageLayout(
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
        vulkan_helper->setImageLayout(
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


// Load a cubemap texture (single file)
void VulkanTextureLoader::loadCubemap(std::string filename, VkFormat format, VulkanTexture *texture)
{
    gli::textureCube texCube(gli::load(filename));
    assert(!texCube.empty());

    texture->width     = static_cast<uint32_t>(texCube.dimensions().x);
    texture->height    = static_cast<uint32_t>(texCube.dimensions().y);
    texture->mipLevels = static_cast<uint32_t>(texCube.levels());

    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.pNext           = nullptr;
    memAllocInfo.allocationSize  = 0;
    memAllocInfo.memoryTypeIndex = 0;
    VkMemoryRequirements memReqs;

    // Create a host-visible staging buffer that contains the raw image data
    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size  = texCube.size();
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
    memcpy(data, texCube.data(), texCube.size());
    vkUnmapMemory(device, stagingMemory);

    // Setup buffer copy regions for each face including all of it's miplevels
    std::vector<VkBufferImageCopy> bufferCopyRegions;
    size_t offset = 0;

    for (uint32_t face = 0; face < 6; face++)
    {
        for (uint32_t level = 0; level < texture->mipLevels; level++)
        {
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel       = level;
            bufferCopyRegion.imageSubresource.baseArrayLayer = face;
            bufferCopyRegion.imageSubresource.layerCount     = 1;
            bufferCopyRegion.imageExtent.width  = static_cast<uint32_t>(texCube[face][level].dimensions().x);
            bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(texCube[face][level].dimensions().y);
            bufferCopyRegion.imageExtent.depth  = 1;
            bufferCopyRegion.bufferOffset       = offset;

            bufferCopyRegions.push_back(bufferCopyRegion);

            // Increase offset into staging buffer for next level / face
            offset += texCube[face][level].size();
        }
    }

    // Create optimal tiled target image
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext         = nullptr;
    imageCreateInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format        = format;
    imageCreateInfo.mipLevels     = texture->mipLevels;
    imageCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent        = { texture->width, texture->height, 1 };
    imageCreateInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    // Cube faces count as array layers in Vulkan
    imageCreateInfo.arrayLayers = 6;
    // This flag is required for cube map images
    imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    HANDLE_VK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &texture->image));

    vkGetImageMemoryRequirements(device, texture->image, &memReqs);

    memAllocInfo.allocationSize  = memReqs.size;
    memAllocInfo.memoryTypeIndex = vulkan_helper->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    HANDLE_VK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &texture->deviceMemory));
    HANDLE_VK_RESULT(vkBindImageMemory(device, texture->image, texture->deviceMemory, 0));

    VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
    cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufferBeginInfo.pNext = nullptr;
    HANDLE_VK_RESULT(vkBeginCommandBuffer(command_buffer, &cmdBufferBeginInfo));

    // Image barrier for optimal image (target)
    // Set initial layout for all array layers (faces) of the optimal (target) tiled texture
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount   = texture->mipLevels;
    subresourceRange.layerCount   = 6;

    vulkan_helper->setImageLayout(
        command_buffer,
        texture->image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        subresourceRange);

    // Copy the cube map faces from the staging buffer to the optimal tiled image
    vkCmdCopyBufferToImage(
        command_buffer,
        stagingBuffer,
        texture->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<uint32_t>(bufferCopyRegions.size()),
        bufferCopyRegions.data());

    // Change texture image layout to shader read after all faces have been copied
    texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vulkan_helper->setImageLayout(
        command_buffer,
        texture->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        texture->imageLayout,
        subresourceRange);

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

    // Create sampler
    VkSamplerCreateInfo sampler = {};
    sampler.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.pNext         = nullptr;
    sampler.magFilter     = VK_FILTER_LINEAR;
    sampler.minFilter     = VK_FILTER_LINEAR;
    sampler.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeV  = sampler.addressModeU;
    sampler.addressModeW  = sampler.addressModeU;
    sampler.mipLodBias    = 0.0f;
    sampler.maxAnisotropy = 8;
    sampler.compareOp     = VK_COMPARE_OP_NEVER;
    sampler.minLod        = 0.0f;
    sampler.maxLod        = (float)texture->mipLevels;
    sampler.borderColor   = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    HANDLE_VK_RESULT(vkCreateSampler(device, &sampler, nullptr, &texture->sampler));

    // Create image view
    VkImageViewCreateInfo view = {};
    view.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext                       = nullptr;
    view.image                       = VK_NULL_HANDLE;
    view.viewType                    = VK_IMAGE_VIEW_TYPE_CUBE;
    view.format                      = format;
    view.components                  = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    view.subresourceRange            = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    view.subresourceRange.layerCount = 6;
    view.subresourceRange.levelCount = texture->mipLevels;
    view.image                       = texture->image;
    HANDLE_VK_RESULT(vkCreateImageView(device, &view, nullptr, &texture->view));

    // Clean up staging resources
    vkFreeMemory(device, stagingMemory, nullptr);
    vkDestroyBuffer(device, stagingBuffer, nullptr);

    // Fill descriptor image info that can be used for setting up descriptor sets
    texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    texture->descriptor.imageView   = texture->view;
    texture->descriptor.sampler     = texture->sampler;
}


// Load an array texture (single file)
void VulkanTextureLoader::loadTextureArray(std::string filename, VkFormat format, VulkanTexture *texture)
{
    gli::texture2DArray tex2DArray(gli::load(filename));
    assert(!tex2DArray.empty());

    texture->width      = static_cast<uint32_t>(tex2DArray.dimensions().x);
    texture->height     = static_cast<uint32_t>(tex2DArray.dimensions().y);
    texture->layerCount = static_cast<uint32_t>(tex2DArray.layers());
    texture->mipLevels  = static_cast<uint32_t>(tex2DArray.levels());

    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.pNext           = nullptr;
    memAllocInfo.allocationSize  = 0;
    memAllocInfo.memoryTypeIndex = 0;
    VkMemoryRequirements memReqs;

    // Create a host-visible staging buffer that contains the raw image data
    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size  = tex2DArray.size();
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
    memcpy(data, tex2DArray.data(), static_cast<size_t>(tex2DArray.size()));
    vkUnmapMemory(device, stagingMemory);

    // Setup buffer copy regions for each layer including all of it's miplevels
    std::vector<VkBufferImageCopy> bufferCopyRegions;
    size_t offset = 0;

    for (uint32_t layer = 0; layer < texture->layerCount; layer++)
    {
        for (uint32_t level = 0; level < texture->mipLevels; level++)
        {
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel       = level;
            bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
            bufferCopyRegion.imageSubresource.layerCount     = 1;
            bufferCopyRegion.imageExtent.width  = static_cast<uint32_t>(tex2DArray[layer][level].dimensions().x);
            bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(tex2DArray[layer][level].dimensions().y);
            bufferCopyRegion.imageExtent.depth  = 1;
            bufferCopyRegion.bufferOffset       = offset;

            bufferCopyRegions.push_back(bufferCopyRegion);

            // Increase offset into staging buffer for next level / face
            offset += tex2DArray[layer][level].size();
        }
    }

    // Create optimal tiled target image
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext         = nullptr;
    imageCreateInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format        = format;
    imageCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent        = { texture->width, texture->height, 1 };
    imageCreateInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.arrayLayers   = texture->layerCount;
    imageCreateInfo.mipLevels     = texture->mipLevels;

    HANDLE_VK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &texture->image));

    vkGetImageMemoryRequirements(device, texture->image, &memReqs);

    memAllocInfo.allocationSize  = memReqs.size;
    memAllocInfo.memoryTypeIndex = vulkan_helper->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    HANDLE_VK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &texture->deviceMemory));
    HANDLE_VK_RESULT(vkBindImageMemory(device, texture->image, texture->deviceMemory, 0));

    VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
    cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufferBeginInfo.pNext = nullptr;
    HANDLE_VK_RESULT(vkBeginCommandBuffer(command_buffer, &cmdBufferBeginInfo));

    // Image barrier for optimal image (target)
    // Set initial layout for all array layers (faces) of the optimal (target) tiled texture
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount   = texture->mipLevels;
    subresourceRange.layerCount   = texture->layerCount;

    vulkan_helper->setImageLayout(
        command_buffer,
        texture->image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        subresourceRange);

    // Copy the layers and mip levels from the staging buffer to the optimal tiled image
    vkCmdCopyBufferToImage(
        command_buffer,
        stagingBuffer,
        texture->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<uint32_t>(bufferCopyRegions.size()),
        bufferCopyRegions.data());

    // Change texture image layout to shader read after all faces have been copied
    texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vulkan_helper->setImageLayout(
        command_buffer,
        texture->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        texture->imageLayout,
        subresourceRange);

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

    // Create sampler
    VkSamplerCreateInfo sampler = {};
    sampler.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.pNext         = nullptr;
    sampler.magFilter     = VK_FILTER_LINEAR;
    sampler.minFilter     = VK_FILTER_LINEAR;
    sampler.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeV  = sampler.addressModeU;
    sampler.addressModeW  = sampler.addressModeU;
    sampler.mipLodBias    = 0.0f;
    sampler.maxAnisotropy = 8;
    sampler.compareOp     = VK_COMPARE_OP_NEVER;
    sampler.minLod        = 0.0f;
    sampler.maxLod        = (float)texture->mipLevels;
    sampler.borderColor   = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    HANDLE_VK_RESULT(vkCreateSampler(device, &sampler, nullptr, &texture->sampler));

    // Create image view
    VkImageViewCreateInfo view = {};
    view.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext                       = nullptr;
    view.image                       = VK_NULL_HANDLE;
    view.viewType                    = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    view.format                      = format;
    view.components                  = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    view.subresourceRange            = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    view.subresourceRange.layerCount = texture->layerCount;
    view.subresourceRange.levelCount = texture->mipLevels;
    view.image                       = texture->image;
    HANDLE_VK_RESULT(vkCreateImageView(device, &view, nullptr, &texture->view));

    // Clean up staging resources
    vkFreeMemory(device, stagingMemory, nullptr);
    vkDestroyBuffer(device, stagingBuffer, nullptr);

    // Fill descriptor image info that can be used for setting up descriptor sets
    texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    texture->descriptor.imageView   = texture->view;
    texture->descriptor.sampler     = texture->sampler;
}
