# Stage #1
FROM emscripten/emsdk:latest as builder
WORKDIR /app
COPY . .
RUN em++ ascii-art.cpp -o ascii-art.js -std=c++23 -O2 --use-port=libpng -lembind -fwasm-exceptions

# Stage #2
FROM busybox:latest
COPY --from=builder /app/index.html /app/ascii-art.js /app/ascii-art.wasm /var/www/
EXPOSE 80
CMD ["httpd", "-v", "-f", "-h", "/var/www/"]
