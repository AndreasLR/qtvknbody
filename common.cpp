#include "common.hpp"

typedef enum AnomalyAction
{
    ACTION_DEBUG   = 0,
    ACTION_WARNING = 1,
    ACTION_EXIT    = 2,
    ACTION_NONE    = -1,
} AnomalyAction;

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugReportCallback(
    VkDebugReportFlagsEXT      flags,
    VkDebugReportObjectTypeEXT objectType,
    uint64_t                   object,
    size_t                     location,
    int32_t                    messageCode,
    const char                 *pLayerPrefix,
    const char                 *pMessage,
    void                       *pUserData)
{
    AnomalyAction action = ACTION_NONE;
    QString       type;

    switch (flags)
    {
    case VK_DEBUG_REPORT_INFORMATION_BIT_EXT:
       {
           type   = "INFO";
           action = ACTION_DEBUG;
           break;
       }

    case VK_DEBUG_REPORT_WARNING_BIT_EXT:
       {
           type   = "WARNING";
           action = ACTION_WARNING;
           break;
       }

    case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT:
       {
           type   = "PERFORMANCE";
           action = ACTION_WARNING;
           break;
       }

    case VK_DEBUG_REPORT_ERROR_BIT_EXT:
       {
           type   = "ERROR";
           action = ACTION_EXIT;
           break;
       }

    case VK_DEBUG_REPORT_DEBUG_BIT_EXT:
       {
           type   = "DEBUG";
           action = ACTION_DEBUG;
           break;
       }

    case VK_DEBUG_REPORT_FLAG_BITS_MAX_ENUM_EXT:
       {
           type   = "FLAG";
           action = ACTION_DEBUG;
           break;
       }

    default:
       {
           type = "Unknown callback type (" + QString::number(flags) + ")";
           break;
       }
    }

    QString msg = type + " [" + QString(pLayerPrefix) + "] " + QString(pMessage) + " (" + QString::number(messageCode) + ")";

    switch (action)
    {
    case ACTION_DEBUG:
       {
           qDebug(msg.toStdString().c_str());
           break;
       }

    case ACTION_WARNING:
       {
           qWarning(msg.toStdString().c_str());
           break;
       }

    case ACTION_EXIT:
       {
           qWarning(msg.toStdString().c_str());
           break;
       }

    default:
       {
           break;
       }
    }

    //    if (flags == VK_DEBUG_REPORT_ERROR_BIT_EXT)
    //    {
    //        QMessageBox msg_box;
    //        msg_box.setIcon (QMessageBox::Critical);
    //        msg_box.setText ("Vulkan error");
    //        msg_box.setInformativeText (msg);
    //        msg_box.exec();
    //    }
    return VK_FALSE;
}


VulkanHelper::VulkanHelper(VkPhysicalDevice physicalDevice, VkDevice device, VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties)
{
    this->physicalDevice = physicalDevice;
    this->device         = device;
    this->physicalDeviceMemoryProperties = physicalDeviceMemoryProperties;
}


VkShaderModule VulkanHelper::createVulkanShaderModule(QString path)
{
    QFile   file(path);
    QString msg = "Could not open shader file " + path;

    if (!file.open(QIODevice::ReadOnly))
    {
        qFatal(msg.toStdString().c_str());
    }
    QByteArray blob = file.readAll();
    file.close();

    VkShaderModuleCreateInfo shader_module_create_info = {};
    shader_module_create_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_create_info.pNext    = nullptr;
    shader_module_create_info.flags    = 0;
    shader_module_create_info.codeSize = blob.size();
    shader_module_create_info.pCode    = reinterpret_cast<const uint32_t *> (&blob.data()[0]);

    VkShaderModule shader_module;
    HANDLE_VK_RESULT(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &shader_module));
    return shader_module;
}


void VulkanHelper::destroyVulkanShaderModule(VkShaderModule& shader_module)
{
    vkDestroyShaderModule(device, shader_module, nullptr);
}


void VulkanHelper::setImageLayout(
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


void VulkanHelper::setImageLayout(
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


void VulkanHelper::createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, void *data, VkBuffer *buffer, VkDeviceMemory *memory)
{
    VkBufferCreateInfo bufferCreateInfo = {};

    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.pNext = nullptr;
    bufferCreateInfo.usage = usageFlags;
    bufferCreateInfo.size  = size;
    bufferCreateInfo.flags = 0;

    HANDLE_VK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer));

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, *buffer, &memReqs);

    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.pNext           = nullptr;
    memAllocInfo.allocationSize  = memReqs.size;
    memAllocInfo.memoryTypeIndex = memoryTypeIndex(memReqs.memoryTypeBits, memoryPropertyFlags);
    HANDLE_VK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, memory));
    if (data != nullptr)
    {
        void *mapped;
        HANDLE_VK_RESULT(vkMapMemory(device, *memory, 0, size, 0, &mapped));
        std::memcpy(mapped, data, size);
        vkUnmapMemory(device, *memory);
    }
    HANDLE_VK_RESULT(vkBindBufferMemory(device, *buffer, *memory, 0));
}


