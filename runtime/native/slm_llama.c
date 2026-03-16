/**
 * Simplex SLM Native Bindings - llama.cpp Integration
 *
 * Stub implementation for SLM operations.
 * Actual llama.cpp integration will be enabled when building with CMake.
 *
 * Copyright (c) 2025-2026 Rod Higgins
 * Licensed under AGPL-3.0
 */

#include "slm_llama.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// Build Configuration
// ============================================================================

// Set to 1 when building with actual llama.cpp
#ifndef SIMPLEX_SLM_HAS_LLAMA
#define SIMPLEX_SLM_HAS_LLAMA 0
#endif

#if SIMPLEX_SLM_HAS_LLAMA
#include "llama.h"
#include "ggml.h"
#endif

// Debug logging macro
#ifdef SIMPLEX_SLM_DEBUG
#define SLM_DEBUG(fmt, ...) fprintf(stderr, "[SLM DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define SLM_DEBUG(fmt, ...) ((void)0)
#endif

// ============================================================================
// Core Operations
// ============================================================================

simplex_slm_t* simplex_slm_load(const char* model_path, int ctx_size, int threads) {
    SLM_DEBUG("simplex_slm_load called: path=%s, ctx=%d, threads=%d",
              model_path ? model_path : "(null)", ctx_size, threads);

    if (model_path == NULL) {
        fprintf(stderr, "[SLM] Error: model_path is NULL\n");
        return NULL;
    }

#if SIMPLEX_SLM_HAS_LLAMA
    // Real llama.cpp implementation
    struct llama_model_params model_params = llama_model_default_params();
    model_params.use_mmap = true;

    llama_model* model = llama_load_model_from_file(model_path, model_params);
    if (model == NULL) {
        fprintf(stderr, "[SLM] Error: Failed to load model from %s\n", model_path);
        return NULL;
    }

    struct llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = ctx_size;
    ctx_params.n_threads = threads;
    ctx_params.n_threads_batch = threads;

    llama_context* ctx = llama_new_context_with_model(model, ctx_params);
    if (ctx == NULL) {
        fprintf(stderr, "[SLM] Error: Failed to create context\n");
        llama_free_model(model);
        return NULL;
    }

    simplex_slm_t* slm = (simplex_slm_t*)malloc(sizeof(simplex_slm_t));
    slm->model = model;
    slm->ctx = ctx;
    slm->n_ctx = ctx_size;
    slm->n_threads = threads;

    SLM_DEBUG("Model loaded successfully: vocab=%d, n_embd=%d",
              llama_n_vocab(model), llama_n_embd(model));

    return slm;

#else
    // Stub implementation
    fprintf(stderr, "[SLM STUB] simplex_slm_load: Loading model from '%s' (ctx=%d, threads=%d)\n",
            model_path, ctx_size, threads);
    fprintf(stderr, "[SLM STUB] Note: This is a stub. Build with SIMPLEX_SLM_HAS_LLAMA=1 for real llama.cpp\n");

    simplex_slm_t* slm = (simplex_slm_t*)malloc(sizeof(simplex_slm_t));
    if (slm == NULL) {
        return NULL;
    }

    slm->model = (void*)0xDEADBEEF;  // Placeholder
    slm->ctx = (void*)0xCAFEBABE;    // Placeholder
    slm->n_ctx = ctx_size;
    slm->n_threads = threads;

    return slm;
#endif
}

void simplex_slm_free(simplex_slm_t* slm) {
    SLM_DEBUG("simplex_slm_free called");

    if (slm == NULL) {
        return;
    }

#if SIMPLEX_SLM_HAS_LLAMA
    if (slm->ctx != NULL) {
        llama_free((llama_context*)slm->ctx);
    }
    if (slm->model != NULL) {
        llama_free_model((llama_model*)slm->model);
    }
#else
    fprintf(stderr, "[SLM STUB] simplex_slm_free: Releasing model resources\n");
#endif

    free(slm);
}

