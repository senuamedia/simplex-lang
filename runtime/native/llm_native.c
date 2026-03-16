/**
 * Simplex LLM Native Bindings
 *
 * High-performance C implementation for LLM operations.
 * Links against llama.cpp for model inference.
 *
 * Copyright (c) 2025-2026 Rod Higgins
 * Licensed under AGPL-3.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

// llama.cpp headers
#include "llama.h"
#include "ggml.h"

// SIMD intrinsics
#if defined(__ARM_NEON)
    #include <arm_neon.h>
    #define SIMPLEX_NEON 1
#elif defined(__AVX512F__)
    #include <immintrin.h>
    #define SIMPLEX_AVX512 1
#elif defined(__AVX2__)
    #include <immintrin.h>
    #define SIMPLEX_AVX2 1
#elif defined(__AVX__)
    #include <immintrin.h>
    #define SIMPLEX_AVX 1
#elif defined(__SSE4_2__)
    #include <nmmintrin.h>
    #define SIMPLEX_SSE42 1
#endif

// ============================================================================
// FILE OPERATIONS
// ============================================================================

int64_t llm_native_open(const char* path) {
    int fd = open(path, O_RDONLY);
    return (int64_t)fd;
}

void llm_native_close(int64_t fd) {
    close((int)fd);
}

uint64_t llm_native_file_size(int64_t fd) {
    struct stat st;
    if (fstat((int)fd, &st) != 0) {
        return 0;
    }
    return (uint64_t)st.st_size;
}

// ============================================================================
// MEMORY MAPPING
// ============================================================================

const uint8_t* llm_native_mmap(int64_t fd, uint64_t size, bool writable) {
    int prot = writable ? (PROT_READ | PROT_WRITE) : PROT_READ;
    int flags = MAP_SHARED;

    void* ptr = mmap(NULL, (size_t)size, prot, flags, (int)fd, 0);
    if (ptr == MAP_FAILED) {
        return NULL;
    }

    // Advise kernel we'll access sequentially
    madvise(ptr, (size_t)size, MADV_SEQUENTIAL);

    return (const uint8_t*)ptr;
}

void llm_native_munmap(const uint8_t* ptr, uint64_t size) {
    if (ptr != NULL) {
        munmap((void*)ptr, (size_t)size);
    }
}

// ============================================================================
// MODEL OPERATIONS
// ============================================================================

typedef struct {
    uint32_t n_ctx;
    uint32_t n_batch;
    uint32_t n_threads;
    uint32_t n_gpu_layers;
    float rope_freq_base;
    float rope_freq_scale;
    bool use_mmap;
    bool use_mlock;
    bool flash_attn;
    bool offload_kqv;
} simplex_inference_config_t;

int64_t llm_native_model_load(const char* path, simplex_inference_config_t config) {
    struct llama_model_params params = llama_model_default_params();

    params.n_gpu_layers = (int32_t)config.n_gpu_layers;
    params.use_mmap = config.use_mmap;
    params.use_mlock = config.use_mlock;

    llama_model* model = llama_load_model_from_file(path, params);
    return (int64_t)model;
}

void llm_native_model_free(int64_t handle) {
    if (handle != 0) {
        llama_free_model((llama_model*)handle);
    }
}

const char* llm_native_model_arch(int64_t handle) {
    if (handle == 0) return "unknown";

    llama_model* model = (llama_model*)handle;
    return llama_model_meta_val_str(model, "general.architecture");
}

uint32_t llm_native_model_vocab_size(int64_t handle) {
    if (handle == 0) return 0;
    return (uint32_t)llama_n_vocab((llama_model*)handle);
}

uint32_t llm_native_model_n_ctx_train(int64_t handle) {
    if (handle == 0) return 0;
    return (uint32_t)llama_n_ctx_train((llama_model*)handle);
}

uint32_t llm_native_model_n_embd(int64_t handle) {
    if (handle == 0) return 0;
    return (uint32_t)llama_n_embd((llama_model*)handle);
}

uint32_t llm_native_model_n_layer(int64_t handle) {
    if (handle == 0) return 0;
    return (uint32_t)llama_n_layer((llama_model*)handle);
}

uint32_t llm_native_model_n_head(int64_t handle) {
    if (handle == 0) return 0;
    return (uint32_t)llama_n_head((llama_model*)handle);
}

uint32_t llm_native_model_n_head_kv(int64_t handle) {
    if (handle == 0) return 0;
    return (uint32_t)llama_n_head_kv((llama_model*)handle);
}

// ============================================================================
// CONTEXT OPERATIONS
// ============================================================================

int64_t llm_native_context_new(int64_t model_handle, simplex_inference_config_t config) {
    if (model_handle == 0) return 0;

    llama_model* model = (llama_model*)model_handle;

    struct llama_context_params params = llama_context_default_params();
    params.n_ctx = config.n_ctx;
    params.n_batch = config.n_batch;
    params.n_threads = config.n_threads;
    params.n_threads_batch = config.n_threads;
    params.rope_freq_base = config.rope_freq_base;
    params.rope_freq_scale = config.rope_freq_scale;
    params.flash_attn = config.flash_attn;
    params.offload_kqv = config.offload_kqv;

    llama_context* ctx = llama_new_context_with_model(model, params);
    return (int64_t)ctx;
}

void llm_native_context_free(int64_t handle) {
    if (handle != 0) {
        llama_free((llama_context*)handle);
    }
}

void llm_native_kv_cache_clear(int64_t handle) {
    if (handle != 0) {
        llama_kv_cache_clear((llama_context*)handle);
    }
}

// ============================================================================
// TOKENIZATION
// ============================================================================

typedef struct {
    int32_t* tokens;
    int32_t n_tokens;
    int32_t capacity;
} simplex_token_vec_t;

simplex_token_vec_t llm_native_tokenize(int64_t model_handle, const char* text, bool add_bos) {
    simplex_token_vec_t result = {0};
    if (model_handle == 0 || text == NULL) return result;

    llama_model* model = (llama_model*)model_handle;

    // Estimate token count
    int n_text = (int)strlen(text);
    int max_tokens = n_text + 4;  // Usually overestimate

    result.tokens = (int32_t*)malloc(max_tokens * sizeof(int32_t));
    result.capacity = max_tokens;

    result.n_tokens = llama_tokenize(
        model,
        text,
        n_text,
        result.tokens,
        max_tokens,
        add_bos,
        true  // special tokens
    );

    if (result.n_tokens < 0) {
        // Need more space
        max_tokens = -result.n_tokens;
        result.tokens = (int32_t*)realloc(result.tokens, max_tokens * sizeof(int32_t));
        result.capacity = max_tokens;

        result.n_tokens = llama_tokenize(
            model,
            text,
            n_text,
            result.tokens,
            max_tokens,
            add_bos,
            true
        );
    }

    return result;
}

char* llm_native_detokenize(int64_t model_handle, const int32_t* tokens, int32_t n_tokens) {
    if (model_handle == 0 || tokens == NULL || n_tokens <= 0) {
        char* empty = (char*)malloc(1);
        empty[0] = '\0';
        return empty;
    }

    llama_model* model = (llama_model*)model_handle;

    // Estimate output size
    int buf_size = n_tokens * 8;  // Conservative estimate
    char* buf = (char*)malloc(buf_size);
    int offset = 0;

    for (int i = 0; i < n_tokens; i++) {
        int remaining = buf_size - offset;
        if (remaining < 32) {
            buf_size *= 2;
            buf = (char*)realloc(buf, buf_size);
            remaining = buf_size - offset;
        }

        int n = llama_token_to_piece(model, tokens[i], buf + offset, remaining, 0, true);
        if (n > 0) {
            offset += n;
        }
    }

    buf[offset] = '\0';
    return buf;
}

// ============================================================================
// DECODE / INFERENCE
// ============================================================================

typedef struct {
    int32_t* tokens;
    int32_t* positions;
    int32_t* seq_ids;
    bool* logits_mask;
    int32_t n_tokens;
} simplex_batch_t;

int32_t llm_native_decode(int64_t ctx_handle, simplex_batch_t* batch) {
    if (ctx_handle == 0 || batch == NULL) return -1;

    llama_context* ctx = (llama_context*)ctx_handle;

    struct llama_batch llama_batch = llama_batch_init(batch->n_tokens, 0, 1);

    for (int i = 0; i < batch->n_tokens; i++) {
        llama_batch.token[i] = batch->tokens[i];
        llama_batch.pos[i] = batch->positions[i];
        llama_batch.n_seq_id[i] = 1;
        llama_batch.seq_id[i][0] = batch->seq_ids[i];
        llama_batch.logits[i] = batch->logits_mask[i];
    }
    llama_batch.n_tokens = batch->n_tokens;

    int result = llama_decode(ctx, llama_batch);

    llama_batch_free(llama_batch);

    return result;
}

float* llm_native_get_logits(int64_t ctx_handle, int32_t pos) {
    if (ctx_handle == 0) return NULL;

    llama_context* ctx = (llama_context*)ctx_handle;
    return llama_get_logits_ith(ctx, pos);
}

float* llm_native_get_logits_all(int64_t ctx_handle) {
    if (ctx_handle == 0) return NULL;

    llama_context* ctx = (llama_context*)ctx_handle;
    return llama_get_logits(ctx);
}

float* llm_native_get_embeddings(int64_t ctx_handle) {
    if (ctx_handle == 0) return NULL;

    llama_context* ctx = (llama_context*)ctx_handle;
    return llama_get_embeddings(ctx);
}

// ============================================================================
// SAMPLING
// ============================================================================

typedef struct {
    float temperature;
    int32_t top_k;
    float top_p;
    float min_p;
    float repeat_penalty;
    int32_t repeat_last_n;
    float frequency_penalty;
    float presence_penalty;
    int32_t mirostat;
    float mirostat_tau;
    float mirostat_eta;
    uint64_t seed;
} simplex_sampling_params_t;

int64_t llm_native_sampler_new(simplex_sampling_params_t params) {
    // Create sampler chain
    struct llama_sampler* chain = llama_sampler_chain_init(llama_sampler_chain_default_params());

    // Add samplers in order
    if (params.repeat_penalty != 1.0f || params.frequency_penalty != 0.0f || params.presence_penalty != 0.0f) {
        llama_sampler_chain_add(chain, llama_sampler_init_penalties(
            params.repeat_last_n,
            params.repeat_penalty,
            params.frequency_penalty,
            params.presence_penalty
        ));
    }

    if (params.mirostat == 1) {
        llama_sampler_chain_add(chain, llama_sampler_init_mirostat(
            llama_n_vocab(NULL), // TODO: need model
            params.seed,
            params.mirostat_tau,
            params.mirostat_eta,
            100
        ));
    } else if (params.mirostat == 2) {
        llama_sampler_chain_add(chain, llama_sampler_init_mirostat_v2(
            params.seed,
            params.mirostat_tau,
            params.mirostat_eta
        ));
    } else {
        // Standard sampling chain
        if (params.top_k > 0) {
            llama_sampler_chain_add(chain, llama_sampler_init_top_k(params.top_k));
        }

        if (params.top_p < 1.0f) {
            llama_sampler_chain_add(chain, llama_sampler_init_top_p(params.top_p, 1));
        }

        if (params.min_p > 0.0f) {
            llama_sampler_chain_add(chain, llama_sampler_init_min_p(params.min_p, 1));
        }

        if (params.temperature > 0.0f) {
            llama_sampler_chain_add(chain, llama_sampler_init_temp(params.temperature));
        }

        llama_sampler_chain_add(chain, llama_sampler_init_dist(params.seed));
    }

    return (int64_t)chain;
}

void llm_native_sampler_free(int64_t handle) {
    if (handle != 0) {
        llama_sampler_free((struct llama_sampler*)handle);
    }
}

int32_t llm_native_sample(int64_t sampler_handle, int64_t ctx_handle, int32_t pos) {
    if (sampler_handle == 0 || ctx_handle == 0) return -1;

    struct llama_sampler* sampler = (struct llama_sampler*)sampler_handle;
    llama_context* ctx = (llama_context*)ctx_handle;

    return llama_sampler_sample(sampler, ctx, pos);
}

// ============================================================================
// SIMD UTILITIES
// ============================================================================

#if defined(SIMPLEX_AVX512)

float llm_native_cosine_similarity(const float* a, const float* b, int32_t dim) {
    __m512 sum_ab = _mm512_setzero_ps();
    __m512 sum_aa = _mm512_setzero_ps();
    __m512 sum_bb = _mm512_setzero_ps();

    int i = 0;
    for (; i + 16 <= dim; i += 16) {
        __m512 va = _mm512_loadu_ps(a + i);
        __m512 vb = _mm512_loadu_ps(b + i);

        sum_ab = _mm512_fmadd_ps(va, vb, sum_ab);
        sum_aa = _mm512_fmadd_ps(va, va, sum_aa);
        sum_bb = _mm512_fmadd_ps(vb, vb, sum_bb);
    }

    float dot = _mm512_reduce_add_ps(sum_ab);
    float norm_a = _mm512_reduce_add_ps(sum_aa);
    float norm_b = _mm512_reduce_add_ps(sum_bb);

    // Handle remainder
    for (; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

#elif defined(SIMPLEX_AVX2)

float llm_native_cosine_similarity(const float* a, const float* b, int32_t dim) {
    __m256 sum_ab = _mm256_setzero_ps();
    __m256 sum_aa = _mm256_setzero_ps();
    __m256 sum_bb = _mm256_setzero_ps();

    int i = 0;
    for (; i + 8 <= dim; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);

        sum_ab = _mm256_fmadd_ps(va, vb, sum_ab);
        sum_aa = _mm256_fmadd_ps(va, va, sum_aa);
        sum_bb = _mm256_fmadd_ps(vb, vb, sum_bb);
    }

    // Horizontal sum
    __m128 hi_ab = _mm256_extractf128_ps(sum_ab, 1);
    __m128 lo_ab = _mm256_castps256_ps128(sum_ab);
    __m128 sum128_ab = _mm_add_ps(hi_ab, lo_ab);
    sum128_ab = _mm_hadd_ps(sum128_ab, sum128_ab);
    sum128_ab = _mm_hadd_ps(sum128_ab, sum128_ab);

    __m128 hi_aa = _mm256_extractf128_ps(sum_aa, 1);
    __m128 lo_aa = _mm256_castps256_ps128(sum_aa);
    __m128 sum128_aa = _mm_add_ps(hi_aa, lo_aa);
    sum128_aa = _mm_hadd_ps(sum128_aa, sum128_aa);
    sum128_aa = _mm_hadd_ps(sum128_aa, sum128_aa);

    __m128 hi_bb = _mm256_extractf128_ps(sum_bb, 1);
    __m128 lo_bb = _mm256_castps256_ps128(sum_bb);
    __m128 sum128_bb = _mm_add_ps(hi_bb, lo_bb);
    sum128_bb = _mm_hadd_ps(sum128_bb, sum128_bb);
    sum128_bb = _mm_hadd_ps(sum128_bb, sum128_bb);

    float dot = _mm_cvtss_f32(sum128_ab);
    float norm_a = _mm_cvtss_f32(sum128_aa);
    float norm_b = _mm_cvtss_f32(sum128_bb);

    // Handle remainder
    for (; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

#elif defined(SIMPLEX_NEON)

float llm_native_cosine_similarity(const float* a, const float* b, int32_t dim) {
    float32x4_t sum_ab = vdupq_n_f32(0);
    float32x4_t sum_aa = vdupq_n_f32(0);
    float32x4_t sum_bb = vdupq_n_f32(0);

    int i = 0;
    for (; i + 4 <= dim; i += 4) {
        float32x4_t va = vld1q_f32(a + i);
        float32x4_t vb = vld1q_f32(b + i);

        sum_ab = vfmaq_f32(sum_ab, va, vb);
        sum_aa = vfmaq_f32(sum_aa, va, va);
        sum_bb = vfmaq_f32(sum_bb, vb, vb);
    }

    float dot = vaddvq_f32(sum_ab);
    float norm_a = vaddvq_f32(sum_aa);
    float norm_b = vaddvq_f32(sum_bb);

    // Handle remainder
    for (; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

#else

// Scalar fallback
float llm_native_cosine_similarity(const float* a, const float* b, int32_t dim) {
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (int i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

#endif

// ============================================================================
// GGUF PARSING HELPERS
// ============================================================================

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t tensor_count;
    uint64_t metadata_kv_count;
} gguf_header_t;

gguf_header_t llm_native_gguf_read_header(const uint8_t* ptr) {
    gguf_header_t header;
    memcpy(&header.magic, ptr, 4);
    memcpy(&header.version, ptr + 4, 4);
    memcpy(&header.tensor_count, ptr + 8, 8);
    memcpy(&header.metadata_kv_count, ptr + 16, 8);
    return header;
}
