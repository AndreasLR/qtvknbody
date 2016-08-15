/*
 * Some defines, functions, structs, and a helper class that are used commonly throughout the project
 * */

#ifndef COMMON_H
#define COMMON_H

#include <QDebug>
#include <QFile>
#include <QMessageBox>

#include <cstring>
#include "BUILD_OPTIONS.h"
#include "platform.hpp"

// Readability defines
#define VERTEX_BUFFER_BIND_ID      0
#define INSTANCE_BUFFER_BIND_ID    1

// Debug functions
#define HANDLE_VK_RESULT(result) \
    VulkanHandleResult((result), # result, __LINE__, __FILE__);

void VulkanHandleResult(VkResult result, const char *argument, size_t line, const char *file);

#define VERIFY_FUNCTION_POINTER(function_ptr)                                                                                                 \
    if (function_ptr == nullptr)                                                                                                              \
    {                                                                                                                                         \
        QString msg = "Line " + QString::number(__LINE__) + " (" + __FILE__ + ") Invalid function pointer (" + QString(# function_ptr) + ")"; \
        qFatal(msg.toStdString().c_str());                                                                                                    \
    }

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugReportCallback(
    VkDebugReportFlagsEXT      flags,
    VkDebugReportObjectTypeEXT objectType,
    uint64_t                   object,
    size_t                     location,
    int32_t                    messageCode,
    const char                 *pLayerPrefix,
    const char                 *pMessage,
    void                       *pUserData);

// Helper class
class VulkanHelper
{
public:
    VulkanHelper(VkPhysicalDevice                 physicalDevice,
                 VkDevice                         device,
                 VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties);

    void createBuffer(VkBufferUsageFlags    usageFlags,
                      VkMemoryPropertyFlags memoryPropertyFlags,
                      VkDeviceSize          size,
                      void                  *data,
                      VkBuffer              *buffer,
                      VkDeviceMemory        *memory);

    void createBuffer(VkBufferUsageFlags     usageFlags,
                      VkMemoryPropertyFlags  memoryPropertyFlags,
                      VkDeviceSize           size,
                      void                   *data,
                      VkBuffer               *buffer,
                      VkDeviceMemory         *memory,
                      VkDescriptorBufferInfo *descriptor)
    {
        createBuffer(usageFlags, memoryPropertyFlags, size, data, buffer, memory);
        descriptor->offset = 0;
        descriptor->buffer = *buffer;
        descriptor->range  = size;
    }

    void createBuffer(VkBufferUsageFlags     usageFlags,
                      VkDeviceSize           size,
                      void                   *data,
                      VkBuffer               *buffer,
                      VkDeviceMemory         *memory,
                      VkDescriptorBufferInfo *descriptor)
    {
        createBuffer(usageFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, size, data, buffer, memory);
        descriptor->offset = 0;
        descriptor->buffer = *buffer;
        descriptor->range  = size;
    }

    uint32_t memoryTypeIndex(uint32_t typeBits, VkFlags properties);

    VkShaderModule createVulkanShaderModule(QString path);
    void destroyVulkanShaderModule(VkShaderModule& shader_module);

private:
    VkPhysicalDevice                 physicalDevice;
    VkDevice                         device;
    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
};


// Convenience structs
struct StandaloneImage
{
    VkImage        image;
    VkDeviceMemory mem;
    VkImageView    view;
};

struct VertexCollection
{
    VkBuffer                                       buffer;
    VkDeviceMemory                                 memory;
    VkPipelineVertexInputStateCreateInfo           inputState;
    std::vector<VkVertexInputBindingDescription>   bindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
};

struct IndexCollection
{
    VkBuffer       buffer;
    VkDeviceMemory memory;
    int            count;
};

struct UniformData
{
    VkBuffer               buffer;
    VkDeviceMemory         memory;
    VkDescriptorBufferInfo descriptor;
    void                   *mapped = nullptr;
};

struct ViewMatrixCollection
{
    float projectionMatrix[16];
    float modelMatrix[16];
    float viewMatrix[16];
};
#endif // COMMON_H
