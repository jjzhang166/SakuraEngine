#pragma once
#include "SkrGuiRenderer/module.configure.h"
#include "SkrGui/interface/gdi_renderer.hpp"
#include "SkrRenderGraph/frontend/render_graph.hpp"
#include <containers/vector.hpp>
#include <EASTL/vector_map.h>

#include "cgpu/cgpux.h"
#include "cgpu/io.h"
#include "rtm/rtmx.h"
#include "platform/atomic.h"
#include "utils/threaded_service.h"

namespace skr {
namespace gdi {

enum EGDIRendererPipelineAttribute
{
    GDI_RENDERER_PIPELINE_ATTRIBUTE_TEST_Z = 0x00000001,
    GDI_RENDERER_PIPELINE_ATTRIBUTE_TEXTURED = GDI_RENDERER_PIPELINE_ATTRIBUTE_TEST_Z << 1,
    GDI_RENDERER_PIPELINE_ATTRIBUTE_WRITE_Z = GDI_RENDERER_PIPELINE_ATTRIBUTE_TEXTURED << 1,
    GDI_RENDERER_PIPELINE_ATTRIBUTE_CUSTOM_SAMPLER = GDI_RENDERER_PIPELINE_ATTRIBUTE_WRITE_Z << 1,
    GDI_RENDERER_PIPELINE_ATTRIBUTE_COUNT = 4
};
using GDIRendererPipelineAttributes = uint32_t;

struct SGDIElementDrawCommand_RenderGraph
{
    GDIRendererPipelineAttributes attributes = 0;
    SGDITextureId texture = nullptr;
    SGDIMaterialId material = nullptr;
    uint32_t first_index = 0;
    uint32_t index_count = 0;
    uint32_t ib_offset = 0;
    uint32_t vb_offset = 0;
    uint32_t tb_offset = 0;
    uint32_t pb_offset = 0;
};

struct SKR_GUI_RENDERER_API SGDICanvasGroupData_RenderGraph
{
    inline SGDICanvasGroupData_RenderGraph(SGDICanvasGroup* group) SKR_NOEXCEPT : canvas_group(group) { }

    SGDICanvasGroup* canvas_group = nullptr;

    skr::vector<skr::render_graph::BufferHandle> vertex_buffers;
    skr::vector<skr::render_graph::BufferHandle> transform_buffers;
    skr::vector<skr::render_graph::BufferHandle> projection_buffers;
    skr::vector<skr::render_graph::BufferHandle> index_buffers;

    skr::vector<SGDIElementDrawCommand_RenderGraph> render_commands;
    skr::vector<SGDIVertex> render_vertices;
    skr::vector<rtm::matrix4x4f> render_transforms;
    skr::vector<rtm::matrix4x4f> render_projections;
    skr::vector<index_t> render_indices;
};

struct SGDIRendererDescriptor_RenderGraph
{
    ECGPUFormat target_format;
    CGPUDeviceId device = nullptr;
    CGPUQueueId transfer_queue = nullptr;
    skr_vfs_t* vfs = nullptr;
    skr_io_ram_service_t* ram_service = nullptr;
    skr_io_vram_service_t* vram_service = nullptr;
    skr_threaded_service_t* aux_service = nullptr;
};

struct SGDIRenderParams_RenderGraph
{
    skr::render_graph::RenderGraph* render_graph = nullptr;
};

struct SGDIImageDescriptor_RenderGraph
{
    bool useImageCoder = false;
    skr_guid_t guid;
};

struct SGDITextureDescriptor_RenderGraph
{
    bool useImageCoder = false;
    skr_guid_t guid;
};

struct SKR_GUI_RENDERER_API SGDIImageAsyncData_RenderGraph
{
    friend struct SGDITextureAsyncData_RenderGraph;

    struct 
    {
        skr::string uri = "";
    } from_file;
    
    struct 
    {
        uint32_t width;
        uint32_t height;
        EGDIImageFormat format;
        // TODO: mip_count
        // uint32_t mip_count = 0;
    } from_data;

    bool useImageCoder = false;
    skr_async_request_t ram_request = {};
    skr_async_request_t aux_request = {};
    skr_threaded_service_t* aux_service = nullptr; 
    
    SGDIImageId DoAsync(struct SGDIImage_RenderGraph* owner, skr_vfs_t* vfs, skr_io_ram_service_t* ram_service) SKR_NOEXCEPT;

protected:
    eastl::function<void()> ram_io_enqueued_callback = {};
    eastl::function<void()> ram_io_finished_callback = {};
    eastl::function<void()> ram_data_finsihed_callback = {};
};

struct SKR_GUI_RENDERER_API SGDITextureAsyncData_RenderGraph
{
    skr_async_request_t vram_request = {};
    skr_async_vtexture_destination_t vram_destination = {};

    CGPUDeviceId device;
    CGPUQueueId transfer_queue;
    CGPURootSignatureId root_signature = nullptr;
    skr_io_vram_service_t* vram_service = nullptr; 

    SGDITextureId DoAsync(struct SGDITexture_RenderGraph* texture, skr_vfs_t* vfs, skr_io_ram_service_t* ram_service) SKR_NOEXCEPT;

protected:
    eastl::function<void()> vram_enqueued_callback = {};
    eastl::function<void()> vram_finsihed_callback = {};
};

struct SKR_GUI_RENDERER_API SGDIImage_RenderGraph : public SGDIImage
{
    SGDIImage_RenderGraph(struct SGDIRenderer_RenderGraph* renderer) SKR_NOEXCEPT
        : renderer(renderer) { }

