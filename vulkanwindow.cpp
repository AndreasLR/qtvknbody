#include "vulkanwindow.hpp"

#include <QGuiApplication>

#ifdef VK_USE_PLATFORM_XCB_KHR
#include <QX11Info>
#endif

VulkanWindow::VulkanWindow()
{
    uptime.start();
}


VulkanWindow::~VulkanWindow()
{
    graphics_timer->stop();
    compute_timer->stop();

    HANDLE_VK_RESULT(vkQueueWaitIdle(vkbase.computeQueue()));
    HANDLE_VK_RESULT(vkQueueWaitIdle(vkbase.graphicsQueue()));

    vulkan_texture_loader->destroyTexture(texture_particle);
    vulkan_texture_loader->destroyTexture(texture_noise);

    delete vulkan_texture_loader;

    commandBuffersFree();
    descriptorPoolDestroy();
    pipelinesDestroy();
    pipelineLayoutsDestroy();
    descriptorSetLayoutsDestroy();

    frameBuffersDestroy();
    pipelineCacheDestroy();
    renderPassDestroy();
    depthStencilDestroy();
    swapChainImageViewsDestroy();
    swapChainDestroy();
    commandPoolDestroy();
    surfaceDestroy();
    semaphoresDestroy();
    fencesDestroy();

    vkDestroyBuffer(vkbase.device(), vertices_fullscreen.buffer, nullptr);
    vkFreeMemory(vkbase.device(), vertices_fullscreen.memory, nullptr);

    vkDestroyBuffer(vkbase.device(), vertices_corner.buffer, nullptr);
    vkFreeMemory(vkbase.device(), vertices_corner.memory, nullptr);

    vkDestroyBuffer(vkbase.device(), vertices_performance_meter_graphics.buffer, nullptr);
    vkFreeMemory(vkbase.device(), vertices_performance_meter_graphics.memory, nullptr);

    vkDestroyBuffer(vkbase.device(), vertices_performance_meter_compute.buffer, nullptr);
    vkFreeMemory(vkbase.device(), vertices_performance_meter_compute.memory, nullptr);

    vkDestroyBuffer(vkbase.device(), indices_quad.buffer, nullptr);
    vkFreeMemory(vkbase.device(), indices_quad.memory, nullptr);

    vkDestroyBuffer(vkbase.device(), uniform_nbody_graphics.buffer, nullptr);
    vkFreeMemory(vkbase.device(), uniform_nbody_graphics.memory, nullptr);

    vkDestroyBuffer(vkbase.device(), uniform_nbody_compute.buffer, nullptr);
    vkFreeMemory(vkbase.device(), uniform_nbody_compute.memory, nullptr);

    vkDestroyBuffer(vkbase.device(), uniform_blur.buffer, nullptr);
    vkFreeMemory(vkbase.device(), uniform_blur.memory, nullptr);

    vkDestroyBuffer(vkbase.device(), uniform_tone_mapping.buffer, nullptr);
    vkFreeMemory(vkbase.device(), uniform_tone_mapping.memory, nullptr);

    vkDestroyBuffer(vkbase.device(), uniform_performance_compute.buffer, nullptr);
    vkFreeMemory(vkbase.device(), uniform_performance_compute.memory, nullptr);

    vkDestroyBuffer(vkbase.device(), uniform_performance_graphics.buffer, nullptr);
    vkFreeMemory(vkbase.device(), uniform_performance_graphics.memory, nullptr);

    vkDestroyBuffer(vkbase.device(), buffer_nbody_compute.buffer, nullptr);
    vkFreeMemory(vkbase.device(), buffer_nbody_compute.memory, nullptr);

    vkDestroyBuffer(vkbase.device(), buffer_nbody_draw.buffer, nullptr);
    vkFreeMemory(vkbase.device(), buffer_nbody_draw.memory, nullptr);

    delete vulkan_helper;

    queryPoolDestroy();
}