uint32_t VulkanHelper::memoryTypeIndex(uint32_t typeBits, VkFlags properties)
{
    for (uint32_t i = 0; i < 32; i++)
    {
        if ((typeBits & 1) == 1)
        {
            if ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }
        typeBits >>= 1;
    }

    qFatal("Could not find index of requested memory type");
    return 0;
}


#if BUILD_ENABLE_VULKAN_RUNTIME_DEBUG

void VulkanHandleResult(VkResult result, const char *argument, size_t line, const char *file)
{
    if (result == VK_SUCCESS)
    {
        return;
    }

    AnomalyAction action = ACTION_EXIT;
    QString       cause;
    switch (result)
    {
    case VK_SUCCESS:
       {
           cause  = "VK_SUCCESS";
           action = ACTION_NONE;
           break;
       }

    case VK_NOT_READY:
       {
           cause  = "VK_NOT_READY";
           action = ACTION_DEBUG;
           break;
       }

    case VK_TIMEOUT:
       {
           cause  = "VK_TIMEOUT";
           action = ACTION_DEBUG;
           break;
       }

    case VK_EVENT_SET:
       {
           cause  = "VK_EVENT_SET";
           action = ACTION_DEBUG;
           break;
       }

    case VK_EVENT_RESET:
       {
           cause  = "VK_EVENT_RESET";
           action = ACTION_DEBUG;
           break;
       }

    case VK_INCOMPLETE:
       {
           cause  = "VK_INCOMPLETE";
           action = ACTION_DEBUG;
           break;
       }

    case VK_SUBOPTIMAL_KHR:
       {
           cause  = "VK_SUBOPTIMAL_KHR";
           action = ACTION_WARNING;
           break;
       }

    case VK_ERROR_OUT_OF_HOST_MEMORY:
       {
           cause = "VK_ERROR_OUT_OF_HOST_MEMORY";
           break;
       }

    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
       {
           cause = "VK_ERROR_OUT_OF_DEVICE_MEMORY";
           break;
       }

    case VK_ERROR_INITIALIZATION_FAILED:
       {
           cause = "VK_ERROR_INITIALIZATION_FAILED";
           break;
       }

    case VK_ERROR_DEVICE_LOST:
       {
           cause = "VK_ERROR_DEVICE_LOST";
           break;
       }

    case VK_ERROR_MEMORY_MAP_FAILED:
       {
           cause = "VK_ERROR_MEMORY_MAP_FAILED";
           break;
       }

    case VK_ERROR_LAYER_NOT_PRESENT:
       {
           cause = "VK_ERROR_LAYER_NOT_PRESENT";
           break;
       }

    case VK_ERROR_EXTENSION_NOT_PRESENT:
       {
           cause = "VK_ERROR_EXTENSION_NOT_PRESENT";
           break;
       }

    case VK_ERROR_FEATURE_NOT_PRESENT:
       {
           cause = "VK_ERROR_FEATURE_NOT_PRESENT";
           break;
       }

    case VK_ERROR_INCOMPATIBLE_DRIVER:
       {
           cause = "VK_ERROR_INCOMPATIBLE_DRIVER";
           break;
       }

    case VK_ERROR_TOO_MANY_OBJECTS:
       {
           cause = "VK_ERROR_TOO_MANY_OBJECTS";
           break;
       }

    case VK_ERROR_FORMAT_NOT_SUPPORTED:
       {
           cause = "VK_ERROR_FORMAT_NOT_SUPPORTED";
           break;
       }

    case VK_ERROR_SURFACE_LOST_KHR:
       {
           cause = "VK_ERROR_SURFACE_LOST_KHR";
           break;
       }

    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
       {
           cause = "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
           break;
       }

    case VK_ERROR_OUT_OF_DATE_KHR:
       {
           cause = "VK_ERROR_OUT_OF_DATE_KHR";
           break;
       }

    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
       {
           cause = "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
           break;
       }

    case VK_ERROR_VALIDATION_FAILED_EXT:
       {
           cause = "VK_ERROR_VALIDATION_FAILED_EXT";
           break;
       }

    case VK_ERROR_INVALID_SHADER_NV:
       {
           cause = "VK_ERROR_INVALID_SHADER_NV";
           break;
       }

    default:
       {
           cause = "Unknown cause (" + QString::number(result) + ")";
           break;
       }
    }
    QString msg = "VK_RESULT [line " + QString::number(line) + " in " + file + "] " + cause + " (" + argument + ")";

    switch (action)
    {
    case ACTION_DEBUG:
       {
           qDebug(msg.toStdString().c_str());
           break;
       }

    case ACTION_WARNING:
       {
           qWarning(msg.toStdString().c_str());
           break;
       }

    case ACTION_EXIT:
       {
           qFatal(msg.toStdString().c_str());
           break;
       }

    default:
       {
           break;
       }
    }
}


#else

void VulkanHandleResult(VkResult result, const char *argument, size_t line, const char *file)
{
}
#endif // BUILD_ENABLE_VULKAN_RUNTIME_DEBUG
