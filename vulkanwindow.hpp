/*
 * A reimplemented QWindow that is displayed by the Qt GUI. This is where everything related to rendering happens.
 * */

#ifndef VULKANWINDOW_H
#define VULKANWINDOW_H
#include <QWindow>
#include <QTimer>
#include <QResizeEvent>
#include <QElapsedTimer>
#include <QQueue>
#include <QVector3D>

#include <random>
#include "BUILD_OPTIONS.h"
#include "platform.hpp"
#include "vulkanbase.hpp"
#include "include/matrix.hpp"
#include "include/ccmatrix.hpp"
#include "include/rotationmatrix.hpp"
#include "vulkantextureloader.hpp"

class VulkanWindow : public QWindow
{
    Q_OBJECT
public:
    VulkanWindow();
    ~VulkanWindow();

    void initialize();

public slots:
    void setGravitationalConstant(double value);
    void setSoftening(double value);
    void setTimeStep(double value);
    void setBloomStrength(int value);
    void setBloomExtent(int value);
    void setParticleCount(int value);
    void setPower(int value);
    void launch();
    void setInitialCondition(int value);
    void pauseCompute(bool value);
    void pauseAll(bool value);
    void setMouseSensitivity(int value);
    void setExposure(int value);
    void setGamma(int value);
    void setToneMappingMode(int value);
    void setParticleSize(int value);

private slots:
    void update();
    void createFpsString();
    void queueComputeSubmit();

signals:
    void fpsStringChanged(QString str);

protected:
    // Reimplemented virtual functions
    void focusOutEvent(QFocusEvent *ev) override;
    void resizeEvent(QResizeEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void mouseDoubleClickEvent(QMouseEvent *ev) override;
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void keyPressEvent(QKeyEvent *ev) override;
    void keyReleaseEvent(QKeyEvent *ev) override;
    void wheelEvent(QWheelEvent *ev) override;

private:
    // Texture loader
    VulkanTextureLoader *vulkan_texture_loader;

    StandaloneImage vulkan_depth_stencil;

    // Nbody
    struct
    {
        float matrix_projection[16];
        float matrix_model[16];
        float matrix_view[16];
        float fbo_size[2];
        float timestamp     = 0;
        float time_step     = 0.001f;
        float particle_size = 20;
    }
    ubo_nbody_graphics;

    struct
    {
        float    gravity_constant  = 0.001;
        float    time_step         = 0.002f;
        float    softening_squared = 0.005;
        float    power             = 1.5;
        uint32_t particle_count;
        uint32_t work_group_offset[3] = { 0, 0, 0 };
    }
    ubo_nbody_compute;

    struct Particle
    {
        float xyzm[4];
        float v[4];
    };

    uint32_t work_item_count_nbody[3] = { 128, 1, 1 }; // Must match that in shader

    UniformData buffer_nbody_compute;
    UniformData buffer_nbody_draw;
    UniformData uniform_nbody_graphics;
    UniformData uniform_nbody_compute;
    UniformData uniform_performance_graphics;
    UniformData uniform_performance_compute;

    // Textures
    VulkanTexture texture_particle;
    VulkanTexture texture_noise;

    // Vertex collections
    VertexCollection vertices_nbody;
    VertexCollection vertices_corner;
    VertexCollection vertices_fullscreen;
    VertexCollection vertices_performance_meter_graphics;
    VertexCollection vertices_performance_meter_compute;
    IndexCollection  indices_quad;

    // Samplers
    VkSampler sampler_standard = VK_NULL_HANDLE;


    // Bloom
    QVector<uint32_t> framebuffer_size_blur_pass;
    double            framebuffer_size_blur_pass_multiplier = 0.5;

    struct
    {
        float blur_extent   = 0.075f;
        float blur_strength = 0.45f;
    }
    ubo_blur;

    struct
    {
        int horizontal = 1;
    }
    push_constants;

    UniformData uniform_blur;

    // Tone mapping
    struct
    {
        float gamma               = 1.0;
        float exposure            = 2.0;
        int   tone_mapping_method = 1;
    }
    ubo_tone_mapping;

    UniformData uniform_tone_mapping;

    // Vulkan substructure
    VulkanBase vkbase;

    // Performance meter
    struct
    {
        float positions[8];
        int   process_count = 1;
        float relative_size = 1.0;
    }
    ubo_performance_meter_graphics,
        ubo_performance_meter_compute;

    // Queries
    void queryPoolCreate();
    void queryPoolDestroy();

    VkQueryPool query_pool_graphics = VK_NULL_HANDLE;
    VkQueryPool query_pool_compute  = VK_NULL_HANDLE;

    struct QueryResult
    {
        uint64_t time      = 0;
        uint64_t available = 0;
    };

    QVector<QueryResult> query_timestamp_graphics;
    QVector<QueryResult> query_timestamp_graphics_scene;
    QVector<QueryResult> query_timestamp_graphics_brightness;
    QVector<QueryResult> query_timestamp_graphics_blur_alpha;
    QVector<QueryResult> query_timestamp_graphics_blur_beta;
    QVector<QueryResult> query_timestamp_graphics_combine;
    QVector<QueryResult> query_timestamp_graphics_tone_map;
    QVector<QueryResult> query_timestamp_compute_leapfrog_step_1;
    QVector<QueryResult> query_timestamp_compute_leapfrog_step_2;

    // Surface
    VkSurfaceKHR             surface        = VK_NULL_HANDLE;
    VkSurfaceFormatKHR       surface_format = {};
    VkSurfaceCapabilitiesKHR surface_capabilities = {};
    void surfaceCreate();
    void surfaceDestroy();

    // Swapchain buffers
    VkSwapchainKHR       swapchain             = VK_NULL_HANDLE;
    uint32_t             swapchain_image_count = 2;
    QVector<VkImage>     swapchain_images;
    QVector<VkImageView> swapchain_image_views;
    void swapChainCreate(VkSwapchainKHR old_swapchain);
    void swapChainDestroy();
    void swapChainRecreate();
    void swapChainImageViewsCreate();
    void swapChainImageViewsDestroy();

    // Fences
    VkFence fence_draw           = VK_NULL_HANDLE;
    VkFence fence_transfer       = VK_NULL_HANDLE;
    VkFence fence_compute_step_1 = VK_NULL_HANDLE;
    void fencesCreate();
    void fencesDestroy();

    // Depth/stencil buffer
    VkFormat depth_format;
    void depthStencilCreate();
    void depthStencilDestroy();

    // Framebuffers
    struct FramebufferAttachment
    {
        VkImage        image;
        VkDeviceMemory mem;
        VkImageView    view;
        bool           enabled = false;
    };
    struct Framebuffer
    {
        uint32_t              width, height;
        VkFramebuffer         framebuffer;
        FramebufferAttachment color_attachment, depth_attachment;
    }
    framebuffer_scene,
        framebuffer_luminosity,
        framebuffer_blur_alpha,
        framebuffer_blur_beta,
        framebuffer_combine;

    QVector<VkFramebuffer> framebuffers_swapchain;
    void frameBuffersCreate();
    void frameBuffersDestroy();
    void framebufferSceneHDRCreate(Framebuffer *framebuffer);
    void framebufferStandardHDRCreate(Framebuffer *framebuffer, uint32_t width, uint32_t height,
                                      VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                      VkAccessFlags dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
                                      VkImageLayout new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    void hdrFramebufferDestroy(Framebuffer *framebuffer);

    // Render pass
    VkRenderPass render_pass_ldr             = VK_NULL_HANDLE;
    VkRenderPass render_pass_hdr_color_depth = VK_NULL_HANDLE;
    VkRenderPass render_pass_hdr             = VK_NULL_HANDLE;
    void renderPassesCreate();
    void renderPassDestroy();

    // Attributes and uniforms
    void uniformBuffersPrepare();
    void uniformBuffersUpdate();
    void generateVerticesNbodyInstance();
    void generateVerticesFullscreenQuad();
    void generateVerticesPerformanceMeterGraphics();
    void generateVerticesPerformanceMeterCompute();
    void generateBuffersNbody();

    // Descriptor set pool
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    void descriptorPoolCreate();
    void descriptorPoolReset();
    void descriptorPoolDestroy();

    // Descriptor set layouts
    VkDescriptorSetLayout descriptor_layout_nbody          = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_layout_performance    = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_layout_leapfrog       = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_layout_blur           = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_layout_normal_texture = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_layout_tone_mapping   = VK_NULL_HANDLE;
    void descriptorSetLayoutsCreate();
    void descriptorSetLayoutsDestroy();

    // Descriptor sets
    VkDescriptorSet descriptor_leapgfrog            = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_performance_graphics = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_performance_compute  = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_nbody                = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_luminosity           = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_blur_alpha           = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_blur_beta            = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_normal_texture_scene = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_normal_texture_blur  = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_tone_mapping         = VK_NULL_HANDLE;
    void descriptorSetsAllocate();
    void descriptorSetsUpdate();
    void descriptorSetsFree();

    // Pipeline layouts
    VkPipelineLayout pipeline_layout_leapfrog       = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_performance    = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_nbody          = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_blur           = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_normal_texture = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_tone_mapping   = VK_NULL_HANDLE;
    void pipelineLayoutsCreate();
    void pipelineLayoutsDestroy();

    // Pipelines
    VkPipeline      pipeline_compute_leapfrog_step_1 = VK_NULL_HANDLE;
    VkPipeline      pipeline_compute_leapfrog_step_2 = VK_NULL_HANDLE;
    VkPipeline      pipeline_performance             = VK_NULL_HANDLE;
    VkPipeline      pipeline_nbody          = VK_NULL_HANDLE;
    VkPipeline      pipeline_luminosity     = VK_NULL_HANDLE;
    VkPipeline      pipeline_blur           = VK_NULL_HANDLE;
    VkPipeline      pipeline_normal_texture = VK_NULL_HANDLE;
    VkPipeline      pipeline_tone_mapping   = VK_NULL_HANDLE;
    VkPipelineCache pipeline_cache          = VK_NULL_HANDLE;
    void pipelinesCreate();
    void pipelinesDestroy();
    void pipelineCacheCreate();
    void pipelineCacheDestroy();

    // Commands
    VkCommandPool            command_pool;
    QVector<VkCommandBuffer> command_buffer_pre_present;
    QVector<VkCommandBuffer> command_buffer_post_present;

    // Merge these two
    QVector<VkCommandBuffer> command_buffer_draw;
    VkCommandBuffer          command_buffer_compute_step_1 = VK_NULL_HANDLE;
    VkCommandBuffer          command_buffer_compute_step_2 = VK_NULL_HANDLE;

    void commandPoolCreate();
    void commandPoolDestroy();
    void commandBuffersAllocate();
    void commandBuffersFree();
    void commandBuffersPresentRecord();
    void commandBuffersGraphicsRecord();
    void commandBuffersComputeRecord();
    VkCommandBuffer commandBufferCreate();
    void commandBufferSubmitAndFree(VkCommandBuffer command_buffer);

    // Draw functions
    void queueGraphicsSubmit();

    // Semaphores
    VkSemaphore semaphore_present_complete        = VK_NULL_HANDLE;
    VkSemaphore semaphore_post_present_complete   = VK_NULL_HANDLE;
    VkSemaphore semaphore_compute_step_1_complete = VK_NULL_HANDLE;
    VkSemaphore semaphore_draw_complete           = VK_NULL_HANDLE;
    VkSemaphore semaphore_pre_present_complete    = VK_NULL_HANDLE;

    void semaphoresCreate();
    void semaphoresDestroy();

    // View matrices
    RotationMatrix<double> rotation_matrix;
    Matrix<double>         rotation_origin_matrix;
    Matrix<double>         translation_matrix;
    Matrix<double>         zoom_matrix;
    CCMatrix<double>       camera_matrix;
    Matrix<double>         model_matrix;

    // Mouse
    QPointF p_last_position;
    QPointF p_pitch_yaw_vector;
    bool    p_ignore_mousemove_event = false;

    // Keys, buttons
    bool p_key_w_active              = false;
    bool p_key_a_active              = false;
    bool p_key_s_active              = false;
    bool p_key_d_active              = false;
    bool p_key_q_active              = false;
    bool p_key_e_active              = false;
    bool p_key_space_active          = false;
    bool p_key_shift_active          = false;
    bool p_key_ctrl_active           = false;
    bool p_mouse_right_button_active = false;

    // Fps & cps
    QQueue<size_t> p_fps_stack;
    QQueue<size_t> p_cps_stack;
    QElapsedTimer  fps_timer;
    QElapsedTimer  cps_timer;
    QTimer         fps_update_timer;
    QElapsedTimer  uptime;
    double         time_total_graphics;
    double         time_total_compute;

    // Refresh timer
    QTimer *graphics_timer;
    QTimer *compute_timer;

    // Helper functions
    VulkanHelper *vulkan_helper;

    //Initialization of particles
    void initializeNbodies(QVector<Particle>& buffer, int method);

    int      initial_condition             = 1;
    uint32_t initialization_particle_count = 20000;

    // Keyboard movement
    QElapsedTimer keyboard_movement_timer;
    void passiveMove();

    // Mouse sensitivity
    double mouse_sensitivity = 0.2;
};

#endif // VULKANWINDOW_H
