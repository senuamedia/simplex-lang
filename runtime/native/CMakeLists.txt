# Simplex LLM Native Library - CMake Build Configuration
# Auto-detects and enables optimal hardware acceleration
#
# Build options:
#   cmake -DSIMPLEX_CUDA=ON ..     # Force CUDA
#   cmake -DSIMPLEX_METAL=ON ..    # Force Metal
#   cmake -DSIMPLEX_VULKAN=ON ..   # Force Vulkan
#   cmake -DSIMPLEX_CPU_ONLY=ON .. # CPU only (portable)

cmake_minimum_required(VERSION 3.18)
project(simplex_llm VERSION 0.11.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Options
option(SIMPLEX_CUDA "Enable CUDA backend" OFF)
option(SIMPLEX_METAL "Enable Metal backend" OFF)
option(SIMPLEX_VULKAN "Enable Vulkan backend" OFF)
option(SIMPLEX_HIP "Enable HIP/ROCm backend" OFF)
option(SIMPLEX_SYCL "Enable SYCL/oneAPI backend" OFF)
option(SIMPLEX_CPU_ONLY "CPU only, no GPU acceleration" OFF)
option(SIMPLEX_AUTO_DETECT "Auto-detect best backend" ON)

# ============================================================================
# AUTO-DETECTION OF HARDWARE
# ============================================================================

if(SIMPLEX_AUTO_DETECT AND NOT SIMPLEX_CPU_ONLY)
    message(STATUS "Auto-detecting hardware acceleration...")

    # Check for Apple Silicon (Metal)
    if(APPLE)
        execute_process(
            COMMAND sysctl -n hw.optional.arm64
            OUTPUT_VARIABLE IS_ARM64
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(IS_ARM64 STREQUAL "1")
            message(STATUS "  Detected: Apple Silicon - enabling Metal")
            set(SIMPLEX_METAL ON)
        endif()
    endif()

    # Check for NVIDIA GPU (CUDA)
    if(NOT SIMPLEX_METAL)
        find_program(NVIDIA_SMI nvidia-smi)
        if(NVIDIA_SMI)
            message(STATUS "  Detected: NVIDIA GPU - enabling CUDA")
            set(SIMPLEX_CUDA ON)
        endif()
    endif()

    # Check for AMD GPU (HIP/ROCm)
    if(NOT SIMPLEX_METAL AND NOT SIMPLEX_CUDA)
        find_program(ROCM_SMI rocm-smi)
        if(ROCM_SMI)
            message(STATUS "  Detected: AMD GPU - enabling HIP/ROCm")
            set(SIMPLEX_HIP ON)
        endif()
    endif()

    # Fallback to Vulkan if available
    if(NOT SIMPLEX_METAL AND NOT SIMPLEX_CUDA AND NOT SIMPLEX_HIP)
        find_package(Vulkan QUIET)
        if(Vulkan_FOUND)
            message(STATUS "  Detected: Vulkan - enabling Vulkan backend")
            set(SIMPLEX_VULKAN ON)
        endif()
    endif()

    if(NOT SIMPLEX_METAL AND NOT SIMPLEX_CUDA AND NOT SIMPLEX_HIP AND NOT SIMPLEX_VULKAN)
        message(STATUS "  No GPU detected - using optimized CPU backend")
    endif()
endif()

# ============================================================================
# FETCH LLAMA.CPP
# ============================================================================

include(FetchContent)

# Pin to a stable release
set(LLAMA_CPP_VERSION "b4620")  # Update as needed

FetchContent_Declare(
    llama_cpp
    GIT_REPOSITORY https://github.com/ggml-org/llama.cpp
    GIT_TAG        ${LLAMA_CPP_VERSION}
    GIT_SHALLOW    TRUE
)

# Configure llama.cpp build options BEFORE fetching
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)

# Set backend options
if(SIMPLEX_METAL)
    set(GGML_METAL ON CACHE BOOL "" FORCE)
    set(GGML_METAL_EMBED_LIBRARY ON CACHE BOOL "" FORCE)
    add_compile_definitions(SIMPLEX_BACKEND_METAL=1)
endif()

if(SIMPLEX_CUDA)
    set(GGML_CUDA ON CACHE BOOL "" FORCE)
    add_compile_definitions(SIMPLEX_BACKEND_CUDA=1)
endif()

if(SIMPLEX_VULKAN)
    set(GGML_VULKAN ON CACHE BOOL "" FORCE)
    add_compile_definitions(SIMPLEX_BACKEND_VULKAN=1)
endif()

if(SIMPLEX_HIP)
    set(GGML_HIP ON CACHE BOOL "" FORCE)
    add_compile_definitions(SIMPLEX_BACKEND_HIP=1)
endif()

if(SIMPLEX_SYCL)
    set(GGML_SYCL ON CACHE BOOL "" FORCE)
    add_compile_definitions(SIMPLEX_BACKEND_SYCL=1)
endif()

# CPU optimizations (always enabled)
set(GGML_NATIVE ON CACHE BOOL "" FORCE)  # Use native CPU instructions
set(GGML_LTO ON CACHE BOOL "" FORCE)     # Link-time optimization

# Fetch and configure llama.cpp
FetchContent_MakeAvailable(llama_cpp)

# ============================================================================
# SIMPLEX LLM NATIVE LIBRARY
# ============================================================================

add_library(simplex_llm SHARED
    llm_native.c
)

target_include_directories(simplex_llm PRIVATE
    ${llama_cpp_SOURCE_DIR}/include
    ${llama_cpp_SOURCE_DIR}/ggml/include
)

target_link_libraries(simplex_llm PRIVATE
    llama
    ggml
)

# ============================================================================
# SIMPLEX SLM NATIVE LIBRARY (Simplified interface for Cognitive Hive AI)
# ============================================================================

add_library(simplex_slm SHARED
    slm_llama.c
)

target_include_directories(simplex_slm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${llama_cpp_SOURCE_DIR}/include
    ${llama_cpp_SOURCE_DIR}/ggml/include
)

target_compile_definitions(simplex_slm PRIVATE
    SIMPLEX_SLM_HAS_LLAMA=1
)

target_link_libraries(simplex_slm PRIVATE
    llama
    ggml
    m  # math library
)

# Optional: Build stub-only version without llama.cpp dependency
add_library(simplex_slm_stub SHARED
    slm_llama.c
)

target_include_directories(simplex_slm_stub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(simplex_slm_stub PRIVATE
    SIMPLEX_SLM_HAS_LLAMA=0
    SIMPLEX_SLM_DEBUG=1
)

target_link_libraries(simplex_slm_stub PRIVATE
    m  # math library
)

# Platform-specific settings
if(APPLE)
    target_link_libraries(simplex_llm PRIVATE
        "-framework Foundation"
        "-framework Accelerate"
    )
    if(SIMPLEX_METAL)
        target_link_libraries(simplex_llm PRIVATE
            "-framework Metal"
            "-framework MetalKit"
        )
    endif()
endif()

if(SIMPLEX_CUDA)
    find_package(CUDAToolkit REQUIRED)
    target_link_libraries(simplex_llm PRIVATE
        CUDA::cudart
        CUDA::cublas
    )
endif()

# Compiler flags
target_compile_options(simplex_llm PRIVATE
    -O3
    -ffast-math
    -fno-finite-math-only
)

# Architecture-specific optimizations
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    target_compile_options(simplex_llm PRIVATE -mcpu=native)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    # Let GGML_NATIVE handle this
endif()

# ============================================================================
# INSTALLATION
# ============================================================================

install(TARGETS simplex_llm simplex_slm simplex_slm_stub
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/llm_native.h
    ${CMAKE_CURRENT_SOURCE_DIR}/slm_llama.h
    DESTINATION include/simplex
)

# ============================================================================
# SUMMARY
# ============================================================================

message(STATUS "")
message(STATUS "Simplex LLM/SLM Native Library Configuration:")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  llama.cpp:      ${LLAMA_CPP_VERSION}")
message(STATUS "  Libraries:")
message(STATUS "    simplex_llm      - Full LLM bindings")
message(STATUS "    simplex_slm      - SLM bindings (with llama.cpp)")
message(STATUS "    simplex_slm_stub - SLM bindings (stub for testing)")
message(STATUS "  Backends:")
message(STATUS "    Metal:          ${SIMPLEX_METAL}")
message(STATUS "    CUDA:           ${SIMPLEX_CUDA}")
message(STATUS "    Vulkan:         ${SIMPLEX_VULKAN}")
message(STATUS "    HIP/ROCm:       ${SIMPLEX_HIP}")
message(STATUS "    SYCL/oneAPI:    ${SIMPLEX_SYCL}")
message(STATUS "    Native CPU:     ON")
message(STATUS "")
