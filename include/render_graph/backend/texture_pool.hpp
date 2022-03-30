#pragma once
#include <EASTL/unordered_map.h>
#include <EASTL/queue.h>
#include "utils/hash.h"
#include "cgpu/api.h"

namespace sakura
{
namespace render_graph
{
class TexturePool
{
public:
    struct Key {
        const CGpuDeviceId device;
        const CGpuTextureCreationFlags flags;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t array_size;
        ECGpuFormat format;
        uint32_t mip_levels;
        ECGpuSampleCount sample_count;
        uint32_t sample_quality;
        CGpuResourceTypes descriptors;
        operator size_t() const;
        friend class TexturePool;

    protected:
        Key(CGpuDeviceId device, const CGpuTextureDescriptor& desc);
    };
    friend class RenderGraphBackend;
    void initialize(CGpuDeviceId device);
    void finalize();
    CGpuTextureId allocate(const CGpuTextureDescriptor& desc, uint64_t frame_index);
    void deallocate(const CGpuTextureDescriptor& desc, CGpuTextureId texture, uint64_t frame_index);

protected:
    CGpuDeviceId device;
    eastl::unordered_map<Key,
        eastl::queue<eastl::pair<CGpuTextureId, uint64_t>>>
        textures;
};

inline TexturePool::Key::Key(CGpuDeviceId device, const CGpuTextureDescriptor& desc)
    : device(device)
    , flags(desc.flags)
    , width(desc.width)
    , height(desc.height)
    , depth(desc.depth)
    , array_size(desc.array_size)
    , format(desc.format)
    , mip_levels(desc.mip_levels)
    , sample_count(desc.sample_count)
    , sample_quality(desc.sample_quality)
    , descriptors(desc.descriptors)
{
}

inline TexturePool::Key::operator size_t() const
{
    return skr_hash(this, sizeof(*this), (size_t)device);
}

inline void TexturePool::initialize(CGpuDeviceId device_)
{
    device = device_;
}

inline void TexturePool::finalize()
{
    for (auto queue : textures)
    {
        while (!queue.second.empty())
        {
            cgpu_free_texture(queue.second.front().first);
            queue.second.pop();
        }
    }
}

inline CGpuTextureId TexturePool::allocate(const CGpuTextureDescriptor& desc, uint64_t frame_index)
{
    CGpuTextureId allocated = nullptr;
    const TexturePool::Key key(device, desc);
    auto&& queue_iter = textures.find(key);
    // add queue
    if (queue_iter == textures.end())
    {
        textures[key] = {};
    }
    if (textures[key].empty())
    {
        textures[key].push({ cgpu_create_texture(device, &desc), frame_index });
    }
    allocated = textures[key].front().first;
    textures[key].pop();
    return allocated;
}

inline void TexturePool::deallocate(const CGpuTextureDescriptor& desc, CGpuTextureId texture, uint64_t frame_index)
{
    const TexturePool::Key key(device, desc);
    textures[key].push({ texture, frame_index });
}
} // namespace render_graph
} // namespace sakura