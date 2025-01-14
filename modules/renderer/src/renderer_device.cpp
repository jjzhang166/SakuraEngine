#include "SkrRenderer/render_device.h"
#include "utils/make_zeroed.hpp"
#include "utils/threaded_service.h"
#include "platform/memory.h"
#include "cgpu/io.h"
#include <EASTL/vector_map.h>
#include <EASTL/string.h>
#ifdef _WIN32
#include "cgpu/extensions/dstorage_windows.h"
#endif
#include "cgpu/extensions/cgpu_nsight.h"

namespace skr
{
struct SKR_RENDERER_API RendererDeviceImpl : public RendererDevice
{
    friend class ::SkrRendererModule;
    void initialize(const Builder& builder) override;
    void finalize() override;

    CGPUSwapChainId register_window(SWindowHandle window) override;
    CGPUSwapChainId recreate_window_swapchain(SWindowHandle window) override;
    void create_api_objects(const Builder& builder) override;

    CGPUDeviceId get_cgpu_device() const override
    {
        return device;
    }

    ECGPUBackend get_backend() const override
    {
        return device->adapter->instance->backend;
    }

    CGPUQueueId get_gfx_queue() const override
    {
        return gfx_queue;
    }

    CGPUQueueId get_cpy_queue(uint32_t idx = 0) const override
    {
        if (idx < cpy_queues.size())
            return cpy_queues[idx];
        return cpy_queues[0];
    }

    CGPUDStorageQueueId get_file_dstorage_queue() const override
    {
        return file_dstorage_queue;
    }

    CGPUDStorageQueueId get_memory_dstorage_queue() const override
    {
        return memory_dstorage_queue;
    }

    ECGPUFormat get_swapchain_format() const override
    {
        if (swapchains.size())
            return (ECGPUFormat)swapchains.at(0).second->back_buffers[0]->format;
        return CGPU_FORMAT_B8G8R8A8_UNORM;
    }

    CGPUSamplerId get_linear_sampler() const override
    {
        return linear_sampler;
    }

    CGPURootSignaturePoolId get_root_signature_pool() const override
    {
        return root_signature_pool;
    }

    skr_io_vram_service_t* get_vram_service() const override
    {
        return vram_service;
    }

    uint32_t get_aux_service_count() const override
    {
        return (uint32_t)aux_services.size();
    }

