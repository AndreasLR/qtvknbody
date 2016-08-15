#include "vulkanbase.hpp"

VulkanBase::VulkanBase()
{
    enableDebugLayersAndExtensions();
    enableExtensions();
    createInstance();
    createDebugCallback();
    enumeratePhysicalDevices();
    createLogicalDevice();
}


#if BUILD_ENABLE_VULKAN_DEBUG

void VulkanBase::enableDebugLayersAndExtensions()
{
    // Specify debug report extension to be able to consume validation errors
    instance_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

    // Specify instance validation layers
    instance_layers.push_back("VK_LAYER_LUNARG_standard_validation");

    // Specify device validation layers
    device_layers.push_back("VK_LAYER_LUNARG_standard_validation");
}


PFN_vkCreateDebugReportCallbackEXT  fvkCreateDebugReportCallbackEXT  = nullptr;
PFN_vkDestroyDebugReportCallbackEXT fvkDestroyDebugReportCallbackEXT = nullptr;

void VulkanBase::createDebugCallback()
{
    // Load VK_EXT_debug_report entry points in debug builds
    fvkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT> (vkGetInstanceProcAddr(p_instance, "vkCreateDebugReportCallbackEXT"));
    VERIFY_FUNCTION_POINTER(fvkCreateDebugReportCallbackEXT);

    fvkDestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT> (vkGetInstanceProcAddr(p_instance, "vkDestroyDebugReportCallbackEXT"));
    VERIFY_FUNCTION_POINTER(fvkDestroyDebugReportCallbackEXT);

    // Register the callback
    HANDLE_VK_RESULT(fvkCreateDebugReportCallbackEXT(p_instance, &vulkan_debug_report_info, nullptr, &vulkan_debug_report));
}


void VulkanBase::destroyDebugCallback()
{
    fvkDestroyDebugReportCallbackEXT(p_instance, vulkan_debug_report, nullptr);
}


#else

void VulkanBase::enableDebugLayersAndExtensions()
{
}


void VulkanBase::createDebugCallback()
{
}


void VulkanBase::destroyDebugCallback()
{
}
#endif // BUILD_ENABLE_VULKAN_DEBUG

void VulkanBase::enableExtensions()
{
    // Enable extensions used in all build types
    instance_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

#if VK_USE_PLATFORM_WIN32_KHR
    instance_extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

#elif VK_USE_PLATFORM_XCB_KHR
    instance_extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
}


void VulkanBase::createInstance()
{
    // Setup callback creation information
    vulkan_debug_report_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    vulkan_debug_report_info.pNext = nullptr;
    vulkan_debug_report_info.flags =
        //            VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
        VK_DEBUG_REPORT_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_ERROR_BIT_EXT |
        //            VK_DEBUG_REPORT_DEBUG_BIT_EXT |
        //            VK_DEBUG_REPORT_FLAG_BITS_MAX_ENUM_EXT |
        0;
    vulkan_debug_report_info.pfnCallback = &VulkanDebugReportCallback;
    vulkan_debug_report_info.pUserData   = nullptr;

    // Enumerate instance layers and extensions
    QMap<QString, VkLayerProperties>     layer_map;
    QMap<QString, VkExtensionProperties> extension_map;
    {
        uint32_t layer_property_count = 0;
        HANDLE_VK_RESULT(vkEnumerateInstanceLayerProperties(&layer_property_count, nullptr));

        QVector<VkLayerProperties> layer_properties(layer_property_count);
        HANDLE_VK_RESULT(vkEnumerateInstanceLayerProperties(&layer_property_count, layer_properties.data()));

        uint32_t extension_property_count = 0;
        HANDLE_VK_RESULT(vkEnumerateInstanceExtensionProperties(nullptr, &extension_property_count, nullptr));

        QVector<VkExtensionProperties> extension_properties(extension_property_count);
        HANDLE_VK_RESULT(vkEnumerateInstanceExtensionProperties(nullptr, &extension_property_count, extension_properties.data()));

        for (auto & layer:layer_properties)
        {
            layer_map[QString(layer.layerName)] = layer;
        }
        for (auto & extension:extension_properties)
        {
            extension_map[QString(extension.extensionName)] = extension;
        }

#if BUILD_ENABLE_VULKAN_VERBOSE
        qDebug() << "Available Vulkan Instance Layers:";
        for (auto & layer:layer_properties)
        {
            qDebug() << "\t" << layer.layerName << "-" << layer.description;
        }
        qDebug() << "Available Vulkan Instance Extensions:";
        for (auto & extension:extension_properties)
        {
            qDebug() << "\t" << extension.extensionName;
        }
#endif
    }

    // Validate instance layers and extensions
    {
        // Validate instance layers
        QMutableVectorIterator<const char *> i(instance_layers);
        while (i.hasNext())
        {
            if (!layer_map.contains(QString(i.next())))
            {
                qDebug() << "Will not be able to load unsupported layer -" << i.value();
                i.remove();
            }
        }

        // Validate instance extensions
        QMutableVectorIterator<const char *> j(instance_extensions);
        while (j.hasNext())
        {
            if (!extension_map.contains(QString(j.next())))
            {
                qDebug() << "Will not be able to load unsupported extension -" << j.value();
                j.remove();
            }
        }
    }

    // Specify application information
    VkApplicationInfo app_info = {};
    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext              = nullptr;
    app_info.pApplicationName   = "Qt Vulkan";
    app_info.applicationVersion = 0;
    app_info.pEngineName        = "Qt Vulkan";
    app_info.engineVersion      = 0;
    app_info.apiVersion         = VK_API_VERSION_1_0;

    // Specify instance creation
    VkInstanceCreateInfo instance_info = {};
    instance_info.flags                   = 0;
    instance_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pNext                   = &vulkan_debug_report_info;
    instance_info.pApplicationInfo        = &app_info;
    instance_info.enabledLayerCount       = static_cast<uint32_t> (instance_layers.size());
    instance_info.ppEnabledLayerNames     = static_cast<const char *const *> (instance_layers.data());
    instance_info.enabledExtensionCount   = static_cast<uint32_t> (instance_extensions.size());
    instance_info.ppEnabledExtensionNames = static_cast<const char *const *> (instance_extensions.data());

    // Create Vulkan instance
    HANDLE_VK_RESULT(vkCreateInstance(&instance_info, nullptr, &p_instance));
}


