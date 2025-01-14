#include "platform/memory.h"
#include "utils/log.h"
#include "image_coder_base.hpp"

namespace skr
{
void BaseImageCoder::freeRaw() SKR_NOEXCEPT
{
    if (raw_data.bytes)
    {
        sakura_free(raw_data.bytes);
        raw_data.bytes = nullptr;
        raw_data.size = 0;
    }
}

void BaseImageCoder::freeEncoded() SKR_NOEXCEPT
{
    if (encoded_data.bytes)
    {
        sakura_free(encoded_data.bytes);
        encoded_data.bytes = nullptr;
        encoded_data.size = 0;
    }
}

void BaseImageCoder::setRawProps(uint32_t _width, uint32_t _height, EImageCoderColorFormat _format, uint32_t _bit_depth, uint32_t _bytes_per_raw) SKR_NOEXCEPT
{
    this->width = _width;
    this->height = _height;
    this->color_format = _format;
    this->raw_bit_depth = _bit_depth;
    this->raw_bytes_per_row = _bytes_per_raw;
    this->bit_depth = _bit_depth;
}

BaseImageCoder::~BaseImageCoder() SKR_NOEXCEPT
{
    freeEncoded();
    freeRaw();
}

bool BaseImageCoder::set_encoded(const uint8_t* data, uint64_t size) SKR_NOEXCEPT
{
    freeEncoded();

    encoded_data.bytes = (uint8_t*)sakura_malloc(size);
    encoded_data.size = size;
    memcpy(encoded_data.bytes, data, size);

    encoded_view = { (uint8_t*)data, size };
    if (!encoding)
    {
        newest_version++;
        encoded_data_version = newest_version;
    }
    return true;
}

bool BaseImageCoder::move_encoded(const uint8_t* data, uint64_t size) SKR_NOEXCEPT
{
    freeEncoded();

    encoded_data.bytes = (uint8_t*)data;
    encoded_data.size = size;

    encoded_view = { (uint8_t*)data, size };
    if (!encoding)
    {
        newest_version++;
        encoded_data_version = newest_version;
    }
    return true;
}

bool BaseImageCoder::view_encoded(const uint8_t* data, uint64_t size) SKR_NOEXCEPT
{
    freeEncoded();

    encoded_view = { (uint8_t*)data, size };
    if (!encoding)
    {
        newest_version++;
        encoded_data_version = newest_version;
    }
    return true;
}

bool BaseImageCoder::set_raw(const uint8_t* data, uint64_t size, uint32_t width, uint32_t height,
EImageCoderColorFormat format, uint32_t bit_depth, uint32_t bytes_per_raw) SKR_NOEXCEPT
{
    freeRaw();

    raw_data.bytes = (uint8_t*)sakura_malloc(size);
    raw_data.size = size;
    memcpy(raw_data.bytes, data, size);

    raw_view = { (uint8_t*)data, size };
    setRawProps(width, height, format, bit_depth, bytes_per_raw);
    if (!decoding) 
    {
        newest_version++;
        raw_data_version = newest_version;
    }
    return true;
}

bool BaseImageCoder::move_raw(const uint8_t* data, uint64_t size, uint32_t width, uint32_t height,
EImageCoderColorFormat format, uint32_t bit_depth, uint32_t bytes_per_raw) SKR_NOEXCEPT
{
    freeRaw();

    raw_data.bytes = (uint8_t*)data;
    raw_data.size = size;

    raw_view = { (uint8_t*)data, size };
    setRawProps(width, height, format, bit_depth, bytes_per_raw);
    if (!decoding) 
    {
        newest_version++;
        raw_data_version = newest_version;
    }
    return true;
}

bool BaseImageCoder::view_raw(const uint8_t* data, uint64_t size, uint32_t width, uint32_t height,
EImageCoderColorFormat format, uint32_t bit_depth, uint32_t bytes_per_raw) SKR_NOEXCEPT
{
    freeRaw();

    raw_view = { (uint8_t*)data, size };
    setRawProps(width, height, format, bit_depth, bytes_per_raw);
    if (!decoding) 
    {
        newest_version++;
        raw_data_version = newest_version;
    }
    return true;
}

uint64_t BaseImageCoder::get_raw_size() const SKR_NOEXCEPT
{
    return raw_view.size();
}

uint64_t BaseImageCoder::get_encoded_size() const SKR_NOEXCEPT
{
    return encoded_view.size();
}

bool BaseImageCoder::lazy_encode() const SKR_NOEXCEPT
{
    if ((encoded_data_version < newest_version) && !encoding)
    {
        auto _this = const_cast<BaseImageCoder*>(this);
        encoding = true;
        if (_this->encode())
        {
            newest_version++;
            encoded_data_version = newest_version;

            encoding = false;
            return true;
        }
        return false;
    }
    return true;
}

bool BaseImageCoder::lazy_decode(EImageCoderColorFormat format, uint32_t bit_depth) const SKR_NOEXCEPT
{
    if ((raw_data_version < newest_version) && !decoding)
    {
        auto _this = const_cast<BaseImageCoder*>(this);
        decoding = true;
        if (_this->decode(format, bit_depth))
        {
            newest_version++;
            raw_data_version = newest_version;

            decoding = false;
            return true;
        }
        return false;
    }
    return true;
}

bool BaseImageCoder::get_raw_data(uint8_t* pData, uint64_t* pSize, EImageCoderColorFormat format, uint32_t bit_depth) const SKR_NOEXCEPT
{
    lazy_decode(format, bit_depth);
    SKR_ASSERT(*pSize >= get_raw_size());
    memcpy(pData, raw_view.data(), raw_view.size());
    *pSize = raw_view.size();
    return true;
}

skr::span<const uint8_t> BaseImageCoder::get_raw_data_view(EImageCoderColorFormat format, uint32_t bit_depth) const SKR_NOEXCEPT
{
    lazy_decode(format, bit_depth);
    return raw_view;
}

void BaseImageCoder::steal_raw_data(struct skr_blob_t* pBlob, EImageCoderColorFormat format, uint32_t bit_depth) SKR_NOEXCEPT
{
    lazy_decode(format, bit_depth);
    pBlob->bytes = raw_data.bytes;
    pBlob->size = raw_data.size;
    raw_data.bytes = nullptr;
    raw_data.size = 0;
    raw_view = {};
}

skr::span<const uint8_t> BaseImageCoder::get_encoded_data_view() const SKR_NOEXCEPT
{
    lazy_encode();
    return encoded_view;
}

bool BaseImageCoder::get_encoded_data(uint8_t* pData, uint64_t* pSize) const SKR_NOEXCEPT
{
    lazy_encode();
    SKR_ASSERT(*pSize >= get_encoded_size());
    memcpy(pData, encoded_view.data(), encoded_view.size());
    *pSize = encoded_view.size();
    return true;
}

void BaseImageCoder::steal_encoded_data(struct skr_blob_t* pBlob) SKR_NOEXCEPT
{
    lazy_encode();
    pBlob->bytes = encoded_data.bytes;
    pBlob->size = encoded_data.size;
    encoded_data.bytes = nullptr;
    encoded_data.size = 0;
    encoded_view = {};
}

EImageCoderColorFormat BaseImageCoder::get_color_format() const SKR_NOEXCEPT
{
    return color_format;
}

uint32_t BaseImageCoder::get_width() const SKR_NOEXCEPT
{
    return width;
}

uint32_t BaseImageCoder::get_height() const SKR_NOEXCEPT
{
    return height;
}

uint32_t BaseImageCoder::get_bit_depth() const SKR_NOEXCEPT
{
    return bit_depth;
}
} // namespace skr