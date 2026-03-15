# Stage #1
FROM --platform=$BUILDPLATFORM emscripten/emsdk:latest as builder
WORKDIR /app
COPY ascii-art.cpp image_reader.hpp index.html CMakeLists.txt .
RUN emcmake cmake -DCMAKE_BUILD_TYPE=Release -B build && \
    cmake --build build -j$(nproc)

# Stage #2
FROM busybox:latest
COPY --from=builder /app/build/index.html /var/www/
COPY --from=builder /app/build/ascii-art.js /var/www/
COPY --from=builder /app/build/ascii-art.wasm /var/www/
EXPOSE 80
CMD ["httpd", "-v", "-f", "-h", "/var/www/"]
