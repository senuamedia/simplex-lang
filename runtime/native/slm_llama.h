/**
 * Simplex SLM Native Bindings - llama.cpp Integration
 *
 * Simplified C interface for SLM (Small Language Model) operations.
 * This provides the core operations needed for Cognitive Hive AI integration.
 *
 * Copyright (c) 2025-2026 Rod Higgins
 * Licensed under AGPL-3.0
 *
 * Architecture:
 *   - simplex_slm_t: Opaque handle containing model and context
 *   - Core ops: load, free, infer
 *   - Embeddings: embed, similarity (for semantic memory search)
 */

#ifndef SIMPLEX_SLM_LLAMA_H
#define SIMPLEX_SLM_LLAMA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SLM Handle
// ============================================================================

/**
 * Opaque SLM handle containing model and inference context.
 * This wraps llama.cpp's llama_model and llama_context.
 */
typedef struct {
    void* model;      // llama_model*
    void* ctx;        // llama_context*
    int n_ctx;        // Context window size
    int n_threads;    // Number of CPU threads for inference
} simplex_slm_t;

// ============================================================================
// Core Operations
// ============================================================================

/**
 * Load an SLM model from a GGUF file.
 *
 * @param model_path  Path to the GGUF model file
 * @param ctx_size    Context window size (e.g., 2048, 4096)
 * @param threads     Number of CPU threads for inference
 * @return            Pointer to SLM handle, or NULL on failure
 *
 * Example:
 *   simplex_slm_t* slm = simplex_slm_load("models/phi-3-mini.gguf", 4096, 4);
 */
simplex_slm_t* simplex_slm_load(const char* model_path, int ctx_size, int threads);

/**
 * Free an SLM model and release all resources.
 *
 * @param slm  Pointer to SLM handle (may be NULL)
 */
void simplex_slm_free(simplex_slm_t* slm);

/**
 * Run inference on the model with a prompt.
 *
 * @param slm         Pointer to SLM handle
 * @param prompt      Input prompt text (null-terminated)
 * @param temp        Temperature for sampling (0.0 = deterministic, 1.0 = creative)
 * @param max_tokens  Maximum number of tokens to generate
 * @return            Generated text (caller must free), or NULL on failure
 *
 * Example:
 *   char* response = simplex_slm_infer(slm, "What is 2+2?", 0.7f, 256);
 *   printf("%s\n", response);
 *   free(response);
 */
char* simplex_slm_infer(simplex_slm_t* slm, const char* prompt, float temp, int max_tokens);

// ============================================================================
// Embedding Operations (for semantic memory)
// ============================================================================

/**
 * Generate embeddings for a text string.
 * Used for semantic similarity search in cognitive memory.
 *
 * @param slm      Pointer to SLM handle
 * @param text     Input text to embed (null-terminated)
 * @param out_dim  Output: embedding dimension (set by function)
 * @return         Float array of embeddings (caller must free), or NULL on failure
 *
 * Example:
 *   int dim;
 *   float* emb = simplex_slm_embed(slm, "Hello world", &dim);
 *   printf("Embedding dimension: %d\n", dim);
 *   free(emb);
 */
float* simplex_slm_embed(simplex_slm_t* slm, const char* text, int* out_dim);

/**
 * Compute cosine similarity between two embedding vectors.
 *
 * @param a    First embedding vector
 * @param b    Second embedding vector
 * @param dim  Dimension of both vectors
 * @return     Cosine similarity in range [-1.0, 1.0]
 *
 * Example:
 *   float sim = simplex_slm_similarity(emb1, emb2, 768);
 *   if (sim > 0.8f) printf("Very similar!\n");
 */
float simplex_slm_similarity(float* a, float* b, int dim);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get the model's embedding dimension.
 *
 * @param slm  Pointer to SLM handle
 * @return     Embedding dimension (e.g., 768, 1024, 4096)
 */
int simplex_slm_embedding_dim(simplex_slm_t* slm);

/**
 * Get the model's context window size.
 *
 * @param slm  Pointer to SLM handle
 * @return     Context size in tokens
 */
int simplex_slm_context_size(simplex_slm_t* slm);

/**
 * Get the model's vocabulary size.
 *
 * @param slm  Pointer to SLM handle
 * @return     Vocabulary size
 */
int simplex_slm_vocab_size(simplex_slm_t* slm);

/**
 * Check if the SLM handle is valid and loaded.
 *
 * @param slm  Pointer to SLM handle (may be NULL)
 * @return     true if valid and loaded, false otherwise
 */
bool simplex_slm_is_valid(simplex_slm_t* slm);

#ifdef __cplusplus
}
#endif

#endif /* SIMPLEX_SLM_LLAMA_H */
