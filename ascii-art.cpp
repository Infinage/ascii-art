// em++ ascii-art.cpp -o ascii-art.js -std=c++23 -O2 --use-port=libpng -lembind -fwasm-exceptions
// python -m http.server

#include "png_reader.hpp"
#include <emscripten/bind.h>

#include <cmath>
#include <limits>
#include <sstream>

// Define density as a seq of chars
constexpr std::string_view DENSITY {" _.,-=+:;cba!?0123456789$W#@Ñ"};

// Map each cell to a corresponding character in the Density string
inline char mapPixel(const double val, const double min, const double max, bool invert = false) {
    double scaled {max > min? (val - min) / (max - min): 0.};
    std::size_t pos {static_cast<std::size_t>(std::round(scaled * (DENSITY.size() - 1)))};
    if (invert) pos = DENSITY.size() - pos - 1;
    return DENSITY[pos];
}

// Apply avg of all the pixels
std::array<std::uint8_t, 4> poolAvg(const std::vector<std::array<std::uint8_t, 4>> &pixels) {
    double R{}, G{}, B{}, A{};
    for (const std::array<std::uint8_t, 4> &pixel: pixels) {
        R += pixel[0]; G += pixel[1];
        B += pixel[2]; A += pixel[3];
    }

    // Avg the result
    double factor {static_cast<double>(pixels.size())};
    return {
        static_cast<std::uint8_t>(R / factor),
        static_cast<std::uint8_t>(G / factor),
        static_cast<std::uint8_t>(B / factor),
        static_cast<std::uint8_t>(A / factor),
    };
}

std::string asciify(const std::string &imageBlob, std::size_t blobSize, std::size_t downscale, bool invertMapping = false) {
    try {
        // Read the image from path if provided, else read from console
        std::stringstream iss(std::ios::in | std::ios::out | std::ios::binary);
        iss.write(imageBlob.data(), static_cast<std::streamsize>(blobSize));
        iss.seekg(0, std::ios::beg); 

        png::Image image {png::read(iss)};

        // Convert RGBA into a single valued 2D vector, store min - max for normalization
        std::size_t opHeight {image.height / downscale}, opWidth {image.width / downscale};
        std::vector<std::vector<double>> grayscaled(opHeight, std::vector<double>(opWidth));
        double min {std::numeric_limits<double>::max()}, max {std::numeric_limits<double>::min()};
        for (std::size_t i {0}; i < opHeight; i++) {
            for (std::size_t j {0}; j < opWidth; j++) {
                // Gather the pixels within the filter range
                std::vector<std::array<std::uint8_t, 4>> pixels;
                for (std::size_t di {0}; di < downscale; di++) {
                    for (std::size_t dj {0}; dj < downscale; dj++) {
                        pixels.emplace_back(image(i * downscale + di, j * downscale + dj));
                    }
                }

                // Apply aggregate function to pool pixels, luma weighted average for grayscaling
                std::array<std::uint8_t, 4> pixel {poolAvg(pixels)};
                double grey {(pixel[3] / 255.) * (.299 * pixel[0] + .587 * pixel[1] + .114 * pixel[2])};
                grayscaled[i][j] = grey; min = std::min(min, grey); max = std::max(max, grey);
            }
        }

        // Map each pixel to char post normalization
        std::string result{};
        result.reserve(grayscaled.size() * (grayscaled[0].size() + 1));
        for (const std::vector<double> &row: grayscaled) {
            for (const double &pixel: row)
                result.push_back(mapPixel(pixel, min, max, invertMapping)) ;
            result.push_back('\n');
        }

        return result;
    } 

    catch (std::exception &ex) {
        return ex.what();
    }
}

// Add EMSCRIPTEN Bindings to expose to javascript
EMSCRIPTEN_BINDINGS(my_module) {
    emscripten::function("asciify", &asciify);
}
