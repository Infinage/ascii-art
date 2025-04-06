# ASCII Art Generator (CPP + WASM)

A simple web app that converts PNG images to ASCII art — powered by C++ compiled to WebAssembly.  
🔗 **Live Demo:** [ascii-art.deesa.space](http://ascii-art.deesa.space/)

## How it Works

- The main logic is written in **C++** and compiled to **WASM** using **Emscripten**.
- It uses **libpng** (automatically handled by Emscripten via `--use-port=libpng`).
- The input PNG is grayscaled, normalized, and mapped to characters based on pixel intensity.
- The image can be **downscaled** by a factor `N` using **average pooling** (`N x N` filter, stride `N`).

## 🛠️ Build & Run (Locally with Emscripten)

```bash
# Compile with em++ from emsdk
em++ ascii-art.cpp -o ascii-art.js -std=c++23 -O2 --use-port=libpng \
    -lembind -s INITIAL_MEMORY=16MB -s MAXIMUM_MEMORY=32MB \
    -s ALLOW_MEMORY_GROWTH=1

# Spin up some webserver
python -m http.server
```

Then visit: [http://localhost:8000](http://localhost:8000)

## 🐳 Run via Docker

```bash
# Build
docker build -t ascii-art .

# Run
docker run --rm -p 8080:80 --name ascii-art ascii-art
```

Then visit: [http://localhost:8080](http://localhost:8080)

## 📦 Dependencies

- Emscripten (via `emsdk`)
- libpng (auto-handled with `--use-port=libpng`)

## ✨ Features

- ASCII art generated in-browser using WebAssembly
- Adjustable downscale factor
- Optional inversion of character mapping
