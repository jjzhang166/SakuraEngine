#include "math/common.h"
#include "vulkan_utils.h"

FORCEINLINE static VkBufferCreateInfo VkUtil_CreateBufferCreateInfo(CGpuAdapter_Vulkan* A, const struct CGpuBufferDescriptor* desc)
{
    uint64_t allocationSize = desc->size;
    // Align the buffer size to multiples of the dynamic uniform buffer minimum size
    if (desc->descriptors & RT_UNIFORM_BUFFER)
    {
        uint64_t minAlignment = A->adapter_detail.uniform_buffer_alignment;
        allocationSize = smath_round_up_64(allocationSize, minAlignment);
    }
    VkBufferCreateInfo add_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = allocationSize,
        // Queues Props
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL
    };
    add_info.usage = VkUtil_DescriptorTypesToBufferUsage(desc->descriptors, desc->format != PF_UNDEFINED);
    // Buffer can be used as dest in a transfer command (Uploading data to a storage buffer, Readback query data)
    if (desc->memory_usage == MU_GPU_ONLY || desc->memory_usage == MU_GPU_TO_CPU)
        add_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return add_info;
}

// Buffer APIs
static_assert(sizeof(CGpuBuffer_Vulkan) <= 8 * sizeof(uint64_t), "Acquire Single CacheLine"); // Cache Line
CGpuBufferId cgpu_create_buffer_vulkan(CGpuDeviceId device, const struct CGpuBufferDescriptor* desc)
{
    CGpuBuffer_Vulkan* B = cgpu_calloc_aligned(1, sizeof(CGpuBuffer_Vulkan), _Alignof(CGpuBuffer_Vulkan));
    CGpuDevice_Vulkan* D = (CGpuDevice_Vulkan*)device;
    CGpuAdapter_Vulkan* A = (CGpuAdapter_Vulkan*)device->adapter;
    // Create VkBufferCreateInfo
    VkBufferCreateInfo add_info = VkUtil_CreateBufferCreateInfo(A, desc);
    // VMA Alloc
    VmaAllocationCreateInfo vma_mem_reqs = { .usage = (VmaMemoryUsage)desc->memory_usage };
    if (desc->flags & BCF_OWN_MEMORY_BIT)
        vma_mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    if (desc->flags & BCF_PERSISTENT_MAP_BIT)
        vma_mem_reqs.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;

    DECLARE_ZERO(VmaAllocationInfo, alloc_info)
    VkResult bufferResult =
        vmaCreateBuffer(D->pVmaAllocator, &add_info, &vma_mem_reqs, &B->pVkBuffer, &B->pVkAllocation, &alloc_info);
    if (bufferResult != VK_SUCCESS)
    {
        assert(0);
        return NULL;
    }
    B->super.cpu_mapped_address = alloc_info.pMappedData;

    // Setup Descriptors
    if ((desc->descriptors & RT_UNIFORM_BUFFER) || (desc->descriptors & RT_BUFFER) ||
        (desc->descriptors & RT_RW_BUFFER))
    {
        if ((desc->descriptors & RT_BUFFER) || (desc->descriptors & RT_RW_BUFFER))
        {
            B->mOffset = desc->element_stride * desc->first_element;
        }
    }
    // Setup Uniform Texel View
    const VkFormat texel_format = pf_translate_to_vulkan(desc->format);
    DECLARE_ZERO(VkFormatProperties, formatProps)
    vkGetPhysicalDeviceFormatProperties(A->pPhysicalDevice, texel_format, &formatProps);
    // Now We Use The Same View Info for Uniform & Storage BufferView on Vulkan Backend.
    VkBufferViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
        .pNext = NULL,
        .buffer = B->pVkBuffer,
        .flags = 0,
        .format = texel_format,
        .offset = desc->first_element * desc->element_stride,
        .range = desc->elemet_count * desc->element_stride
    };
    if (add_info.usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT)
    {
        if (!(formatProps.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT))
        {
            // LOGF(LogLevel::eWARNING, "Failed to create uniform texel buffer view for format %u", (uint32_t)pDesc->mFormat);
        }
        else if (vkCreateBufferView(D->pVkDevice, &viewInfo,
                     GLOBAL_VkAllocationCallbacks, &B->pVkUniformTexelView) != VK_SUCCESS)
        {
        }
    }
    // Setup Storage Texel View
    if (add_info.usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)
    {
        if (!(formatProps.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT))
        {
            // LOGF(LogLevel::eWARNING, "Failed to create storage texel buffer view for format %u", (uint32_t)pDesc->mFormat);
        }
        else if (vkCreateBufferView(D->pVkDevice, &viewInfo,
                     GLOBAL_VkAllocationCallbacks, &B->pVkStorageTexelView) != VK_SUCCESS)
        {
        }
    }
    // Set Buffer Name
    VkUtil_OptionalSetObjectName(D, (uint64_t)B->pVkBuffer, VK_OBJECT_TYPE_BUFFER, desc->name);
    // Set Buffer Object Props
    B->super.size = (uint32_t)desc->size;
    B->super.memory_usage = desc->memory_usage;
    B->super.descriptors = desc->descriptors;
    return &B->super;
}
void cgpu_free_buffer_vulkan(CGpuBufferId buffer)
{
    CGpuBuffer_Vulkan* B = (CGpuBuffer_Vulkan*)buffer;
    CGpuDevice_Vulkan* D = (CGpuDevice_Vulkan*)B->super.device;
    assert(B->pVkAllocation && "pVkAllocation must not be null!");
    if (B->pVkUniformTexelView)
    {
        vkDestroyBufferView(D->pVkDevice, B->pVkUniformTexelView, GLOBAL_VkAllocationCallbacks);
        B->pVkUniformTexelView = VK_NULL_HANDLE;
    }
    if (B->pVkStorageTexelView)
    {
        vkDestroyBufferView(D->pVkDevice, B->pVkUniformTexelView, GLOBAL_VkAllocationCallbacks);
        B->pVkStorageTexelView = VK_NULL_HANDLE;
    }
    vmaDestroyBuffer(D->pVmaAllocator, B->pVkBuffer, B->pVkAllocation);
    cgpu_free(B);
}

// Shader APIs
CGpuShaderLibraryId cgpu_create_shader_library_vulkan(
    CGpuDeviceId device, const struct CGpuShaderLibraryDescriptor* desc)
{
    CGpuDevice_Vulkan* D = (CGpuDevice_Vulkan*)device;
    VkShaderModuleCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = desc->code_size,
        .pCode = desc->code
    };
    CGpuShaderLibrary_Vulkan* S = (CGpuShaderLibrary_Vulkan*)cgpu_calloc(1, sizeof(CGpuSwapChain_Vulkan));
    D->mVkDeviceTable.vkCreateShaderModule(D->pVkDevice, &info, GLOBAL_VkAllocationCallbacks, &S->mShaderModule);
    return &S->super;
}

void cgpu_free_shader_library_vulkan(CGpuShaderLibraryId module)
{
    CGpuShaderLibrary_Vulkan* S = (CGpuShaderLibrary_Vulkan*)module;
    CGpuDevice_Vulkan* D = (CGpuDevice_Vulkan*)module->device;
    D->mVkDeviceTable.vkDestroyShaderModule(D->pVkDevice, S->mShaderModule, GLOBAL_VkAllocationCallbacks);
    cgpu_free(S);
}