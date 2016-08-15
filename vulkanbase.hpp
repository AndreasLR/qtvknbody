#ifndef VULKANBASE_H
#define VULKANBASE_H

#include "BUILD_OPTIONS.h"
#include "platform.hpp"
#include "common.hpp"

class VulkanBase
{
public:
    VulkanBase();
    ~VulkanBase();

    VkInstance instance() const;
    VkPhysicalDevice physicalDevice() const;
    VkDevice device() const;
    VkQueue graphicsQueue() const;
    VkQueue computeQueue() const;
    uint32_t queueFamilyIndex() const;
    const VkPhysicalDeviceProperties&       physicalDeviceProperties() const;
    const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties() const;

private:
    // Debug
    void enableDebugLayersAndExtensions();
    void createDebugCallback();
    void destroyDebugCallback();

    void enableExtensions();
    void createInstance();
    void destroyInstance();
    void enumeratePhysicalDevices();
    void createLogicalDevice();
    void destroyLogicalDevice();

    QVector<const char *> instance_layers;
    QVector<const char *> instance_extensions;
    QVector<const char *> device_layers;
    QVector<const char *> device_extensions;

    // Instance
    VkInstance p_instance = VK_NULL_HANDLE;

    // Device
    VkPhysicalDevice                 physical_device            = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties       physical_device_properties = {};
    VkPhysicalDeviceMemoryProperties physical_device_memory_properties = {};
    VkDevice p_device           = VK_NULL_HANDLE;
    uint32_t queue_family_index = 0;

    // Queue
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue compute_queue  = VK_NULL_HANDLE;

    // Debug
    VkDebugReportCallbackEXT           vulkan_debug_report      = VK_NULL_HANDLE;
    VkDebugReportCallbackCreateInfoEXT vulkan_debug_report_info = {};
};

#endif // VULKANBASE_H