void VulkanBase::destroyInstance()
{
    vkDestroyInstance(p_instance, nullptr);
    p_instance = VK_NULL_HANDLE;
}


void VulkanBase::enumeratePhysicalDevices()
{
    // Enumerate physical devices
    uint32_t physical_device_count = 0;

    HANDLE_VK_RESULT(vkEnumeratePhysicalDevices(p_instance, &physical_device_count, nullptr));

    if (physical_device_count == 0)
    {
        qFatal("No device with Vulkan support detected");
    }

    QVector<VkPhysicalDevice> physical_devices(physical_device_count);
    HANDLE_VK_RESULT(vkEnumeratePhysicalDevices(p_instance, &physical_device_count, physical_devices.data()));

    // Fetch physical device properties
    QMap<VkPhysicalDevice, VkPhysicalDeviceProperties> physical_device_map;
    for (auto & physical_device:physical_devices)
    {
        VkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties(physical_device, &properties);
        physical_device_map[physical_device] = properties;
    }

#if BUILD_ENABLE_VULKAN_VERBOSE
    // Print device properties
    QMapIterator<VkPhysicalDevice, VkPhysicalDeviceProperties> i(physical_device_map);
    qDebug() << "Available Vulkan Physical Devices:";
    while (i.hasNext())
    {
        i.next();
        qDebug() << "\t" << i.value().deviceName << "- Vulkan API Version" << VK_VER_MAJOR(i.value().apiVersion) << VK_VER_MINOR(i.value().apiVersion) << VK_VER_PATCH(i.value().apiVersion);
    }
#endif

    // Todo: physical device selection
    physical_device            = physical_devices[0];
    physical_device_properties = physical_device_map[physical_device];

    vkGetPhysicalDeviceMemoryProperties(physical_device, &physical_device_memory_properties);

    // Check if time stamps are supported
    if (physical_device_properties.limits.timestampComputeAndGraphics != VK_TRUE)
    {
        qDebug() << "Time stamps not supported.";
    }
}


void VulkanBase::createLogicalDevice()
{
    // Enumerate device layers and extensions
    QMap<QString, VkLayerProperties>     layer_map;
    QMap<QString, VkExtensionProperties> extension_map;
    {
        uint32_t layer_property_count = 0;
        HANDLE_VK_RESULT(vkEnumerateDeviceLayerProperties(physical_device, &layer_property_count, nullptr));

        QVector<VkLayerProperties> layer_properties(layer_property_count);
        HANDLE_VK_RESULT(vkEnumerateDeviceLayerProperties(physical_device, &layer_property_count, layer_properties.data()));

        uint32_t extension_property_count = 0;
        HANDLE_VK_RESULT(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_property_count, nullptr));

        QVector<VkExtensionProperties> extension_properties(extension_property_count);
        HANDLE_VK_RESULT(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_property_count, extension_properties.data()));

        for (auto & layer:layer_properties)
        {
            layer_map[QString(layer.layerName)] = layer;
        }
        for (auto & extension:extension_properties)
        {
            extension_map[QString(extension.extensionName)] = extension;
        }

#if BUILD_ENABLE_VULKAN_VERBOSE
        qDebug() << "Available Vulkan Device Layers:";
        for (auto & layer:layer_properties)
        {
            qDebug() << "\t" << layer.layerName << "-" << layer.description;
        }
        qDebug() << "Available Vulkan Device Extensions:";
        for (auto & extension:extension_properties)
        {
            qDebug() << "\t" << extension.extensionName;
        }