char* simplex_slm_infer(simplex_slm_t* slm, const char* prompt, float temp, int max_tokens) {
    SLM_DEBUG("simplex_slm_infer called: temp=%.2f, max_tokens=%d", temp, max_tokens);

    if (slm == NULL || prompt == NULL) {
        fprintf(stderr, "[SLM] Error: Invalid arguments to simplex_slm_infer\n");
        return NULL;
    }

#if SIMPLEX_SLM_HAS_LLAMA
    llama_model* model = (llama_model*)slm->model;
    llama_context* ctx = (llama_context*)slm->ctx;

    // Tokenize prompt
    int n_prompt = strlen(prompt);
    int max_prompt_tokens = n_prompt + 4;
    int32_t* prompt_tokens = (int32_t*)malloc(max_prompt_tokens * sizeof(int32_t));

    int n_tokens = llama_tokenize(model, prompt, n_prompt, prompt_tokens,
                                   max_prompt_tokens, true, true);

    if (n_tokens < 0) {
        max_prompt_tokens = -n_tokens;
        prompt_tokens = (int32_t*)realloc(prompt_tokens, max_prompt_tokens * sizeof(int32_t));
        n_tokens = llama_tokenize(model, prompt, n_prompt, prompt_tokens,
                                   max_prompt_tokens, true, true);
    }

    // Clear KV cache
    llama_kv_cache_clear(ctx);

    // Prepare batch for prompt processing
    struct llama_batch batch = llama_batch_init(n_tokens, 0, 1);
    for (int i = 0; i < n_tokens; i++) {
        batch.token[i] = prompt_tokens[i];
        batch.pos[i] = i;
        batch.n_seq_id[i] = 1;
        batch.seq_id[i][0] = 0;
        batch.logits[i] = (i == n_tokens - 1);  // Only last token needs logits
    }
    batch.n_tokens = n_tokens;

    // Process prompt
    if (llama_decode(ctx, batch) != 0) {
        fprintf(stderr, "[SLM] Error: Failed to decode prompt\n");
        llama_batch_free(batch);
        free(prompt_tokens);
        return NULL;
    }
    llama_batch_free(batch);
    free(prompt_tokens);

    // Set up sampler
    struct llama_sampler* sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    if (temp > 0.0f) {
        llama_sampler_chain_add(sampler, llama_sampler_init_temp(temp));
        llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
        llama_sampler_chain_add(sampler, llama_sampler_init_dist(0));
    } else {
        llama_sampler_chain_add(sampler, llama_sampler_init_greedy());
    }

    // Generate tokens
    int buf_size = 4096;
    char* output = (char*)malloc(buf_size);
    int output_len = 0;

    int32_t eos_token = llama_token_eos(model);
    int cur_pos = n_tokens;

    for (int i = 0; i < max_tokens; i++) {
        int32_t new_token = llama_sampler_sample(sampler, ctx, -1);

        if (new_token == eos_token) {
            break;
        }

        // Decode token to text
        char token_buf[128];
        int token_len = llama_token_to_piece(model, new_token, token_buf, sizeof(token_buf), 0, true);

        if (token_len > 0) {
            if (output_len + token_len >= buf_size - 1) {
                buf_size *= 2;
                output = (char*)realloc(output, buf_size);
            }
            memcpy(output + output_len, token_buf, token_len);
            output_len += token_len;
        }

        // Prepare next batch
        struct llama_batch next_batch = llama_batch_init(1, 0, 1);
        next_batch.token[0] = new_token;
        next_batch.pos[0] = cur_pos++;
        next_batch.n_seq_id[0] = 1;
        next_batch.seq_id[0][0] = 0;
        next_batch.logits[0] = true;
        next_batch.n_tokens = 1;

        if (llama_decode(ctx, next_batch) != 0) {
            llama_batch_free(next_batch);
            break;
        }
        llama_batch_free(next_batch);
    }

    llama_sampler_free(sampler);
    output[output_len] = '\0';

    return output;

#else
    // Stub implementation
    fprintf(stderr, "[SLM STUB] simplex_slm_infer: Processing prompt (%.32s...)\n",
            prompt);
    fprintf(stderr, "[SLM STUB] Parameters: temp=%.2f, max_tokens=%d\n", temp, max_tokens);

    // Return a placeholder response
    const char* stub_response = "[SLM STUB RESPONSE] This is a placeholder. "
                                 "Build with llama.cpp for real inference.";
    size_t len = strlen(stub_response);
    char* result = (char*)malloc(len + 1);
    strcpy(result, stub_response);

    return result;
#endif
}

// ============================================================================
// Embedding Operations
// ============================================================================

