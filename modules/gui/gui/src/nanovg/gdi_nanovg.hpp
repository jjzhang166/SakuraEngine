#pragma once
#include "../private/gdi_private.hpp"
#include "nanovg.h"

namespace skr {
namespace gdi {

struct GDIElementNVG : public GDIElementPrivate
{
    inline GDIElementNVG() : nvg(nullptr) {}

    void begin_frame(float devicePixelRatio) final;
    void begin_path() final;
    void close_path() final;
    void arc(float cx, float cy, float r, float a0, float a1, EGDIWinding dir) final;
    void rect(float x, float y, float w, float h) final;
    void circle(float cx, float cy, float r) final;
    void rounded_rect_varying(float x, float y, float w, float h, float radTopLeft, float radTopRight, float radBottomRight, float radBottomLeft) final;
    
    void translate(float x, float y) final;
    void rotate(float r) final;
    void move_to(float x, float y) final;
    void line_to(float x, float y) final;

    void stroke_color(uint32_t r, uint32_t g, uint32_t b, uint32_t a) final;
    void stroke_color(float r, float g, float b, float a) final;
    void stroke_paint(GDIPaint* paint) final;
    void stroke_width(float size) final;
    void stroke() final;

    void path_winding(EGDISolidity dir) final;

    void fill_color(uint32_t r, uint32_t g, uint32_t b, uint32_t a) final;
    void fill_color(float r, float g, float b, float a) final;
    void fill_paint(GDIPaint* paint) final;
    void fill() final;
    void fill_no_aa() final;

    void restore() final;
    void save() final;

    NVGcontext* nvg = nullptr;
    struct GDIPaintNVG* gdi_paint = nullptr;
};

struct GDIPaintNVG : public GDIPaintPrivate
{
    void set_pattern(float cx, float cy, float w, float h, float angle, GDITextureId texture, skr_float4_t ocol) SKR_NOEXCEPT final;
    void set_pattern(float cx, float cy, float w, float h, float angle, GDIMaterialId texture, skr_float4_t ocol) SKR_NOEXCEPT final;

    void enable_imagespace_coordinate(bool enable) SKR_NOEXCEPT final;
    void custom_vertex_color(skr_gdi_custom_vertex_painter_t painter, void* usrdata) SKR_NOEXCEPT final;

    NVGpaint nvg_paint = {};
    skr_float4_t ocol = {};
    skr_gdi_custom_vertex_painter_t custom_painter = nullptr;
    void* custom_painter_data = nullptr;
    enum CoordinateMethod
    {
        None,
        NVG,
        ImageSpace,
    } coordinate_method_override = None;
};

struct GDIViewportNVG : public GDIViewportPrivate
{
    // void add_canvas(GDIViewport* canvas) final;
    // void remove_canvas(GDIViewport* canvas) final;
};

struct GDICanvasNVG : public GDICanvasPrivate
{
    void add_element(GDIElement* element) SKR_NOEXCEPT final;
};

struct GDIDeviceNVG : public GDIDevicePrivate
{
    ~GDIDeviceNVG();
    int initialize() SKR_NOEXCEPT;
    int finalize() SKR_NOEXCEPT;

    GDICanvas* create_canvas() final;
    void free_canvas(GDICanvas* canvas) final;

    GDIViewport* create_viewport() final;
    void free_viewport(GDIViewport* group) final;

    GDIElement* create_element() final;
    void free_element(GDIElement* element) final;

    GDIPaint* create_paint() final;
    void free_paint(GDIPaint* paint) final;
};


} }