    EGDIResourceState get_state() const SKR_NOEXCEPT final;
    SGDIRendererId get_renderer() const SKR_NOEXCEPT final;
    uint32_t get_width() const SKR_NOEXCEPT final;
    uint32_t get_height() const SKR_NOEXCEPT final;
    LiteSpan<const uint8_t> get_data() const SKR_NOEXCEPT final;
    EGDIImageFormat get_format() const SKR_NOEXCEPT final;

    void preInit(const SGDIImageDescriptor* desc);

    uint32_t image_width = 0;
    uint32_t image_height = 0;
    uint32_t image_depth = 0;
    ECGPUFormat format = CGPU_FORMAT_UNDEFINED;

    SAtomic32 state = static_cast<uint32_t>(EGDIResourceState::Requsted);
    EGDIImageSource source = EGDIImageSource::Count;

    skr_blob_t pixel_data;
    skr_async_ram_destination_t raw_data = {};

    SGDIImageAsyncData_RenderGraph async_data;
    struct SGDIRenderer_RenderGraph* renderer = nullptr;
};

struct SKR_GUI_RENDERER_API SGDITexture_RenderGraph : public SGDITexture
{
    SGDITexture_RenderGraph(struct SGDIRenderer_RenderGraph* renderer) SKR_NOEXCEPT
        : intermediate_image(renderer), renderer(renderer) { }

    EGDIResourceState get_state() const SKR_NOEXCEPT final;
    SGDIRendererId get_renderer() const SKR_NOEXCEPT final;
    uint32_t get_width() const SKR_NOEXCEPT final;
    uint32_t get_height() const SKR_NOEXCEPT final;

    EGDITextureType get_type() const SKR_NOEXCEPT final;

    void intializeBindTable() SKR_NOEXCEPT;
    
    SAtomic32 state = static_cast<uint32_t>(EGDIResourceState::Requsted);
    EGDITextureSource source = EGDITextureSource::Count;
    SGDITextureAsyncData_RenderGraph async_data;

    SGDIImage_RenderGraph intermediate_image;

    CGPUTextureId texture = nullptr;
    CGPUTextureViewId texture_view = nullptr;
    CGPUSamplerId sampler = nullptr;
    CGPUXBindTableId bind_table = nullptr;
    struct SGDIRenderer_RenderGraph* renderer = nullptr;
};

struct SKR_GUI_RENDERER_API SGDIRenderer_RenderGraph : public SGDIRenderer
{
    // Tier 1
    int initialize(const SGDIRendererDescriptor* desc) SKR_NOEXCEPT final;
    int finalize() SKR_NOEXCEPT final;
    SGDIImageId create_image(const SGDIImageDescriptor* descriptor) SKR_NOEXCEPT final;
    SGDITextureId create_texture(const SGDITextureDescriptor* descriptor) SKR_NOEXCEPT final;
    void free_image(SGDIImageId image) SKR_NOEXCEPT final;
    void free_texture(SGDITextureId texture) SKR_NOEXCEPT final;
    void render(SGDICanvasGroup* canvas_group, SGDIRenderParams* params) SKR_NOEXCEPT final;

    // Tier 2
    bool support_hardware_z(float* out_min, float* out_max) const SKR_NOEXCEPT final
    {
        if (out_min) *out_min = 0;
        if (out_max) *out_max = 1000.f;
        return true;
    }
    bool support_mipmap_generation() const SKR_NOEXCEPT final;

protected:
    CGPURenderPipelineId findOrCreateRenderPipeline(GDIRendererPipelineAttributes attributes, ECGPUSampleCount sample_count = CGPU_SAMPLE_COUNT_1);
    CGPURenderPipelineId createRenderPipeline(GDIRendererPipelineAttributes attributes, ECGPUSampleCount sample_count = CGPU_SAMPLE_COUNT_1);
    void createRenderPipelines();

    CGPUVertexLayout vertex_layout = {};
    struct PipelineKey
    {
        GDIRendererPipelineAttributes attributes;
        ECGPUSampleCount sample_count;
        inline bool operator==(const PipelineKey& other) const
        {
            return attributes == other.attributes && sample_count == other.sample_count;
        }
        inline bool operator!=(const PipelineKey& other) const
        {
            return attributes != other.attributes || sample_count != other.sample_count;
        }
        inline bool operator<(const PipelineKey& other) const
        {
            return attributes < other.attributes || (attributes == other.attributes && sample_count < other.sample_count);
        }
    };
    eastl::vector_map<PipelineKey, CGPURenderPipelineId> pipelines;
    skr_threaded_service_t* aux_service = nullptr;
    skr_io_ram_service_t* ram_service = nullptr;
    skr_io_vram_service_t* vram_service = nullptr;
    skr_vfs_t* vfs = nullptr;
    CGPUDeviceId device = nullptr;
    CGPUQueueId transfer_queue = nullptr;
    CGPUSamplerId static_color_sampler = nullptr;
    CGPURootSignaturePoolId rs_pool = nullptr;
    ECGPUFormat target_format;
};

} }