    skr_threaded_service_t* get_aux_service(uint32_t index) const override
    {
        return aux_services.size() ? aux_services[index] : nullptr;
    }

#ifdef _WIN32
    skr_win_dstorage_decompress_service_id get_win_dstorage_decompress_service() const override
    {
        return decompress_service;
    }
#endif
protected:
    // Device objects
    uint32_t backbuffer_index = 0;
    eastl::vector_map<SWindowHandle, CGPUSurfaceId> surfaces;
    eastl::vector_map<SWindowHandle, CGPUSwapChainId> swapchains;
    CGPUInstanceId instance = nullptr;
    CGPUAdapterId adapter = nullptr;
    CGPUDeviceId device = nullptr;
    CGPUQueueId gfx_queue = nullptr;
    eastl::vector<CGPUQueueId> cpy_queues;
    CGPUSamplerId linear_sampler = nullptr;
    skr_io_vram_service_t* vram_service = nullptr;
    CGPUDStorageQueueId file_dstorage_queue = nullptr;
    CGPUDStorageQueueId memory_dstorage_queue = nullptr;
    CGPURootSignaturePoolId root_signature_pool = nullptr;
#ifdef _WIN32
    skr_win_dstorage_decompress_service_id decompress_service = nullptr;
#endif
    CGPUNSightTrackerId nsight_tracker = nullptr;
    eastl::vector<skr_threaded_service_t*> aux_services;
};

RendererDevice* RendererDevice::Create() SKR_NOEXCEPT
{
    return SkrNew<RendererDeviceImpl>();
}

void RendererDevice::Free(RendererDevice* device) SKR_NOEXCEPT
{
    SkrDelete(device);
}

void RendererDeviceImpl::initialize(const Builder& builder)
{
    create_api_objects(builder);

    auto vram_service_desc = make_zeroed<skr_vram_io_service_desc_t>();
    vram_service_desc.lockless = true;
    vram_service_desc.name = u8"vram_service";
    vram_service_desc.sleep_mode = SKR_ASYNC_SERVICE_SLEEP_MODE_SLEEP;
    vram_service_desc.sleep_time = 1000 / 60;
    vram_service_desc.sort_method = SKR_ASYNC_SERVICE_SORT_METHOD_PARTIAL;
    vram_service = skr_io_vram_service_t::create(&vram_service_desc);

    aux_services.resize(builder.aux_thread_count);
    for (uint32_t i = 0; i < aux_services.size(); i++)
    {
        auto name = "RenderAuxService-" + eastl::to_string(i);
        auto desc = make_zeroed<skr_threaded_service_desc_t>();
        desc.lockless = true;
        desc.name = (const char8_t*)name.c_str();
        desc.sleep_mode = SKR_ASYNC_SERVICE_SLEEP_MODE_COND_VAR;
        aux_services[i] = skr_threaded_service_t::create(&desc);
    }
}

void RendererDeviceImpl::finalize()
{
    skr_io_vram_service_t::destroy(vram_service);
    for (auto& swapchain : swapchains)
    {
        if (swapchain.second) cgpu_free_swapchain(swapchain.second);
    }
    swapchains.clear();
    for (auto& surface : surfaces)
    {
        if (surface.second) cgpu_free_surface(device, surface.second);
    }
    surfaces.clear();
    cgpu_free_sampler(linear_sampler);
    // free dstorage services & queues
#ifdef _WIN32
    cgpu_win_free_decompress_service(decompress_service);
#endif
    if(file_dstorage_queue) cgpu_free_dstorage_queue(file_dstorage_queue);
    if(memory_dstorage_queue) cgpu_free_dstorage_queue(memory_dstorage_queue);
    // free queues & device
    for (auto& cpy_queue : cpy_queues)
    {
        if (cpy_queue && cpy_queue != gfx_queue) 
            cgpu_free_queue(cpy_queue);
    }
    cpy_queues.clear();
    cgpu_free_queue(gfx_queue);
    cgpu_free_device(device);
    // free nsight tracker
    if (nsight_tracker) cgpu_free_nsight_tracker(nsight_tracker);
    cgpu_free_instance(instance);
    // join & destroy aux threads
    for (auto& aux_service : aux_services)
    {
        aux_service->drain();
        skr_threaded_service_t::destroy(aux_service);
    }
}

#define MAX_CPY_QUEUE_COUNT 2
void RendererDeviceImpl::create_api_objects(const Builder& builder)
{
    // create instance
    CGPUInstanceDescriptor instance_desc = {};
    instance_desc.backend = builder.backend;
    instance_desc.enable_debug_layer = builder.enable_debug_layer;
    instance_desc.enable_gpu_based_validation = builder.enable_gpu_based_validation;
    instance_desc.enable_set_name = builder.enable_set_name;
    instance = cgpu_create_instance(&instance_desc);

    // filter adapters
    uint32_t adapters_count = 0;
    cgpu_enum_adapters(instance, CGPU_NULLPTR, &adapters_count);
    CGPUAdapterId adapters[64];
    cgpu_enum_adapters(instance, adapters, &adapters_count);
    adapter = adapters[0];

    // create default nsight tracker on nvidia devices
    if (cgpux_adapter_is_nvidia(adapter))
    {
        CGPUNSightTrackerDescriptor desc = {};
        nsight_tracker = cgpu_create_nsight_tracker(instance, &desc);
    }

    // create device
    const auto cpy_queue_count_ = cgpu_query_queue_count(adapter, CGPU_QUEUE_TYPE_TRANSFER);
    CGPUQueueGroupDescriptor Gs[2];
    Gs[0].queue_type = CGPU_QUEUE_TYPE_GRAPHICS;
    Gs[0].queue_count = 1;
    Gs[1].queue_type = CGPU_QUEUE_TYPE_TRANSFER;
    Gs[1].queue_count = cgpu_min(cpy_queue_count_, MAX_CPY_QUEUE_COUNT);
    if (Gs[1].queue_count) // request at least one copy queue by default
    {
        CGPUDeviceDescriptor device_desc = {};
        device_desc.queue_groups = Gs;
        device_desc.queue_group_count = 2;
        device = cgpu_create_device(adapter, &device_desc);
        gfx_queue = cgpu_get_queue(device, CGPU_QUEUE_TYPE_GRAPHICS, 0);
        cpy_queues.resize(Gs[1].queue_count);
        for (uint32_t i = 0; i < cpy_queues.size(); i++)
        {
            cpy_queues[i] = cgpu_get_queue(device, CGPU_QUEUE_TYPE_TRANSFER, i);
        }
    }
    else // fallback: request only one graphics queue
    {
        CGPUDeviceDescriptor device_desc = {};
        device_desc.queue_groups = Gs;
        device_desc.queue_group_count = 1;
        device = cgpu_create_device(adapter, &device_desc);
        gfx_queue = cgpu_get_queue(device, CGPU_QUEUE_TYPE_GRAPHICS, 0);
        cpy_queues.emplace_back(gfx_queue);
    }

    // dstorage queue
    auto dstorage_cap = cgpu_query_dstorage_availability(device);
    const bool supportDirectStorage = (dstorage_cap != CGPU_DSTORAGE_AVAILABILITY_NONE);
    if (supportDirectStorage)
    {
#ifdef _WIN32
        cgpu_win_dstorage_set_staging_buffer_size(instance, 4096 * 4096 * 8);
#endif
        {
            auto queue_desc = make_zeroed<CGPUDStorageQueueDescriptor>();
            queue_desc.name = "DirectStorageFileQueue";
            queue_desc.capacity = CGPU_DSTORAGE_MAX_QUEUE_CAPACITY;
            queue_desc.source = CGPU_DSTORAGE_SOURCE_FILE;
            queue_desc.priority = CGPU_DSTORAGE_PRIORITY_NORMAL;
            file_dstorage_queue = cgpu_create_dstorage_queue(device, &queue_desc);
        }
        {
            auto queue_desc = make_zeroed<CGPUDStorageQueueDescriptor>();
            queue_desc.name = "DirectStorageMemoryQueue";
            queue_desc.capacity = CGPU_DSTORAGE_MAX_QUEUE_CAPACITY;
            queue_desc.source = CGPU_DSTORAGE_SOURCE_MEMORY;
            queue_desc.priority = CGPU_DSTORAGE_PRIORITY_NORMAL;
            memory_dstorage_queue = cgpu_create_dstorage_queue(device, &queue_desc);
        }
    }

    // create default linear sampler
    CGPUSamplerDescriptor sampler_desc = {};
    sampler_desc.address_u = CGPU_ADDRESS_MODE_REPEAT;
    sampler_desc.address_v = CGPU_ADDRESS_MODE_REPEAT;
    sampler_desc.address_w = CGPU_ADDRESS_MODE_REPEAT;
    sampler_desc.mipmap_mode = CGPU_MIPMAP_MODE_LINEAR;
    sampler_desc.min_filter = CGPU_FILTER_TYPE_LINEAR;
    sampler_desc.mag_filter = CGPU_FILTER_TYPE_LINEAR;
    sampler_desc.compare_func = CGPU_CMP_NEVER;
    linear_sampler = cgpu_create_sampler(device, &sampler_desc);

#ifdef _WIN32
    decompress_service = cgpu_win_create_decompress_service(instance);
#endif
}

CGPUSwapChainId RendererDeviceImpl::register_window(SWindowHandle window)
{
    // find registered swapchain
    {
        auto _ = swapchains.find(window);
        if (_ != swapchains.end()) return _->second;
    }
    // find registered surface
    CGPUSurfaceId surface = nullptr;
    {
        auto _ = surfaces.find(window);
        if (_ != surfaces.end())
            surface = _->second;
        else
        {
            surface = cgpu_surface_from_native_view(device, skr_window_get_native_view(window));
            surfaces[window] = surface;
        }
    }
    int32_t width, height;
    skr_window_get_extent(window, &width, &height);
    // create swapchain
    CGPUSwapChainDescriptor chain_desc = {};
    chain_desc.present_queues = &gfx_queue;
    chain_desc.present_queues_count = 1;
    chain_desc.width = width;
    chain_desc.height = height;
    chain_desc.surface = surface;
    chain_desc.imageCount = 2;
    chain_desc.format = CGPU_FORMAT_B8G8R8A8_UNORM;
    chain_desc.enable_vsync = false;
    auto swapchain = cgpu_create_swapchain(device, &chain_desc);
    swapchains[window] = swapchain;
    return swapchain;
}

CGPUSwapChainId RendererDeviceImpl::recreate_window_swapchain(SWindowHandle window)
{
    // find registered
    CGPUSwapChainId old = nullptr;
    {
        auto _ = swapchains.find(window);
        if (_ == swapchains.end()) return nullptr;
        else old = _->second;
    }
    CGPUSurfaceId surface = nullptr;
    // free existed
    {
        auto _ = surfaces.find(window);
        cgpu_free_swapchain(old);
        if (_ != surfaces.end())
        {
            cgpu_free_surface(device, _->second);
        }
        {
            surface = cgpu_surface_from_native_view(device, skr_window_get_native_view(window));
            surfaces[window] = surface;
        }
    }
    int32_t width, height;
    skr_window_get_extent(window, &width, &height);
    // create swapchain
    CGPUSwapChainDescriptor chain_desc = {};
    chain_desc.present_queues = &gfx_queue;
    chain_desc.present_queues_count = 1;
    chain_desc.width = width;
    chain_desc.height = height;
    chain_desc.surface = surface;
    chain_desc.imageCount = 2;
    chain_desc.format = CGPU_FORMAT_B8G8R8A8_UNORM;
    chain_desc.enable_vsync = false;
    auto swapchain = cgpu_create_swapchain(gfx_queue->device, &chain_desc);
    swapchains[window] = swapchain;
    return swapchain;
}
}