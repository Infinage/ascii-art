cmake_minimum_required(VERSION 3.28)
project(ascii-art)

# Use emscripten toolchain
if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "This project must be built with Emscripten")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message("EMSCRIPTEN ROOT PATH: ${EMSCRIPTEN_ROOT_PATH}")
message("EMSCRIPTEN SYSROOT PATH: ${EMSCRIPTEN_SYSROOT}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --sysroot=${EMSCRIPTEN_SYSROOT}")

add_executable(ascii-art ascii-art.cpp)
set_target_properties(ascii-art PROPERTIES SUFFIX ".js")

target_compile_options(ascii-art PRIVATE -lembind -fwasm-exceptions 
    --use-port=libpng --use-port=libjpeg)

file(COPY index.html DESTINATION ${CMAKE_BINARY_DIR})

target_link_options(ascii-art PRIVATE --use-port=libpng --use-port=libjpeg
    -sINITIAL_MEMORY=16MB -sMAXIMUM_MEMORY=128MB -sALLOW_MEMORY_GROWTH=1 
    -lembind -fwasm-exceptions)

target_include_directories(ascii-art PRIVATE
    "${EMSCRIPTEN_ROOT_PATH}/system/include" 
    "${EMSCRIPTEN_SYSROOT}/include")