void VulkanWindow::initialize()
{
    // Init time stamps
    query_timestamp_graphics.resize(2);
    query_timestamp_graphics_scene.resize(2);
    query_timestamp_graphics_brightness.resize(2);
    query_timestamp_graphics_blur_alpha.resize(2);
    query_timestamp_graphics_blur_beta.resize(2);
    query_timestamp_graphics_combine.resize(2);
    query_timestamp_graphics_tone_map.resize(2);
    query_timestamp_compute_leapfrog_step_1.resize(2);
    query_timestamp_compute_leapfrog_step_2.resize(2);

    // Initial values
    camera_matrix.setN(0.1);
    camera_matrix.setF(256.0);
    camera_matrix.setFov(60.0);
    camera_matrix.setProjection(true);

    rotation_origin_matrix.setIdentity(4);

    rotation_matrix.setIdentity(4);
    zoom_matrix.setIdentity(4);
    translation_matrix.setIdentity(4);
    translation_matrix[11] = -2.5;
    model_matrix.setIdentity(4);

    // Init Vulkan surface
    vulkan_helper = new VulkanHelper(vkbase.physicalDevice(), vkbase.device(), vkbase.physicalDeviceMemoryProperties());
    surfaceCreate();

    queryPoolCreate();

    ubo_nbody_graphics.fbo_size[0] = static_cast<float>(surface_capabilities.currentExtent.width);
    ubo_nbody_graphics.fbo_size[1] = static_cast<float>(surface_capabilities.currentExtent.height);

    // Init bloom framebuffer size
    framebuffer_size_blur_pass =
    {
        static_cast<uint32_t>(surface_capabilities.currentExtent.width * framebuffer_size_blur_pass_multiplier),
        static_cast<uint32_t>(surface_capabilities.currentExtent.height * framebuffer_size_blur_pass_multiplier)
    };

    // More Vulkan things
    commandPoolCreate();
    vulkan_texture_loader = new VulkanTextureLoader(vkbase.physicalDevice(), vkbase.device(), vkbase.graphicsQueue(), command_pool);
    swapChainCreate(VK_NULL_HANDLE);
    swapChainImageViewsCreate();
    commandBuffersAllocate();
    depthStencilCreate();
    renderPassesCreate();
    pipelineCacheCreate();
    frameBuffersCreate();

    vulkan_texture_loader->loadTexture(
        "textures/particle02_rgba.ktx",
        VK_FORMAT_R8G8B8A8_UNORM,
        &texture_particle,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

    vulkan_texture_loader->loadTexture(
        "textures/noise_texture_0002.ktx",
        VK_FORMAT_R8G8B8A8_UNORM,
        &texture_noise,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT);

    fencesCreate();
    semaphoresCreate();
    generateVerticesFullscreenQuad();
    generateVerticesPerformanceMeterGraphics();
    generateVerticesPerformanceMeterCompute();
    generateVerticesNbodyInstance();
    generateBuffersNbody();
    uniformBuffersPrepare();
    descriptorSetLayoutsCreate();
    pipelineLayoutsCreate();
    pipelinesCreate();
    descriptorPoolCreate();
    descriptorSetsAllocate();
    descriptorSetsUpdate();
    commandBuffersPresentRecord();
    commandBuffersComputeRecord();
    commandBuffersGraphicsRecord();


    // Update timer
    graphics_timer = new QTimer;
    graphics_timer->setInterval(1);
    connect(graphics_timer, &QTimer::timeout, this, &VulkanWindow::update);
    graphics_timer->start();

    compute_timer = new QTimer;
    compute_timer->setInterval(1);
    connect(compute_timer, &QTimer::timeout, this, &VulkanWindow::queueComputeSubmit);
    compute_timer->start();

    // Fps polling timer
    fps_update_timer.setInterval(50);
    connect(&fps_update_timer, SIGNAL(timeout()), this, SLOT(createFpsString()));
    fps_update_timer.start();
}


void VulkanWindow::setGravitationalConstant(double value)
{
    ubo_nbody_compute.gravity_constant = value;
    uniformBuffersUpdate();
}


void VulkanWindow::setSoftening(double value)
{
    ubo_nbody_compute.softening_squared = value;
    uniformBuffersUpdate();
}


void VulkanWindow::setTimeStep(double value)
{
    ubo_nbody_compute.time_step  = value;
    ubo_nbody_graphics.time_step = value;
    uniformBuffersUpdate();
}


void VulkanWindow::setBloomStrength(int value)
{
    ubo_blur.blur_strength = static_cast<float>(value) / 100.0;
    uniformBuffersUpdate();
}


void VulkanWindow::setBloomExtent(int value)
{
    ubo_blur.blur_extent = static_cast<float>(value) / 200.0;
    uniformBuffersUpdate();
}


void VulkanWindow::setParticleCount(int value)
{
    initialization_particle_count = value;
}


void VulkanWindow::setParticleSize(int value)
{
    ubo_nbody_graphics.particle_size = static_cast<float>(value);
    uniformBuffersUpdate();
}


void VulkanWindow::setPower(int value)
{
    ubo_nbody_compute.power = static_cast<float>(value) * 0.1;
    uniformBuffersUpdate();
}


void VulkanWindow::setInitialCondition(int value)
{
    initial_condition = value;
}


void VulkanWindow::launch()
{
    bool paused = true;

    if (compute_timer->isActive())
    {
        paused = false;
    }

    graphics_timer->stop();
    compute_timer->stop();

    HANDLE_VK_RESULT(vkQueueWaitIdle(vkbase.computeQueue()));
    HANDLE_VK_RESULT(vkQueueWaitIdle(vkbase.graphicsQueue()));

    descriptorPoolReset();

    vkDestroyBuffer(vkbase.device(), buffer_nbody_compute.buffer, nullptr);
    vkFreeMemory(vkbase.device(), buffer_nbody_compute.memory, nullptr);

    vkDestroyBuffer(vkbase.device(), buffer_nbody_draw.buffer, nullptr);
    vkFreeMemory(vkbase.device(), buffer_nbody_draw.memory, nullptr);

    generateBuffersNbody();
    descriptorSetsAllocate();
    descriptorSetsUpdate();
    commandBuffersComputeRecord();
    commandBuffersGraphicsRecord();

    graphics_timer->start();
    if (!paused)
    {
        compute_timer->start();
    }
}


void VulkanWindow::pauseCompute(bool value)
{
    if (value)
    {
        compute_timer->stop();
    }
    else
    {
        compute_timer->start();
    }
}


void VulkanWindow::pauseAll(bool value)
{
    if (value)
    {
        compute_timer->stop();
        graphics_timer->stop();

        HANDLE_VK_RESULT(vkDeviceWaitIdle(vkbase.device()));
    }
    else
    {
        compute_timer->start();
        graphics_timer->start();
    }
}


void VulkanWindow::setMouseSensitivity(int value)
{
    mouse_sensitivity = static_cast<double>(value * 0.01);
}


void VulkanWindow::setExposure(int value)
{
    ubo_tone_mapping.exposure = static_cast<float>(value) / 20.0;
    uniformBuffersUpdate();
}


void VulkanWindow::setGamma(int value)
{
    ubo_tone_mapping.gamma = static_cast<float>(value) / 30.0;
    uniformBuffersUpdate();
}


void VulkanWindow::setToneMappingMode(int value)
{
    ubo_tone_mapping.tone_mapping_method = value;
    uniformBuffersUpdate();
}


void VulkanWindow::update()
{
    queueGraphicsSubmit();
}


void VulkanWindow::createFpsString()
{
    double time_elapsed_graphics = 0;
    double time_elapsed_compute  = 0;

    for (int i = 0; i < p_fps_stack.size(); i++)
    {
        time_elapsed_graphics += p_fps_stack[i];
    }

    for (int i = 0; i < p_cps_stack.size(); i++)
    {
        time_elapsed_compute += p_cps_stack[i];
    }

    emit fpsStringChanged("Qt+Vulkan N-body simulation - [fps: " +
                          QString::number(static_cast<double> (p_fps_stack.size()) / (time_elapsed_graphics * 1.0e-9), 'f', 0) + " @ " +
                          QString("%1").arg(time_total_graphics / 1.0e6, -4, 'g', 3, QLatin1Char('0')) + " ms] - [cps: " +
                          QString::number(static_cast<double> (p_cps_stack.size()) / (time_elapsed_compute * 1.0e-9), 'f', 0) + " @ " +
                          QString("%1").arg(time_total_compute / 1.0e6, -4, 'g', 3, QLatin1Char('0')) + " ms]");
}


void VulkanWindow::queueComputeSubmit()
{
    // Submit the first compute step
    {
        // Ensure that the previous invocation has finished
        {
            VkResult result = vkGetFenceStatus(vkbase.device(), fence_compute_step_1);

            switch (result)
            {
            case VK_NOT_READY:
                // Wait for vertex buffer
                HANDLE_VK_RESULT(vkWaitForFences(vkbase.device(), 1, &fence_compute_step_1, VK_TRUE, 1e9));
                break;

            default:
                HANDLE_VK_RESULT(result);
                break;
            }
        }

        // Poll timers
        {
            VkResult result = vkGetQueryPoolResults(vkbase.device(), query_pool_compute, 0, 2, sizeof(QueryResult) * 2, query_timestamp_compute_leapfrog_step_1.data(), sizeof(QueryResult), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

            switch (result)
            {
            case VK_NOT_READY:
                break;

            default:
                HANDLE_VK_RESULT(result);

                float time_step_1 = static_cast<double>(query_timestamp_compute_leapfrog_step_1[1].time - query_timestamp_compute_leapfrog_step_1[0].time);

                ubo_performance_meter_compute.process_count = 2;
                ubo_performance_meter_compute.positions[0]  = time_step_1;

                break;
            }
        }

        HANDLE_VK_RESULT(vkResetFences(vkbase.device(), 1, &fence_compute_step_1));

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = nullptr;
        submit_info.commandBufferCount   = 1;
        submit_info.pCommandBuffers      = &command_buffer_compute_step_1;
        submit_info.pSignalSemaphores    = &semaphore_compute_step_1_complete;
        submit_info.signalSemaphoreCount = 1;

        HANDLE_VK_RESULT(vkQueueSubmit(vkbase.computeQueue(), 1, &submit_info, fence_compute_step_1));

        // Enable to enforce syncing with the graphics queue
        if (0)
        {
            vkQueueWaitIdle(vkbase.computeQueue());
        }
    }

    // Check fence to ensure vertex buffer is not being read from
    {
        VkResult result = vkGetFenceStatus(vkbase.device(), fence_draw);

        switch (result)
        {
        case VK_NOT_READY:
            // Wait for vertex buffer
            HANDLE_VK_RESULT(vkWaitForFences(vkbase.device(), 1, &fence_draw, VK_TRUE, 1e9));
            break;

        default:
            HANDLE_VK_RESULT(result);
            break;
        }

        // Poll timers
        {
            VkResult result = vkGetQueryPoolResults(vkbase.device(), query_pool_compute, 2, 2, sizeof(QueryResult) * 2, query_timestamp_compute_leapfrog_step_2.data(), sizeof(QueryResult), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

            switch (result)
            {
            case VK_NOT_READY:
                break;

            default:
                HANDLE_VK_RESULT(result);
                float time_step_2 = static_cast<double>(query_timestamp_compute_leapfrog_step_2[1].time - query_timestamp_compute_leapfrog_step_2[0].time);

                ubo_performance_meter_compute.positions[1] = time_step_2;

                float time_total = ubo_performance_meter_compute.positions[0] + ubo_performance_meter_compute.positions[1];
                ubo_performance_meter_compute.positions[0] /= time_total;
                ubo_performance_meter_compute.positions[1] /= time_total;
                ubo_performance_meter_compute.positions[2]  = time_total;
            }
        }

        // Computations per second (cps)
        p_cps_stack.enqueue(cps_timer.nsecsElapsed());
        if (p_cps_stack.size() > 100)
        {
            p_cps_stack.dequeue();
        }
        cps_timer.restart();
    }

    // Submit the second compute step and transfer operation
    {
        // Ensure that the previous invocation has finished
        {
            VkResult result = vkGetFenceStatus(vkbase.device(), fence_transfer);

            switch (result)
            {
            case VK_NOT_READY:
                // Wait for vertex buffer
                HANDLE_VK_RESULT(vkWaitForFences(vkbase.device(), 1, &fence_transfer, VK_TRUE, 1e9));
                break;

            default:
                HANDLE_VK_RESULT(result);
                break;
            }
        }

        HANDLE_VK_RESULT(vkResetFences(vkbase.device(), 1, &fence_transfer));

        VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        VkSubmitInfo         submit_info         = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = nullptr;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers    = &command_buffer_compute_step_2;
        submit_info.pWaitSemaphores    = &semaphore_compute_step_1_complete;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitDstStageMask  = &wait_dst_stage_mask;

        HANDLE_VK_RESULT(vkQueueSubmit(vkbase.computeQueue(), 1, &submit_info, fence_transfer));
    }
}


void VulkanWindow::focusOutEvent(QFocusEvent *ev)
{
    p_key_w_active     = false;
    p_key_a_active     = false;
    p_key_s_active     = false;
    p_key_d_active     = false;
    p_key_q_active     = false;
    p_key_e_active     = false;
    p_key_ctrl_active  = false;
    p_key_shift_active = false;
    p_key_space_active = false;
}


void VulkanWindow::queueGraphicsSubmit()
{
    // Update view matrices
    passiveMove();

    // Aquire the index of the currently active image
    uint32_t buffer_index;
    VkResult result = vkAcquireNextImageKHR(vkbase.device(), swapchain, UINT64_MAX, semaphore_present_complete, VK_NULL_HANDLE, &buffer_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        swapChainRecreate();
        return;
    }
    else
    {
        HANDLE_VK_RESULT(result);
    }

    // Submit post present cb if present complete
    {
        VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        VkSubmitInfo         info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.pNext = nullptr;
        info.commandBufferCount   = 1;
        info.pWaitDstStageMask    = &wait_dst_stage_mask;
        info.waitSemaphoreCount   = 1;
        info.pWaitSemaphores      = &semaphore_present_complete;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores    = &semaphore_post_present_complete;
        info.pCommandBuffers      = &command_buffer_post_present[buffer_index];

        HANDLE_VK_RESULT(vkQueueSubmit(vkbase.graphicsQueue(), 1, &info, VK_NULL_HANDLE));
    }

    // Submit the draw cb
    {
        // Check fence to ensure vertex data buffer is not being written to
        {
            VkResult result = vkGetFenceStatus(vkbase.device(), fence_transfer);

            switch (result)
            {
            case VK_NOT_READY:
                // Wait for vertex buffer
                HANDLE_VK_RESULT(vkWaitForFences(vkbase.device(), 1, &fence_transfer, VK_TRUE, 1e9));
                break;

            default:
                HANDLE_VK_RESULT(result);
                break;
            }
        }

        HANDLE_VK_RESULT(vkResetFences(vkbase.device(), 1, &fence_draw));

        VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkSubmitInfo         info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.pNext = nullptr;
        info.commandBufferCount   = 1;
        info.pWaitDstStageMask    = &wait_dst_stage_mask;
        info.waitSemaphoreCount   = 1;
        info.pWaitSemaphores      = &semaphore_post_present_complete;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores    = &semaphore_draw_complete;
        info.pCommandBuffers      = &command_buffer_draw[buffer_index];

        HANDLE_VK_RESULT(vkQueueSubmit(vkbase.graphicsQueue(), 1, &info, fence_draw));
    }
    // Submit pre present cb when draw cb complete
    {
        VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        VkSubmitInfo         info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.pNext = nullptr;
        info.commandBufferCount   = 1;
        info.pWaitDstStageMask    = &wait_dst_stage_mask;
        info.waitSemaphoreCount   = 1;
        info.pWaitSemaphores      = &semaphore_draw_complete;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores    = &semaphore_pre_present_complete;
        info.pCommandBuffers      = &command_buffer_pre_present[buffer_index];

        HANDLE_VK_RESULT(vkQueueSubmit(vkbase.graphicsQueue(), 1, &info, VK_NULL_HANDLE));
    }

    {
        // Present the current image to the swap chain
        VkPresentInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        info.pNext = nullptr;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores    = &semaphore_pre_present_complete;
        info.swapchainCount     = 1;
        info.pSwapchains        = &swapchain;
        info.pImageIndices      = &buffer_index;
        info.pResults           = nullptr;

        result = vkQueuePresentKHR(vkbase.graphicsQueue(), &info);
        if (result == VK_SUBOPTIMAL_KHR)
        {
            swapChainRecreate();
            return;
        }
        else
        {
            HANDLE_VK_RESULT(result);
        }
    }

    // Fps
    p_fps_stack.enqueue(fps_timer.nsecsElapsed());
    if (p_fps_stack.size() > 100)
    {
        p_fps_stack.dequeue();
    }
    fps_timer.restart();

    HANDLE_VK_RESULT(vkQueueWaitIdle(vkbase.graphicsQueue()));

    // Poll timers and submit performance metrics
    {
        HANDLE_VK_RESULT(vkGetQueryPoolResults(vkbase.device(), query_pool_graphics, 0, 2, sizeof(QueryResult) * 2, query_timestamp_graphics_scene.data(), sizeof(QueryResult), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
        HANDLE_VK_RESULT(vkGetQueryPoolResults(vkbase.device(), query_pool_graphics, 2, 2, sizeof(QueryResult) * 2, query_timestamp_graphics_brightness.data(), sizeof(QueryResult), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
        HANDLE_VK_RESULT(vkGetQueryPoolResults(vkbase.device(), query_pool_graphics, 4, 2, sizeof(QueryResult) * 2, query_timestamp_graphics_blur_alpha.data(), sizeof(QueryResult), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
        HANDLE_VK_RESULT(vkGetQueryPoolResults(vkbase.device(), query_pool_graphics, 6, 2, sizeof(QueryResult) * 2, query_timestamp_graphics_blur_beta.data(), sizeof(QueryResult), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
        HANDLE_VK_RESULT(vkGetQueryPoolResults(vkbase.device(), query_pool_graphics, 8, 2, sizeof(QueryResult) * 2, query_timestamp_graphics_combine.data(), sizeof(QueryResult), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
        HANDLE_VK_RESULT(vkGetQueryPoolResults(vkbase.device(), query_pool_graphics, 10, 2, sizeof(QueryResult) * 2, query_timestamp_graphics_tone_map.data(), sizeof(QueryResult), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
        HANDLE_VK_RESULT(vkGetQueryPoolResults(vkbase.device(), query_pool_graphics, 12, 2, sizeof(QueryResult) * 2, query_timestamp_graphics.data(), sizeof(QueryResult), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));

        float time_scene      = static_cast<double>(query_timestamp_graphics_scene[1].time - query_timestamp_graphics_scene[0].time);
        float time_brightness = static_cast<double>(query_timestamp_graphics_brightness[1].time - query_timestamp_graphics_brightness[0].time);
        float time_blur_alpha = static_cast<double>(query_timestamp_graphics_blur_alpha[1].time - query_timestamp_graphics_blur_alpha[0].time);
        float time_blur_beta  = static_cast<double>(query_timestamp_graphics_blur_beta[1].time - query_timestamp_graphics_blur_beta[0].time);
        float time_combine    = static_cast<double>(query_timestamp_graphics_combine[1].time - query_timestamp_graphics_combine[0].time);
        float time_tone_map   = static_cast<double>(query_timestamp_graphics_tone_map[1].time - query_timestamp_graphics_tone_map[0].time);
        time_total_graphics = static_cast<double>(query_timestamp_graphics[1].time - query_timestamp_graphics[0].time);
        float time_overhead = time_total_graphics - time_scene - time_brightness - time_blur_alpha - time_blur_beta - time_combine - time_tone_map;

        ubo_performance_meter_graphics.process_count = 7;
        ubo_performance_meter_graphics.positions[0]  = time_scene / time_total_graphics;
        ubo_performance_meter_graphics.positions[1]  = time_brightness / time_total_graphics;
        ubo_performance_meter_graphics.positions[2]  = time_blur_alpha / time_total_graphics;
        ubo_performance_meter_graphics.positions[3]  = time_blur_beta / time_total_graphics;
        ubo_performance_meter_graphics.positions[4]  = time_combine / time_total_graphics;
        ubo_performance_meter_graphics.positions[5]  = time_tone_map / time_total_graphics;
        ubo_performance_meter_graphics.positions[6]  = time_overhead / time_total_graphics;

        time_total_compute = ubo_performance_meter_compute.positions[2];

        if (time_total_compute > time_total_graphics)
        {
            ubo_performance_meter_compute.relative_size  = 1.0;
            ubo_performance_meter_graphics.relative_size = time_total_graphics / time_total_compute;
        }
        else
        {
            ubo_performance_meter_compute.relative_size  = time_total_compute / time_total_graphics;
            ubo_performance_meter_graphics.relative_size = 1.0;
        }
    }


    ubo_nbody_graphics.timestamp = static_cast<double>(uptime.nsecsElapsed()) / 1.0e9;
}


void VulkanWindow::resizeEvent(QResizeEvent *ev)
{
    swapChainRecreate();
}


void VulkanWindow::mouseMoveEvent(QMouseEvent *ev)
{
    QPoint middle_of_widget(this->width() / 2, this->height() / 2);
    QPoint middle_of_widget_global = mapToGlobal(middle_of_widget);

    if (p_ignore_mousemove_event == true)
    {
        ev->ignore();
        p_ignore_mousemove_event = false;
        p_last_position          = middle_of_widget;
        p_pitch_yaw_vector       = QPointF(0, 0);
    }
    else
    {
        if (p_mouse_right_button_active)
        {
            if (!p_key_ctrl_active)
            {
                // Instant rotation
                p_pitch_yaw_vector.setX(p_last_position.x() - ev->localPos().x());
                p_pitch_yaw_vector.setY(p_last_position.y() - ev->localPos().y());

                double eta = std::atan2(p_pitch_yaw_vector.y(), p_pitch_yaw_vector.x()) - 0.5 * pi;
                double magnitude = -mouse_sensitivity *pi *std::sqrt(p_pitch_yaw_vector.x() * p_pitch_yaw_vector.x() + p_pitch_yaw_vector.y() * p_pitch_yaw_vector.y()) / static_cast<double> (this->height());

                RotationMatrix<double> pitch_yaw_rotation;
                pitch_yaw_rotation.setArbRotation(-0.5 * pi, eta, magnitude);

                rotation_matrix = pitch_yaw_rotation * rotation_matrix;

                // If cursor moves to edge of widget, place back at original position and making sure to ignore the resulting event
                if ((ev->pos().x() <= 10) ||
                    (ev->pos().x() >= this->width() - 10) ||
                    (ev->pos().y() <= 10) ||
                    (ev->pos().y() >= this->height() - 10))
                {
                    this->cursor().setPos(middle_of_widget_global);
                    p_ignore_mousemove_event = true;
                }
            }
            else
            {
                // Smooth rotation
                QPoint middle_of_widget(this->width() / 2, this->height() / 2);
                QPoint middle_of_widget_global = mapToGlobal(middle_of_widget);

                p_pitch_yaw_vector.setX(p_last_position.x() - middle_of_widget.x());
                p_pitch_yaw_vector.setY(p_last_position.y() - middle_of_widget.y());

                double pitch_yaw_magnitude     = std::sqrt(p_pitch_yaw_vector.x() * p_pitch_yaw_vector.x() + p_pitch_yaw_vector.y() * p_pitch_yaw_vector.y());
                double pitch_yaw_magnitude_max = 0.5 * this->height();

                // Move cursor closer to origin defined by mouse press
                if (pitch_yaw_magnitude > pitch_yaw_magnitude_max)
                {
                    p_pitch_yaw_vector.setX(p_pitch_yaw_vector.x() / pitch_yaw_magnitude * pitch_yaw_magnitude_max * 0.9);
                    p_pitch_yaw_vector.setY(p_pitch_yaw_vector.y() / pitch_yaw_magnitude * pitch_yaw_magnitude_max * 0.9);

                    this->cursor().setPos(QPoint(p_pitch_yaw_vector.x() + middle_of_widget_global.x(), p_pitch_yaw_vector.y() + middle_of_widget_global.y()));
                }
            }
        }
        uniformBuffersUpdate();
        p_last_position = ev->localPos();
    }

    if (p_mouse_right_button_active)
    {
        this->setCursor(Qt::BlankCursor);
    }
    else
    {
        this->setCursor(Qt::CrossCursor);
    }
}


void VulkanWindow::mouseDoubleClickEvent(QMouseEvent *ev)
{
}


void VulkanWindow::mousePressEvent(QMouseEvent *ev)
{
    p_last_position = ev->localPos();

    if (ev->buttons() & Qt::RightButton)
    {
        p_mouse_right_button_active = true;
    }
}


void VulkanWindow::mouseReleaseEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::RightButton)
    {
        p_mouse_right_button_active = false;
        p_pitch_yaw_vector.setX(0);
        p_pitch_yaw_vector.setY(0);
        this->setCursor(Qt::CrossCursor);
    }
}


void VulkanWindow::keyPressEvent(QKeyEvent *ev)
{
    if (ev->key() == Qt::Key_W)
    {
        p_key_w_active = true;
    }
    if (ev->key() == Qt::Key_A)
    {
        p_key_a_active = true;
    }
    if (ev->key() == Qt::Key_S)
    {
        p_key_s_active = true;
    }
    if (ev->key() == Qt::Key_D)
    {
        p_key_d_active = true;
    }
    if (ev->key() == Qt::Key_Q)
    {
        p_key_q_active = true;
    }
    if (ev->key() == Qt::Key_E)
    {
        p_key_e_active = true;
    }
    if (ev->key() == Qt::Key_Shift)
    {
        p_key_shift_active = true;
    }
    if (ev->key() == Qt::Key_Control)
    {
        p_key_ctrl_active = true;
    }
    if (ev->key() == Qt::Key_Space)
    {
        p_key_space_active = true;
    }
}


void VulkanWindow::keyReleaseEvent(QKeyEvent *ev)
{
    if (ev->key() == Qt::Key_W)
    {
        p_key_w_active = false;
    }
    if (ev->key() == Qt::Key_A)
    {
        p_key_a_active = false;
    }
    if (ev->key() == Qt::Key_S)
    {
        p_key_s_active = false;
    }
    if (ev->key() == Qt::Key_D)
    {
        p_key_d_active = false;
    }
    if (ev->key() == Qt::Key_Q)
    {
        p_key_q_active = false;
    }
    if (ev->key() == Qt::Key_E)
    {
        p_key_e_active = false;
    }
    if (ev->key() == Qt::Key_Control)
    {
        p_key_ctrl_active = false;
    }
    if (ev->key() == Qt::Key_Shift)
    {
        p_key_shift_active = false;
    }
    if (ev->key() == Qt::Key_Space)
    {
        p_key_space_active = false;
    }
}


void VulkanWindow::wheelEvent(QWheelEvent *ev)
{
    float move_scaling = 2.0;

    if (ev->modifiers() & Qt::ShiftModifier)
    {
        move_scaling = 5.0;
    }
    else if (ev->modifiers() & Qt::ControlModifier)
    {
        move_scaling = 0.2;
    }

    double delta;

    if (ev->delta() > 0)
    {
        delta = 0.1;
    }
    else if (ev->delta() < 0)
    {
        delta = -0.1;
    }
    else
    {
        delta = 0;
    }

    delta *= move_scaling;

    if ((delta + rotation_origin_matrix[11] >= -5) && (delta + rotation_origin_matrix[11] <= 0))
    {
        rotation_origin_matrix[11] += delta;
    }
    uniformBuffersUpdate();
}


void VulkanWindow::queryPoolCreate()
{
    {
        VkQueryPoolCreateInfo info = {};
        info.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        info.pNext      = nullptr;
        info.flags      = 0;
        info.queryType  = VK_QUERY_TYPE_TIMESTAMP;
        info.queryCount = 14;
        HANDLE_VK_RESULT(vkCreateQueryPool(vkbase.device(), &info, nullptr, &query_pool_graphics));
    }
    {
        VkQueryPoolCreateInfo info = {};
        info.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        info.pNext      = nullptr;
        info.flags      = 0;
        info.queryType  = VK_QUERY_TYPE_TIMESTAMP;
        info.queryCount = 4;
        HANDLE_VK_RESULT(vkCreateQueryPool(vkbase.device(), &info, nullptr, &query_pool_compute));
    }
}


void VulkanWindow::queryPoolDestroy()
{
    vkDestroyQueryPool(vkbase.device(), query_pool_graphics, nullptr);
    vkDestroyQueryPool(vkbase.device(), query_pool_compute, nullptr);
}


void VulkanWindow::swapChainCreate(VkSwapchainKHR old_swapchain)
{
    HANDLE_VK_RESULT(vkDeviceWaitIdle(vkbase.device()));

    HANDLE_VK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkbase.physicalDevice(), surface, &surface_capabilities));

    // Find a present mode
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    {
        uint32_t present_mode_count = 0;
        HANDLE_VK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(vkbase.physicalDevice(), surface, &present_mode_count, nullptr));
        QVector<VkPresentModeKHR> present_mode_list(present_mode_count);
        HANDLE_VK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(vkbase.physicalDevice(), surface, &present_mode_count, present_mode_list.data()));
        for (auto m : present_mode_list)
        {
            if (m == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                present_mode          = m;
                swapchain_image_count = 3;
            }
        }
    }

    if ((surface_capabilities.maxImageCount > 0))
    {
        if ((swapchain_image_count > surface_capabilities.maxImageCount))
        {
            swapchain_image_count = surface_capabilities.maxImageCount;
        }
    }
    if (swapchain_image_count < surface_capabilities.minImageCount)
    {
        swapchain_image_count = surface_capabilities.minImageCount;
    }

    // Ensure swapchain images respect surface capabilities
    uint32_t vulkan_surface_width  = surface_capabilities.currentExtent.width;
    uint32_t vulkan_surface_height = surface_capabilities.currentExtent.height;

    if (vulkan_surface_width > surface_capabilities.maxImageExtent.width)
    {
        vulkan_surface_width = surface_capabilities.maxImageExtent.width;
    }
    if (vulkan_surface_height > surface_capabilities.maxImageExtent.height)
    {
        vulkan_surface_height = surface_capabilities.maxImageExtent.height;
    }
    if (vulkan_surface_width < surface_capabilities.minImageExtent.width)
    {
        vulkan_surface_width = surface_capabilities.minImageExtent.width;
    }
    if (vulkan_surface_height < surface_capabilities.minImageExtent.height)
    {
        vulkan_surface_height = surface_capabilities.minImageExtent.height;
    }

    VkSwapchainCreateInfoKHR info = {};
    info.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.pNext                 = nullptr;
    info.surface               = surface;
    info.minImageCount         = swapchain_image_count;
    info.imageFormat           = surface_format.format;
    info.imageColorSpace       = surface_format.colorSpace;
    info.imageExtent.width     = vulkan_surface_width;
    info.imageExtent.height    = vulkan_surface_height;
    info.imageArrayLayers      = 1;
    info.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices   = nullptr;
    info.preTransform          = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    info.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode           = present_mode;
    info.clipped               = VK_TRUE;
    info.oldSwapchain          = old_swapchain;

    // (Re)create a swap chain
    HANDLE_VK_RESULT(vkCreateSwapchainKHR(vkbase.device(), &info, nullptr, &swapchain));

    if (old_swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(vkbase.device(), old_swapchain, nullptr);
    }
}


void VulkanWindow::swapChainDestroy()
{
    vkDestroySwapchainKHR(vkbase.device(), swapchain, nullptr);
}


void VulkanWindow::swapChainImageViewsCreate()
{
    // Swap chain images, managed by swapchain
    HANDLE_VK_RESULT(vkGetSwapchainImagesKHR(vkbase.device(), swapchain, &swapchain_image_count, nullptr));

    swapchain_images.resize(swapchain_image_count);

    HANDLE_VK_RESULT(vkGetSwapchainImagesKHR(vkbase.device(), swapchain, &swapchain_image_count, swapchain_images.data()));

    swapchain_image_views.resize(swapchain_image_count);

    for (uint32_t i = 0; i < swapchain_image_count; ++i)
    {
        VkImageViewCreateInfo info = {};
        info.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.pNext        = nullptr;
        info.image        = swapchain_images[i];
        info.viewType     = VK_IMAGE_VIEW_TYPE_2D;
        info.format       = surface_format.format;
        info.components.r = VK_COMPONENT_SWIZZLE_R;
        info.components.g = VK_COMPONENT_SWIZZLE_G;
        info.components.b = VK_COMPONENT_SWIZZLE_B;
        info.components.a = VK_COMPONENT_SWIZZLE_A;
        info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel   = 0;
        info.subresourceRange.levelCount     = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount     = 1;

        HANDLE_VK_RESULT(vkCreateImageView(vkbase.device(), &info, nullptr, &swapchain_image_views[i]));
    }
}


void VulkanWindow::swapChainImageViewsDestroy()
{
    for (auto view : swapchain_image_views)
    {
        vkDestroyImageView(vkbase.device(), view, nullptr);
    }
}


void VulkanWindow::fencesCreate()
{
    VkFenceCreateInfo info = {};

    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    HANDLE_VK_RESULT(vkCreateFence(vkbase.device(), &info, nullptr, &fence_draw));
    HANDLE_VK_RESULT(vkCreateFence(vkbase.device(), &info, nullptr, &fence_transfer));
    HANDLE_VK_RESULT(vkCreateFence(vkbase.device(), &info, nullptr, &fence_compute_step_1));
}


void VulkanWindow::fencesDestroy()
{
    vkDestroyFence(vkbase.device(), fence_draw, nullptr);
    vkDestroyFence(vkbase.device(), fence_transfer, nullptr);
    vkDestroyFence(vkbase.device(), fence_compute_step_1, nullptr);
}


void VulkanWindow::depthStencilCreate()
{
    VkImageCreateInfo image_info = {};

    image_info.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.pNext       = nullptr;
    image_info.imageType   = VK_IMAGE_TYPE_2D;
    image_info.format      = depth_format;
    image_info.extent      = { surface_capabilities.currentExtent.width, surface_capabilities.currentExtent.height, 1 };
    image_info.mipLevels   = 1;
    image_info.arrayLayers = 1;
    image_info.samples     = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling      = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage       = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image_info.flags       = 0;
    //    image_create_info.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkMemoryAllocateInfo mem_alloc = {};
    mem_alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_alloc.pNext           = nullptr;
    mem_alloc.allocationSize  = 0;
    mem_alloc.memoryTypeIndex = 0;

    VkImageViewCreateInfo view_info = {};
    view_info.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.pNext            = nullptr;
    view_info.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format           = depth_format;
    view_info.subresourceRange = {};
    view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    view_info.subresourceRange.baseMipLevel   = 0;
    view_info.subresourceRange.levelCount     = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount     = 1;

    HANDLE_VK_RESULT(vkCreateImage(vkbase.device(), &image_info, nullptr, &vulkan_depth_stencil.image));

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(vkbase.device(), vulkan_depth_stencil.image, &memory_requirements);

    VkMemoryAllocateInfo memory_info = {};
    memory_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_info.allocationSize  = memory_requirements.size;
    memory_info.memoryTypeIndex = vulkan_helper->memoryTypeIndex(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    HANDLE_VK_RESULT(vkAllocateMemory(vkbase.device(), &memory_info, nullptr, &vulkan_depth_stencil.mem));

    HANDLE_VK_RESULT(vkBindImageMemory(vkbase.device(), vulkan_depth_stencil.image, vulkan_depth_stencil.mem, 0));

    // Change layout of depth stencil image to VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    {
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask   = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount   = 1;
        subresourceRange.layerCount   = 1;

        VkImageMemoryBarrier barrier = {};
        barrier.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext            = nullptr;
        barrier.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.image            = vulkan_depth_stencil.image;
        barrier.subresourceRange = subresourceRange;
        barrier.srcAccessMask    = 0;
        barrier.dstAccessMask    = barrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        // Put barrier inside setup command buffer
        VkCommandBuffer setup_command_buffer = commandBufferCreate();

        vkCmdPipelineBarrier(
            setup_command_buffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        commandBufferSubmitAndFree(setup_command_buffer);
    }

    view_info.image = vulkan_depth_stencil.image;
    HANDLE_VK_RESULT(vkCreateImageView(vkbase.device(), &view_info, nullptr, &vulkan_depth_stencil.view));
}


void VulkanWindow::depthStencilDestroy()
{
    vkDestroyImageView(vkbase.device(), vulkan_depth_stencil.view, nullptr);
    vkDestroyImage(vkbase.device(), vulkan_depth_stencil.image, nullptr);
    vkFreeMemory(vkbase.device(), vulkan_depth_stencil.mem, nullptr);
}


void VulkanWindow::semaphoresCreate()
{
    // Semaphores are used to coordinate operations between queues and between vulkan_substructure.vulkanPresentQueue () submissions within a single vulkan_substructure.vulkanPresentQueue ()
    VkSemaphoreCreateInfo semaphore_create_info = {};

    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    HANDLE_VK_RESULT(vkCreateSemaphore(vkbase.device(), &semaphore_create_info, nullptr, &semaphore_present_complete));
    HANDLE_VK_RESULT(vkCreateSemaphore(vkbase.device(), &semaphore_create_info, nullptr, &semaphore_post_present_complete));
    HANDLE_VK_RESULT(vkCreateSemaphore(vkbase.device(), &semaphore_create_info, nullptr, &semaphore_compute_step_1_complete));
    HANDLE_VK_RESULT(vkCreateSemaphore(vkbase.device(), &semaphore_create_info, nullptr, &semaphore_draw_complete));
    HANDLE_VK_RESULT(vkCreateSemaphore(vkbase.device(), &semaphore_create_info, nullptr, &semaphore_pre_present_complete));
}


void VulkanWindow::semaphoresDestroy()
{
    vkDestroySemaphore(vkbase.device(), semaphore_present_complete, nullptr);
    vkDestroySemaphore(vkbase.device(), semaphore_post_present_complete, nullptr);
    vkDestroySemaphore(vkbase.device(), semaphore_compute_step_1_complete, nullptr);
    vkDestroySemaphore(vkbase.device(), semaphore_draw_complete, nullptr);
    vkDestroySemaphore(vkbase.device(), semaphore_pre_present_complete, nullptr);
}


void VulkanWindow::renderPassesCreate()
{
    // Low dynamic range
    {
        QVector<VkAttachmentDescription> attachment_descriptions(2);

        // Color attachment
        attachment_descriptions[0].flags          = 0;
        attachment_descriptions[0].format         = surface_format.format;
        attachment_descriptions[0].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachment_descriptions[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment_descriptions[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachment_descriptions[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment_descriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment_descriptions[0].initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment_descriptions[0].finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Depth attachment
        attachment_descriptions[1].flags          = 0;
        attachment_descriptions[1].format         = depth_format;
        attachment_descriptions[1].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachment_descriptions[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment_descriptions[1].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachment_descriptions[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment_descriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment_descriptions[1].initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachment_descriptions[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment_reference;
        color_attachment_reference.attachment = 0;
        color_attachment_reference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_attachment_references = {};
        depth_attachment_references.attachment = 1;
        depth_attachment_references.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        QVector<VkSubpassDescription> subpass_descriptions(1);
        subpass_descriptions[0].flags                   = 0;
        subpass_descriptions[0].pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_descriptions[0].inputAttachmentCount    = 0;
        subpass_descriptions[0].pInputAttachments       = nullptr;
        subpass_descriptions[0].colorAttachmentCount    = 1;
        subpass_descriptions[0].pColorAttachments       = &color_attachment_reference;
        subpass_descriptions[0].pResolveAttachments     = nullptr;
        subpass_descriptions[0].pDepthStencilAttachment = &depth_attachment_references;
        subpass_descriptions[0].preserveAttachmentCount = 0;
        subpass_descriptions[0].pPreserveAttachments    = nullptr;

        VkRenderPassCreateInfo render_pass_create_info = {};
        render_pass_create_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.pNext           = nullptr;
        render_pass_create_info.flags           = 0;
        render_pass_create_info.attachmentCount = static_cast<uint32_t> (attachment_descriptions.size());
        render_pass_create_info.pAttachments    = attachment_descriptions.data();
        render_pass_create_info.subpassCount    = static_cast<uint32_t> (subpass_descriptions.size());
        render_pass_create_info.pSubpasses      = subpass_descriptions.data();
        render_pass_create_info.dependencyCount = 0;
        render_pass_create_info.pDependencies   = nullptr;

        HANDLE_VK_RESULT(vkCreateRenderPass(vkbase.device(), &render_pass_create_info, nullptr, &render_pass_ldr));
    }
    // High dynamic range with depth attachment in addition
    {
        QVector<VkAttachmentDescription> attachment_descriptions(2);

        // Color attachment
        attachment_descriptions[0].flags          = 0;
        attachment_descriptions[0].format         = VK_FORMAT_R32G32B32A32_SFLOAT;
        attachment_descriptions[0].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachment_descriptions[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment_descriptions[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachment_descriptions[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment_descriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment_descriptions[0].initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment_descriptions[0].finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Depth attachment
        attachment_descriptions[1].flags          = 0;
        attachment_descriptions[1].format         = depth_format;
        attachment_descriptions[1].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachment_descriptions[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment_descriptions[1].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachment_descriptions[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment_descriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment_descriptions[1].initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachment_descriptions[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        QVector<VkAttachmentReference> color_attachment_references;
        {
            VkAttachmentReference reference = {};
            reference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            reference.attachment = 0;

            color_attachment_references << reference;
        }

        VkAttachmentReference depth_attachment_reference = {};
        depth_attachment_reference.attachment = 1;
        depth_attachment_reference.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        QVector<VkSubpassDescription> subpass_descriptions(1);
        subpass_descriptions[0].flags                   = 0;
        subpass_descriptions[0].pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_descriptions[0].inputAttachmentCount    = 0;
        subpass_descriptions[0].pInputAttachments       = nullptr;
        subpass_descriptions[0].colorAttachmentCount    = static_cast<uint32_t> (color_attachment_references.size());
        subpass_descriptions[0].pColorAttachments       = color_attachment_references.data();
        subpass_descriptions[0].pResolveAttachments     = nullptr;
        subpass_descriptions[0].pDepthStencilAttachment = &depth_attachment_reference;
        subpass_descriptions[0].preserveAttachmentCount = 0;
        subpass_descriptions[0].pPreserveAttachments    = nullptr;

        VkRenderPassCreateInfo render_pass_create_info = {};
        render_pass_create_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.pNext           = nullptr;
        render_pass_create_info.flags           = 0;
        render_pass_create_info.attachmentCount = static_cast<uint32_t> (attachment_descriptions.size());
        render_pass_create_info.pAttachments    = attachment_descriptions.data();
        render_pass_create_info.subpassCount    = static_cast<uint32_t> (subpass_descriptions.size());
        render_pass_create_info.pSubpasses      = subpass_descriptions.data();
        render_pass_create_info.dependencyCount = 0;
        render_pass_create_info.pDependencies   = nullptr;

        HANDLE_VK_RESULT(vkCreateRenderPass(vkbase.device(), &render_pass_create_info, nullptr, &render_pass_hdr_color_depth));
    }
    // High dynamic range, only color attachment
    {
        QVector<VkAttachmentDescription> attachment_descriptions(1);

        // Color attachment
        attachment_descriptions[0].flags          = 0;
        attachment_descriptions[0].format         = VK_FORMAT_R32G32B32A32_SFLOAT;
        attachment_descriptions[0].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachment_descriptions[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment_descriptions[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachment_descriptions[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment_descriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment_descriptions[0].initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment_descriptions[0].finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment_reference;
        color_attachment_reference.attachment = 0;
        color_attachment_reference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        QVector<VkSubpassDescription> subpass_descriptions(1);
        subpass_descriptions[0].flags                   = 0;
        subpass_descriptions[0].pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_descriptions[0].inputAttachmentCount    = 0;
        subpass_descriptions[0].pInputAttachments       = nullptr;
        subpass_descriptions[0].colorAttachmentCount    = 1;
        subpass_descriptions[0].pColorAttachments       = &color_attachment_reference;
        subpass_descriptions[0].pResolveAttachments     = nullptr;
        subpass_descriptions[0].pDepthStencilAttachment = nullptr;
        subpass_descriptions[0].preserveAttachmentCount = 0;
        subpass_descriptions[0].pPreserveAttachments    = nullptr;

        VkRenderPassCreateInfo render_pass_create_info = {};
        render_pass_create_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.pNext           = nullptr;
        render_pass_create_info.flags           = 0;
        render_pass_create_info.attachmentCount = static_cast<uint32_t> (attachment_descriptions.size());
        render_pass_create_info.pAttachments    = attachment_descriptions.data();
        render_pass_create_info.subpassCount    = static_cast<uint32_t> (subpass_descriptions.size());
        render_pass_create_info.pSubpasses      = subpass_descriptions.data();
        render_pass_create_info.dependencyCount = 0;
        render_pass_create_info.pDependencies   = nullptr;

        HANDLE_VK_RESULT(vkCreateRenderPass(vkbase.device(), &render_pass_create_info, nullptr, &render_pass_hdr));
    }
}


void VulkanWindow::renderPassDestroy()
{
    vkDestroyRenderPass(vkbase.device(), render_pass_ldr, nullptr);
    vkDestroyRenderPass(vkbase.device(), render_pass_hdr_color_depth, nullptr);
    vkDestroyRenderPass(vkbase.device(), render_pass_hdr, nullptr);
}


void VulkanWindow::frameBuffersCreate()
{
    // Swapchain fbos
    QVector<VkImageView> attachments(2);
    attachments[1] = vulkan_depth_stencil.view;

    VkFramebufferCreateInfo framebuffer_create_info = {};
    framebuffer_create_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.pNext           = nullptr;
    framebuffer_create_info.flags           = 0;
    framebuffer_create_info.renderPass      = render_pass_ldr;
    framebuffer_create_info.attachmentCount = static_cast<uint32_t> (attachments.size());
    framebuffer_create_info.pAttachments    = attachments.data();
    framebuffer_create_info.width           = surface_capabilities.currentExtent.width;
    framebuffer_create_info.height          = surface_capabilities.currentExtent.height;
    framebuffer_create_info.layers          = 1;

    framebuffers_swapchain.resize(swapchain_image_count);

    for (auto i = 0; i < framebuffers_swapchain.size(); ++i)
    {
        attachments[0] = swapchain_image_views[i];

        HANDLE_VK_RESULT(vkCreateFramebuffer(vkbase.device(), &framebuffer_create_info, nullptr, &framebuffers_swapchain[i]));
    }

    // Other fbos, all HDR
    framebufferSceneHDRCreate(&framebuffer_scene);
    framebufferStandardHDRCreate(&framebuffer_luminosity, framebuffer_size_blur_pass[0], framebuffer_size_blur_pass[1]);
    framebufferStandardHDRCreate(&framebuffer_blur_alpha, framebuffer_size_blur_pass[0], framebuffer_size_blur_pass[1]);
    framebufferStandardHDRCreate(&framebuffer_blur_beta, framebuffer_size_blur_pass[0], framebuffer_size_blur_pass[1]);
    framebufferStandardHDRCreate(&framebuffer_combine, surface_capabilities.currentExtent.width, surface_capabilities.currentExtent.height);


    // Sampler for the HDR framebuffers
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.pNext         = nullptr;
    samplerCreateInfo.magFilter     = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter     = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo.addressModeV  = samplerCreateInfo.addressModeU;
    samplerCreateInfo.addressModeW  = samplerCreateInfo.addressModeU;
    samplerCreateInfo.mipLodBias    = 0.0f;
    samplerCreateInfo.maxAnisotropy = 0;
    samplerCreateInfo.minLod        = 0.0f;
    samplerCreateInfo.maxLod        = 0.0f;
    samplerCreateInfo.borderColor   = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    HANDLE_VK_RESULT(vkCreateSampler(vkbase.device(), &samplerCreateInfo, nullptr, &sampler_standard));
}


void VulkanWindow::framebufferStandardHDRCreate(Framebuffer *framebuffer, uint32_t width, uint32_t height, VkImageUsageFlags usage, VkAccessFlags dst_access_mask, VkImageLayout new_layout)
{
    framebuffer->width  = width;
    framebuffer->height = height;

    QVector<VkImageView> attachments;

    // Color attachment
    {
        framebuffer->color_attachment.enabled = true;

        // Color attachment
        VkImageCreateInfo image_info = {};
        image_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.pNext         = nullptr;
        image_info.imageType     = VK_IMAGE_TYPE_2D;
        image_info.format        = VK_FORMAT_R32G32B32A32_SFLOAT;
        image_info.extent.width  = framebuffer->width;
        image_info.extent.height = framebuffer->height;
        image_info.extent.depth  = 1;
        image_info.mipLevels     = 1;
        image_info.arrayLayers   = 1;
        image_info.samples       = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling        = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage         = usage;

        HANDLE_VK_RESULT(vkCreateImage(vkbase.device(), &image_info, nullptr, &framebuffer->color_attachment.image));

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(vkbase.device(), framebuffer->color_attachment.image, &memReqs);

        VkMemoryAllocateInfo memAlloc = {};
        memAlloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memAlloc.pNext           = NULL;
        memAlloc.allocationSize  = 0;
        memAlloc.memoryTypeIndex = 0;
        memAlloc.allocationSize  = memReqs.size;
        memAlloc.memoryTypeIndex = vulkan_helper->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        HANDLE_VK_RESULT(vkAllocateMemory(vkbase.device(), &memAlloc, nullptr, &framebuffer->color_attachment.mem));
        HANDLE_VK_RESULT(vkBindImageMemory(vkbase.device(), framebuffer->color_attachment.image, framebuffer->color_attachment.mem, 0));

        VkImageViewCreateInfo view_info = {};
        view_info.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.pNext            = nullptr;
        view_info.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
        view_info.flags            = 0;
        view_info.subresourceRange = {};
        view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel   = 0;
        view_info.subresourceRange.levelCount     = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount     = 1;
        view_info.image = framebuffer->color_attachment.image;
        HANDLE_VK_RESULT(vkCreateImageView(vkbase.device(), &view_info, nullptr, &framebuffer->color_attachment.view));

        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = nullptr;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            barrier.image         = framebuffer->color_attachment.image;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = dst_access_mask;
            barrier.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout     = new_layout;

            VkCommandBuffer command_buffer = commandBufferCreate();

            vkCmdPipelineBarrier(
                command_buffer,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            commandBufferSubmitAndFree(command_buffer);
        }

        attachments << framebuffer->color_attachment.view;
    }

    VkFramebufferCreateInfo fbufCreateInfo = {};
    fbufCreateInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbufCreateInfo.pNext           = nullptr;
    fbufCreateInfo.renderPass      = render_pass_hdr;
    fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    fbufCreateInfo.pAttachments    = attachments.data();
    fbufCreateInfo.width           = framebuffer->width;
    fbufCreateInfo.height          = framebuffer->height;
    fbufCreateInfo.layers          = 1;

    HANDLE_VK_RESULT(vkCreateFramebuffer(vkbase.device(), &fbufCreateInfo, nullptr, &framebuffer->framebuffer));
}


void VulkanWindow::hdrFramebufferDestroy(VulkanWindow::Framebuffer *framebuffer)
{
    vkDestroyImageView(vkbase.device(), framebuffer->color_attachment.view, nullptr);
    vkDestroyImage(vkbase.device(), framebuffer->color_attachment.image, nullptr);
    vkFreeMemory(vkbase.device(), framebuffer->color_attachment.mem, nullptr);

    if (framebuffer->depth_attachment.enabled == true)
    {
        vkDestroyImageView(vkbase.device(), framebuffer->depth_attachment.view, nullptr);
        vkDestroyImage(vkbase.device(), framebuffer->depth_attachment.image, nullptr);
        vkFreeMemory(vkbase.device(), framebuffer->depth_attachment.mem, nullptr);
    }

    vkDestroyFramebuffer(vkbase.device(), framebuffer->framebuffer, nullptr);
}


void VulkanWindow::frameBuffersDestroy()
{
    for (auto i = 0; i < framebuffers_swapchain.size(); ++i)
    {
        vkDestroyFramebuffer(vkbase.device(), framebuffers_swapchain[i], nullptr);
    }

    hdrFramebufferDestroy(&framebuffer_scene);
    hdrFramebufferDestroy(&framebuffer_blur_alpha);
    hdrFramebufferDestroy(&framebuffer_blur_beta);
    hdrFramebufferDestroy(&framebuffer_luminosity);
    hdrFramebufferDestroy(&framebuffer_combine);

    vkDestroySampler(vkbase.device(), sampler_standard, nullptr);
}


void VulkanWindow::framebufferSceneHDRCreate(VulkanWindow::Framebuffer *framebuffer)
{
    framebuffer->width  = surface_capabilities.currentExtent.width;
    framebuffer->height = surface_capabilities.currentExtent.height;

    QVector<VkImageView> attachments;

    // Color attachment
    {
        framebuffer->color_attachment.enabled = true;

        // Color attachment
        VkImageCreateInfo image_info = {};
        image_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.pNext         = nullptr;
        image_info.imageType     = VK_IMAGE_TYPE_2D;
        image_info.format        = VK_FORMAT_R32G32B32A32_SFLOAT;
        image_info.extent.width  = framebuffer->width;
        image_info.extent.height = framebuffer->height;
        image_info.extent.depth  = 1;
        image_info.mipLevels     = 1;
        image_info.arrayLayers   = 1;
        image_info.samples       = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling        = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        HANDLE_VK_RESULT(vkCreateImage(vkbase.device(), &image_info, nullptr, &framebuffer->color_attachment.image));

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(vkbase.device(), framebuffer->color_attachment.image, &memReqs);

        VkMemoryAllocateInfo memAlloc = {};
        memAlloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memAlloc.pNext           = NULL;
        memAlloc.allocationSize  = 0;
        memAlloc.memoryTypeIndex = 0;
        memAlloc.allocationSize  = memReqs.size;
        memAlloc.memoryTypeIndex = vulkan_helper->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        HANDLE_VK_RESULT(vkAllocateMemory(vkbase.device(), &memAlloc, nullptr, &framebuffer->color_attachment.mem));
        HANDLE_VK_RESULT(vkBindImageMemory(vkbase.device(), framebuffer->color_attachment.image, framebuffer->color_attachment.mem, 0));

        VkImageViewCreateInfo view_info = {};
        view_info.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.pNext            = nullptr;
        view_info.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
        view_info.flags            = 0;
        view_info.subresourceRange = {};
        view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel   = 0;
        view_info.subresourceRange.levelCount     = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount     = 1;
        view_info.image = framebuffer->color_attachment.image;
        HANDLE_VK_RESULT(vkCreateImageView(vkbase.device(), &view_info, nullptr, &framebuffer->color_attachment.view));

        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = nullptr;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            barrier.image         = framebuffer->color_attachment.image;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkCommandBuffer command_buffer = commandBufferCreate();

            vkCmdPipelineBarrier(
                command_buffer,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            commandBufferSubmitAndFree(command_buffer);
        }

        attachments << framebuffer->color_attachment.view;
    }

    // Depth stencil attachment
    {
        framebuffer->depth_attachment.enabled = true;

        VkImageCreateInfo image_info = {};
        image_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.pNext         = nullptr;
        image_info.imageType     = VK_IMAGE_TYPE_2D;
        image_info.extent.width  = framebuffer->width;
        image_info.extent.height = framebuffer->height;
        image_info.extent.depth  = 1;
        image_info.mipLevels     = 1;
        image_info.arrayLayers   = 1;
        image_info.samples       = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling        = VK_IMAGE_TILING_OPTIMAL;
        image_info.format        = depth_format;
        image_info.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        HANDLE_VK_RESULT(vkCreateImage(vkbase.device(), &image_info, nullptr, &framebuffer->depth_attachment.image));

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(vkbase.device(), framebuffer->depth_attachment.image, &memReqs);

        VkMemoryAllocateInfo memAlloc = {};
        memAlloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memAlloc.pNext           = NULL;
        memAlloc.allocationSize  = 0;
        memAlloc.memoryTypeIndex = 0;
        memAlloc.allocationSize  = memReqs.size;
        memAlloc.memoryTypeIndex = vulkan_helper->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        HANDLE_VK_RESULT(vkAllocateMemory(vkbase.device(), &memAlloc, nullptr, &framebuffer->depth_attachment.mem));
        HANDLE_VK_RESULT(vkBindImageMemory(vkbase.device(), framebuffer->depth_attachment.image, framebuffer->depth_attachment.mem, 0));

        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = nullptr;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange    = { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 };
            barrier.image         = framebuffer->depth_attachment.image;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barrier.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            VkCommandBuffer command_buffer = commandBufferCreate();

            vkCmdPipelineBarrier(
                command_buffer,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            commandBufferSubmitAndFree(command_buffer);
        }

        VkImageViewCreateInfo view_info = {};
        view_info.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.pNext            = nullptr;
        view_info.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format           = depth_format;
        view_info.flags            = 0;
        view_info.subresourceRange = {};
        view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        view_info.subresourceRange.baseMipLevel   = 0;
        view_info.subresourceRange.levelCount     = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount     = 1;
        view_info.image = framebuffer->depth_attachment.image;
        HANDLE_VK_RESULT(vkCreateImageView(vkbase.device(), &view_info, nullptr, &framebuffer->depth_attachment.view));

        attachments << framebuffer->depth_attachment.view;
    }

    VkFramebufferCreateInfo fbufCreateInfo = {};
    fbufCreateInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbufCreateInfo.pNext           = nullptr;
    fbufCreateInfo.renderPass      = render_pass_hdr_color_depth;
    fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    fbufCreateInfo.pAttachments    = attachments.data();
    fbufCreateInfo.width           = framebuffer->width;
    fbufCreateInfo.height          = framebuffer->height;
    fbufCreateInfo.layers          = 1;

    HANDLE_VK_RESULT(vkCreateFramebuffer(vkbase.device(), &fbufCreateInfo, nullptr, &framebuffer->framebuffer));
}


void VulkanWindow::commandPoolCreate()
{
    // Create command pool
    VkCommandPoolCreateInfo pool_create_info = {};

    pool_create_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create_info.queueFamilyIndex = vkbase.queueFamilyIndex();
    pool_create_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    HANDLE_VK_RESULT(vkCreateCommandPool(vkbase.device(), &pool_create_info, nullptr, &command_pool));
}


void VulkanWindow::commandPoolDestroy()
{
    HANDLE_VK_RESULT(vkDeviceWaitIdle(vkbase.device()));

    vkDestroyCommandPool(vkbase.device(), command_pool, nullptr);
}


void VulkanWindow::commandBuffersAllocate()
{
    command_buffer_draw.resize(swapchain_image_count);
    command_buffer_pre_present.resize(swapchain_image_count);
    command_buffer_post_present.resize(swapchain_image_count);

    VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
    command_buffer_allocate_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.commandPool        = command_pool;
    command_buffer_allocate_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount = swapchain_image_count;

    HANDLE_VK_RESULT(vkAllocateCommandBuffers(vkbase.device(), &command_buffer_allocate_info, command_buffer_draw.data()));
    HANDLE_VK_RESULT(vkAllocateCommandBuffers(vkbase.device(), &command_buffer_allocate_info, command_buffer_pre_present.data()));
    HANDLE_VK_RESULT(vkAllocateCommandBuffers(vkbase.device(), &command_buffer_allocate_info, command_buffer_post_present.data()));

    command_buffer_allocate_info.commandBufferCount = 1;
    HANDLE_VK_RESULT(vkAllocateCommandBuffers(vkbase.device(), &command_buffer_allocate_info, &command_buffer_compute_step_1));
    HANDLE_VK_RESULT(vkAllocateCommandBuffers(vkbase.device(), &command_buffer_allocate_info, &command_buffer_compute_step_2));
}


void VulkanWindow::commandBuffersPresentRecord()
{
    VkCommandBufferBeginInfo cmdBufferBeginInfo = {};

    cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufferBeginInfo.pNext = nullptr;

    for (uint32_t i = 0; i < swapchain_image_count; i++)
    {
        HANDLE_VK_RESULT(vkBeginCommandBuffer(command_buffer_post_present[i], &cmdBufferBeginInfo));

        VkImageMemoryBarrier postPresentBarrier = {};
        postPresentBarrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        postPresentBarrier.pNext               = nullptr;
        postPresentBarrier.srcAccessMask       = 0;
        postPresentBarrier.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        postPresentBarrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        postPresentBarrier.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        postPresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postPresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postPresentBarrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        postPresentBarrier.image               = swapchain_images[i];

        vkCmdPipelineBarrier(
            command_buffer_post_present[i],
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &postPresentBarrier);

        HANDLE_VK_RESULT(vkEndCommandBuffer(command_buffer_post_present[i]));

        HANDLE_VK_RESULT(vkBeginCommandBuffer(command_buffer_pre_present[i], &cmdBufferBeginInfo));

        VkImageMemoryBarrier prePresentBarrier = {};
        prePresentBarrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        prePresentBarrier.pNext               = nullptr;
        prePresentBarrier.srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        prePresentBarrier.dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT;
        prePresentBarrier.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        prePresentBarrier.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        prePresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        prePresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        prePresentBarrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        prePresentBarrier.image               = swapchain_images[i];

        vkCmdPipelineBarrier(
            command_buffer_pre_present[i],
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &prePresentBarrier);

        HANDLE_VK_RESULT(vkEndCommandBuffer(command_buffer_pre_present[i]));
    }
}


VkCommandBuffer VulkanWindow::commandBufferCreate()
{
    VkCommandBuffer command_buffer;

    VkCommandBufferAllocateInfo command_buffer_allocate_info = {};

    command_buffer_allocate_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.commandPool        = command_pool;
    command_buffer_allocate_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount = 1;

    HANDLE_VK_RESULT(vkAllocateCommandBuffers(vkbase.device(), &command_buffer_allocate_info, &command_buffer));

    VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
    cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufferBeginInfo.pNext = nullptr;
    HANDLE_VK_RESULT(vkBeginCommandBuffer(command_buffer, &cmdBufferBeginInfo));

    return command_buffer;
}


void VulkanWindow::commandBufferSubmitAndFree(VkCommandBuffer command_buffer)
{
    if (command_buffer == VK_NULL_HANDLE)
    {
        qWarning("A command buffer that was a null handle was sent to be freed");
        return;
    }

    HANDLE_VK_RESULT(vkEndCommandBuffer(command_buffer));

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &command_buffer;

    HANDLE_VK_RESULT(vkQueueSubmit(vkbase.graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE));
    HANDLE_VK_RESULT(vkQueueWaitIdle(vkbase.graphicsQueue()));

    vkFreeCommandBuffers(vkbase.device(), command_pool, 1, &command_buffer);
}


void VulkanWindow::commandBuffersGraphicsRecord()
{
    VkCommandBufferBeginInfo cmd_buffer_begin_info = {};

    cmd_buffer_begin_info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.pNext            = nullptr;
    cmd_buffer_begin_info.pInheritanceInfo = nullptr;

    for (uint32_t i = 0; i < swapchain_image_count; ++i)
    {
        HANDLE_VK_RESULT(vkBeginCommandBuffer(command_buffer_draw[i], &cmd_buffer_begin_info));

        vkCmdResetQueryPool(command_buffer_draw[i], query_pool_graphics, 0, 14);
        vkCmdWriteTimestamp(command_buffer_draw[i], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, query_pool_graphics, 12);

        // Scene
        {
            vkCmdWriteTimestamp(command_buffer_draw[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool_graphics, 0);

            VkViewport viewport = {};
            viewport.width    = static_cast<float> (framebuffer_scene.width);
            viewport.height   = static_cast<float> (framebuffer_scene.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor = {};
            scissor.extent.width  = framebuffer_scene.width;
            scissor.extent.height = framebuffer_scene.height;
            scissor.offset.x      = 0;
            scissor.offset.y      = 0;

            VkClearValue clear_values[3];
            clear_values[0].color        = { 0.0f, 0.0f, 0.0f, 0.0f };
            clear_values[1].color        = { 0.0f, 0.0f, 0.0f, 0.0f };
            clear_values[2].depthStencil = { 1.0f, 0 };

            VkRenderPassBeginInfo render_pass_begin_info = {};
            render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            render_pass_begin_info.pNext = nullptr;
            render_pass_begin_info.renderArea.offset.x      = 0;
            render_pass_begin_info.renderArea.offset.y      = 0;
            render_pass_begin_info.renderPass               = render_pass_hdr_color_depth;
            render_pass_begin_info.renderArea.extent.width  = framebuffer_scene.width;
            render_pass_begin_info.renderArea.extent.height = framebuffer_scene.height;
            render_pass_begin_info.clearValueCount          = 3;
            render_pass_begin_info.pClearValues             = clear_values;

            // Change layout of framebuffer attachments
            {
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.pNext = nullptr;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                barrier.image         = framebuffer_scene.color_attachment.image;
                barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.oldLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                vkCmdPipelineBarrier(
                    command_buffer_draw[i],
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier);
            }

            render_pass_begin_info.framebuffer = framebuffer_scene.framebuffer;

            // Clear the color and depth attachment
            vkCmdBeginRenderPass(command_buffer_draw[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdSetViewport(command_buffer_draw[i], 0, 1, &viewport);
            vkCmdSetScissor(command_buffer_draw[i], 0, 1, &scissor);

            // Draw particles
            vkCmdBindPipeline(command_buffer_draw[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_nbody);
            vkCmdBindDescriptorSets(command_buffer_draw[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_nbody, 0, 1, &descriptor_nbody, 0, nullptr);

            VkDeviceSize offsets[1] = { 0 };
            vkCmdBindVertexBuffers(command_buffer_draw[i],
                                   INSTANCE_BUFFER_BIND_ID,
                                   1,
                                   &buffer_nbody_draw.buffer,
                                   offsets);
            vkCmdBindVertexBuffers(command_buffer_draw[i],
                                   VERTEX_BUFFER_BIND_ID,
                                   1,
                                   &vertices_corner.buffer,
                                   offsets);
            vkCmdBindIndexBuffer(command_buffer_draw[i], indices_quad.buffer, 0, VK_INDEX_TYPE_UINT32);


            vkCmdDrawIndexed(command_buffer_draw[i], 6, ubo_nbody_compute.particle_count, 0, 0, 0);

            // End render pass
            vkCmdEndRenderPass(command_buffer_draw[i]);

            // Change layout of framebuffer attachments
            {
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.pNext = nullptr;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                barrier.image         = framebuffer_scene.color_attachment.image;
                barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                vkCmdPipelineBarrier(
                    command_buffer_draw[i],
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier);
            }

            vkCmdWriteTimestamp(command_buffer_draw[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool_graphics, 1);
        }


        // Luminosity
        {
            vkCmdWriteTimestamp(command_buffer_draw[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool_graphics, 2);

            VkViewport viewport = {};
            viewport.width    = static_cast<float> (framebuffer_luminosity.width);
            viewport.height   = static_cast<float> (framebuffer_luminosity.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor = {};
            scissor.extent.width  = framebuffer_luminosity.width;
            scissor.extent.height = framebuffer_luminosity.height;
            scissor.offset.x      = 0;
            scissor.offset.y      = 0;

            VkClearValue clear_values[1];
            clear_values[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };

            VkRenderPassBeginInfo render_pass_begin_info = {};
            render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            render_pass_begin_info.pNext = nullptr;
            render_pass_begin_info.renderArea.offset.x      = 0;
            render_pass_begin_info.renderArea.offset.y      = 0;
            render_pass_begin_info.renderPass               = render_pass_hdr;
            render_pass_begin_info.renderArea.extent.width  = framebuffer_luminosity.width;
            render_pass_begin_info.renderArea.extent.height = framebuffer_luminosity.height;
            render_pass_begin_info.clearValueCount          = 1;
            render_pass_begin_info.pClearValues             = clear_values;

            // Change layout of output framebuffer
            {
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.pNext = nullptr;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                barrier.image         = framebuffer_luminosity.color_attachment.image;
                barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.oldLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                vkCmdPipelineBarrier(
                    command_buffer_draw[i],
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier);
            }

            // Execute shader
            {
                render_pass_begin_info.framebuffer = framebuffer_luminosity.framebuffer;

                // Clear the color and depth attachment
                vkCmdBeginRenderPass(command_buffer_draw[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
                vkCmdSetViewport(command_buffer_draw[i], 0, 1, &viewport);
                vkCmdSetScissor(command_buffer_draw[i], 0, 1, &scissor);

                vkCmdBindPipeline(command_buffer_draw[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_luminosity);
                vkCmdBindDescriptorSets(command_buffer_draw[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_normal_texture, 0, 1, &descriptor_luminosity, 0, nullptr);

                VkDeviceSize offsets[1] = { 0 };
                vkCmdBindVertexBuffers(command_buffer_draw[i], VERTEX_BUFFER_BIND_ID, 1, &vertices_fullscreen.buffer, offsets);

                // Bind index buffer
                vkCmdBindIndexBuffer(command_buffer_draw[i], indices_quad.buffer, 0, VK_INDEX_TYPE_UINT32);

                // Draw vertices
                vkCmdDrawIndexed(command_buffer_draw[i], indices_quad.count, 1, 0, 0, 0);

                // End render pass
                vkCmdEndRenderPass(command_buffer_draw[i]);
            }

            // Change layout of output framebuffer
            {
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.pNext = nullptr;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                barrier.image         = framebuffer_luminosity.color_attachment.image;
                barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                vkCmdPipelineBarrier(
                    command_buffer_draw[i],
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier);
            }

            vkCmdWriteTimestamp(command_buffer_draw[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool_graphics, 3);
        }

        // Bloom (two blur passes)
        {
            vkCmdWriteTimestamp(command_buffer_draw[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool_graphics, 4);

            VkViewport viewport = {};
            viewport.width    = static_cast<float> (framebuffer_blur_alpha.width);
            viewport.height   = static_cast<float> (framebuffer_blur_alpha.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor = {};
            scissor.extent.width  = framebuffer_blur_alpha.width;
            scissor.extent.height = framebuffer_blur_alpha.height;
            scissor.offset.x      = 0;
            scissor.offset.y      = 0;

            VkClearValue clear_values[1];
            clear_values[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };

            VkRenderPassBeginInfo render_pass_begin_info = {};
            render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            render_pass_begin_info.pNext = nullptr;
            render_pass_begin_info.renderArea.offset.x      = 0;
            render_pass_begin_info.renderArea.offset.y      = 0;
            render_pass_begin_info.renderPass               = render_pass_hdr;
            render_pass_begin_info.renderArea.extent.width  = framebuffer_blur_alpha.width;
            render_pass_begin_info.renderArea.extent.height = framebuffer_blur_alpha.height;
            render_pass_begin_info.clearValueCount          = 1;
            render_pass_begin_info.pClearValues             = clear_values;

            // Blur in one direction
            {
                // Change layout of output framebuffer
                {
                    VkImageMemoryBarrier barrier = {};
                    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    barrier.pNext = nullptr;
                    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                    barrier.image         = framebuffer_blur_alpha.color_attachment.image;
                    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    barrier.oldLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    barrier.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                    vkCmdPipelineBarrier(
                        command_buffer_draw[i],
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0,
                        0, nullptr,
                        0, nullptr,
                        1, &barrier);
                }

                // Execute shader
                {
                    render_pass_begin_info.framebuffer = framebuffer_blur_alpha.framebuffer;

                    // Clear the color and depth attachment
                    vkCmdBeginRenderPass(command_buffer_draw[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
                    vkCmdSetViewport(command_buffer_draw[i], 0, 1, &viewport);
                    vkCmdSetScissor(command_buffer_draw[i], 0, 1, &scissor);

                    push_constants.horizontal = 1;

                    vkCmdPushConstants(
                        command_buffer_draw[i],
                        pipeline_layout_blur,
                        VK_SHADER_STAGE_FRAGMENT_BIT,
                        0,
                        sizeof(push_constants),
                        &push_constants);

                    vkCmdBindPipeline(command_buffer_draw[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_blur);
                    vkCmdBindDescriptorSets(command_buffer_draw[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_blur, 0, 1, &descriptor_blur_alpha, 0, nullptr);

                    VkDeviceSize offsets[1] = { 0 };
                    vkCmdBindVertexBuffers(command_buffer_draw[i], VERTEX_BUFFER_BIND_ID, 1, &vertices_fullscreen.buffer, offsets);

                    // Bind index buffer
                    vkCmdBindIndexBuffer(command_buffer_draw[i], indices_quad.buffer, 0, VK_INDEX_TYPE_UINT32);

                    // Draw vertices
                    vkCmdDrawIndexed(command_buffer_draw[i], indices_quad.count, 1, 0, 0, 0);

                    // End render pass
                    vkCmdEndRenderPass(command_buffer_draw[i]);
                }

                // Change layout of output framebuffer
                {
                    VkImageMemoryBarrier barrier = {};
                    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    barrier.pNext = nullptr;
                    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                    barrier.image         = framebuffer_blur_alpha.color_attachment.image;
                    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    barrier.oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                    vkCmdPipelineBarrier(
                        command_buffer_draw[i],
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0,
                        0, nullptr,
                        0, nullptr,
                        1, &barrier);
                }
            }

            vkCmdWriteTimestamp(command_buffer_draw[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool_graphics, 5);
        }
        // Blur in the other direction, using the previous framebuffers as inputs
        {
            vkCmdWriteTimestamp(command_buffer_draw[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool_graphics, 6);

            VkViewport viewport = {};
            viewport.width    = static_cast<float> (framebuffer_blur_beta.width);
            viewport.height   = static_cast<float> (framebuffer_blur_beta.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor = {};
            scissor.extent.width  = framebuffer_blur_beta.width;
            scissor.extent.height = framebuffer_blur_beta.height;
            scissor.offset.x      = 0;
            scissor.offset.y      = 0;

            VkClearValue clear_values[1];
            clear_values[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };

            VkRenderPassBeginInfo render_pass_begin_info = {};
            render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            render_pass_begin_info.pNext = nullptr;
            render_pass_begin_info.renderArea.offset.x      = 0;
            render_pass_begin_info.renderArea.offset.y      = 0;
            render_pass_begin_info.renderPass               = render_pass_hdr;
            render_pass_begin_info.renderArea.extent.width  = framebuffer_blur_beta.width;
            render_pass_begin_info.renderArea.extent.height = framebuffer_blur_beta.height;
            render_pass_begin_info.clearValueCount          = 1;
            render_pass_begin_info.pClearValues             = clear_values;

            {
                // Change layout of output framebuffer
                {
                    VkImageMemoryBarrier barrier = {};
                    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    barrier.pNext = nullptr;
                    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                    barrier.image         = framebuffer_blur_beta.color_attachment.image;
                    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    barrier.oldLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    barrier.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                    vkCmdPipelineBarrier(
                        command_buffer_draw[i],
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0,
                        0, nullptr,
                        0, nullptr,
                        1, &barrier);
                }

                // Execute shader
                {
                    render_pass_begin_info.framebuffer = framebuffer_blur_beta.framebuffer;

                    // Clear the color and depth attachment
                    vkCmdBeginRenderPass(command_buffer_draw[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
                    vkCmdSetViewport(command_buffer_draw[i], 0, 1, &viewport);
                    vkCmdSetScissor(command_buffer_draw[i], 0, 1, &scissor);

                    vkCmdBindPipeline(command_buffer_draw[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_blur);

                    VkDeviceSize offsets[1] = { 0 };
                    vkCmdBindVertexBuffers(command_buffer_draw[i], VERTEX_BUFFER_BIND_ID, 1, &vertices_fullscreen.buffer, offsets);

                    // Bind index buffer
                    vkCmdBindIndexBuffer(command_buffer_draw[i], indices_quad.buffer, 0, VK_INDEX_TYPE_UINT32);


                    push_constants.horizontal = 0;

                    vkCmdPushConstants(
                        command_buffer_draw[i],
                        pipeline_layout_blur,
                        VK_SHADER_STAGE_FRAGMENT_BIT,
                        0,
                        sizeof(push_constants),
                        &push_constants);

                    vkCmdBindDescriptorSets(command_buffer_draw[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_blur, 0, 1, &descriptor_blur_beta, 0, nullptr);

                    // Draw vertices
                    vkCmdDrawIndexed(command_buffer_draw[i], indices_quad.count, 1, 0, 0, 0);

                    // End render pass
                    vkCmdEndRenderPass(command_buffer_draw[i]);
                }

                // Change layout of framebuffer
                {
                    VkImageMemoryBarrier barrier = {};
                    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    barrier.pNext = nullptr;
                    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                    barrier.image         = framebuffer_blur_beta.color_attachment.image;
                    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    barrier.oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                    vkCmdPipelineBarrier(
                        command_buffer_draw[i],
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0,
                        0, nullptr,
                        0, nullptr,
                        1, &barrier);
                }
            }

            vkCmdWriteTimestamp(command_buffer_draw[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool_graphics, 7);
        }

        // Combine
        {
            vkCmdWriteTimestamp(command_buffer_draw[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool_graphics, 8);

            VkViewport viewport = {};
            viewport.width    = static_cast<float> (framebuffer_combine.width);
            viewport.height   = static_cast<float> (framebuffer_combine.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor = {};
            scissor.extent.width  = framebuffer_combine.width;
            scissor.extent.height = framebuffer_combine.height;
            scissor.offset.x      = 0;
            scissor.offset.y      = 0;

            VkClearValue clear_values[1];
            clear_values[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };

            VkRenderPassBeginInfo render_pass_begin_info = {};
            render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            render_pass_begin_info.pNext = nullptr;
            render_pass_begin_info.renderArea.offset.x      = 0;
            render_pass_begin_info.renderArea.offset.y      = 0;
            render_pass_begin_info.renderPass               = render_pass_hdr;
            render_pass_begin_info.renderArea.extent.width  = framebuffer_combine.width;
            render_pass_begin_info.renderArea.extent.height = framebuffer_combine.height;
            render_pass_begin_info.clearValueCount          = 1;
            render_pass_begin_info.pClearValues             = clear_values;

            // Change layout of output framebuffer
            {
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.pNext = nullptr;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                barrier.image         = framebuffer_combine.color_attachment.image;
                barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.oldLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                vkCmdPipelineBarrier(
                    command_buffer_draw[i],
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier);
            }

            // Execute shader
            {
                render_pass_begin_info.framebuffer = framebuffer_combine.framebuffer;

                // Clear the color and depth attachment
                vkCmdBeginRenderPass(command_buffer_draw[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
                vkCmdSetViewport(command_buffer_draw[i], 0, 1, &viewport);
                vkCmdSetScissor(command_buffer_draw[i], 0, 1, &scissor);

                vkCmdBindPipeline(command_buffer_draw[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_normal_texture);

                VkDeviceSize offsets[1] = { 0 };
                vkCmdBindVertexBuffers(command_buffer_draw[i], VERTEX_BUFFER_BIND_ID, 1, &vertices_fullscreen.buffer, offsets);

                // Bind index buffer
                vkCmdBindIndexBuffer(command_buffer_draw[i], indices_quad.buffer, 0, VK_INDEX_TYPE_UINT32);

                // Draw blur fbo
                vkCmdBindDescriptorSets(command_buffer_draw[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_normal_texture, 0, 1, &descriptor_normal_texture_blur, 0, nullptr);
                vkCmdDrawIndexed(command_buffer_draw[i], indices_quad.count, 1, 0, 0, 0);

                // Draw scene fbo
                vkCmdBindDescriptorSets(command_buffer_draw[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_normal_texture, 0, 1, &descriptor_normal_texture_scene, 0, nullptr);
                vkCmdDrawIndexed(command_buffer_draw[i], indices_quad.count, 1, 0, 0, 0);

                // Draw Performance meter for graphics
                vkCmdBindPipeline(command_buffer_draw[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_performance);
                vkCmdBindDescriptorSets(command_buffer_draw[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_performance, 0, 1, &descriptor_performance_graphics, 0, nullptr);

                vkCmdBindVertexBuffers(command_buffer_draw[i], VERTEX_BUFFER_BIND_ID, 1, &vertices_performance_meter_graphics.buffer, offsets);
                vkCmdBindIndexBuffer(command_buffer_draw[i], indices_quad.buffer, 0, VK_INDEX_TYPE_UINT32);

                vkCmdDrawIndexed(command_buffer_draw[i], indices_quad.count, 1, 0, 0, 0);

                // Draw Performance meter for compute
                vkCmdBindDescriptorSets(command_buffer_draw[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_performance, 0, 1, &descriptor_performance_compute, 0, nullptr);

                vkCmdBindVertexBuffers(command_buffer_draw[i], VERTEX_BUFFER_BIND_ID, 1, &vertices_performance_meter_compute.buffer, offsets);
                vkCmdDrawIndexed(command_buffer_draw[i], indices_quad.count, 1, 0, 0, 0);

                // End render pass
                vkCmdEndRenderPass(command_buffer_draw[i]);
            }

            // Change layout of output framebuffer
            {
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.pNext = nullptr;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                barrier.image         = framebuffer_combine.color_attachment.image;
                barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                vkCmdPipelineBarrier(
                    command_buffer_draw[i],
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier);
            }

            vkCmdWriteTimestamp(command_buffer_draw[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool_graphics, 9);
        }

        // Tone mapping
        {
            vkCmdWriteTimestamp(command_buffer_draw[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool_graphics, 10);

            VkViewport viewport = {};
            viewport.width    = static_cast<float> (surface_capabilities.currentExtent.width);
            viewport.height   = static_cast<float> (surface_capabilities.currentExtent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor = {};
            scissor.extent.width  = surface_capabilities.currentExtent.width;
            scissor.extent.height = surface_capabilities.currentExtent.height;
            scissor.offset.x      = 0;
            scissor.offset.y      = 0;

            VkClearValue clear_values[2];
            clear_values[0].color        = { 0.0f, 0.0f, 0.0f, 0.0f };
            clear_values[1].depthStencil = { 1.0f, 0 };

            VkRenderPassBeginInfo render_pass_begin_info = {};
            render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            render_pass_begin_info.pNext = nullptr;
            render_pass_begin_info.renderArea.offset.x      = 0;
            render_pass_begin_info.renderArea.offset.y      = 0;
            render_pass_begin_info.renderPass               = render_pass_ldr;
            render_pass_begin_info.renderArea.extent.width  = surface_capabilities.currentExtent.width;
            render_pass_begin_info.renderArea.extent.height = surface_capabilities.currentExtent.height;
            render_pass_begin_info.clearValueCount          = 2;
            render_pass_begin_info.pClearValues             = clear_values;

            // Execute shader
            {
                render_pass_begin_info.framebuffer = framebuffers_swapchain[i];

                // Clear the color and depth attachment
                vkCmdBeginRenderPass(command_buffer_draw[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
                vkCmdSetViewport(command_buffer_draw[i], 0, 1, &viewport);
                vkCmdSetScissor(command_buffer_draw[i], 0, 1, &scissor);

                vkCmdBindPipeline(command_buffer_draw[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_tone_mapping);
                vkCmdBindDescriptorSets(command_buffer_draw[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_tone_mapping, 0, 1, &descriptor_tone_mapping, 0, nullptr);

                VkDeviceSize offsets[1] = { 0 };
                vkCmdBindVertexBuffers(command_buffer_draw[i], VERTEX_BUFFER_BIND_ID, 1, &vertices_fullscreen.buffer, offsets);

                // Draw post processed scene
                vkCmdBindIndexBuffer(command_buffer_draw[i], indices_quad.buffer, 0, VK_INDEX_TYPE_UINT32);

                vkCmdDrawIndexed(command_buffer_draw[i], indices_quad.count, 1, 0, 0, 0);

                vkCmdEndRenderPass(command_buffer_draw[i]);
            }

            vkCmdWriteTimestamp(command_buffer_draw[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool_graphics, 11);
        }

        vkCmdWriteTimestamp(command_buffer_draw[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool_graphics, 13);

        HANDLE_VK_RESULT(vkEndCommandBuffer(command_buffer_draw[i]));
    }
}


void VulkanWindow::commandBuffersComputeRecord()
{
    VkCommandBufferBeginInfo cmd_buffer_begin_info = {};

    cmd_buffer_begin_info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.pNext            = nullptr;
    cmd_buffer_begin_info.pInheritanceInfo = nullptr;

    // Compute command buffer step 1
    {
        HANDLE_VK_RESULT(vkBeginCommandBuffer(command_buffer_compute_step_1, &cmd_buffer_begin_info));

        vkCmdResetQueryPool(command_buffer_compute_step_1, query_pool_compute, 0, 2);
        vkCmdWriteTimestamp(command_buffer_compute_step_1, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool_compute, 0);

        vkCmdBindPipeline(command_buffer_compute_step_1, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_compute_leapfrog_step_1);
        vkCmdBindDescriptorSets(command_buffer_compute_step_1, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_leapfrog, 0, 1, &descriptor_leapgfrog, 0, 0);

        // Dispatch part of the compute job
        uint32_t work_group_count_x  = static_cast<uint32_t>(std::ceil(static_cast<double>(ubo_nbody_compute.particle_count) / static_cast<double>(work_item_count_nbody[0])));
        uint32_t work_group_count[3] = { work_group_count_x, 1, 1 };

        vkCmdDispatch(command_buffer_compute_step_1, work_group_count[0], work_group_count[1], work_group_count[2]);

        vkCmdWriteTimestamp(command_buffer_compute_step_1, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool_compute, 1);

        HANDLE_VK_RESULT(vkEndCommandBuffer(command_buffer_compute_step_1));
    }
    // Compute command buffer step 2
    {
        HANDLE_VK_RESULT(vkBeginCommandBuffer(command_buffer_compute_step_2, &cmd_buffer_begin_info));

        vkCmdResetQueryPool(command_buffer_compute_step_2, query_pool_compute, 2, 2);

        // Dispatch compute job
        {
            vkCmdWriteTimestamp(command_buffer_compute_step_2, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool_compute, 2);

            vkCmdBindPipeline(command_buffer_compute_step_2, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_compute_leapfrog_step_2);
            vkCmdBindDescriptorSets(command_buffer_compute_step_2, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_leapfrog, 0, 1, &descriptor_leapgfrog, 0, 0);

            // Find required number of work groups
            uint32_t work_group_count_x  = static_cast<uint32_t>(std::ceil(static_cast<double>(ubo_nbody_compute.particle_count) / static_cast<double>(work_item_count_nbody[0])));
            uint32_t work_group_count[3] = { work_group_count_x, 1, 1 };

            vkCmdDispatch(command_buffer_compute_step_2, work_group_count[0], work_group_count[1], work_group_count[2]);
        }
        // Pipeline barrier turning compute buffer into transfer source
        {
            VkBufferMemoryBarrier barrier = {};
            barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.pNext               = nullptr;
            barrier.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.buffer              = buffer_nbody_compute.buffer;
            barrier.size                = buffer_nbody_compute.descriptor.range;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            // Make readable/writable for compute shader
            vkCmdPipelineBarrier(
                command_buffer_compute_step_2,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                1, &barrier,
                0, nullptr);
        }
        // Pipeline barrier turning vertex buffer into transfer destination
        {
            VkBufferMemoryBarrier barrier = {};
            barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.pNext               = nullptr;
            barrier.srcAccessMask       = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.buffer              = buffer_nbody_draw.buffer;
            barrier.size                = buffer_nbody_draw.descriptor.range;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            // Make readable/writable for compute shader
            vkCmdPipelineBarrier(
                command_buffer_compute_step_2,
                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                1, &barrier,
                0, nullptr);
        }

        // Transfer data
        {
            VkBufferCopy region = {};
            region.size = ubo_nbody_compute.particle_count * sizeof(Particle);
            vkCmdCopyBuffer(
                command_buffer_compute_step_2,
                buffer_nbody_compute.buffer,
                buffer_nbody_draw.buffer,
                1,
                &region);
        }

        // Pipeline barrier making compute buffer shader readable/writable
        {
            VkBufferMemoryBarrier barrier = {};
            barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.pNext               = nullptr;
            barrier.srcAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
            barrier.buffer              = buffer_nbody_compute.buffer;
            barrier.size                = buffer_nbody_compute.descriptor.range;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            // Make readable/writable for compute shader
            vkCmdPipelineBarrier(
                command_buffer_compute_step_2,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0, nullptr,
                1, &barrier,
                0, nullptr);
        }

        // Pipeline barrier making vertex buffer readable
        {
            VkBufferMemoryBarrier barrier = {};
            barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.pNext               = nullptr;
            barrier.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask       = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            barrier.buffer              = buffer_nbody_draw.buffer;
            barrier.size                = buffer_nbody_draw.descriptor.range;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            // Make readable/writable for compute shader
            vkCmdPipelineBarrier(
                command_buffer_compute_step_2,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                0,
                0, nullptr,
                1, &barrier,
                0, nullptr);
        }

        vkCmdWriteTimestamp(command_buffer_compute_step_2, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool_compute, 3);

        HANDLE_VK_RESULT(vkEndCommandBuffer(command_buffer_compute_step_2));
    }
}


void VulkanWindow::commandBuffersFree()
{
    HANDLE_VK_RESULT(vkQueueWaitIdle(vkbase.graphicsQueue()));

    vkFreeCommandBuffers(vkbase.device(), command_pool, static_cast<uint32_t> (command_buffer_draw.size()), command_buffer_draw.data());
    vkFreeCommandBuffers(vkbase.device(), command_pool, static_cast<uint32_t> (command_buffer_pre_present.size()), command_buffer_pre_present.data());
    vkFreeCommandBuffers(vkbase.device(), command_pool, static_cast<uint32_t> (command_buffer_post_present.size()), command_buffer_post_present.data());
    vkFreeCommandBuffers(vkbase.device(), command_pool, 1, &command_buffer_compute_step_1);
    vkFreeCommandBuffers(vkbase.device(), command_pool, 1, &command_buffer_compute_step_2);
}


void VulkanWindow::uniformBuffersPrepare()
{
    // Particles / compute
    {
        vulkan_helper->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            sizeof(ubo_nbody_graphics),
            nullptr,
            &uniform_nbody_graphics.buffer,
            &uniform_nbody_graphics.memory,
            &uniform_nbody_graphics.descriptor);

        HANDLE_VK_RESULT(vkMapMemory(vkbase.device(), uniform_nbody_graphics.memory, 0, sizeof(ubo_nbody_graphics), 0, (void **)&uniform_nbody_graphics.mapped));

        vulkan_helper->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            sizeof(ubo_nbody_compute),
            nullptr,
            &uniform_nbody_compute.buffer,
            &uniform_nbody_compute.memory,
            &uniform_nbody_compute.descriptor);

        HANDLE_VK_RESULT(vkMapMemory(vkbase.device(), uniform_nbody_compute.memory, 0, sizeof(ubo_nbody_compute), 0, (void **)&uniform_nbody_compute.mapped));
    }
    // Performance
    {
        vulkan_helper->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            sizeof(ubo_performance_meter_compute),
            &ubo_performance_meter_compute,
            &uniform_performance_compute.buffer,
            &uniform_performance_compute.memory,
            &uniform_performance_compute.descriptor);

        HANDLE_VK_RESULT(vkMapMemory(vkbase.device(), uniform_performance_compute.memory, 0, sizeof(ubo_performance_meter_compute), 0, (void **)&uniform_performance_compute.mapped));

        vulkan_helper->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            sizeof(ubo_performance_meter_graphics),
            &ubo_performance_meter_graphics,
            &uniform_performance_graphics.buffer,
            &uniform_performance_graphics.memory,
            &uniform_performance_graphics.descriptor);

        HANDLE_VK_RESULT(vkMapMemory(vkbase.device(), uniform_performance_graphics.memory, 0, sizeof(ubo_performance_meter_graphics), 0, (void **)&uniform_performance_graphics.mapped));
    }
    // Bloom
    {
        vulkan_helper->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            sizeof(ubo_blur),
            nullptr,
            &uniform_blur.buffer,
            &uniform_blur.memory,
            &uniform_blur.descriptor);


        HANDLE_VK_RESULT(vkMapMemory(vkbase.device(), uniform_blur.memory, 0, sizeof(ubo_blur), 0, (void **)&uniform_blur.mapped));
    }
    // Tone mapping
    {
        vulkan_helper->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            sizeof(ubo_tone_mapping),
            nullptr,
            &uniform_tone_mapping.buffer,
            &uniform_tone_mapping.memory,
            &uniform_tone_mapping.descriptor);


        HANDLE_VK_RESULT(vkMapMemory(vkbase.device(), uniform_tone_mapping.memory, 0, sizeof(ubo_tone_mapping), 0, (void **)&uniform_tone_mapping.mapped));
    }
    uniformBuffersUpdate();
}


void VulkanWindow::uniformBuffersUpdate()
{
    std::memcpy(ubo_nbody_graphics.matrix_projection, (camera_matrix).colmajor().toFloat().data(), 16 * sizeof(float));
    std::memcpy(ubo_nbody_graphics.matrix_view, (rotation_origin_matrix * zoom_matrix * rotation_matrix * translation_matrix).colmajor().toFloat().data(), 16 * sizeof(float));
    std::memcpy(ubo_nbody_graphics.matrix_model, (model_matrix).colmajor().toFloat().data(), 16 * sizeof(float));

    std::memcpy(uniform_nbody_graphics.mapped, &ubo_nbody_graphics, sizeof(ubo_nbody_graphics));
    std::memcpy(uniform_nbody_compute.mapped, &ubo_nbody_compute, sizeof(ubo_nbody_compute));
    std::memcpy(uniform_blur.mapped, &ubo_blur, sizeof(ubo_blur));
    std::memcpy(uniform_tone_mapping.mapped, &ubo_tone_mapping, sizeof(ubo_tone_mapping));
    std::memcpy(uniform_performance_compute.mapped, &ubo_performance_meter_compute, sizeof(ubo_performance_meter_compute));
    std::memcpy(uniform_performance_graphics.mapped, &ubo_performance_meter_graphics, sizeof(ubo_performance_meter_graphics));
}


void VulkanWindow::generateVerticesPerformanceMeterGraphics()
{
    // Staging buffers
    struct
    {
        VkBuffer       buf;
        VkDeviceMemory mem;
    }
    vertexStaging;

    // Setup vertices
    struct Vertex
    {
        float pos[2];
        float uv[2];
    };

    QVector<Vertex> vertexBuffer =
    {
        { {  1.0f, -0.98f }, { 1.0f, 1.0f } },
        { { -1.0f, -0.98f }, { 0.0f, 1.0f } },
        { { -1.0f, -0.95f }, { 0.0f, 0.0f } },
        { {  1.0f, -0.95f }, { 1.0f, 0.0f } }
    };
#undef dim
#undef normal

    uint32_t vertexBufferSize = static_cast<uint32_t> (vertexBuffer.size()) * sizeof(Vertex);

    vulkan_helper->createBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        vertexBufferSize,
        vertexBuffer.data(),
        &vertexStaging.buf,
        &vertexStaging.mem);

    vulkan_helper->createBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBufferSize,
        nullptr,
        &vertices_performance_meter_graphics.buffer,
        &vertices_performance_meter_graphics.memory);

    // Transfer staging buffers to device
    VkCommandBuffer copyCmd = commandBufferCreate();

    VkBufferCopy copyRegion = {};

    copyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(
        copyCmd,
        vertexStaging.buf,
        vertices_performance_meter_graphics.buffer,
        1,
        &copyRegion);

    commandBufferSubmitAndFree(copyCmd);

    // Destroy staging buffers
    vkDestroyBuffer(vkbase.device(), vertexStaging.buf, nullptr);
    vkFreeMemory(vkbase.device(), vertexStaging.mem, nullptr);

    // Binding description
    vertices_performance_meter_graphics.bindingDescriptions.resize(1);
    vertices_performance_meter_graphics.bindingDescriptions[0].binding   = VERTEX_BUFFER_BIND_ID;
    vertices_performance_meter_graphics.bindingDescriptions[0].stride    = sizeof(Vertex);
    vertices_performance_meter_graphics.bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    // Attribute descriptions
    vertices_performance_meter_graphics.attributeDescriptions.resize(2);

    // Location 0 : Position
    vertices_performance_meter_graphics.attributeDescriptions[0].location = 0;
    vertices_performance_meter_graphics.attributeDescriptions[0].binding  = VERTEX_BUFFER_BIND_ID;
    vertices_performance_meter_graphics.attributeDescriptions[0].format   = VK_FORMAT_R32G32_SFLOAT;
    vertices_performance_meter_graphics.attributeDescriptions[0].offset   = 0;

    // Location 1 : Texture coordinates
    vertices_performance_meter_graphics.attributeDescriptions[1].location = 1;
    vertices_performance_meter_graphics.attributeDescriptions[1].binding  = VERTEX_BUFFER_BIND_ID;
    vertices_performance_meter_graphics.attributeDescriptions[1].format   = VK_FORMAT_R32G32_SFLOAT;
    vertices_performance_meter_graphics.attributeDescriptions[1].offset   = sizeof(float) * 2;

    vertices_performance_meter_graphics.inputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertices_performance_meter_graphics.inputState.pNext = nullptr;
    vertices_performance_meter_graphics.inputState.flags = 0;
    vertices_performance_meter_graphics.inputState.vertexBindingDescriptionCount   = static_cast<uint32_t> (vertices_performance_meter_graphics.bindingDescriptions.size());
    vertices_performance_meter_graphics.inputState.pVertexBindingDescriptions      = vertices_performance_meter_graphics.bindingDescriptions.data();
    vertices_performance_meter_graphics.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t> (vertices_performance_meter_graphics.attributeDescriptions.size());
    vertices_performance_meter_graphics.inputState.pVertexAttributeDescriptions    = vertices_performance_meter_graphics.attributeDescriptions.data();
}


void VulkanWindow::generateVerticesPerformanceMeterCompute()
{
    // Staging buffers
    struct
    {
        VkBuffer       buf;
        VkDeviceMemory mem;
    }
    vertexStaging;

    // Setup vertices
    struct Vertex
    {
        float pos[2];
        float uv[2];
    };

    QVector<Vertex> vertexBuffer =
    {
        { {  1.0f, -0.93f }, { 1.0f, 1.0f } },
        { { -1.0f, -0.93f }, { 0.0f, 1.0f } },
        { { -1.0f, -0.90f }, { 0.0f, 0.0f } },
        { {  1.0f, -0.90f }, { 1.0f, 0.0f } }
    };
#undef dim
#undef normal

    uint32_t vertexBufferSize = static_cast<uint32_t> (vertexBuffer.size()) * sizeof(Vertex);

    vulkan_helper->createBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        vertexBufferSize,
        vertexBuffer.data(),
        &vertexStaging.buf,
        &vertexStaging.mem);

    vulkan_helper->createBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBufferSize,
        nullptr,
        &vertices_performance_meter_compute.buffer,
        &vertices_performance_meter_compute.memory);

    // Transfer staging buffers to device
    VkCommandBuffer copyCmd = commandBufferCreate();

    VkBufferCopy copyRegion = {};

    copyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(
        copyCmd,
        vertexStaging.buf,
        vertices_performance_meter_compute.buffer,
        1,
        &copyRegion);

    commandBufferSubmitAndFree(copyCmd);

    // Destroy staging buffers
    vkDestroyBuffer(vkbase.device(), vertexStaging.buf, nullptr);
    vkFreeMemory(vkbase.device(), vertexStaging.mem, nullptr);

    // Binding description
    vertices_performance_meter_compute.bindingDescriptions.resize(1);
    vertices_performance_meter_compute.bindingDescriptions[0].binding   = VERTEX_BUFFER_BIND_ID;
    vertices_performance_meter_compute.bindingDescriptions[0].stride    = sizeof(Vertex);
    vertices_performance_meter_compute.bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    // Attribute descriptions
    vertices_performance_meter_compute.attributeDescriptions.resize(2);

    // Location 0 : Position
    vertices_performance_meter_compute.attributeDescriptions[0].location = 0;
    vertices_performance_meter_compute.attributeDescriptions[0].binding  = VERTEX_BUFFER_BIND_ID;
    vertices_performance_meter_compute.attributeDescriptions[0].format   = VK_FORMAT_R32G32_SFLOAT;
    vertices_performance_meter_compute.attributeDescriptions[0].offset   = 0;

    // Location 1 : Texture coordinates
    vertices_performance_meter_compute.attributeDescriptions[1].location = 1;
    vertices_performance_meter_compute.attributeDescriptions[1].binding  = VERTEX_BUFFER_BIND_ID;
    vertices_performance_meter_compute.attributeDescriptions[1].format   = VK_FORMAT_R32G32_SFLOAT;
    vertices_performance_meter_compute.attributeDescriptions[1].offset   = sizeof(float) * 2;

    vertices_performance_meter_compute.inputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertices_performance_meter_compute.inputState.pNext = nullptr;
    vertices_performance_meter_compute.inputState.flags = 0;
    vertices_performance_meter_compute.inputState.vertexBindingDescriptionCount   = static_cast<uint32_t> (vertices_performance_meter_compute.bindingDescriptions.size());
    vertices_performance_meter_compute.inputState.pVertexBindingDescriptions      = vertices_performance_meter_compute.bindingDescriptions.data();
    vertices_performance_meter_compute.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t> (vertices_performance_meter_compute.attributeDescriptions.size());
    vertices_performance_meter_compute.inputState.pVertexAttributeDescriptions    = vertices_performance_meter_compute.attributeDescriptions.data();
}


void VulkanWindow::generateVerticesFullscreenQuad()
{
    // Staging buffers
    struct
    {
        VkBuffer       buf;
        VkDeviceMemory mem;
    }
    vertexStaging, indexStaging;

    // Setup vertices
    struct Vertex
    {
        float pos[2];
        float uv[2];
    };

#define DIM    1.0f
    QVector<Vertex> vertexBuffer =
    {
        { { DIM,  DIM  }, { 1.0f, 1.0f } },
        { { -DIM, DIM  }, { 0.0f, 1.0f } },
        { { -DIM, -DIM }, { 0.0f, 0.0f } },
        { { DIM,  -DIM }, { 1.0f, 0.0f } }
    };
#undef dim
#undef normal

    uint32_t vertexBufferSize = static_cast<uint32_t> (vertexBuffer.size()) * sizeof(Vertex);

    vulkan_helper->createBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        vertexBufferSize,
        vertexBuffer.data(),
        &vertexStaging.buf,
        &vertexStaging.mem);

    vulkan_helper->createBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBufferSize,
        nullptr,
        &vertices_fullscreen.buffer,
        &vertices_fullscreen.memory);

    // Setup indices
    QVector<uint32_t> indexBuffer = { 0, 1, 2, 2, 3, 0 };
    indices_quad.count = indexBuffer.size();
    uint32_t indexBufferSize = indices_quad.count * sizeof(uint32_t);

    vulkan_helper->createBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        indexBufferSize,
        indexBuffer.data(),
        &indexStaging.buf,
        &indexStaging.mem);

    vulkan_helper->createBuffer(
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBufferSize,
        nullptr,
        &indices_quad.buffer,
        &indices_quad.memory);

    // Transfer staging buffers to device
    VkCommandBuffer copyCmd = commandBufferCreate();

    VkBufferCopy copyRegion = {};

    copyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(
        copyCmd,
        vertexStaging.buf,
        vertices_fullscreen.buffer,
        1,
        &copyRegion);

    copyRegion.size = indexBufferSize;
    vkCmdCopyBuffer(
        copyCmd,
        indexStaging.buf,
        indices_quad.buffer,
        1,
        &copyRegion);

    commandBufferSubmitAndFree(copyCmd);

    // Destroy staging buffers
    vkDestroyBuffer(vkbase.device(), vertexStaging.buf, nullptr);
    vkFreeMemory(vkbase.device(), vertexStaging.mem, nullptr);
    vkDestroyBuffer(vkbase.device(), indexStaging.buf, nullptr);
    vkFreeMemory(vkbase.device(), indexStaging.mem, nullptr);

    // Binding description
    vertices_fullscreen.bindingDescriptions.resize(1);
    vertices_fullscreen.bindingDescriptions[0].binding   = VERTEX_BUFFER_BIND_ID;
    vertices_fullscreen.bindingDescriptions[0].stride    = sizeof(Vertex);
    vertices_fullscreen.bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    // Attribute descriptions
    vertices_fullscreen.attributeDescriptions.resize(2);

    // Location 0 : Position
    vertices_fullscreen.attributeDescriptions[0].location = 0;
    vertices_fullscreen.attributeDescriptions[0].binding  = VERTEX_BUFFER_BIND_ID;
    vertices_fullscreen.attributeDescriptions[0].format   = VK_FORMAT_R32G32_SFLOAT;
    vertices_fullscreen.attributeDescriptions[0].offset   = 0;

    // Location 1 : Texture coordinates
    vertices_fullscreen.attributeDescriptions[1].location = 1;
    vertices_fullscreen.attributeDescriptions[1].binding  = VERTEX_BUFFER_BIND_ID;
    vertices_fullscreen.attributeDescriptions[1].format   = VK_FORMAT_R32G32_SFLOAT;
    vertices_fullscreen.attributeDescriptions[1].offset   = sizeof(float) * 2;

    vertices_fullscreen.inputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertices_fullscreen.inputState.pNext = nullptr;
    vertices_fullscreen.inputState.flags = 0;
    vertices_fullscreen.inputState.vertexBindingDescriptionCount   = static_cast<uint32_t> (vertices_fullscreen.bindingDescriptions.size());
    vertices_fullscreen.inputState.pVertexBindingDescriptions      = vertices_fullscreen.bindingDescriptions.data();
    vertices_fullscreen.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t> (vertices_fullscreen.attributeDescriptions.size());
    vertices_fullscreen.inputState.pVertexAttributeDescriptions    = vertices_fullscreen.attributeDescriptions.data();
}


void VulkanWindow::initializeNbodies(QVector<Particle>& buffer, int method)
{
    std::mt19937 rng;
    rng.seed(std::random_device()());

    switch (method)
    {
    case 0:
        {
            // Initial particle positions
            std::normal_distribution<float> dist_distr(0.0f, 0.8f);
            std::normal_distribution<float> velocity_distr(0.0f, 0.5f);
            std::normal_distribution<float> mass_distr(0.5f, 0.1f);

            for (int i = 0; i < buffer.size() / 2; i++)
            {
                double mass = std::fabs(mass_distr(rng));//*2.0e30;

                QVector3D angular_velocity(0, 0, -0.4);

                QVector3D position_base(1.5, 0.1, 0.0);

                QVector3D position(position_base.x() + dist_distr(rng),
                                   position_base.y() + dist_distr(rng),
                                   position_base.z() + dist_distr(rng) * 0.1);

                QVector3D velocity_base(0 + velocity_distr(rng) * 0.02,
                                        0 + velocity_distr(rng) * 0.02,
                                        0 + velocity_distr(rng) * 0.02);

                QVector3D r = position - position_base;

                QVector3D velocity = QVector3D::crossProduct(r, angular_velocity) + velocity_base;

                buffer[i].xyzm[0] = position.x();
                buffer[i].xyzm[1] = position.y();
                buffer[i].xyzm[2] = position.z();
                buffer[i].xyzm[3] = mass;//*2.0e30;
                buffer[i].v[0]    = velocity.x();
                buffer[i].v[1]    = velocity.y();
                buffer[i].v[2]    = velocity.z();
            }


            for (int i = buffer.size() / 2; i < buffer.size(); i++)
            {
                double mass = std::fabs(mass_distr(rng));//*2.0e30;

                QVector3D angular_velocity(0, 0, -0.4);

                QVector3D position_base(-1.5, -0.1, 0.0);

                QVector3D position(position_base.x() + dist_distr(rng),
                                   position_base.y() + dist_distr(rng),
                                   position_base.z() + dist_distr(rng) * 0.1);

                QVector3D velocity_base(0 + velocity_distr(rng) * 0.02,
                                        0 + velocity_distr(rng) * 0.02,
                                        0 + velocity_distr(rng) * 0.02);

                QVector3D r = position - position_base;

                QVector3D velocity = QVector3D::crossProduct(r, angular_velocity) + velocity_base;//.normalized()*r.length()*0.3;//*std::sqrt(particleComputeUbo.G*(mass + 3000)/r.length());// + velocity_base;


                buffer[i].xyzm[0] = position.x();
                buffer[i].xyzm[1] = position.y();
                buffer[i].xyzm[2] = position.z();
                buffer[i].xyzm[3] = mass;
                buffer[i].v[0]    = velocity.x();
                buffer[i].v[1]    = velocity.y();
                buffer[i].v[2]    = velocity.z();
            }
            break;
        }

    case 1:
        {
            // Initial particle positions
            std::normal_distribution<float> dist_distr(0.0f, 0.7f);
            std::normal_distribution<float> velocity_distr(0.0f, 0.5f);
            std::normal_distribution<float> mass_distr(0.5f, 0.3f);

            for (int i = 0; i < buffer.size() / 2; i++)
            {
                double mass = std::fabs(mass_distr(rng));//*2.0e30;

                QVector3D angular_velocity(0, 0, -0.4);

                QVector3D position_base(1.5, 0.1, 0.0);

                QVector3D position(position_base.x() + dist_distr(rng),
                                   position_base.y() + dist_distr(rng),
                                   position_base.z() + dist_distr(rng) * 0.1);

                QVector3D velocity_base(0 + velocity_distr(rng) * 0.02,
                                        0 + velocity_distr(rng) * 0.02,
                                        0 + velocity_distr(rng) * 0.02);

                QVector3D r = position - position_base;

                QVector3D velocity = sqrt(ubo_nbody_compute.gravity_constant * (9500) / r.length()) * QVector3D::crossProduct(r, angular_velocity).normalized();// + velocity_base;

                buffer[i].xyzm[0] = position.x();
                buffer[i].xyzm[1] = position.y();
                buffer[i].xyzm[2] = position.z();
                buffer[i].xyzm[3] = mass;//*2.0e30;
                buffer[i].v[0]    = velocity.x();
                buffer[i].v[1]    = velocity.y();
                buffer[i].v[2]    = velocity.z();

                if (i == 0)
                {
                    buffer[i].xyzm[0] = position_base.x();
                    buffer[i].xyzm[1] = position_base.y();
                    buffer[i].xyzm[2] = position_base.z();
                    buffer[i].xyzm[3] = 9500;
                    buffer[i].v[0]    = 0;
                    buffer[i].v[1]    = 0;
                    buffer[i].v[2]    = 0;
                }
            }


            for (int i = buffer.size() / 2; i < buffer.size(); i++)
            {
                double mass = std::fabs(mass_distr(rng));//*2.0e30;

                QVector3D angular_velocity(0, 0, -0.4);

                QVector3D position_base(-1.5, -0.1, 0.0);

                QVector3D position(position_base.x() + dist_distr(rng),
                                   position_base.y() + dist_distr(rng),
                                   position_base.z() + dist_distr(rng) * 0.1);

                QVector3D velocity_base(0 + velocity_distr(rng) * 0.02,
                                        0 + velocity_distr(rng) * 0.02,
                                        0 + velocity_distr(rng) * 0.02);

                QVector3D r = position - position_base;

                QVector3D velocity = sqrt(ubo_nbody_compute.gravity_constant * (10000) / r.length()) * QVector3D::crossProduct(r, angular_velocity).normalized();// + velocity_base;

                buffer[i].xyzm[0] = position.x();
                buffer[i].xyzm[1] = position.y();
                buffer[i].xyzm[2] = position.z();
                buffer[i].xyzm[3] = mass;
                buffer[i].v[0]    = velocity.x();
                buffer[i].v[1]    = velocity.y();
                buffer[i].v[2]    = velocity.z();

                if (i == buffer.size() / 2)
                {
                    buffer[i].xyzm[0] = position_base.x();
                    buffer[i].xyzm[1] = position_base.y();
                    buffer[i].xyzm[2] = position_base.z();
                    buffer[i].xyzm[3] = 10000;
                    buffer[i].v[0]    = 0;
                    buffer[i].v[1]    = 0;
                    buffer[i].v[2]    = 0;
                }
            }
            break;
        }

    case 2:
        {
            // Initial particle positions
            std::normal_distribution<float> dist_distr(0.0f, 0.5f);
            std::normal_distribution<float> velocity_distr(0.0f, 0.5f);
            std::normal_distribution<float> mass_distr(0.5f, 0.1f);

            for (int i = 0; i < buffer.size(); i++)
            {
                double mass = std::fabs(mass_distr(rng));//*2.0e30;

                QVector3D angular_velocity(0, 0, 0.4);

                QVector3D position_base(0, 0, 0);

                QVector3D position(position_base.x() + dist_distr(rng),
                                   position_base.y() + dist_distr(rng),
                                   position_base.z() + dist_distr(rng));

                QVector3D velocity_base(0 + velocity_distr(rng) * 0.02,
                                        0 + velocity_distr(rng) * 0.02,
                                        0 + velocity_distr(rng) * 0.02);

                QVector3D r = position - position_base;

                QVector3D velocity = sqrt(ubo_nbody_compute.gravity_constant * (10000) / r.length()) * QVector3D::crossProduct(r, angular_velocity).normalized();// + velocity_base;

                buffer[i].xyzm[0] = position.x();
                buffer[i].xyzm[1] = position.y();
                buffer[i].xyzm[2] = position.z();
                buffer[i].xyzm[3] = mass;//*2.0e30;
                buffer[i].v[0]    = velocity.x();
                buffer[i].v[1]    = velocity.y();
                buffer[i].v[2]    = velocity.z();

                if (i == 0)
                {
                    buffer[i].xyzm[0] = position_base.x();
                    buffer[i].xyzm[1] = position_base.y();
                    buffer[i].xyzm[2] = position_base.z();
                    buffer[i].xyzm[3] = 10000;
                    buffer[i].v[0]    = 0;
                    buffer[i].v[1]    = 0;
                    buffer[i].v[2]    = 0;
                }
            }
            break;
        }

    case 3:
        {
            // Initial particle positions
            std::normal_distribution<float> dist_distr(0.0f, 0.001f);
            std::normal_distribution<float> mass_distr(0.5f, 0.1f);

            int side = static_cast<int>(std::ceil(std::pow(static_cast<double>(buffer.size()), 1.0 / 3.0)));

            if (side < 1)
            {
                side = 1;
            }

            for (int i = 0; i < side; i++)
            {
                for (int j = 0; j < side; j++)
                {
                    for (int k = 0; k < side; k++)
                    {
                        int index = i * side * side + j * side + k;

                        if (index < buffer.size())
                        {
                            double mass = std::fabs(mass_distr(rng));

                            QVector3D position_base((static_cast<double>(i) - static_cast<double>(side - 1) * 0.5) / static_cast<double>(side - 1) * 2,
                                                    (static_cast<double>(j) - static_cast<double>(side - 1) * 0.5) / static_cast<double>(side - 1) * 2,
                                                    (static_cast<double>(k) - static_cast<double>(side - 1) * 0.5) / static_cast<double>(side - 1) * 2);

                            QVector3D position(position_base.x() + dist_distr(rng),
                                               position_base.y() + dist_distr(rng),
                                               position_base.z() + dist_distr(rng));

                            buffer[index].xyzm[0] = position.x();
                            buffer[index].xyzm[1] = position.y();
                            buffer[index].xyzm[2] = position.z();
                            buffer[index].xyzm[3] = mass;
                            buffer[index].v[0]    = 0;
                            buffer[index].v[1]    = 0;
                            buffer[index].v[2]    = 0;
                        }
                    }
                }
            }
            break;
        }

    case 4:
        {
            // Initial particle positions
            std::normal_distribution<float> dist_distr(0.0f, 0.001f);
            std::normal_distribution<float> mass_distr(0.5f, 0.1f);

            int side = static_cast<int>(std::ceil(std::pow(static_cast<double>(buffer.size()), 1.0 / 3.0)));

            if (side < 1)
            {
                side = 1;
            }

            for (int i = 0; i < side; i++)
            {
                for (int j = 0; j < side; j++)
                {
                    for (int k = 0; k < side; k++)
                    {
                        int index = i * side * side + j * side + k;

                        if (index < buffer.size())
                        {
                            double mass = std::fabs(mass_distr(rng));

                            QVector3D position_base((static_cast<double>(i) - static_cast<double>(side - 1) * 0.5) / static_cast<double>(side - 1) * 2,
                                                    (static_cast<double>(j) - static_cast<double>(side - 1) * 0.5) / static_cast<double>(side - 1) * 2,
                                                    (static_cast<double>(k) - static_cast<double>(side - 1) * 0.5) / static_cast<double>(side - 1) * 2);

                            QVector3D position(position_base.x() + dist_distr(rng),
                                               position_base.y() + dist_distr(rng),
                                               position_base.z() + dist_distr(rng));

                            buffer[index].xyzm[0] = position.x();
                            buffer[index].xyzm[1] = position.y();
                            buffer[index].xyzm[2] = position.z();
                            buffer[index].xyzm[3] = mass;
                            buffer[index].v[0]    = 5.0 * std::sin(static_cast<double>(j) / static_cast<double>(side - 1) * 100);
                            buffer[index].v[1]    = 0;
                            buffer[index].v[2]    = 0;
                        }
                    }
                }
            }
            break;
        }

    case 5:
        {
            // Initial particle positions
            std::normal_distribution<float> dist_distr(0.0f, 1.0f);
            std::normal_distribution<float> mass_distr(0.5f, 0.1f);

            int blob_count       = 20;
            int blob_point_count = buffer.size() / blob_count;

            for (int i = 0; i < blob_count; i++)
            {
                QVector3D position_base(dist_distr(rng),
                                        dist_distr(rng),
                                        dist_distr(rng));

                for (int j = 0; j < blob_point_count; j++)
                {
                    int index = i * blob_point_count + j;

                    if (index < buffer.size())
                    {
                        QVector3D position(position_base.x() + dist_distr(rng) * 0.2,
                                           position_base.y() + dist_distr(rng) * 0.2,
                                           position_base.z() + dist_distr(rng) * 0.2);

                        double mass = std::fabs(mass_distr(rng));

                        buffer[index].xyzm[0] = position.x();
                        buffer[index].xyzm[1] = position.y();
                        buffer[index].xyzm[2] = position.z();
                        buffer[index].xyzm[3] = mass;
                        buffer[index].v[0]    = 0;
                        buffer[index].v[1]    = 0;
                        buffer[index].v[2]    = 0;
                    }
                }
            }
            break;
        }

    default:
        break;
    }
}


void VulkanWindow::passiveMove()
{
    double translation_speed = 2.0;

    if (p_key_shift_active)
    {
        translation_speed *= 5;
    }

    double time_since_last_movement = std::min((double)keyboard_movement_timer.nsecsElapsed() / (double)(1e9), 1.0 / 30.0);

    keyboard_movement_timer.restart();

    Matrix<double> initial_camera_direction(4, 1);
    initial_camera_direction[0] = 0;
    initial_camera_direction[1] = 0;
    initial_camera_direction[2] = -1;
    initial_camera_direction[3] = 1;

    Matrix<double> rotated_camera_direction = rotation_matrix.inverse4x4() * initial_camera_direction;

    Matrix<double> initial_camera_horizon(4, 1);
    initial_camera_horizon[0] = 1;
    initial_camera_horizon[1] = 0;
    initial_camera_horizon[2] = 0;
    initial_camera_horizon[3] = 1;

    Matrix<double> rotated_camera_horizon = rotation_matrix.inverse4x4() * initial_camera_horizon;

    Matrix<double> initial_camera_vertical(4, 1);
    initial_camera_vertical[0] = 0;
    initial_camera_vertical[1] = 1;
    initial_camera_vertical[2] = 0;
    initial_camera_vertical[3] = 1;

    Matrix<double> rotated_camera_vertical = rotation_matrix.inverse4x4() * initial_camera_vertical;

    Matrix<double> camera_translation;
    camera_translation.setIdentity(4);
    RotationMatrix<double> pitch_yaw_rotation;
    RotationMatrix<double> roll_rotation;

    if (p_key_w_active || p_key_s_active)
    {
        double magnitude = (p_key_s_active ? 1 : -1) * time_since_last_movement * translation_speed / zoom_matrix[0];
        camera_translation[3]  += rotated_camera_direction[0] * magnitude;
        camera_translation[7]  += rotated_camera_direction[1] * magnitude;
        camera_translation[11] += rotated_camera_direction[2] * magnitude;
    }
    if (p_key_a_active || p_key_d_active)
    {
        double magnitude = (p_key_a_active ? 1 : -1) * time_since_last_movement * translation_speed / zoom_matrix[0];
        camera_translation[3]  += rotated_camera_horizon[0] * magnitude;
        camera_translation[7]  += rotated_camera_horizon[1] * magnitude;
        camera_translation[11] += rotated_camera_horizon[2] * magnitude;
    }
    if (p_key_space_active)
    {
        double magnitude = time_since_last_movement * translation_speed / zoom_matrix[0];
        camera_translation[3]  += rotated_camera_vertical[0] * magnitude;
        camera_translation[7]  += rotated_camera_vertical[1] * magnitude;
        camera_translation[11] += rotated_camera_vertical[2] * magnitude;
    }
    if (p_key_q_active || p_key_e_active)
    {
        double x = rotated_camera_direction[0];
        double y = rotated_camera_direction[1];
        double z = rotated_camera_direction[2];

        double r   = std::sqrt(x * x + y * y + z * z);
        double eta = pi * 0.5 - std::acos(y / r);
        double zeta;
        if ((z == 0) && (x == 0))
        {
            zeta = 0;
        }
        else
        {
            zeta = -std::atan2(x, z);
        }
        double roll = time_since_last_movement * 1.5;

        if (p_key_e_active)
        {
            roll = -roll;
        }

        roll_rotation.setArbRotation(zeta, eta, roll);
    }

    // Mouse movement
    if (p_mouse_right_button_active && p_key_ctrl_active)
    {
        double eta = std::atan2(p_pitch_yaw_vector.y(), p_pitch_yaw_vector.x()) - 0.5 * pi;
        double relative_magnitude = std::min(((double)this->height() * 0.5), (double)std::sqrt(p_pitch_yaw_vector.x() * p_pitch_yaw_vector.x() + p_pitch_yaw_vector.y() * p_pitch_yaw_vector.y())) / ((double)this->height() * 0.5);
        double magnitude          = time_since_last_movement * std::pow(relative_magnitude, 2.0);

        pitch_yaw_rotation.setArbRotation(-0.5 * pi, eta, magnitude);
    }

    rotation_matrix = pitch_yaw_rotation * rotation_matrix * roll_rotation;

    translation_matrix = translation_matrix * camera_translation;

    uniformBuffersUpdate();
}


void VulkanWindow::generateVerticesNbodyInstance()
{
    struct
    {
        VkDeviceMemory memory;
        VkBuffer       buffer;
    }
    stagingBuffer;

    struct Vertex
    {
        float uv[2];
    };

    // "Corner" quad
    {
        QVector<Vertex> vertexBuffer =
        {
            { 1.0f, 1.0f },
            { 0.0f, 1.0f },
            { 0.0f, 0.0f },
            { 1.0f, 0.0f }
        };

        uint32_t vertexBufferSize = static_cast<uint32_t> (vertexBuffer.size()) * sizeof(Vertex);

        vulkan_helper->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            vertexBufferSize,
            vertexBuffer.data(),
            &stagingBuffer.buffer,
            &stagingBuffer.memory);

        vulkan_helper->createBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            vertexBufferSize,
            nullptr,
            &vertices_corner.buffer,
            &vertices_corner.memory);

        // Transfer staging buffers to device
        VkCommandBuffer copyCmd = commandBufferCreate();

        VkBufferCopy copyRegion = {};

        copyRegion.size = vertexBufferSize;
        vkCmdCopyBuffer(
            copyCmd,
            stagingBuffer.buffer,
            vertices_corner.buffer,
            1,
            &copyRegion);

        commandBufferSubmitAndFree(copyCmd);

        // Destroy staging buffers
        vkDestroyBuffer(vkbase.device(), stagingBuffer.buffer, nullptr);
        vkFreeMemory(vkbase.device(), stagingBuffer.memory, nullptr);
    }
}


void VulkanWindow::generateBuffersNbody()
{
    struct
    {
        VkDeviceMemory memory;
        VkBuffer       buffer;
    }
    stagingBuffer;

    struct Vertex
    {
        float uv[2];
    };

    {
        // Nbodies / particles
        ubo_nbody_compute.particle_count = initialization_particle_count;

        QVector<Particle> particleBuffer(ubo_nbody_compute.particle_count);
        initializeNbodies(particleBuffer, initial_condition);

        uint32_t storageBufferSize = particleBuffer.size() * sizeof(Particle);

        vulkan_helper->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            storageBufferSize,
            particleBuffer.data(),
            &stagingBuffer.buffer,
            &stagingBuffer.memory);

        vulkan_helper->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            storageBufferSize,
            nullptr,
            &buffer_nbody_compute.buffer,
            &buffer_nbody_compute.memory);

        vulkan_helper->createBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, // TODO: remove VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            storageBufferSize,
            nullptr,
            &buffer_nbody_draw.buffer,
            &buffer_nbody_draw.memory);

        // Copy to staging buffer
        VkCommandBuffer copyCmd = commandBufferCreate();

        VkBufferCopy copyRegion = {};
        copyRegion.size = storageBufferSize;
        vkCmdCopyBuffer(
            copyCmd,
            stagingBuffer.buffer,
            buffer_nbody_compute.buffer,
            1,
            &copyRegion);

        vkCmdCopyBuffer(
            copyCmd,
            stagingBuffer.buffer,
            buffer_nbody_draw.buffer,
            1,
            &copyRegion);


        // Todo: Barriers to change initial usage of buffers
        commandBufferSubmitAndFree(copyCmd);

        vkFreeMemory(vkbase.device(), stagingBuffer.memory, nullptr);
        vkDestroyBuffer(vkbase.device(), stagingBuffer.buffer, nullptr);

        buffer_nbody_compute.descriptor.range  = storageBufferSize;
        buffer_nbody_compute.descriptor.buffer = buffer_nbody_compute.buffer;
        buffer_nbody_compute.descriptor.offset = 0;

        buffer_nbody_draw.descriptor.range  = storageBufferSize;
        buffer_nbody_draw.descriptor.buffer = buffer_nbody_draw.buffer;
        buffer_nbody_draw.descriptor.offset = 0;
    }

    // Binding description
    vertices_nbody.bindingDescriptions.resize(2);
    vertices_nbody.bindingDescriptions[0].binding   = INSTANCE_BUFFER_BIND_ID;
    vertices_nbody.bindingDescriptions[0].stride    = sizeof(Particle);
    vertices_nbody.bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    vertices_nbody.bindingDescriptions[1].binding   = VERTEX_BUFFER_BIND_ID;
    vertices_nbody.bindingDescriptions[1].stride    = sizeof(Vertex);
    vertices_nbody.bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    // Attribute descriptions
    vertices_nbody.attributeDescriptions.resize(3);

    // Location 0 : Position and mass
    vertices_nbody.attributeDescriptions[0].binding  = INSTANCE_BUFFER_BIND_ID;
    vertices_nbody.attributeDescriptions[0].location = 0;
    vertices_nbody.attributeDescriptions[0].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertices_nbody.attributeDescriptions[0].offset   = 0;

    // Location 1 : Velocity
    vertices_nbody.attributeDescriptions[1].binding  = INSTANCE_BUFFER_BIND_ID;
    vertices_nbody.attributeDescriptions[1].location = 1;
    vertices_nbody.attributeDescriptions[1].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertices_nbody.attributeDescriptions[1].offset   = 4 * sizeof(float);

    // Location 2 : Instanced attribute
    vertices_nbody.attributeDescriptions[2].location = 2;
    vertices_nbody.attributeDescriptions[2].binding  = VERTEX_BUFFER_BIND_ID;
    vertices_nbody.attributeDescriptions[2].format   = VK_FORMAT_R32G32_SFLOAT;
    vertices_nbody.attributeDescriptions[2].offset   = 0;

    // Assign to vertex buffer
    vertices_nbody.inputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertices_nbody.inputState.pNext = nullptr;
    vertices_nbody.inputState.flags = 0;
    vertices_nbody.inputState.vertexBindingDescriptionCount   = vertices_nbody.bindingDescriptions.size();
    vertices_nbody.inputState.pVertexBindingDescriptions      = vertices_nbody.bindingDescriptions.data();
    vertices_nbody.inputState.vertexAttributeDescriptionCount = vertices_nbody.attributeDescriptions.size();
    vertices_nbody.inputState.pVertexAttributeDescriptions    = vertices_nbody.attributeDescriptions.data();
}


void VulkanWindow::descriptorSetLayoutsCreate()
{
    // Leapfrog
    {
        QVector<VkDescriptorSetLayoutBinding> bindings;
        {
            VkDescriptorSetLayoutBinding binding = {};
            binding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            binding.descriptorCount    = 1;
            binding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
            binding.pImmutableSamplers = nullptr;
            binding.binding            = 0;

            bindings << binding;
        }

        {
            VkDescriptorSetLayoutBinding binding = {};
            binding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            binding.descriptorCount    = 1;
            binding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
            binding.pImmutableSamplers = nullptr;
            binding.binding            = 1;

            bindings << binding;
        }

        VkDescriptorSetLayoutCreateInfo layout = {};
        layout.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout.pNext        = nullptr;
        layout.bindingCount = static_cast<uint32_t> (bindings.size());
        layout.pBindings    = bindings.data();

        HANDLE_VK_RESULT(vkCreateDescriptorSetLayout(vkbase.device(), &layout, nullptr, &descriptor_layout_leapfrog));
    }
    // Performance meter
    {
        QVector<VkDescriptorSetLayoutBinding> bindings;
        {
            VkDescriptorSetLayoutBinding binding = {};
            binding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            binding.descriptorCount    = 1;
            binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
            binding.pImmutableSamplers = nullptr;
            binding.binding            = 0;

            bindings << binding;
        }

        VkDescriptorSetLayoutCreateInfo layout = {};
        layout.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout.pNext        = nullptr;
        layout.bindingCount = static_cast<uint32_t> (bindings.size());
        layout.pBindings    = bindings.data();

        HANDLE_VK_RESULT(vkCreateDescriptorSetLayout(vkbase.device(), &layout, nullptr, &descriptor_layout_performance));
    }
    // Nbody draw
    {
        QVector<VkDescriptorSetLayoutBinding> bindings;

        {
            VkDescriptorSetLayoutBinding binding = {};
            binding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            binding.descriptorCount    = 1;
            binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
            binding.pImmutableSamplers = nullptr;
            binding.binding            = 0;

            bindings << binding;
        }
        {
            VkDescriptorSetLayoutBinding binding = {};
            binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount    = 1;
            binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
            binding.pImmutableSamplers = nullptr;
            binding.binding            = 1;

            bindings << binding;
        }
        {
            VkDescriptorSetLayoutBinding binding = {};
            binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount    = 1;
            binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
            binding.pImmutableSamplers = nullptr;
            binding.binding            = 2;

            bindings << binding;
        }

        VkDescriptorSetLayoutCreateInfo layout = {};
        layout.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout.pNext        = nullptr;
        layout.bindingCount = static_cast<uint32_t> (bindings.size());
        layout.pBindings    = bindings.data();

        HANDLE_VK_RESULT(vkCreateDescriptorSetLayout(vkbase.device(), &layout, nullptr, &descriptor_layout_nbody));
    }
    // Blur
    {
        QVector<VkDescriptorSetLayoutBinding> bindings;
        {
            VkDescriptorSetLayoutBinding binding = {};
            binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount    = 1;
            binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
            binding.pImmutableSamplers = nullptr;
            binding.binding            = 0;

            bindings << binding;
        }
        {
            VkDescriptorSetLayoutBinding binding = {};
            binding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            binding.descriptorCount    = 1;
            binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
            binding.pImmutableSamplers = nullptr;
            binding.binding            = 1;

            bindings << binding;
        }

        VkDescriptorSetLayoutCreateInfo layout = {};
        layout.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout.pNext        = nullptr;
        layout.bindingCount = static_cast<uint32_t> (bindings.size());
        layout.pBindings    = bindings.data();

        HANDLE_VK_RESULT(vkCreateDescriptorSetLayout(vkbase.device(), &layout, nullptr, &descriptor_layout_blur));
    }
    // Normal texture
    {
        QVector<VkDescriptorSetLayoutBinding> bindings;
        {
            VkDescriptorSetLayoutBinding binding = {};
            binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount    = 1;
            binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
            binding.pImmutableSamplers = nullptr;
            binding.binding            = 0;

            bindings << binding;
        }


        VkDescriptorSetLayoutCreateInfo layout = {};
        layout.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout.pNext        = nullptr;
        layout.bindingCount = static_cast<uint32_t> (bindings.size());
        layout.pBindings    = bindings.data();

        HANDLE_VK_RESULT(vkCreateDescriptorSetLayout(vkbase.device(), &layout, nullptr, &descriptor_layout_normal_texture));
    }
    // Tone mapping
    {
        QVector<VkDescriptorSetLayoutBinding> bindings;
        {
            VkDescriptorSetLayoutBinding binding = {};
            binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount    = 1;
            binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
            binding.pImmutableSamplers = nullptr;
            binding.binding            = 0;

            bindings << binding;
        }
        {
            VkDescriptorSetLayoutBinding binding = {};
            binding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            binding.descriptorCount    = 1;
            binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
            binding.pImmutableSamplers = nullptr;
            binding.binding            = 1;

            bindings << binding;
        }

        VkDescriptorSetLayoutCreateInfo layout = {};
        layout.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout.pNext        = nullptr;
        layout.bindingCount = static_cast<uint32_t> (bindings.size());
        layout.pBindings    = bindings.data();

        HANDLE_VK_RESULT(vkCreateDescriptorSetLayout(vkbase.device(), &layout, nullptr, &descriptor_layout_tone_mapping));
    }
}


void VulkanWindow::descriptorSetLayoutsDestroy()
{
    vkDestroyDescriptorSetLayout(vkbase.device(), descriptor_layout_nbody, nullptr);
    vkDestroyDescriptorSetLayout(vkbase.device(), descriptor_layout_leapfrog, nullptr);
    vkDestroyDescriptorSetLayout(vkbase.device(), descriptor_layout_performance, nullptr);
    vkDestroyDescriptorSetLayout(vkbase.device(), descriptor_layout_blur, nullptr);
    vkDestroyDescriptorSetLayout(vkbase.device(), descriptor_layout_normal_texture, nullptr);
    vkDestroyDescriptorSetLayout(vkbase.device(), descriptor_layout_tone_mapping, nullptr);
}


void VulkanWindow::descriptorPoolCreate()
{
    QVector<VkDescriptorPoolSize> type_counts(3);
    type_counts[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    type_counts[0].descriptorCount = 20;
    type_counts[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    type_counts[1].descriptorCount = 30;
    type_counts[2].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    type_counts[2].descriptorCount = 5;

    // Create the global descriptor pool
    VkDescriptorPoolCreateInfo descriptor_pool_info = {};
    descriptor_pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_info.pNext         = nullptr;
    descriptor_pool_info.poolSizeCount = static_cast<uint32_t>(type_counts.size());
    descriptor_pool_info.pPoolSizes    = type_counts.data();
    descriptor_pool_info.maxSets       = 30;

    HANDLE_VK_RESULT(vkCreateDescriptorPool(vkbase.device(), &descriptor_pool_info, nullptr, &descriptor_pool));
}


void VulkanWindow::descriptorPoolReset()
{
    HANDLE_VK_RESULT(vkResetDescriptorPool(vkbase.device(), descriptor_pool, 0));
}


void VulkanWindow::descriptorPoolDestroy()
{
    vkDestroyDescriptorPool(vkbase.device(), descriptor_pool, nullptr);
}


void VulkanWindow::descriptorSetsAllocate()
{
    VkDescriptorSetAllocateInfo allocate_info = {};

    allocate_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool     = descriptor_pool;
    allocate_info.descriptorSetCount = 1;
    // Leapfrog compute
    {
        allocate_info.pSetLayouts = &descriptor_layout_leapfrog;

        HANDLE_VK_RESULT(vkAllocateDescriptorSets(vkbase.device(), &allocate_info, &descriptor_leapgfrog));
    }
    // Performance
    {
        allocate_info.pSetLayouts = &descriptor_layout_performance;
        HANDLE_VK_RESULT(vkAllocateDescriptorSets(vkbase.device(), &allocate_info, &descriptor_performance_compute));
        HANDLE_VK_RESULT(vkAllocateDescriptorSets(vkbase.device(), &allocate_info, &descriptor_performance_graphics));
    }
    // Nbody
    {
        allocate_info.pSetLayouts = &descriptor_layout_nbody;
        HANDLE_VK_RESULT(vkAllocateDescriptorSets(vkbase.device(), &allocate_info, &descriptor_nbody));
    }
    // Luminosity
    {
        allocate_info.pSetLayouts = &descriptor_layout_normal_texture;
        HANDLE_VK_RESULT(vkAllocateDescriptorSets(vkbase.device(), &allocate_info, &descriptor_luminosity));
    }
    // Blur alpha
    {
        allocate_info.pSetLayouts = &descriptor_layout_blur;

        HANDLE_VK_RESULT(vkAllocateDescriptorSets(vkbase.device(), &allocate_info, &descriptor_blur_alpha));
    }
    // Blur beta
    {
        allocate_info.pSetLayouts = &descriptor_layout_blur;

        HANDLE_VK_RESULT(vkAllocateDescriptorSets(vkbase.device(), &allocate_info, &descriptor_blur_beta));
    }
    // Normal texture
    {
        allocate_info.pSetLayouts = &descriptor_layout_normal_texture;

        HANDLE_VK_RESULT(vkAllocateDescriptorSets(vkbase.device(), &allocate_info, &descriptor_normal_texture_scene));

        HANDLE_VK_RESULT(vkAllocateDescriptorSets(vkbase.device(), &allocate_info, &descriptor_normal_texture_blur));
    }
    // Tone mapping
    {
        allocate_info.pSetLayouts = &descriptor_layout_tone_mapping;

        HANDLE_VK_RESULT(vkAllocateDescriptorSets(vkbase.device(), &allocate_info, &descriptor_tone_mapping));
    }
}


void VulkanWindow::descriptorSetsUpdate()
{
    // Leapfrog compute
    {
        {
            VkWriteDescriptorSet write = {};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext           = nullptr;
            write.dstSet          = descriptor_leapgfrog;
            write.descriptorCount = 1;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.pBufferInfo     = &buffer_nbody_compute.descriptor;
            write.dstBinding      = 0;

            vkUpdateDescriptorSets(vkbase.device(), 1, &write, 0, nullptr);
        }
        {
            VkWriteDescriptorSet write = {};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext           = nullptr;
            write.dstSet          = descriptor_leapgfrog;
            write.descriptorCount = 1;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo     = &uniform_nbody_compute.descriptor;
            write.dstBinding      = 1;

            vkUpdateDescriptorSets(vkbase.device(), 1, &write, 0, nullptr);
        }
    }
    // Performance
    {
        {
            VkWriteDescriptorSet write = {};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext           = nullptr;
            write.dstSet          = descriptor_performance_graphics;
            write.descriptorCount = 1;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo     = &uniform_performance_graphics.descriptor;
            write.dstBinding      = 0;

            vkUpdateDescriptorSets(vkbase.device(), 1, &write, 0, nullptr);
        }
        {
            VkWriteDescriptorSet write = {};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext           = nullptr;
            write.dstSet          = descriptor_performance_compute;
            write.descriptorCount = 1;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo     = &uniform_performance_compute.descriptor;
            write.dstBinding      = 0;

            vkUpdateDescriptorSets(vkbase.device(), 1, &write, 0, nullptr);
        }
    }
    // Nbody draw
    {
        {
            VkWriteDescriptorSet write = {};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext           = nullptr;
            write.dstSet          = descriptor_nbody;
            write.descriptorCount = 1;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo     = &uniform_nbody_graphics.descriptor;
            write.dstBinding      = 0;

            vkUpdateDescriptorSets(vkbase.device(), 1, &write, 0, nullptr);
        }
        {
            VkDescriptorImageInfo image_info = {};
            image_info.sampler     = texture_particle.sampler;
            image_info.imageView   = texture_particle.view;
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet write = {};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext           = nullptr;
            write.dstSet          = descriptor_nbody;
            write.descriptorCount = 1;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo      = &image_info;
            write.dstBinding      = 1;

            vkUpdateDescriptorSets(vkbase.device(), 1, &write, 0, nullptr);
        }
        {
            VkDescriptorImageInfo image_info = {};
            image_info.sampler     = texture_noise.sampler;
            image_info.imageView   = texture_noise.view;
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet write = {};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext           = nullptr;
            write.dstSet          = descriptor_nbody;
            write.descriptorCount = 1;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo      = &image_info;
            write.dstBinding      = 2;

            vkUpdateDescriptorSets(vkbase.device(), 1, &write, 0, nullptr);
        }
    }
    // Luminosity
    {
        {
            VkDescriptorImageInfo image_info = {};
            image_info.sampler     = sampler_standard;
            image_info.imageView   = framebuffer_scene.color_attachment.view;
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet write = {};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext           = nullptr;
            write.descriptorCount = 1;
            write.dstSet          = descriptor_luminosity;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo      = &image_info;
            write.dstBinding      = 0;

            vkUpdateDescriptorSets(vkbase.device(), 1, &write, 0, nullptr);
        }
    }
    // Blur alpha
    {
        {
            VkDescriptorImageInfo image_info = {};
            image_info.sampler     = sampler_standard;
            image_info.imageView   = framebuffer_luminosity.color_attachment.view;
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet write = {};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext           = nullptr;
            write.descriptorCount = 1;
            write.dstSet          = descriptor_blur_alpha;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo      = &image_info;
            write.dstBinding      = 0;

            vkUpdateDescriptorSets(vkbase.device(), 1, &write, 0, nullptr);
        }

        {
            VkWriteDescriptorSet write = {};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext           = nullptr;
            write.descriptorCount = 1;
            write.dstSet          = descriptor_blur_alpha;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo     = &uniform_blur.descriptor;
            write.dstBinding      = 1;

            vkUpdateDescriptorSets(vkbase.device(), 1, &write, 0, nullptr);
        }
    }
    // Blur beta
    {
        {
            {
                VkDescriptorImageInfo image_info = {};
                image_info.sampler     = sampler_standard;
                image_info.imageView   = framebuffer_blur_alpha.color_attachment.view;
                image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                VkWriteDescriptorSet write = {};
                write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.pNext           = nullptr;
                write.descriptorCount = 1;
                write.dstSet          = descriptor_blur_beta;
                write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                write.pImageInfo      = &image_info;
                write.dstBinding      = 0;

                vkUpdateDescriptorSets(vkbase.device(), 1, &write, 0, nullptr);
            }

            {
                VkWriteDescriptorSet write = {};
                write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.pNext           = nullptr;
                write.descriptorCount = 1;
                write.dstSet          = descriptor_blur_beta;
                write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                write.pBufferInfo     = &uniform_blur.descriptor;
                write.dstBinding      = 1;

                vkUpdateDescriptorSets(vkbase.device(), 1, &write, 0, nullptr);
            }
        }
    }
    // Normal texture
    {
        {
            VkDescriptorImageInfo image_info = {};
            image_info.sampler     = sampler_standard;
            image_info.imageView   = framebuffer_scene.color_attachment.view;
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet write = {};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext           = nullptr;
            write.descriptorCount = 1;
            write.dstSet          = descriptor_normal_texture_scene;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo      = &image_info;
            write.dstBinding      = 0;

            vkUpdateDescriptorSets(vkbase.device(), 1, &write, 0, nullptr);
        }

        {
            VkDescriptorImageInfo image_info = {};
            image_info.sampler     = sampler_standard;
            image_info.imageView   = framebuffer_blur_beta.color_attachment.view;
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet write = {};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext           = nullptr;
            write.descriptorCount = 1;
            write.dstSet          = descriptor_normal_texture_blur;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo      = &image_info;
            write.dstBinding      = 0;

            vkUpdateDescriptorSets(vkbase.device(), 1, &write, 0, nullptr);
        }
    }
    // Tone mapping
    {
        {
            VkDescriptorImageInfo image_info = {};
            image_info.sampler     = sampler_standard;
            image_info.imageView   = framebuffer_combine.color_attachment.view;
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet write = {};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext           = nullptr;
            write.descriptorCount = 1;
            write.dstSet          = descriptor_tone_mapping;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo      = &image_info;
            write.dstBinding      = 0;

            vkUpdateDescriptorSets(vkbase.device(), 1, &write, 0, nullptr);
        }

        {
            VkWriteDescriptorSet write = {};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext           = nullptr;
            write.descriptorCount = 1;
            write.dstSet          = descriptor_tone_mapping;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo     = &uniform_tone_mapping.descriptor;
            write.dstBinding      = 1;

            vkUpdateDescriptorSets(vkbase.device(), 1, &write, 0, nullptr);
        }
    }
}


void VulkanWindow::descriptorSetsFree()
{
    HANDLE_VK_RESULT(vkFreeDescriptorSets(vkbase.device(), descriptor_pool, 1, &descriptor_leapgfrog));
    HANDLE_VK_RESULT(vkFreeDescriptorSets(vkbase.device(), descriptor_pool, 1, &descriptor_nbody));
    HANDLE_VK_RESULT(vkFreeDescriptorSets(vkbase.device(), descriptor_pool, 1, &descriptor_performance_compute));
    HANDLE_VK_RESULT(vkFreeDescriptorSets(vkbase.device(), descriptor_pool, 1, &descriptor_performance_graphics));
    HANDLE_VK_RESULT(vkFreeDescriptorSets(vkbase.device(), descriptor_pool, 1, &descriptor_luminosity));
    HANDLE_VK_RESULT(vkFreeDescriptorSets(vkbase.device(), descriptor_pool, 1, &descriptor_blur_alpha));
    HANDLE_VK_RESULT(vkFreeDescriptorSets(vkbase.device(), descriptor_pool, 1, &descriptor_blur_beta));
    HANDLE_VK_RESULT(vkFreeDescriptorSets(vkbase.device(), descriptor_pool, 1, &descriptor_normal_texture_scene));
    HANDLE_VK_RESULT(vkFreeDescriptorSets(vkbase.device(), descriptor_pool, 1, &descriptor_normal_texture_blur));
    HANDLE_VK_RESULT(vkFreeDescriptorSets(vkbase.device(), descriptor_pool, 1, &descriptor_tone_mapping));
}


void VulkanWindow::pipelineLayoutsCreate()
{
    // Pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};

    pipeline_layout_create_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.pNext                  = nullptr;
    pipeline_layout_create_info.flags                  = 0;
    pipeline_layout_create_info.setLayoutCount         = 1;
    pipeline_layout_create_info.pushConstantRangeCount = 0;
    pipeline_layout_create_info.pPushConstantRanges    = nullptr;
    {
        pipeline_layout_create_info.pSetLayouts = &descriptor_layout_nbody;
        HANDLE_VK_RESULT(vkCreatePipelineLayout(vkbase.device(), &pipeline_layout_create_info, nullptr, &pipeline_layout_nbody));
    }
    {
        pipeline_layout_create_info.pSetLayouts = &descriptor_layout_leapfrog;
        HANDLE_VK_RESULT(vkCreatePipelineLayout(vkbase.device(), &pipeline_layout_create_info, nullptr, &pipeline_layout_leapfrog));
    }
    {
        pipeline_layout_create_info.pSetLayouts = &descriptor_layout_performance;
        HANDLE_VK_RESULT(vkCreatePipelineLayout(vkbase.device(), &pipeline_layout_create_info, nullptr, &pipeline_layout_performance));
    }
    {
        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset     = 0;
        pushConstantRange.size       = sizeof(push_constants);

        pipeline_layout_create_info.pushConstantRangeCount = 1;
        pipeline_layout_create_info.pPushConstantRanges    = &pushConstantRange;
        pipeline_layout_create_info.pSetLayouts            = &descriptor_layout_blur;
        HANDLE_VK_RESULT(vkCreatePipelineLayout(vkbase.device(), &pipeline_layout_create_info, nullptr, &pipeline_layout_blur));
    }
    {
        pipeline_layout_create_info.pSetLayouts = &descriptor_layout_normal_texture;
        HANDLE_VK_RESULT(vkCreatePipelineLayout(vkbase.device(), &pipeline_layout_create_info, nullptr, &pipeline_layout_normal_texture));
    }
    {
        pipeline_layout_create_info.pSetLayouts = &descriptor_layout_tone_mapping;
        HANDLE_VK_RESULT(vkCreatePipelineLayout(vkbase.device(), &pipeline_layout_create_info, nullptr, &pipeline_layout_tone_mapping));
    }
}


void VulkanWindow::pipelineLayoutsDestroy()
{
    vkDestroyPipelineLayout(vkbase.device(), pipeline_layout_leapfrog, nullptr);
    vkDestroyPipelineLayout(vkbase.device(), pipeline_layout_performance, nullptr);
    vkDestroyPipelineLayout(vkbase.device(), pipeline_layout_nbody, nullptr);
    vkDestroyPipelineLayout(vkbase.device(), pipeline_layout_blur, nullptr);
    vkDestroyPipelineLayout(vkbase.device(), pipeline_layout_normal_texture, nullptr);
    vkDestroyPipelineLayout(vkbase.device(), pipeline_layout_tone_mapping, nullptr);
}


void VulkanWindow::pipelinesCreate()
{
    // Leapfrog
    {
        // Shaders
        VkShaderModule shader_module_leapfrog_step_1 = vulkan_helper->createVulkanShaderModule("shaders/nbody_leapfrog_step_one.comp.spv");
        VkShaderModule shader_module_leapfrog_step_2 = vulkan_helper->createVulkanShaderModule("shaders/nbody_leapfrog_step_two.comp.spv");

        VkPipelineShaderStageCreateInfo stages = {};
        stages.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages.pNext = nullptr;
        stages.flags = 0;
        stages.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stages.pName = "main";
        stages.pSpecializationInfo = nullptr;

        VkComputePipelineCreateInfo pipe_info = {};
        pipe_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipe_info.flags = 0;

        {
            stages.module    = shader_module_leapfrog_step_1;
            pipe_info.layout = pipeline_layout_leapfrog;
            pipe_info.stage  = stages;

            HANDLE_VK_RESULT(vkCreateComputePipelines(vkbase.device(), pipeline_cache, 1, &pipe_info, nullptr, &pipeline_compute_leapfrog_step_1));
        }
        {
            stages.module    = shader_module_leapfrog_step_2;
            pipe_info.layout = pipeline_layout_leapfrog;
            pipe_info.stage  = stages;

            HANDLE_VK_RESULT(vkCreateComputePipelines(vkbase.device(), pipeline_cache, 1, &pipe_info, nullptr, &pipeline_compute_leapfrog_step_2));
        }

        // Clean up shaders
        vulkan_helper->destroyVulkanShaderModule(shader_module_leapfrog_step_1);
        vulkan_helper->destroyVulkanShaderModule(shader_module_leapfrog_step_2);
    }

    VkPipelineViewportStateCreateInfo viewport_state_create_info = {};

    viewport_state_create_info.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_create_info.pNext         = nullptr;
    viewport_state_create_info.flags         = 0;
    viewport_state_create_info.viewportCount = 1;
    viewport_state_create_info.scissorCount  = 1;

    // Input assembly state
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {};
    input_assembly_state_create_info.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state_create_info.pNext    = nullptr;
    input_assembly_state_create_info.flags    = 0;
    input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_state_create_info.primitiveRestartEnable = VK_FALSE;

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {};
    rasterization_state_create_info.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state_create_info.pNext                   = nullptr;
    rasterization_state_create_info.flags                   = 0;
    rasterization_state_create_info.depthClampEnable        = VK_FALSE;
    rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;
    rasterization_state_create_info.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterization_state_create_info.cullMode                = VK_CULL_MODE_NONE;
    rasterization_state_create_info.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_state_create_info.depthBiasEnable         = VK_FALSE;
    rasterization_state_create_info.depthBiasConstantFactor = 0.0f;
    rasterization_state_create_info.depthBiasClamp          = 0.0f;
    rasterization_state_create_info.depthBiasSlopeFactor    = 0.0f;
    rasterization_state_create_info.lineWidth               = 1.0f;

    // Multisample state
    VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {};
    multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state_create_info.pNext = nullptr;
    multisample_state_create_info.flags = 0;
    multisample_state_create_info.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    multisample_state_create_info.sampleShadingEnable   = VK_FALSE;
    multisample_state_create_info.minSampleShading      = 1.0f;
    multisample_state_create_info.pSampleMask           = nullptr;
    multisample_state_create_info.alphaToCoverageEnable = VK_FALSE;
    multisample_state_create_info.alphaToOneEnable      = VK_FALSE;

    // Enable dynamic states
    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {};
    // The dynamic state properties themselves are stored in the command buffer
    std::vector<VkDynamicState> dynamicStateEnables;
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
    dynamic_state_create_info.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_create_info.pDynamicStates    = dynamicStateEnables.data();
    dynamic_state_create_info.dynamicStateCount = static_cast<uint32_t> (dynamicStateEnables.size());

    // Pipeline, assign states and create
    VkGraphicsPipelineCreateInfo pipeline_create_info = {};
    pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_create_info.pNext = nullptr;
    pipeline_create_info.flags = 0;
    pipeline_create_info.pInputAssemblyState = &input_assembly_state_create_info;
    pipeline_create_info.pTessellationState  = nullptr;
    pipeline_create_info.pViewportState      = &viewport_state_create_info;
    pipeline_create_info.pRasterizationState = &rasterization_state_create_info;
    pipeline_create_info.pMultisampleState   = &multisample_state_create_info;
    pipeline_create_info.pDynamicState       = &dynamic_state_create_info;
    pipeline_create_info.subpass             = 0;
    pipeline_create_info.basePipelineHandle  = VK_NULL_HANDLE;
    pipeline_create_info.basePipelineIndex   = -1;

    // Performance
    {
        // Blend state
        QVector<VkPipelineColorBlendAttachmentState> color_blend_attachment_states;
        {
            VkPipelineColorBlendAttachmentState state = {};
            state.blendEnable         = VK_TRUE;
            state.colorBlendOp        = VK_BLEND_OP_ADD;
            state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            state.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
            state.alphaBlendOp        = VK_BLEND_OP_ADD;
            state.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            color_blend_attachment_states << state;
        }

        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
        color_blend_state_create_info.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_create_info.pNext             = nullptr;
        color_blend_state_create_info.flags             = 0;
        color_blend_state_create_info.logicOpEnable     = VK_FALSE;
        color_blend_state_create_info.logicOp           = VK_LOGIC_OP_COPY;
        color_blend_state_create_info.attachmentCount   = static_cast<uint32_t>(color_blend_attachment_states.size());
        color_blend_state_create_info.pAttachments      = color_blend_attachment_states.data();
        color_blend_state_create_info.blendConstants[0] = 0.0f;
        color_blend_state_create_info.blendConstants[1] = 0.0f;
        color_blend_state_create_info.blendConstants[2] = 0.0f;
        color_blend_state_create_info.blendConstants[3] = 0.0f;

        pipeline_create_info.pDepthStencilState = nullptr;
        pipeline_create_info.pColorBlendState   = &color_blend_state_create_info;
        pipeline_create_info.renderPass         = render_pass_hdr;

        // Change some structs for this pipeline
        input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // Shaders
        VkShaderModule shader_module_vert = vulkan_helper->createVulkanShaderModule("shaders/performance_meter.vert.spv");
        VkShaderModule shader_module_frag = vulkan_helper->createVulkanShaderModule("shaders/performance_meter.frag.spv");

        // Pipeline shader stages
        QVector<VkPipelineShaderStageCreateInfo> pipeline_shader_stage_create_info(2);
        pipeline_shader_stage_create_info[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stage_create_info[0].pNext  = nullptr;
        pipeline_shader_stage_create_info[0].flags  = 0;
        pipeline_shader_stage_create_info[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        pipeline_shader_stage_create_info[0].module = shader_module_vert;
        pipeline_shader_stage_create_info[0].pName  = "main";
        pipeline_shader_stage_create_info[0].pSpecializationInfo = nullptr;

        pipeline_shader_stage_create_info[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stage_create_info[1].pNext  = nullptr;
        pipeline_shader_stage_create_info[1].flags  = 0;
        pipeline_shader_stage_create_info[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        pipeline_shader_stage_create_info[1].module = shader_module_frag;
        pipeline_shader_stage_create_info[1].pName  = "main";
        pipeline_shader_stage_create_info[1].pSpecializationInfo = nullptr;

        pipeline_create_info.stageCount        = static_cast<uint32_t> (pipeline_shader_stage_create_info.size());
        pipeline_create_info.pStages           = pipeline_shader_stage_create_info.data();
        pipeline_create_info.pVertexInputState = &vertices_performance_meter_graphics.inputState;
        pipeline_create_info.layout            = pipeline_layout_performance;

        HANDLE_VK_RESULT(vkCreateGraphicsPipelines(vkbase.device(), pipeline_cache, 1, &pipeline_create_info, nullptr, &pipeline_performance));

        // Clean up shaders
        vulkan_helper->destroyVulkanShaderModule(shader_module_vert);
        vulkan_helper->destroyVulkanShaderModule(shader_module_frag);
    }

    // Nbody draw
    {
        // Depth and stencil state
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {};
        // No stencil used
        depth_stencil_state_create_info.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_state_create_info.depthTestEnable       = VK_FALSE;
        depth_stencil_state_create_info.stencilTestEnable     = VK_TRUE;
        depth_stencil_state_create_info.depthWriteEnable      = VK_TRUE;
        depth_stencil_state_create_info.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;
        depth_stencil_state_create_info.depthBoundsTestEnable = VK_FALSE;
        depth_stencil_state_create_info.back.failOp           = VK_STENCIL_OP_KEEP;
        depth_stencil_state_create_info.back.passOp           = VK_STENCIL_OP_KEEP;
        depth_stencil_state_create_info.back.compareOp        = VK_COMPARE_OP_ALWAYS;
        depth_stencil_state_create_info.front                 = depth_stencil_state_create_info.back;

        // Blend state
        QVector<VkPipelineColorBlendAttachmentState> color_blend_attachment_states;
        {
            VkPipelineColorBlendAttachmentState state = {};
            state.blendEnable         = VK_TRUE;
            state.colorBlendOp        = VK_BLEND_OP_ADD;
            state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            state.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
            state.alphaBlendOp        = VK_BLEND_OP_ADD;
            state.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            color_blend_attachment_states << state;
        }

        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
        color_blend_state_create_info.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_create_info.pNext             = nullptr;
        color_blend_state_create_info.flags             = 0;
        color_blend_state_create_info.logicOpEnable     = VK_FALSE;
        color_blend_state_create_info.logicOp           = VK_LOGIC_OP_COPY;
        color_blend_state_create_info.attachmentCount   = static_cast<uint32_t>(color_blend_attachment_states.size());
        color_blend_state_create_info.pAttachments      = color_blend_attachment_states.data();
        color_blend_state_create_info.blendConstants[0] = 0.0f;
        color_blend_state_create_info.blendConstants[1] = 0.0f;
        color_blend_state_create_info.blendConstants[2] = 0.0f;
        color_blend_state_create_info.blendConstants[3] = 0.0f;

        pipeline_create_info.pDepthStencilState = &depth_stencil_state_create_info;
        pipeline_create_info.pColorBlendState   = &color_blend_state_create_info;
        pipeline_create_info.renderPass         = render_pass_hdr_color_depth;

        // Change some structs for this pipeline
        input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // Shaders
        VkShaderModule shader_module_vert = vulkan_helper->createVulkanShaderModule("shaders/nbody.vert.spv");
        VkShaderModule shader_module_frag = vulkan_helper->createVulkanShaderModule("shaders/nbody.frag.spv");

        // Pipeline shader stages
        QVector<VkPipelineShaderStageCreateInfo> pipeline_shader_stage_create_info(2);
        pipeline_shader_stage_create_info[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stage_create_info[0].pNext  = nullptr;
        pipeline_shader_stage_create_info[0].flags  = 0;
        pipeline_shader_stage_create_info[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        pipeline_shader_stage_create_info[0].module = shader_module_vert;
        pipeline_shader_stage_create_info[0].pName  = "main";
        pipeline_shader_stage_create_info[0].pSpecializationInfo = nullptr;

        pipeline_shader_stage_create_info[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stage_create_info[1].pNext  = nullptr;
        pipeline_shader_stage_create_info[1].flags  = 0;
        pipeline_shader_stage_create_info[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        pipeline_shader_stage_create_info[1].module = shader_module_frag;
        pipeline_shader_stage_create_info[1].pName  = "main";
        pipeline_shader_stage_create_info[1].pSpecializationInfo = nullptr;

        pipeline_create_info.stageCount        = static_cast<uint32_t> (pipeline_shader_stage_create_info.size());
        pipeline_create_info.pStages           = pipeline_shader_stage_create_info.data();
        pipeline_create_info.pVertexInputState = &vertices_nbody.inputState;
        pipeline_create_info.layout            = pipeline_layout_nbody;

        HANDLE_VK_RESULT(vkCreateGraphicsPipelines(vkbase.device(), pipeline_cache, 1, &pipeline_create_info, nullptr, &pipeline_nbody));

        // Clean up shaders
        vulkan_helper->destroyVulkanShaderModule(shader_module_vert);
        vulkan_helper->destroyVulkanShaderModule(shader_module_frag);
    }
    // Luminosity
    {
        // Blend state
        VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
        color_blend_attachment_state.blendEnable         = VK_TRUE;
        color_blend_attachment_state.colorBlendOp        = VK_BLEND_OP_ADD;
        color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
        color_blend_attachment_state.alphaBlendOp        = VK_BLEND_OP_ADD;
        color_blend_attachment_state.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
        color_blend_state_create_info.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_create_info.pNext             = nullptr;
        color_blend_state_create_info.flags             = 0;
        color_blend_state_create_info.logicOpEnable     = VK_FALSE;
        color_blend_state_create_info.logicOp           = VK_LOGIC_OP_COPY;
        color_blend_state_create_info.attachmentCount   = 1;
        color_blend_state_create_info.pAttachments      = &color_blend_attachment_state;
        color_blend_state_create_info.blendConstants[0] = 0.0f;
        color_blend_state_create_info.blendConstants[1] = 0.0f;
        color_blend_state_create_info.blendConstants[2] = 0.0f;
        color_blend_state_create_info.blendConstants[3] = 0.0f;

        pipeline_create_info.pDepthStencilState = nullptr;
        pipeline_create_info.pColorBlendState   = &color_blend_state_create_info;
        pipeline_create_info.renderPass         = render_pass_hdr;

        // Change some structs for this pipeline
        input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // Shaders
        VkShaderModule shader_module_vert = vulkan_helper->createVulkanShaderModule("shaders/luminosity.vert.spv");
        VkShaderModule shader_module_frag = vulkan_helper->createVulkanShaderModule("shaders/luminosity.frag.spv");

        // Pipeline shader stages
        QVector<VkPipelineShaderStageCreateInfo> pipeline_shader_stage_create_info(2);
        pipeline_shader_stage_create_info[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stage_create_info[0].pNext  = nullptr;
        pipeline_shader_stage_create_info[0].flags  = 0;
        pipeline_shader_stage_create_info[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        pipeline_shader_stage_create_info[0].module = shader_module_vert;
        pipeline_shader_stage_create_info[0].pName  = "main";
        pipeline_shader_stage_create_info[0].pSpecializationInfo = nullptr;

        pipeline_shader_stage_create_info[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stage_create_info[1].pNext  = nullptr;
        pipeline_shader_stage_create_info[1].flags  = 0;
        pipeline_shader_stage_create_info[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        pipeline_shader_stage_create_info[1].module = shader_module_frag;
        pipeline_shader_stage_create_info[1].pName  = "main";
        pipeline_shader_stage_create_info[1].pSpecializationInfo = nullptr;

        pipeline_create_info.stageCount        = static_cast<uint32_t> (pipeline_shader_stage_create_info.size());
        pipeline_create_info.pStages           = pipeline_shader_stage_create_info.data();
        pipeline_create_info.pVertexInputState = &vertices_fullscreen.inputState;
        pipeline_create_info.layout            = pipeline_layout_normal_texture;

        HANDLE_VK_RESULT(vkCreateGraphicsPipelines(vkbase.device(), pipeline_cache, 1, &pipeline_create_info, nullptr, &pipeline_luminosity));

        // Clean up shaders
        vulkan_helper->destroyVulkanShaderModule(shader_module_vert);
        vulkan_helper->destroyVulkanShaderModule(shader_module_frag);
    }
    // Blur
    {
        // Blend state
        VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
        color_blend_attachment_state.blendEnable         = VK_TRUE;
        color_blend_attachment_state.colorBlendOp        = VK_BLEND_OP_ADD;
        color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
        color_blend_attachment_state.alphaBlendOp        = VK_BLEND_OP_ADD;
        color_blend_attachment_state.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
        color_blend_state_create_info.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_create_info.pNext             = nullptr;
        color_blend_state_create_info.flags             = 0;
        color_blend_state_create_info.logicOpEnable     = VK_FALSE;
        color_blend_state_create_info.logicOp           = VK_LOGIC_OP_COPY;
        color_blend_state_create_info.attachmentCount   = 1;
        color_blend_state_create_info.pAttachments      = &color_blend_attachment_state;
        color_blend_state_create_info.blendConstants[0] = 0.0f;
        color_blend_state_create_info.blendConstants[1] = 0.0f;
        color_blend_state_create_info.blendConstants[2] = 0.0f;
        color_blend_state_create_info.blendConstants[3] = 0.0f;

        pipeline_create_info.pDepthStencilState = nullptr;
        pipeline_create_info.pColorBlendState   = &color_blend_state_create_info;
        pipeline_create_info.renderPass         = render_pass_hdr;

        // Change some structs for this pipeline
        input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // Shaders
        VkShaderModule shader_module_vert = vulkan_helper->createVulkanShaderModule("shaders/gaussblur.vert.spv");
        VkShaderModule shader_module_frag = vulkan_helper->createVulkanShaderModule("shaders/gaussblur.frag.spv");

        // Pipeline shader stages
        QVector<VkPipelineShaderStageCreateInfo> pipeline_shader_stage_create_info(2);
        pipeline_shader_stage_create_info[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stage_create_info[0].pNext  = nullptr;
        pipeline_shader_stage_create_info[0].flags  = 0;
        pipeline_shader_stage_create_info[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        pipeline_shader_stage_create_info[0].module = shader_module_vert;
        pipeline_shader_stage_create_info[0].pName  = "main";
        pipeline_shader_stage_create_info[0].pSpecializationInfo = nullptr;

        pipeline_shader_stage_create_info[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stage_create_info[1].pNext  = nullptr;
        pipeline_shader_stage_create_info[1].flags  = 0;
        pipeline_shader_stage_create_info[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        pipeline_shader_stage_create_info[1].module = shader_module_frag;
        pipeline_shader_stage_create_info[1].pName  = "main";
        pipeline_shader_stage_create_info[1].pSpecializationInfo = nullptr;

        pipeline_create_info.stageCount        = static_cast<uint32_t> (pipeline_shader_stage_create_info.size());
        pipeline_create_info.pStages           = pipeline_shader_stage_create_info.data();
        pipeline_create_info.pVertexInputState = &vertices_fullscreen.inputState;
        pipeline_create_info.layout            = pipeline_layout_blur;

        HANDLE_VK_RESULT(vkCreateGraphicsPipelines(vkbase.device(), pipeline_cache, 1, &pipeline_create_info, nullptr, &pipeline_blur));

        // Clean up shaders
        vulkan_helper->destroyVulkanShaderModule(shader_module_vert);
        vulkan_helper->destroyVulkanShaderModule(shader_module_frag);
    }
    // Normal texture
    {
        // Blend state
        VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
        color_blend_attachment_state.blendEnable         = VK_TRUE;
        color_blend_attachment_state.colorBlendOp        = VK_BLEND_OP_ADD;
        color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
        color_blend_attachment_state.alphaBlendOp        = VK_BLEND_OP_ADD;
        color_blend_attachment_state.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
        color_blend_state_create_info.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_create_info.pNext             = nullptr;
        color_blend_state_create_info.flags             = 0;
        color_blend_state_create_info.logicOpEnable     = VK_FALSE;
        color_blend_state_create_info.logicOp           = VK_LOGIC_OP_COPY;
        color_blend_state_create_info.attachmentCount   = 1;
        color_blend_state_create_info.pAttachments      = &color_blend_attachment_state;
        color_blend_state_create_info.blendConstants[0] = 0.0f;
        color_blend_state_create_info.blendConstants[1] = 0.0f;
        color_blend_state_create_info.blendConstants[2] = 0.0f;
        color_blend_state_create_info.blendConstants[3] = 0.0f;

        pipeline_create_info.pDepthStencilState = nullptr;
        pipeline_create_info.pColorBlendState   = &color_blend_state_create_info;
        pipeline_create_info.renderPass         = render_pass_hdr;

        // Change some structs for this pipeline
        input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // Shaders
        VkShaderModule shader_module_vert = vulkan_helper->createVulkanShaderModule("shaders/normal_texture.vert.spv");
        VkShaderModule shader_module_frag = vulkan_helper->createVulkanShaderModule("shaders/normal_texture.frag.spv");

        // Pipeline shader stages
        QVector<VkPipelineShaderStageCreateInfo> pipeline_shader_stage_create_info(2);
        pipeline_shader_stage_create_info[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stage_create_info[0].pNext  = nullptr;
        pipeline_shader_stage_create_info[0].flags  = 0;
        pipeline_shader_stage_create_info[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        pipeline_shader_stage_create_info[0].module = shader_module_vert;
        pipeline_shader_stage_create_info[0].pName  = "main";
        pipeline_shader_stage_create_info[0].pSpecializationInfo = nullptr;

        pipeline_shader_stage_create_info[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stage_create_info[1].pNext  = nullptr;
        pipeline_shader_stage_create_info[1].flags  = 0;
        pipeline_shader_stage_create_info[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        pipeline_shader_stage_create_info[1].module = shader_module_frag;
        pipeline_shader_stage_create_info[1].pName  = "main";
        pipeline_shader_stage_create_info[1].pSpecializationInfo = nullptr;

        pipeline_create_info.stageCount        = static_cast<uint32_t> (pipeline_shader_stage_create_info.size());
        pipeline_create_info.pStages           = pipeline_shader_stage_create_info.data();
        pipeline_create_info.pVertexInputState = &vertices_fullscreen.inputState;
        pipeline_create_info.layout            = pipeline_layout_normal_texture;

        HANDLE_VK_RESULT(vkCreateGraphicsPipelines(vkbase.device(), pipeline_cache, 1, &pipeline_create_info, nullptr, &pipeline_normal_texture));

        // Clean up shaders
        vulkan_helper->destroyVulkanShaderModule(shader_module_vert);
        vulkan_helper->destroyVulkanShaderModule(shader_module_frag);
    }
    {
        // Blend state
        VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
        color_blend_attachment_state.blendEnable         = VK_TRUE;
        color_blend_attachment_state.colorBlendOp        = VK_BLEND_OP_ADD;
        color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
        color_blend_attachment_state.alphaBlendOp        = VK_BLEND_OP_ADD;
        color_blend_attachment_state.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
        color_blend_state_create_info.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_create_info.pNext             = nullptr;
        color_blend_state_create_info.flags             = 0;
        color_blend_state_create_info.logicOpEnable     = VK_FALSE;
        color_blend_state_create_info.logicOp           = VK_LOGIC_OP_COPY;
        color_blend_state_create_info.attachmentCount   = 1;
        color_blend_state_create_info.pAttachments      = &color_blend_attachment_state;
        color_blend_state_create_info.blendConstants[0] = 0.0f;
        color_blend_state_create_info.blendConstants[1] = 0.0f;
        color_blend_state_create_info.blendConstants[2] = 0.0f;
        color_blend_state_create_info.blendConstants[3] = 0.0f;

        pipeline_create_info.pDepthStencilState = nullptr;
        pipeline_create_info.pColorBlendState   = &color_blend_state_create_info;
        pipeline_create_info.renderPass         = render_pass_ldr;

        // Change some structs for this pipeline
        input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // Shaders
        VkShaderModule shader_module_vert = vulkan_helper->createVulkanShaderModule("shaders/tone_mapping.vert.spv");
        VkShaderModule shader_module_frag = vulkan_helper->createVulkanShaderModule("shaders/tone_mapping.frag.spv");

        // Pipeline shader stages
        QVector<VkPipelineShaderStageCreateInfo> pipeline_shader_stage_create_info(2);
        pipeline_shader_stage_create_info[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stage_create_info[0].pNext  = nullptr;
        pipeline_shader_stage_create_info[0].flags  = 0;
        pipeline_shader_stage_create_info[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        pipeline_shader_stage_create_info[0].module = shader_module_vert;
        pipeline_shader_stage_create_info[0].pName  = "main";
        pipeline_shader_stage_create_info[0].pSpecializationInfo = nullptr;

        pipeline_shader_stage_create_info[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stage_create_info[1].pNext  = nullptr;
        pipeline_shader_stage_create_info[1].flags  = 0;
        pipeline_shader_stage_create_info[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        pipeline_shader_stage_create_info[1].module = shader_module_frag;
        pipeline_shader_stage_create_info[1].pName  = "main";
        pipeline_shader_stage_create_info[1].pSpecializationInfo = nullptr;

        pipeline_create_info.stageCount        = static_cast<uint32_t> (pipeline_shader_stage_create_info.size());
        pipeline_create_info.pStages           = pipeline_shader_stage_create_info.data();
        pipeline_create_info.pVertexInputState = &vertices_fullscreen.inputState;
        pipeline_create_info.layout            = pipeline_layout_tone_mapping;

        HANDLE_VK_RESULT(vkCreateGraphicsPipelines(vkbase.device(), pipeline_cache, 1, &pipeline_create_info, nullptr, &pipeline_tone_mapping));

        // Clean up shaders
        vulkan_helper->destroyVulkanShaderModule(shader_module_vert);
        vulkan_helper->destroyVulkanShaderModule(shader_module_frag);
    }
}


void VulkanWindow::pipelinesDestroy()
{
    vkDestroyPipeline(vkbase.device(), pipeline_compute_leapfrog_step_1, nullptr);
    vkDestroyPipeline(vkbase.device(), pipeline_compute_leapfrog_step_2, nullptr);
    vkDestroyPipeline(vkbase.device(), pipeline_performance, nullptr);
    vkDestroyPipeline(vkbase.device(), pipeline_nbody, nullptr);
    vkDestroyPipeline(vkbase.device(), pipeline_luminosity, nullptr);
    vkDestroyPipeline(vkbase.device(), pipeline_blur, nullptr);
    vkDestroyPipeline(vkbase.device(), pipeline_normal_texture, nullptr);
    vkDestroyPipeline(vkbase.device(), pipeline_tone_mapping, nullptr);
}


void VulkanWindow::pipelineCacheCreate()
{
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};

    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    HANDLE_VK_RESULT(vkCreatePipelineCache(vkbase.device(), &pipelineCacheCreateInfo, nullptr, &pipeline_cache));
}


void VulkanWindow::pipelineCacheDestroy()
{
    vkDestroyPipelineCache(vkbase.device(), pipeline_cache, nullptr);
}


void VulkanWindow::swapChainRecreate()
{
    // Recreate swap chain
    swapChainCreate(swapchain);
    swapChainImageViewsDestroy();
    swapChainImageViewsCreate();

    ubo_nbody_graphics.fbo_size[0] = static_cast<float>(surface_capabilities.currentExtent.width);
    ubo_nbody_graphics.fbo_size[1] = static_cast<float>(surface_capabilities.currentExtent.height);

    framebuffer_size_blur_pass =
    {
        static_cast<uint32_t>(surface_capabilities.currentExtent.width * framebuffer_size_blur_pass_multiplier),
        static_cast<uint32_t>(surface_capabilities.currentExtent.height * framebuffer_size_blur_pass_multiplier)
    };

    // Recreate depth stencil
    depthStencilDestroy();
    depthStencilCreate();

    // Recreate framebuffers
    frameBuffersDestroy();
    frameBuffersCreate();

    // Recreate descriptor sets
    descriptorPoolReset();
    descriptorSetsAllocate();
    descriptorSetsUpdate();

    // Recreate command buffers
    commandBuffersFree();
    commandBuffersAllocate();
    commandBuffersPresentRecord();
    commandBuffersComputeRecord();
    commandBuffersGraphicsRecord();

    camera_matrix.setWindow(surface_capabilities.currentExtent.width, surface_capabilities.currentExtent.height);

    uniformBuffersUpdate();
}


void VulkanWindow::surfaceCreate()
{
#if VK_USE_PLATFORM_WIN32_KHR
    VkWin32SurfaceCreateInfoKHR create_info = {};
    create_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_info.hinstance = GetModuleHandle(nullptr);
    create_info.hwnd      = reinterpret_cast<HWND> (this->winId());
    HANDLE_VK_RESULT(vkCreateWin32SurfaceKHR(vkbase.instance(), &create_info, nullptr, &surface));
#elif VK_USE_PLATFORM_XCB_KHR
    VkXcbSurfaceCreateInfoKHR create_info = {};
    create_info.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    create_info.connection = QX11Info::connection();
    create_info.window     = static_cast<xcb_window_t>(this->winId());
    HANDLE_VK_RESULT(vkCreateXcbSurfaceKHR(vkbase.instance(), &create_info, nullptr, &surface));
#endif

    VkBool32 WSI_supported = false;
    HANDLE_VK_RESULT(vkGetPhysicalDeviceSurfaceSupportKHR(vkbase.physicalDevice(), vkbase.queueFamilyIndex(), surface, &WSI_supported));
    if (!WSI_supported)
    {
        qFatal("WSI not supported");
    }
    HANDLE_VK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkbase.physicalDevice(), surface, &surface_capabilities));

    uint32_t format_count = 0;
    HANDLE_VK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(vkbase.physicalDevice(), surface, &format_count, nullptr));
    if (format_count == 0)
    {
        qFatal("Surface formats missing");
    }
    QVector<VkSurfaceFormatKHR> surface_formats(format_count);
    HANDLE_VK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(vkbase.physicalDevice(), surface, &format_count, surface_formats.data()));
    if (surface_formats[0].format == VK_FORMAT_UNDEFINED)
    {
        surface_format.format     = VK_FORMAT_R8G8B8A8_UNORM;//VK_FORMAT_R32G32B32A32_SFLOAT;
        surface_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    }
    else
    {
        surface_format = surface_formats[0];
    }

    // Since all depth formats may be optional, we need to find a suitable depth format to use
    // Start with the highest precision packed format
    std::vector<VkFormat> depth_formats =
    {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM
    };

    bool appropriate_depth_format_found = false;

    for (auto & format : depth_formats)
    {
        VkFormatProperties format_properties;
        vkGetPhysicalDeviceFormatProperties(vkbase.physicalDevice(), format, &format_properties);

        // Format must support depth stencil attachment for optimal tiling
        if (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            depth_format = format;
            appropriate_depth_format_found = true;
            break;
        }
    }

    if (!appropriate_depth_format_found)
    {
        qFatal("No appropriate depth format found");
    }
}


void VulkanWindow::surfaceDestroy()
{
    vkDestroySurfaceKHR(vkbase.instance(), surface, nullptr);
}