#endif
    }

    // Activate and validate device layers and extensions
    {
        // Enable device extensions used in all build types
        device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        // Validate device layers
        QMutableVectorIterator<const char *> i(device_layers);
        while (i.hasNext())
        {
            if (!layer_map.contains(QString(i.next())))
            {
                qDebug() << "Will not be able to load unsupported layer -" << i.value();
                i.remove();
            }
        }

        // Validate device extensions
        QMutableVectorIterator<const char *> j(device_extensions);
        while (j.hasNext())
        {
            if (!extension_map.contains(QString(j.next())))
            {
                qDebug() << "Will not be able to load unsupported  extension -" << j.value();
                j.remove();
            }
        }
    }

    // Get physical device queue family properties
    uint32_t family_property_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_property_count, nullptr);

    QVector<VkQueueFamilyProperties> family_properties(family_property_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_property_count, family_properties.data());

    // Validate and locate queue families for computation and graphics
    // Spec: If an implementation exposes any queue family that supports graphics operations,
    // at least one queue family of at least one physical device exposed by the implementation
    // must support both graphics and compute operations.
#if BUILD_ENABLE_VULKAN_VERBOSE
    for (auto j = 0; j < family_properties.size(); j++)
    {
        QString str;
        str = "Queue family " + QString::number(j) + " (" + QString::number(family_properties[j].queueCount) + " queues) [";
        if ((family_properties[j].queueFlags) & VK_QUEUE_GRAPHICS_BIT)
        {
            str += " VK_QUEUE_GRAPHICS_BIT";
        }
        if ((family_properties[j].queueFlags) & VK_QUEUE_COMPUTE_BIT)
        {
            str += " VK_QUEUE_COMPUTE_BIT";
        }
        if ((family_properties[j].queueFlags) & VK_QUEUE_TRANSFER_BIT)
        {
            str += " VK_QUEUE_TRANSFER_BIT";
        }
        if ((family_properties[j].queueFlags) & VK_QUEUE_SPARSE_BINDING_BIT)
        {
            str += " VK_QUEUE_SPARSE_BINDING_BIT";
        }
        str += " ]";

        qDebug() << str;
    }
#endif
    bool graphics_compute_queue_found = false;
    for (auto j = 0; j < family_properties.size(); j++)
    {
        if (family_properties[j].timestampValidBits == 0)
        {
            qDebug() << "Queue" << j << "does not support time stamps";
        }

        if (((family_properties[j].queueFlags) & (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) && (family_properties[j].queueCount >= 2))
        {
            graphics_compute_queue_found = true;
            queue_family_index           = j;
            break;
        }
    }
    if (!graphics_compute_queue_found)
    {
        qFatal("No suitable queue found");
    }

    // Specify device queue creation. Each device exposes a number of queue families each having one or more queues
    VkDeviceQueueCreateInfo device_queue_create_info = {};
    device_queue_create_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    device_queue_create_info.queueFamilyIndex = queue_family_index;
    QVector<float> queue_priorities = { 1.0f, 0.0f };
    device_queue_create_info.queueCount       = queue_priorities.size();
    device_queue_create_info.pQueuePriorities = queue_priorities.data();

    QVector<VkDeviceQueueCreateInfo> device_queue_info_list = { device_queue_create_info };

    // Specify device creation
    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.enabledLayerCount       = device_layers.size();
    device_create_info.ppEnabledLayerNames     = device_layers.data();
    device_create_info.enabledExtensionCount   = device_extensions.size();
    device_create_info.ppEnabledExtensionNames = device_extensions.data();
    device_create_info.queueCreateInfoCount    = device_queue_info_list.size();
    device_create_info.pQueueCreateInfos       = device_queue_info_list.data();
    device_create_info.pEnabledFeatures        = nullptr;

    // Create device
    HANDLE_VK_RESULT(vkCreateDevice(physical_device, &device_create_info, nullptr, &p_device));

    vkGetDeviceQueue(p_device, queue_family_index, 0, &graphics_queue);
    vkGetDeviceQueue(p_device, queue_family_index, 1, &compute_queue);
}


void VulkanBase::destroyLogicalDevice()
{
    vkDestroyDevice(p_device, nullptr);
    p_device = VK_NULL_HANDLE;
}


VulkanBase::~VulkanBase()
{
    destroyLogicalDevice();
    destroyDebugCallback();
    destroyInstance();
}


VkInstance VulkanBase::instance() const
{
    return p_instance;
}


VkPhysicalDevice VulkanBase::physicalDevice() const
{
    return physical_device;
}


VkDevice VulkanBase::device() const
{
    return p_device;
}


VkQueue VulkanBase::graphicsQueue() const
{
    return graphics_queue;
}


VkQueue VulkanBase::computeQueue() const
{
    return compute_queue;
}


uint32_t VulkanBase::queueFamilyIndex() const
{
    return queue_family_index;
}


const VkPhysicalDeviceProperties& VulkanBase::physicalDeviceProperties() const
{
    return physical_device_properties;
}


const VkPhysicalDeviceMemoryProperties& VulkanBase::physicalDeviceMemoryProperties() const
{
    return physical_device_memory_properties;
}