float* simplex_slm_embed(simplex_slm_t* slm, const char* text, int* out_dim) {
    SLM_DEBUG("simplex_slm_embed called");

    if (slm == NULL || text == NULL || out_dim == NULL) {
        fprintf(stderr, "[SLM] Error: Invalid arguments to simplex_slm_embed\n");
        return NULL;
    }

#if SIMPLEX_SLM_HAS_LLAMA
    llama_model* model = (llama_model*)slm->model;
    llama_context* ctx = (llama_context*)slm->ctx;

    int n_embd = llama_n_embd(model);
    *out_dim = n_embd;

    // Tokenize text
    int n_text = strlen(text);
    int max_tokens = n_text + 4;
    int32_t* tokens = (int32_t*)malloc(max_tokens * sizeof(int32_t));

    int n_tokens = llama_tokenize(model, text, n_text, tokens, max_tokens, true, true);
    if (n_tokens < 0) {
        max_tokens = -n_tokens;
        tokens = (int32_t*)realloc(tokens, max_tokens * sizeof(int32_t));
        n_tokens = llama_tokenize(model, text, n_text, tokens, max_tokens, true, true);
    }

    // Clear KV cache
    llama_kv_cache_clear(ctx);

    // Prepare batch
    struct llama_batch batch = llama_batch_init(n_tokens, 0, 1);
    for (int i = 0; i < n_tokens; i++) {
        batch.token[i] = tokens[i];
        batch.pos[i] = i;
        batch.n_seq_id[i] = 1;
        batch.seq_id[i][0] = 0;
        batch.logits[i] = true;  // Need embeddings
    }
    batch.n_tokens = n_tokens;

    // Decode
    if (llama_decode(ctx, batch) != 0) {
        fprintf(stderr, "[SLM] Error: Failed to decode for embeddings\n");
        llama_batch_free(batch);
        free(tokens);
        return NULL;
    }

    llama_batch_free(batch);
    free(tokens);

    // Get embeddings (mean pooling over sequence)
    float* embeddings = llama_get_embeddings(ctx);
    if (embeddings == NULL) {
        fprintf(stderr, "[SLM] Error: Model does not support embeddings\n");
        return NULL;
    }

    // Copy embeddings
    float* result = (float*)malloc(n_embd * sizeof(float));
    memcpy(result, embeddings, n_embd * sizeof(float));

    return result;

#else
    // Stub implementation
    fprintf(stderr, "[SLM STUB] simplex_slm_embed: Generating embeddings for '%.32s...'\n", text);

    // Return placeholder embeddings (768-dim like common models)
    int dim = 768;
    *out_dim = dim;

    float* result = (float*)malloc(dim * sizeof(float));
    for (int i = 0; i < dim; i++) {
        // Generate pseudo-random values based on text content
        result[i] = sinf((float)(i + 1) * 0.1f + (float)(text[i % strlen(text)])) * 0.5f;
    }

    return result;
#endif
}

float simplex_slm_similarity(float* a, float* b, int dim) {
    SLM_DEBUG("simplex_slm_similarity called: dim=%d", dim);

    if (a == NULL || b == NULL || dim <= 0) {
        return 0.0f;
    }

    // Compute cosine similarity
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (int i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    if (norm_a == 0.0f || norm_b == 0.0f) {
        return 0.0f;
    }

    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

// ============================================================================
// Utility Functions
// ============================================================================

int simplex_slm_embedding_dim(simplex_slm_t* slm) {
    if (slm == NULL) {
        return 0;
    }

#if SIMPLEX_SLM_HAS_LLAMA
    return llama_n_embd((llama_model*)slm->model);
#else
    fprintf(stderr, "[SLM STUB] simplex_slm_embedding_dim: Returning 768 (stub value)\n");
    return 768;  // Common embedding dimension
#endif
}

int simplex_slm_context_size(simplex_slm_t* slm) {
    if (slm == NULL) {
        return 0;
    }
    return slm->n_ctx;
}

int simplex_slm_vocab_size(simplex_slm_t* slm) {
    if (slm == NULL) {
        return 0;
    }

#if SIMPLEX_SLM_HAS_LLAMA
    return llama_n_vocab((llama_model*)slm->model);
#else
    fprintf(stderr, "[SLM STUB] simplex_slm_vocab_size: Returning 32000 (stub value)\n");
    return 32000;  // Common vocab size
#endif
}

bool simplex_slm_is_valid(simplex_slm_t* slm) {
    if (slm == NULL) {
        return false;
    }
    if (slm->model == NULL || slm->ctx == NULL) {
        return false;
    }
    return true;
}
