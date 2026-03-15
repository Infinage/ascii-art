#pragma once

#include <istream>
#include <array>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <cstring>
#include <csetjmp>

#include <png.h>
#include <jpeglib.h>

namespace image {

struct Image {
    std::size_t rb;
    std::uint32_t width, height;
    std::vector<std::uint8_t> data;

    [[nodiscard]] std::array<std::uint8_t, 4> operator()(std::size_t row, std::size_t col) const {
        if (col >= width || row >= height) throw std::out_of_range("Pixel access out of range");
        std::size_t idx = (row * rb) + (col * 4);
        return { data[idx], data[idx+1], data[idx+2], data[idx+3] };
    }
};

// Forward declarations
inline Image read_png(std::istream &is, const std::array<std::uint8_t, 8>& header);
inline Image read_jpeg(std::istream &is, const std::array<std::uint8_t, 8>& header);

inline Image read(std::istream &is) {
    std::array<std::uint8_t, 8> header{};
    is.read(reinterpret_cast<char*>(header.data()), header.size());
    
    // Check if we actually read enough bytes
    if (!is && is.gcount() < 8)
        throw std::runtime_error("Failed to read image header");

    // PNG signature check
    if (png_sig_cmp(header.data(), 0, 8) == 0)
        return read_png(is, header);

    // JPEG signature check (0xFF 0xD8)
    if (header[0] == 0xFF && header[1] == 0xD8)
        return read_jpeg(is, header);

    throw std::runtime_error("Unsupported image format");
}

// ---------------- PNG Loader ----------------

struct PngGuard {
    png_structp png = nullptr;
    png_infop info = nullptr;
    ~PngGuard() {
        if (png) {
            png_destroy_read_struct(&png, info ? &info : nullptr, nullptr);
        }
    }
};

inline Image read_png(std::istream &is, const std::array<std::uint8_t, 8>& header) {
    PngGuard guard;
    guard.png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!guard.png) throw std::runtime_error("png_create_read_struct failed");

    guard.info = png_create_info_struct(guard.png);
    if (!guard.info) throw std::runtime_error("png_create_info_struct failed");

    if (setjmp(png_jmpbuf(guard.png))) {
        throw std::runtime_error("PNG read error");
    }

    png_set_read_fn(guard.png, &is, [](png_structp p, png_bytep out, png_size_t n) {
        std::istream* s = static_cast<std::istream*>(png_get_io_ptr(p));
        s->read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(n));
        if (!(*s)) {
            png_error(p, "Stream read error");
        }
    });

    // Tell libpng we already read the 8-byte signature in read()
    png_set_sig_bytes(guard.png, 8);
    png_read_info(guard.png, guard.info);

    std::uint32_t width = png_get_image_width(guard.png, guard.info);
    std::uint32_t height = png_get_image_height(guard.png, guard.info);
    png_byte color_type = png_get_color_type(guard.png, guard.info);
    png_byte bit_depth  = png_get_bit_depth(guard.png, guard.info);

    if (bit_depth == 16) png_set_strip_16(guard.png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(guard.png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(guard.png);
    if (png_get_valid(guard.png, guard.info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(guard.png);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY) png_set_add_alpha(guard.png, 0xff, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(guard.png);

    png_read_update_info(guard.png, guard.info);
    std::size_t row_bytes = png_get_rowbytes(guard.png, guard.info);

    std::vector<std::uint8_t> data(height * row_bytes);
    std::vector<png_bytep> rows(height);
    for (std::size_t i = 0; i < height; ++i) {
        rows[i] = &data[i * row_bytes];
    }

    png_read_image(guard.png, rows.data());

    return Image{.rb = row_bytes, .width = width, .height = height, .data = std::move(data)};
}

// ---------------- JPEG Loader ----------------

struct my_error_mgr {
    struct jpeg_error_mgr pub;
    std::jmp_buf setjmp_buffer;
};

struct JpegGuard {
    jpeg_decompress_struct cinfo;
    my_error_mgr jerr;
    bool initialized = false;
    ~JpegGuard() {
        if (initialized) jpeg_destroy_decompress(&cinfo);
    }
};

inline Image read_jpeg(std::istream &is, const std::array<std::uint8_t, 8>& header) {
    JpegGuard guard;

    guard.cinfo.err = jpeg_std_error(&guard.jerr.pub);
    guard.jerr.pub.error_exit = [](j_common_ptr cinfo_ptr) {
        my_error_mgr* myerr = reinterpret_cast<my_error_mgr*>(cinfo_ptr->err);
        std::longjmp(myerr->setjmp_buffer, 1);
    };

    if (setjmp(guard.jerr.setjmp_buffer)) {
        throw std::runtime_error("JPEG decompression error");
    }

    jpeg_create_decompress(&guard.cinfo);
    guard.initialized = true;

    // Load header and rest of stream into memory
    std::vector<std::uint8_t> mem;
    mem.reserve(8192); // Prevent excessive early reallocations
    mem.insert(mem.end(), header.begin(), header.end());
    mem.insert(mem.end(), std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>());

    jpeg_mem_src(&guard.cinfo, mem.data(), mem.size());
    jpeg_read_header(&guard.cinfo, TRUE);

    // Standard libjpeg fallback (manual RGB to RGBA expansion)
    guard.cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&guard.cinfo);

    std::size_t rgb_stride = guard.cinfo.output_width * 3;
    std::size_t rgba_stride = guard.cinfo.output_width * 4;
    std::vector<std::uint8_t> data(guard.cinfo.output_width * guard.cinfo.output_height * 4);
    std::vector<std::uint8_t> row_buffer(rgb_stride);

    while (guard.cinfo.output_scanline < guard.cinfo.output_height) {
        std::uint8_t* rowptr = row_buffer.data();
        std::uint32_t current_line = guard.cinfo.output_scanline;
        jpeg_read_scanlines(&guard.cinfo, &rowptr, 1);

        std::uint8_t* out_ptr = &data[current_line * rgba_stride];
        for (std::uint32_t i = 0; i < guard.cinfo.output_width; ++i) {
            out_ptr[i * 4 + 0] = rowptr[i * 3 + 0];
            out_ptr[i * 4 + 1] = rowptr[i * 3 + 1];
            out_ptr[i * 4 + 2] = rowptr[i * 3 + 2];
            out_ptr[i * 4 + 3] = 255; // Force opaque alpha
        }
    }

    jpeg_finish_decompress(&guard.cinfo);

    return Image{
        .rb = guard.cinfo.output_width * 4,
        .width = guard.cinfo.output_width,
        .height = guard.cinfo.output_height,
        .data = std::move(data)
    };
}

} // namespace image
