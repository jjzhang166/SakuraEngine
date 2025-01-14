#pragma once
#include "cgpu/api.h"

typedef struct skr_win_dstorage_decompress_service_t skr_win_dstorage_decompress_service_t;
typedef struct skr_win_dstorage_decompress_service_t* skr_win_dstorage_decompress_service_id;

#define SKR_WIN_DSTORAGE_COMPRESSION_TYPE_IMAGE CGPU_DSTORAGE_COMPRESSION_CUSTOM + 1

typedef enum EWinDStorageDecompressionFlag {
    WIN_DSTORAGE_DECOMPRESSION_FLAG_NONE = 0x00,
    WIN_DSTORAGE_DECOMPRESSION_FLAG_DEST_IN_UPLOAD_HEAP = 0x01,

    WIN_DSTORAGE_DECOMPRESSION_FLAG_MAX_ENUM_BIT = 0xFFFFFFFF
} EWinDStorageDecompressionFlag;
typedef uint32_t WinDStorageDecompressionFlags;

typedef struct skr_win_dstorage_decompress_request_t {
    uint64_t id;
    CGPUDStorageCompression compression;
    WinDStorageDecompressionFlags flags;
    uint64_t src_size;
    void const* src_buffer;
    uint64_t dst_size;
    void* dst_buffer;
} skr_win_dstorage_decompress_request_t;

#ifndef _HRESULT_DEFINED
#define _HRESULT_DEFINED
typedef long HRESULT;
#endif
typedef HRESULT (*skr_win_dstorage_decompress_callback_t)(skr_win_dstorage_decompress_request_t* request, void* user_data);

// This operation is unsafe and may result in driver corruption.
// Do not call this unless you are sure the backend queues are all idle.
RUNTIME_EXTERN_C RUNTIME_API
void cgpu_win_dstorage_set_staging_buffer_size(CGPUInstanceId instance, uint64_t size);

RUNTIME_EXTERN_C RUNTIME_API
skr_win_dstorage_decompress_service_id cgpu_win_create_decompress_service(CGPUInstanceId instance);

RUNTIME_EXTERN_C RUNTIME_API
bool cgpu_win_decompress_service_register_callback(skr_win_dstorage_decompress_service_id service, CGPUDStorageCompression, skr_win_dstorage_decompress_callback_t, void*);

RUNTIME_EXTERN_C RUNTIME_API
void cgpu_win_free_decompress_service(skr_win_dstorage_decompress_service_id service);