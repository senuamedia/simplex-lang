// Simplex Standalone Runtime
// Minimal C runtime for bootstrap compiler - self-contained with no external dependencies
//
// Copyright (c) 2025-2026 Rod Higgins
// Licensed under AGPL-3.0 - see LICENSE file
// https://github.com/senuamedia/simplex

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <execinfo.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <math.h>

// String representation matching simplex runtime
typedef struct {
    size_t len;
    size_t cap;
    char* data;
} SxString;

// Vector representation
typedef struct {
    void** items;
    size_t len;
    size_t cap;
} SxVec;

// Entry point wrapper
extern int64_t simplex_main(void);

static int program_argc;
static char** program_argv;

int main(int argc, char** argv) {
    program_argc = argc;
    program_argv = argv;
    return (int)simplex_main();
}

// Memory access helpers (work with 8-byte slots)
void* store_ptr(void* base, int64_t slot, void* value) {
    ((void**)base)[slot] = value;
    return base;
}

static int store_i64_count = 0;
void* store_i64(void* base, int64_t slot, int64_t value) {
    store_i64_count++;
    ((int64_t*)base)[slot] = value;
    return base;
}

void* load_ptr(void* base, int64_t slot) {
    return ((void**)base)[slot];
}

int64_t load_i64(void* base, int64_t slot) {
    return ((int64_t*)base)[slot];
}

// Debug trace function (can be called from LLVM IR if needed)
void debug_trace_i64(int64_t marker, int64_t value) {
    // No-op in release mode
}

// String functions
SxString* intrinsic_string_new(const char* data) {
    SxString* s = malloc(sizeof(SxString));
    if (!data) {
        s->len = 0;
        s->cap = 1;
        s->data = malloc(1);
        s->data[0] = '\0';
    } else {
        s->len = strlen(data);
        s->cap = s->len + 1;
        s->data = malloc(s->cap);
        memcpy(s->data, data, s->cap);
    }
    return s;
}

SxString* intrinsic_string_from_char(int64_t c) {
    SxString* s = malloc(sizeof(SxString));
    s->len = 1;
    s->cap = 2;
    s->data = malloc(2);
    s->data[0] = (char)c;
    s->data[1] = '\0';
    return s;
}

SxString* intrinsic_string_concat(SxString* a, SxString* b) {
    if (!a || !b) return intrinsic_string_new("");
    SxString* s = malloc(sizeof(SxString));
    s->len = a->len + b->len;
    s->cap = s->len + 1;
    s->data = malloc(s->cap);
    memcpy(s->data, a->data, a->len);
    memcpy(s->data + a->len, b->data, b->len);
    s->data[s->len] = '\0';
    return s;
}

int64_t intrinsic_string_len(SxString* str) {
    return str ? str->len : 0;
}

int8_t intrinsic_string_eq(SxString* a, SxString* b) {
    if (!a || !b) return a == b;
    if (a->len != b->len) return 0;
    return memcmp(a->data, b->data, a->len) == 0;
}

int64_t intrinsic_string_char_at(SxString* str, int64_t index) {
    if (!str || index < 0 || (size_t)index >= str->len) return -1;
    return (unsigned char)str->data[index];
}

SxString* intrinsic_string_slice(SxString* str, int64_t start, int64_t end) {
    if (!str || !str->data) return intrinsic_string_new("");
    if (start < 0) start = 0;
    if (end > (int64_t)str->len) end = str->len;
    if (start >= end) return intrinsic_string_new("");

    size_t len = end - start;
    SxString* result = malloc(sizeof(SxString));
    result->len = len;
    result->cap = len + 1;
    result->data = malloc(len + 1);
    memcpy(result->data, str->data + start, len);
    result->data[len] = '\0';
    return result;
}

int64_t intrinsic_string_to_int(SxString* str) {
    if (!str || !str->data) return 0;
    return strtoll(str->data, NULL, 10);
}

SxString* intrinsic_int_to_string(int64_t value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)value);
    return intrinsic_string_new(buf);
}

// Vector functions
SxVec* intrinsic_vec_new(void) {
    SxVec* v = malloc(sizeof(SxVec));
    v->items = NULL;
    v->len = 0;
    v->cap = 0;
    return v;
}

void intrinsic_vec_push(SxVec* vec, void* item) {
    if (!vec) return;
    if (vec->len >= vec->cap) {
        size_t new_cap = vec->cap == 0 ? 8 : vec->cap * 2;
        vec->items = realloc(vec->items, new_cap * sizeof(void*));
        vec->cap = new_cap;
    }
    vec->items[vec->len++] = item;
}

void* intrinsic_vec_get(SxVec* vec, int64_t index) {
    if (!vec || index < 0 || (size_t)index >= vec->len) {
        return NULL;
    }
    return vec->items[index];
}

void intrinsic_vec_free(SxVec* vec) {
    if (!vec) return;
    if (vec->items) free(vec->items);
    free(vec);
}

int64_t intrinsic_vec_len(SxVec* vec) {
    return vec ? vec->len : 0;
}

// Set item at index
void intrinsic_vec_set(SxVec* vec, int64_t index, void* item) {
    if (!vec || index < 0 || (size_t)index >= vec->len) {
        return;
    }
    vec->items[index] = item;
}

// Pop last item (returns NULL if empty)
void* intrinsic_vec_pop(SxVec* vec) {
    if (!vec || vec->len == 0) return NULL;
    return vec->items[--vec->len];
}

// Clear all items
void intrinsic_vec_clear(SxVec* vec) {
    if (!vec) return;
    vec->len = 0;
}

// Remove item at index (shifts remaining elements)
void intrinsic_vec_remove(SxVec* vec, int64_t index) {
    if (!vec || index < 0 || (size_t)index >= vec->len) return;
    for (size_t i = (size_t)index; i < vec->len - 1; i++) {
        vec->items[i] = vec->items[i + 1];
    }
    vec->len--;
}

// Iterator type for Vec iteration
typedef struct {
    SxVec* vec;
    size_t index;
} VecIterator;

// Create an iterator over a Vec
int64_t vec_iter(int64_t vec_ptr) {
    SxVec* vec = (SxVec*)vec_ptr;
    if (!vec) return 0;

    VecIterator* iter = malloc(sizeof(VecIterator));
    iter->vec = vec;
    iter->index = 0;
    return (int64_t)iter;
}

// Get next element from iterator (returns Option<T>: 0=None, value|1=Some)
int64_t iter_next(int64_t iter_ptr) {
    VecIterator* iter = (VecIterator*)iter_ptr;
    if (!iter || !iter->vec || iter->index >= iter->vec->len) {
        return 0;  // None
    }

    int64_t value = (int64_t)iter->vec->items[iter->index];
    iter->index++;
    // Return Option::Some(value) - pack value in upper bits, tag 1 in lower byte
    return (value << 8) | 1;
}

// Free iterator
void iter_free(int64_t iter_ptr) {
    if (iter_ptr) free((void*)iter_ptr);
}

// IO functions
void intrinsic_println(SxString* str) {
    if (str && str->data) {
        printf("%s\n", str->data);
    } else {
        printf("\n");
    }
}

SxString* intrinsic_read_file(SxString* path) {
    if (!path || !path->data) return NULL;

    FILE* f = fopen(path->data, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    SxString* result = malloc(sizeof(SxString));
    result->len = size;
    result->cap = size + 1;
    result->data = malloc(size + 1);
    fread(result->data, 1, size, f);
    result->data[size] = '\0';
    fclose(f);

    return result;
}

void intrinsic_write_file(SxString* path, SxString* content) {
    if (!path || !path->data || !content || !content->data) return;

    FILE* f = fopen(path->data, "wb");
    if (!f) return;

    fwrite(content->data, 1, content->len, f);
    fclose(f);
}

SxVec* intrinsic_get_args(void) {
    SxVec* args = intrinsic_vec_new();
    for (int i = 0; i < program_argc; i++) {
        intrinsic_vec_push(args, intrinsic_string_new(program_argv[i]));
    }
    return args;
}

// Forward declarations for AI functions
int64_t http_request_new(int64_t method_ptr, int64_t url_ptr);
void http_request_header(int64_t req_ptr, int64_t name_ptr, int64_t value_ptr);
void http_request_body(int64_t req_ptr, int64_t body_ptr);
int64_t http_request_send(int64_t req_ptr);
void http_request_free(int64_t req_ptr);
int64_t http_response_status(int64_t resp_ptr);
int64_t http_response_body(int64_t resp_ptr);
void http_response_free(int64_t resp_ptr);

// ========================================
// Native LLM Integration (llama.cpp)
// ========================================
#ifdef SIMPLEX_LLAMA
#include "llama.h"

// Helper functions (from common.h, implemented here to avoid C++ dependency)
static void llama_batch_add_token(struct llama_batch *batch, llama_token id, llama_pos pos, const llama_seq_id *seq_ids, size_t n_seq, bool logits) {
    batch->token[batch->n_tokens] = id;
    batch->pos[batch->n_tokens] = pos;
    batch->n_seq_id[batch->n_tokens] = n_seq;
    for (size_t i = 0; i < n_seq; ++i) {
        batch->seq_id[batch->n_tokens][i] = seq_ids[i];
    }
    batch->logits[batch->n_tokens] = logits;
    batch->n_tokens++;
}

static void llama_batch_clear_tokens(struct llama_batch *batch) {
    batch->n_tokens = 0;
}

// Global model state for the hive
typedef struct {
    struct llama_model* model;
    struct llama_context* ctx;
    char* model_path;
    int n_ctx;       // context size
    int n_threads;   // inference threads
} NativeModel;

static NativeModel* g_native_model = NULL;
static pthread_mutex_t g_model_mutex = PTHREAD_MUTEX_INITIALIZER;

// Load a GGUF model natively
int64_t native_model_load(int64_t path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path || !path->data) return 0;

    pthread_mutex_lock(&g_model_mutex);

    // Free existing model if any
    if (g_native_model) {
        if (g_native_model->ctx) llama_free(g_native_model->ctx);
        if (g_native_model->model) llama_model_free(g_native_model->model);
        if (g_native_model->model_path) free(g_native_model->model_path);
        free(g_native_model);
        g_native_model = NULL;
    }

    // Initialize llama backend
    llama_backend_init();

    // Model parameters - use all GPU layers if available
    struct llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = -1;  // -1 = all layers on GPU (falls back to CPU if no GPU)

    // Load model
    struct llama_model* model = llama_model_load_from_file(path->data, model_params);
    if (!model) {
        fprintf(stderr, "[Native LLM] Failed to load model: %s\n", path->data);
        pthread_mutex_unlock(&g_model_mutex);
        return 0;
    }

    // Detect CPU cores for optimal threading
    int n_threads = sysconf(_SC_NPROCESSORS_ONLN);
    if (n_threads <= 0) n_threads = 4;
    if (n_threads > 8) n_threads = 8;  // Cap at 8 for efficiency

    // Context parameters - optimized for speed
    struct llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 2048;          // Reduced context for faster processing
    ctx_params.n_threads = n_threads;
    ctx_params.n_threads_batch = n_threads;

    // Create context
    struct llama_context* ctx = llama_init_from_model(model, ctx_params);
    if (!ctx) {
        fprintf(stderr, "[Native LLM] Failed to create context\n");
        llama_model_free(model);
        pthread_mutex_unlock(&g_model_mutex);
        return 0;
    }

    // Store in global state
    g_native_model = (NativeModel*)malloc(sizeof(NativeModel));
    g_native_model->model = model;
    g_native_model->ctx = ctx;
    g_native_model->model_path = strdup(path->data);
    g_native_model->n_ctx = 2048;
    g_native_model->n_threads = n_threads;

    fprintf(stderr, "[Native LLM] Loaded model: %s\n", path->data);
    pthread_mutex_unlock(&g_model_mutex);
    return (int64_t)g_native_model;
}

// Run inference on loaded model
int64_t native_model_infer(int64_t prompt_ptr, int64_t max_tokens) {
    SxString* prompt = (SxString*)prompt_ptr;
    if (!prompt || !prompt->data) return (int64_t)intrinsic_string_new("[Error: No prompt]");

    pthread_mutex_lock(&g_model_mutex);

    if (!g_native_model || !g_native_model->ctx) {
        pthread_mutex_unlock(&g_model_mutex);
        return (int64_t)intrinsic_string_new("[Error: No model loaded]");
    }

    // Get vocab from model
    const struct llama_vocab* vocab = llama_model_get_vocab(g_native_model->model);

    // Tokenize input - first call returns NEGATIVE count when buffer too small
    int n_tokens = llama_tokenize(vocab, prompt->data, prompt->len, NULL, 0, true, false);
    if (n_tokens < 0) {
        n_tokens = -n_tokens;  // Negate to get actual token count
    }
    if (n_tokens == 0) {
        pthread_mutex_unlock(&g_model_mutex);
        return (int64_t)intrinsic_string_new("[Error: Empty prompt or tokenization failed]");
    }
    llama_token* tokens = (llama_token*)malloc((n_tokens + 1) * sizeof(llama_token));
    int actual = llama_tokenize(vocab, prompt->data, prompt->len, tokens, n_tokens + 1, true, false);
    if (actual < 0) actual = -actual;  // Handle negative return

    // Clear context memory (use llama_get_memory to get memory handle)
    llama_memory_clear(llama_get_memory(g_native_model->ctx), true);

    // Evaluate prompt
    struct llama_batch batch = llama_batch_init(512, 0, 1);
    for (int i = 0; i < n_tokens; i++) {
        llama_batch_add_token(&batch, tokens[i], i, (llama_seq_id[]){0}, 1, false);
    }
    batch.logits[batch.n_tokens - 1] = true;  // Only get logits for last token

    if (llama_decode(g_native_model->ctx, batch) != 0) {
        free(tokens);
        llama_batch_free(batch);
        pthread_mutex_unlock(&g_model_mutex);
        return (int64_t)intrinsic_string_new("[Error: Decode failed]");
    }

    // Generate response using optimized llama sampler
    char* output = (char*)malloc(max_tokens * 4 + 1);
    int output_len = 0;

    // Create optimized greedy sampler chain
    struct llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    struct llama_sampler* smpl = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(smpl, llama_sampler_init_greedy());

    int n_cur = batch.n_tokens;
    int n_gen = 0;

    while (n_gen < max_tokens) {
        // Use optimized sampler instead of manual O(vocab) loop
        llama_token new_token_id = llama_sampler_sample(smpl, g_native_model->ctx, batch.n_tokens - 1);

        // Check for end of generation
        if (llama_vocab_is_eog(vocab, new_token_id)) {
            break;
        }

        // Convert token to text
        char token_buf[256];
        int token_len = llama_token_to_piece(vocab, new_token_id, token_buf, sizeof(token_buf), 0, false);
        if (token_len > 0 && output_len + token_len < max_tokens * 4) {
            memcpy(output + output_len, token_buf, token_len);
            output_len += token_len;
        }

        // Check for Qwen chat end token in output
        if (output_len >= 10) {
            const char* end_marker = "<|im_end|>";
            if (strstr(output + output_len - 15, end_marker) != NULL) {
                char* pos = strstr(output, end_marker);
                if (pos) *pos = '\0';
                break;
            }
        }

        // Prepare next batch
        llama_batch_clear_tokens(&batch);
        llama_batch_add_token(&batch, new_token_id, n_cur, (llama_seq_id[]){0}, 1, true);

        if (llama_decode(g_native_model->ctx, batch) != 0) {
            break;
        }

        n_cur++;
        n_gen++;
    }

    output[output_len] = '\0';
    llama_sampler_free(smpl);

    free(tokens);
    llama_batch_free(batch);
    pthread_mutex_unlock(&g_model_mutex);

    SxString* result = intrinsic_string_new(output);
    free(output);
    return (int64_t)result;
}

// Free the loaded model
void native_model_free(void) {
    pthread_mutex_lock(&g_model_mutex);
    if (g_native_model) {
        if (g_native_model->ctx) llama_free(g_native_model->ctx);
        if (g_native_model->model) llama_model_free(g_native_model->model);
        if (g_native_model->model_path) free(g_native_model->model_path);
        free(g_native_model);
        g_native_model = NULL;
    }
    llama_backend_free();
    pthread_mutex_unlock(&g_model_mutex);
}

// Check if native model is loaded
int64_t native_model_loaded(void) {
    pthread_mutex_lock(&g_model_mutex);
    int64_t loaded = (g_native_model != NULL) ? 1 : 0;
    pthread_mutex_unlock(&g_model_mutex);
    return loaded;
}

#else
// Stub implementations when llama.cpp not available
int64_t native_model_load(int64_t path_ptr) {
    (void)path_ptr;
    fprintf(stderr, "[Native LLM] Not compiled with SIMPLEX_LLAMA support\n");
    return 0;
}

int64_t native_model_infer(int64_t prompt_ptr, int64_t max_tokens) {
    (void)prompt_ptr;
    (void)max_tokens;
    return (int64_t)intrinsic_string_new("[Native LLM not available - compile with -DSIMPLEX_LLAMA]");
}

void native_model_free(void) {}

int64_t native_model_loaded(void) { return 0; }
#endif // SIMPLEX_LLAMA

// AI intrinsics (real implementation with fallback)
SxString* intrinsic_ai_infer(SxString* model, SxString* prompt, int64_t temperature) {
    (void)temperature;  // TODO: Use temperature in native inference

    // Try native inference first if model is loaded
    // Use 256 max tokens for complete responses (GPU makes this fast)
    if (native_model_loaded()) {
        return (SxString*)native_model_infer((int64_t)prompt, 256);
    }

    // Fallback to API if no native model
    const char* api_key = getenv("ANTHROPIC_API_KEY");
    if (!api_key) api_key = getenv("OPENAI_API_KEY");

    if (!api_key) {
        // Fall back to mock if no API key
        char buf[512];
        const char* model_name = model && model->data ? model->data : "default";
        const char* prompt_text = prompt && prompt->data ? prompt->data : "";
        snprintf(buf, sizeof(buf), "[No API key - Mock AI:%s] Response to: %.100s", model_name, prompt_text);
        return intrinsic_string_new(buf);
    }

    // Build JSON request
    const char* model_name = model && model->data ? model->data : "claude-3-haiku-20240307";
    const char* prompt_text = prompt && prompt->data ? prompt->data : "";

    // Escape prompt for JSON
    size_t plen = strlen(prompt_text);
    char* escaped = (char*)malloc(plen * 2 + 1);
    char* w = escaped;
    for (const char* r = prompt_text; *r; r++) {
        switch (*r) {
            case '"': *w++ = '\\'; *w++ = '"'; break;
            case '\\': *w++ = '\\'; *w++ = '\\'; break;
            case '\n': *w++ = '\\'; *w++ = 'n'; break;
            case '\r': *w++ = '\\'; *w++ = 'r'; break;
            case '\t': *w++ = '\\'; *w++ = 't'; break;
            default: *w++ = *r; break;
        }
    }
    *w = '\0';

    char* body = (char*)malloc(strlen(escaped) + 512);
    sprintf(body,
        "{\"model\":\"%s\",\"max_tokens\":1024,\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}",
        model_name, escaped);
    free(escaped);

    // Create HTTP request
    const char* url = "https://api.anthropic.com/v1/messages";
    SxString url_str = { .data = (char*)url, .len = strlen(url), .cap = 0 };
    SxString method_str = { .data = "POST", .len = 4, .cap = 0 };
    int64_t req = http_request_new((int64_t)&method_str, (int64_t)&url_str);

    // Set headers
    SxString ct_name = { .data = "Content-Type", .len = 12, .cap = 0 };
    SxString ct_value = { .data = "application/json", .len = 16, .cap = 0 };
    http_request_header(req, (int64_t)&ct_name, (int64_t)&ct_value);

    SxString auth_name = { .data = "x-api-key", .len = 9, .cap = 0 };
    SxString auth_value = { .data = (char*)api_key, .len = strlen(api_key), .cap = 0 };
    http_request_header(req, (int64_t)&auth_name, (int64_t)&auth_value);

    SxString ver_name = { .data = "anthropic-version", .len = 17, .cap = 0 };
    SxString ver_value = { .data = "2023-06-01", .len = 10, .cap = 0 };
    http_request_header(req, (int64_t)&ver_name, (int64_t)&ver_value);

    SxString body_str = { .data = body, .len = strlen(body), .cap = 0 };
    http_request_body(req, (int64_t)&body_str);

    // Send request
    int64_t resp = http_request_send(req);
    http_request_free(req);
    free(body);

    if (!resp) {
        return intrinsic_string_new("[Error: HTTP request failed]");
    }

    int64_t status = http_response_status(resp);
    if (status != 200) {
        char err[256];
        snprintf(err, sizeof(err), "[Error: HTTP %lld]", (long long)status);
        http_response_free(resp);
        return intrinsic_string_new(err);
    }

    // Parse response - extract "text" field
    int64_t body_ptr = http_response_body(resp);
    SxString* response_body = (SxString*)body_ptr;
    if (!response_body || !response_body->data) {
        http_response_free(resp);
        return intrinsic_string_new("[Error: Empty response]");
    }

    // Simple JSON extraction for "text" field
    const char* text_key = "\"text\"";
    char* pos = strstr(response_body->data, text_key);
    if (pos) {
        pos = strchr(pos + strlen(text_key), ':');
        if (pos) {
            pos++;
            while (*pos == ' ' || *pos == '\t') pos++;
            if (*pos == '"') {
                pos++;
                char* end = pos;
                while (*end && *end != '"') {
                    if (*end == '\\' && *(end+1)) end += 2;
                    else end++;
                }
                size_t len = end - pos;
                char* result = (char*)malloc(len + 1);
                memcpy(result, pos, len);
                result[len] = '\0';
                http_response_free(resp);
                SxString* ret = intrinsic_string_new(result);
                free(result);
                return ret;
            }
        }
    }

    http_response_free(resp);
    return intrinsic_string_new("[Error: Could not parse response]");
}

double* intrinsic_ai_embed(SxString* text) {
    // Mock implementation: return zero vector
    double* vec = malloc(1536 * sizeof(double));
    for (int i = 0; i < 1536; i++) {
        vec[i] = 0.0;
    }
    // Set first element based on text length for basic similarity testing
    if (text && text->data) {
        vec[0] = (double)text->len / 100.0;
    }
    return vec;
}

// ========================================
// Phase 3: Hive Router System
// ========================================

typedef enum {
    ROUTER_RULE = 0,      // Route based on message type rules
    ROUTER_ROUND_ROBIN,   // Round-robin distribution
    ROUTER_RANDOM,        // Random selection
    ROUTER_LEAST_BUSY,    // Route to least busy specialist
    ROUTER_SEMANTIC       // AI-based semantic routing
} RouterType;

typedef struct RoutingRule {
    char* message_type;     // Pattern to match
    int64_t specialist_idx; // Target specialist index
    struct RoutingRule* next;
} RoutingRule;

typedef struct HiveRouter {
    RouterType type;
    int64_t specialist_count;
    int64_t* specialists;     // Array of specialist pointers
    int64_t current_idx;      // For round-robin
    RoutingRule* rules;       // For rule-based routing
    pthread_mutex_t lock;
} HiveRouter;

// Create a new router
int64_t router_new(int64_t type, int64_t spec_count) {
    HiveRouter* router = (HiveRouter*)malloc(sizeof(HiveRouter));
    if (!router) return 0;

    router->type = (RouterType)type;
    router->specialist_count = spec_count;
    router->specialists = (int64_t*)calloc(spec_count, sizeof(int64_t));
    router->current_idx = 0;
    router->rules = NULL;
    pthread_mutex_init(&router->lock, NULL);

    return (int64_t)router;
}

// Set specialist at index
int64_t router_set_specialist(int64_t router_ptr, int64_t idx, int64_t spec_ptr) {
    HiveRouter* router = (HiveRouter*)router_ptr;
    if (!router || idx < 0 || idx >= router->specialist_count) return 0;

    pthread_mutex_lock(&router->lock);
    router->specialists[idx] = spec_ptr;
    pthread_mutex_unlock(&router->lock);
    return 1;
}

// Add routing rule (for ROUTER_RULE type)
int64_t router_add_rule(int64_t router_ptr, int64_t pattern_ptr, int64_t spec_idx) {
    HiveRouter* router = (HiveRouter*)router_ptr;
    SxString* pattern = (SxString*)pattern_ptr;
    if (!router || !pattern || !pattern->data) return 0;

    RoutingRule* rule = (RoutingRule*)malloc(sizeof(RoutingRule));
    rule->message_type = strdup(pattern->data);
    rule->specialist_idx = spec_idx;

    pthread_mutex_lock(&router->lock);
    rule->next = router->rules;
    router->rules = rule;
    pthread_mutex_unlock(&router->lock);

    return 1;
}

// Route a message - returns specialist pointer
int64_t router_route(int64_t router_ptr, int64_t message_type_ptr) {
    HiveRouter* router = (HiveRouter*)router_ptr;
    if (!router || router->specialist_count == 0) return 0;

    pthread_mutex_lock(&router->lock);
    int64_t result = 0;

    switch (router->type) {
        case ROUTER_RULE: {
            SxString* msg_type = (SxString*)message_type_ptr;
            if (msg_type && msg_type->data) {
                RoutingRule* rule = router->rules;
                while (rule) {
                    if (strstr(msg_type->data, rule->message_type)) {
                        if (rule->specialist_idx < router->specialist_count) {
                            result = router->specialists[rule->specialist_idx];
                            break;
                        }
                    }
                    rule = rule->next;
                }
            }
            // Fall back to first specialist if no rule matches
            if (!result && router->specialist_count > 0) {
                result = router->specialists[0];
            }
            break;
        }

        case ROUTER_ROUND_ROBIN: {
            result = router->specialists[router->current_idx];
            router->current_idx = (router->current_idx + 1) % router->specialist_count;
            break;
        }

        case ROUTER_RANDOM: {
            int idx = rand() % router->specialist_count;
            result = router->specialists[idx];
            break;
        }

        case ROUTER_LEAST_BUSY:
        case ROUTER_SEMANTIC:
        default:
            // Default to round-robin for unimplemented types
            result = router->specialists[router->current_idx];
            router->current_idx = (router->current_idx + 1) % router->specialist_count;
            break;
    }

    pthread_mutex_unlock(&router->lock);
    return result;
}

// Get router type constants
int64_t router_type_rule(void) { return ROUTER_RULE; }
int64_t router_type_round_robin(void) { return ROUTER_ROUND_ROBIN; }
int64_t router_type_random(void) { return ROUTER_RANDOM; }
int64_t router_type_least_busy(void) { return ROUTER_LEAST_BUSY; }
int64_t router_type_semantic(void) { return ROUTER_SEMANTIC; }

// Close router
void router_close(int64_t router_ptr) {
    HiveRouter* router = (HiveRouter*)router_ptr;
    if (!router) return;

    pthread_mutex_lock(&router->lock);

    // Free rules
    RoutingRule* rule = router->rules;
    while (rule) {
        RoutingRule* next = rule->next;
        free(rule->message_type);
        free(rule);
        rule = next;
    }

    free(router->specialists);
    pthread_mutex_unlock(&router->lock);
    pthread_mutex_destroy(&router->lock);
    free(router);
}

// ========================================
// Phase 3: Hive Management
// ========================================

typedef struct Hive {
    char* name;
    int64_t* specialists;
    int64_t specialist_count;
    int64_t router;
    int64_t strategy;  // Supervision strategy
    pthread_mutex_t lock;
} Hive;

// Create a new hive
int64_t hive_new(int64_t name_ptr, int64_t spec_count) {
    Hive* hive = (Hive*)malloc(sizeof(Hive));
    if (!hive) return 0;

    SxString* name = (SxString*)name_ptr;
    hive->name = name && name->data ? strdup(name->data) : strdup("Hive");
    hive->specialists = (int64_t*)calloc(spec_count, sizeof(int64_t));
    hive->specialist_count = spec_count;
    hive->router = 0;
    hive->strategy = 0;  // OneForOne default
    pthread_mutex_init(&hive->lock, NULL);

    return (int64_t)hive;
}

// Add specialist to hive
int64_t hive_add_specialist(int64_t hive_ptr, int64_t idx, int64_t spec_ptr) {
    Hive* hive = (Hive*)hive_ptr;
    if (!hive || idx < 0 || idx >= hive->specialist_count) return 0;

    pthread_mutex_lock(&hive->lock);
    hive->specialists[idx] = spec_ptr;
    pthread_mutex_unlock(&hive->lock);
    return 1;
}

// Set router for hive
int64_t hive_set_router(int64_t hive_ptr, int64_t router_ptr) {
    Hive* hive = (Hive*)hive_ptr;
    if (!hive) return 0;

    pthread_mutex_lock(&hive->lock);
    hive->router = router_ptr;
    pthread_mutex_unlock(&hive->lock);
    return 1;
}

// Set supervision strategy
int64_t hive_set_strategy(int64_t hive_ptr, int64_t strategy) {
    Hive* hive = (Hive*)hive_ptr;
    if (!hive) return 0;

    pthread_mutex_lock(&hive->lock);
    hive->strategy = strategy;
    pthread_mutex_unlock(&hive->lock);
    return 1;
}

// Route message through hive
int64_t hive_route(int64_t hive_ptr, int64_t message_type_ptr, int64_t message_ptr) {
    Hive* hive = (Hive*)hive_ptr;
    if (!hive) return 0;

    if (hive->router) {
        return router_route(hive->router, message_type_ptr);
    }

    // Default: return first specialist
    if (hive->specialist_count > 0) {
        return hive->specialists[0];
    }
    return 0;
}

// Get specialist by index
int64_t hive_get_specialist(int64_t hive_ptr, int64_t idx) {
    Hive* hive = (Hive*)hive_ptr;
    if (!hive || idx < 0 || idx >= hive->specialist_count) return 0;

    pthread_mutex_lock(&hive->lock);
    int64_t result = hive->specialists[idx];
    pthread_mutex_unlock(&hive->lock);
    return result;
}

// Get specialist count
int64_t hive_specialist_count(int64_t hive_ptr) {
    Hive* hive = (Hive*)hive_ptr;
    return hive ? hive->specialist_count : 0;
}

// Close hive
void hive_close(int64_t hive_ptr) {
    Hive* hive = (Hive*)hive_ptr;
    if (!hive) return;

    pthread_mutex_lock(&hive->lock);

    if (hive->router) {
        router_close(hive->router);
    }
    free(hive->name);
    free(hive->specialists);

    pthread_mutex_unlock(&hive->lock);
    pthread_mutex_destroy(&hive->lock);
    free(hive);
}

// ========================================
// Phase 3: Shared Vector Store
// ========================================

// Entry in vector store
typedef struct VectorEntry {
    char* key;           // Unique key
    char* content;       // Text content
    double* embedding;   // Vector embedding
    int64_t dim;         // Embedding dimension
    int64_t owner;       // Owner specialist ID (0 = shared)
    int64_t timestamp;   // Creation timestamp
    struct VectorEntry* next;
} VectorEntry;

// Shared vector store for specialist collaboration
typedef struct SharedVectorStore {
    char* name;
    VectorEntry* entries;
    int64_t entry_count;
    int64_t dim;             // Default embedding dimension
    pthread_mutex_t lock;
} SharedVectorStore;

// Global shared store (singleton for simplicity)
static SharedVectorStore* global_shared_store = NULL;
static pthread_mutex_t global_store_lock = PTHREAD_MUTEX_INITIALIZER;

// Create shared vector store
int64_t shared_store_new(int64_t name_ptr, int64_t dim) {
    SharedVectorStore* store = (SharedVectorStore*)malloc(sizeof(SharedVectorStore));
    if (!store) return 0;

    SxString* name = (SxString*)name_ptr;
    store->name = name && name->data ? strdup(name->data) : strdup("SharedStore");
    store->entries = NULL;
    store->entry_count = 0;
    store->dim = dim > 0 ? dim : 64;
    pthread_mutex_init(&store->lock, NULL);

    return (int64_t)store;
}

// Get or create global shared store
int64_t shared_store_global(int64_t dim) {
    pthread_mutex_lock(&global_store_lock);
    if (!global_shared_store) {
        global_shared_store = (SharedVectorStore*)malloc(sizeof(SharedVectorStore));
        global_shared_store->name = strdup("GlobalSharedStore");
        global_shared_store->entries = NULL;
        global_shared_store->entry_count = 0;
        global_shared_store->dim = dim > 0 ? dim : 64;
        pthread_mutex_init(&global_shared_store->lock, NULL);
    }
    pthread_mutex_unlock(&global_store_lock);
    return (int64_t)global_shared_store;
}

// Simple embedding for text (using hash-based approach)
static void compute_shared_embedding(const char* text, double* out, int dim) {
    memset(out, 0, dim * sizeof(double));
    if (!text) return;

    size_t len = strlen(text);
    for (size_t i = 0; i < len; i++) {
        int idx = (int)(text[i] * 31 + i) % dim;
        if (idx < 0) idx = -idx;
        out[idx] += 1.0 / (1.0 + i * 0.01);
    }

    // Normalize
    double norm = 0;
    for (int i = 0; i < dim; i++) norm += out[i] * out[i];
    norm = sqrt(norm);
    if (norm > 0) {
        for (int i = 0; i < dim; i++) out[i] /= norm;
    }
}

// Store content with auto-generated embedding
int64_t shared_store_put(int64_t store_ptr, int64_t key_ptr, int64_t content_ptr, int64_t owner) {
    SharedVectorStore* store = (SharedVectorStore*)store_ptr;
    SxString* key = (SxString*)key_ptr;
    SxString* content = (SxString*)content_ptr;

    if (!store || !key || !key->data || !content || !content->data) return 0;

    VectorEntry* entry = (VectorEntry*)malloc(sizeof(VectorEntry));
    entry->key = strdup(key->data);
    entry->content = strdup(content->data);
    entry->dim = store->dim;
    entry->embedding = (double*)malloc(store->dim * sizeof(double));
    entry->owner = owner;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    entry->timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    compute_shared_embedding(content->data, entry->embedding, (int)store->dim);

    pthread_mutex_lock(&store->lock);
    entry->next = store->entries;
    store->entries = entry;
    store->entry_count++;
    pthread_mutex_unlock(&store->lock);

    return 1;
}

// Get content by key
int64_t shared_store_get(int64_t store_ptr, int64_t key_ptr) {
    SharedVectorStore* store = (SharedVectorStore*)store_ptr;
    SxString* key = (SxString*)key_ptr;

    if (!store || !key || !key->data) return 0;

    pthread_mutex_lock(&store->lock);
    VectorEntry* entry = store->entries;
    while (entry) {
        if (strcmp(entry->key, key->data) == 0) {
            SxString* result = intrinsic_string_new(entry->content);
            pthread_mutex_unlock(&store->lock);
            return (int64_t)result;
        }
        entry = entry->next;
    }
    pthread_mutex_unlock(&store->lock);
    return 0;
}

// Cosine similarity between two vectors
static double vector_cosine_similarity(double* a, double* b, int dim) {
    double dot = 0, norm_a = 0, norm_b = 0;
    for (int i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    double denom = sqrt(norm_a) * sqrt(norm_b);
    return denom > 0 ? dot / denom : 0;
}

// Search by similarity - returns array of matching keys
int64_t shared_store_search(int64_t store_ptr, int64_t query_ptr, int64_t top_k) {
    SharedVectorStore* store = (SharedVectorStore*)store_ptr;
    SxString* query = (SxString*)query_ptr;

    if (!store || !query || !query->data || top_k <= 0) return 0;

    // Compute query embedding
    double* query_emb = (double*)malloc(store->dim * sizeof(double));
    compute_shared_embedding(query->data, query_emb, (int)store->dim);

    pthread_mutex_lock(&store->lock);

    // Collect all entries with scores
    int64_t count = store->entry_count;
    if (count == 0) {
        pthread_mutex_unlock(&store->lock);
        free(query_emb);
        return 0;
    }

    typedef struct { VectorEntry* entry; double score; } Scored;
    Scored* scored = (Scored*)malloc(count * sizeof(Scored));
    VectorEntry* entry = store->entries;
    int64_t i = 0;
    while (entry && i < count) {
        scored[i].entry = entry;
        scored[i].score = vector_cosine_similarity(query_emb, entry->embedding, (int)store->dim);
        entry = entry->next;
        i++;
    }

    // Simple sort by score (descending)
    for (int64_t j = 0; j < i - 1; j++) {
        for (int64_t k = j + 1; k < i; k++) {
            if (scored[k].score > scored[j].score) {
                Scored tmp = scored[j];
                scored[j] = scored[k];
                scored[k] = tmp;
            }
        }
    }

    // Build result string with top_k keys
    int64_t actual_k = top_k < i ? top_k : i;
    size_t result_size = 1;
    for (int64_t j = 0; j < actual_k; j++) {
        result_size += strlen(scored[j].entry->key) + 1;
    }

    char* result = (char*)malloc(result_size);
    result[0] = '\0';
    for (int64_t j = 0; j < actual_k; j++) {
        if (j > 0) strcat(result, "\n");
        strcat(result, scored[j].entry->key);
    }

    pthread_mutex_unlock(&store->lock);
    free(query_emb);
    free(scored);

    return (int64_t)intrinsic_string_new(result);
}

// Delete entry by key
int64_t shared_store_delete(int64_t store_ptr, int64_t key_ptr) {
    SharedVectorStore* store = (SharedVectorStore*)store_ptr;
    SxString* key = (SxString*)key_ptr;

    if (!store || !key || !key->data) return 0;

    pthread_mutex_lock(&store->lock);
    VectorEntry** pp = &store->entries;
    while (*pp) {
        if (strcmp((*pp)->key, key->data) == 0) {
            VectorEntry* to_free = *pp;
            *pp = (*pp)->next;
            free(to_free->key);
            free(to_free->content);
            free(to_free->embedding);
            free(to_free);
            store->entry_count--;
            pthread_mutex_unlock(&store->lock);
            return 1;
        }
        pp = &(*pp)->next;
    }
    pthread_mutex_unlock(&store->lock);
    return 0;
}

// Get entry count
int64_t shared_store_count(int64_t store_ptr) {
    SharedVectorStore* store = (SharedVectorStore*)store_ptr;
    return store ? store->entry_count : 0;
}

// Close store
void shared_store_close(int64_t store_ptr) {
    SharedVectorStore* store = (SharedVectorStore*)store_ptr;
    if (!store) return;

    pthread_mutex_lock(&store->lock);

    VectorEntry* entry = store->entries;
    while (entry) {
        VectorEntry* next = entry->next;
        free(entry->key);
        free(entry->content);
        free(entry->embedding);
        free(entry);
        entry = next;
    }

    free(store->name);
    pthread_mutex_unlock(&store->lock);
    pthread_mutex_destroy(&store->lock);
    free(store);
}

// Timing intrinsics for performance measurement
int64_t intrinsic_get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;
}

int64_t intrinsic_get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + (int64_t)tv.tv_usec;
}

// Arena allocator for efficient memory management
typedef struct {
    char* base;
    int64_t capacity;
    int64_t offset;
} Arena;

Arena* intrinsic_arena_create(int64_t capacity) {
    Arena* a = malloc(sizeof(Arena));
    a->base = malloc(capacity);
    a->capacity = capacity;
    a->offset = 0;
    return a;
}

void* intrinsic_arena_alloc(Arena* a, int64_t size) {
    if (!a || a->offset + size > a->capacity) return NULL;
    void* ptr = a->base + a->offset;
    a->offset += size;
    // Align to 8 bytes
    a->offset = (a->offset + 7) & ~7;
    return ptr;
}

void intrinsic_arena_reset(Arena* a) {
    if (a) a->offset = 0;
}

void intrinsic_arena_free(Arena* a) {
    if (a) {
        free(a->base);
        free(a);
    }
}

int64_t intrinsic_arena_used(Arena* a) {
    return a ? a->offset : 0;
}

// StringBuilder for efficient string concatenation
// Critical for codegen performance - avoids O(n²) concat
typedef struct {
    char* data;
    int64_t len;
    int64_t cap;
} StringBuilder;

StringBuilder* intrinsic_sb_new(void) {
    StringBuilder* sb = malloc(sizeof(StringBuilder));
    sb->cap = 1024;  // Start with 1KB
    sb->len = 0;
    sb->data = malloc(sb->cap);
    sb->data[0] = '\0';
    return sb;
}

StringBuilder* intrinsic_sb_new_cap(int64_t initial_cap) {
    StringBuilder* sb = malloc(sizeof(StringBuilder));
    sb->cap = initial_cap > 0 ? initial_cap : 64;
    sb->len = 0;
    sb->data = malloc(sb->cap);
    sb->data[0] = '\0';
    return sb;
}

void intrinsic_sb_append(StringBuilder* sb, SxString* str) {
    if (!sb || !str || !str->data) return;

    int64_t needed = sb->len + str->len + 1;
    if (needed > sb->cap) {
        // Double until we have enough
        while (sb->cap < needed) {
            sb->cap *= 2;
        }
        sb->data = realloc(sb->data, sb->cap);
    }
    memcpy(sb->data + sb->len, str->data, str->len);
    sb->len += str->len;
    sb->data[sb->len] = '\0';
}

void intrinsic_sb_append_cstr(StringBuilder* sb, const char* cstr) {
    if (!sb || !cstr) return;

    int64_t slen = strlen(cstr);
    int64_t needed = sb->len + slen + 1;
    if (needed > sb->cap) {
        while (sb->cap < needed) {
            sb->cap *= 2;
        }
        sb->data = realloc(sb->data, sb->cap);
    }
    memcpy(sb->data + sb->len, cstr, slen);
    sb->len += slen;
    sb->data[sb->len] = '\0';
}

void intrinsic_sb_append_char(StringBuilder* sb, int64_t c) {
    if (!sb) return;

    if (sb->len + 2 > sb->cap) {
        sb->cap *= 2;
        sb->data = realloc(sb->data, sb->cap);
    }
    sb->data[sb->len++] = (char)c;
    sb->data[sb->len] = '\0';
}

void intrinsic_sb_append_i64(StringBuilder* sb, int64_t value) {
    if (!sb) return;

    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%lld", (long long)value);

    if (sb->len + len + 1 > sb->cap) {
        while (sb->cap < sb->len + len + 1) {
            sb->cap *= 2;
        }
        sb->data = realloc(sb->data, sb->cap);
    }
    memcpy(sb->data + sb->len, buf, len);
    sb->len += len;
    sb->data[sb->len] = '\0';
}

SxString* intrinsic_sb_to_string(StringBuilder* sb) {
    if (!sb) return intrinsic_string_new("");

    SxString* result = malloc(sizeof(SxString));
    result->len = sb->len;
    result->cap = sb->len + 1;
    result->data = malloc(result->cap);
    memcpy(result->data, sb->data, sb->len);
    result->data[sb->len] = '\0';
    return result;
}

void intrinsic_sb_clear(StringBuilder* sb) {
    if (sb) {
        sb->len = 0;
        sb->data[0] = '\0';
    }
}

void intrinsic_sb_free(StringBuilder* sb) {
    if (sb) {
        free(sb->data);
        free(sb);
    }
}

int64_t intrinsic_sb_len(StringBuilder* sb) {
    return sb ? sb->len : 0;
}

// Print stack trace
void intrinsic_print_stack_trace(void) {
    void* buffer[64];
    int nptrs = backtrace(buffer, 64);
    char** symbols = backtrace_symbols(buffer, nptrs);

    if (symbols == NULL) {
        fprintf(stderr, "  (stack trace unavailable)\n");
        return;
    }

    fprintf(stderr, "Stack trace:\n");
    for (int i = 1; i < nptrs; i++) {  // Skip the first entry (this function)
        fprintf(stderr, "  %s\n", symbols[i]);
    }
    free(symbols);
}

// Panic function for unrecoverable errors
void intrinsic_panic(SxString* message) {
    if (message && message->data) {
        fprintf(stderr, "PANIC: %s\n", message->data);
    } else {
        fprintf(stderr, "PANIC: (no message)\n");
    }
    intrinsic_print_stack_trace();
    exit(1);
}

// Panic with file and line info
void intrinsic_panic_at(SxString* message, SxString* file, int64_t line) {
    fprintf(stderr, "PANIC at %s:%lld: %s\n",
            file ? file->data : "unknown",
            line,
            message ? message->data : "(no message)");
    intrinsic_print_stack_trace();
    exit(1);
}

// Performance diagnostics
static int64_t string_concat_count = 0;
static int64_t string_concat_bytes = 0;

SxString* intrinsic_string_concat_tracked(SxString* a, SxString* b) {
    string_concat_count++;
    if (a) string_concat_bytes += a->len;
    if (b) string_concat_bytes += b->len;
    return intrinsic_string_concat(a, b);
}

void intrinsic_print_perf_stats(void) {
    printf("=== Performance Stats ===\n");
    printf("String concats: %lld\n", (long long)string_concat_count);
    printf("Bytes copied: %lld\n", (long long)string_concat_bytes);
    printf("store_i64 calls: %d\n", store_i64_count);
}

// ========================================
// Phase 7: Actor Runtime Support
// ========================================

#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>

// Get number of CPU cores
int64_t intrinsic_get_num_cpus(void) {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

// Thread management
typedef struct {
    pthread_t thread;
    void* (*fn)(void*);
    void* arg;
    int64_t id;
} ThreadHandle;

static _Atomic int64_t next_thread_id = 1;

void* intrinsic_thread_spawn(void* fn, void* arg) {
    ThreadHandle* handle = malloc(sizeof(ThreadHandle));
    handle->fn = fn;
    handle->arg = arg;
    handle->id = atomic_fetch_add(&next_thread_id, 1);
    pthread_create(&handle->thread, NULL, fn, arg);
    return handle;
}

void intrinsic_thread_join(void* handle) {
    if (handle) {
        ThreadHandle* h = (ThreadHandle*)handle;
        pthread_join(h->thread, NULL);
        free(h);
    }
}

int64_t intrinsic_thread_id(void* handle) {
    if (handle) {
        return ((ThreadHandle*)handle)->id;
    }
    return 0;
}

// Mutex primitives
void* intrinsic_mutex_new(void) {
    pthread_mutex_t* mutex = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(mutex, NULL);
    return mutex;
}

void intrinsic_mutex_lock(void* mutex) {
    if (mutex) {
        pthread_mutex_lock((pthread_mutex_t*)mutex);
    }
}

void intrinsic_mutex_unlock(void* mutex) {
    if (mutex) {
        pthread_mutex_unlock((pthread_mutex_t*)mutex);
    }
}

void intrinsic_mutex_free(void* mutex) {
    if (mutex) {
        pthread_mutex_destroy((pthread_mutex_t*)mutex);
        free(mutex);
    }
}

// Condition variable primitives
void* intrinsic_condvar_new(void) {
    pthread_cond_t* cond = malloc(sizeof(pthread_cond_t));
    pthread_cond_init(cond, NULL);
    return cond;
}

void intrinsic_condvar_wait(void* cond, void* mutex) {
    if (cond && mutex) {
        pthread_cond_wait((pthread_cond_t*)cond, (pthread_mutex_t*)mutex);
    }
}

void intrinsic_condvar_signal(void* cond) {
    if (cond) {
        pthread_cond_signal((pthread_cond_t*)cond);
    }
}

void intrinsic_condvar_broadcast(void* cond) {
    if (cond) {
        pthread_cond_broadcast((pthread_cond_t*)cond);
    }
}

void intrinsic_condvar_free(void* cond) {
    if (cond) {
        pthread_cond_destroy((pthread_cond_t*)cond);
        free(cond);
    }
}

// Atomic operations for lock-free structures (using GCC builtins for portability)
int64_t intrinsic_atomic_load(int64_t* ptr) {
    return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
}

void intrinsic_atomic_store(int64_t* ptr, int64_t value) {
    __atomic_store_n(ptr, value, __ATOMIC_RELEASE);
}

int64_t intrinsic_atomic_add(int64_t* ptr, int64_t value) {
    return __atomic_fetch_add(ptr, value, __ATOMIC_ACQ_REL);
}

int64_t intrinsic_atomic_sub(int64_t* ptr, int64_t value) {
    return __atomic_fetch_sub(ptr, value, __ATOMIC_ACQ_REL);
}

int8_t intrinsic_atomic_cas(int64_t* ptr, int64_t expected, int64_t desired) {
    return __atomic_compare_exchange_n(ptr, &expected, desired, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

void* intrinsic_atomic_load_ptr(void** ptr) {
    return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
}

void intrinsic_atomic_store_ptr(void** ptr, void* value) {
    __atomic_store_n(ptr, value, __ATOMIC_RELEASE);
}

int8_t intrinsic_atomic_cas_ptr(void** ptr, void* expected, void* desired) {
    return __atomic_compare_exchange_n(ptr, &expected, desired, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

// ========================================
// Simple Mailbox (mutex-protected for correctness in bootstrap)
// ========================================

typedef struct MailboxNode {
    void* message;
    struct MailboxNode* next;
} MailboxNode;

typedef struct {
    MailboxNode* head;
    MailboxNode* tail;
    int64_t count;
    pthread_mutex_t lock;
} Mailbox;

void* intrinsic_mailbox_new(void) {
    Mailbox* mb = malloc(sizeof(Mailbox));
    mb->head = NULL;
    mb->tail = NULL;
    mb->count = 0;
    pthread_mutex_init(&mb->lock, NULL);
    return mb;
}

void intrinsic_mailbox_send(void* mailbox, void* message) {
    if (!mailbox) return;
    Mailbox* mb = (Mailbox*)mailbox;

    MailboxNode* node = malloc(sizeof(MailboxNode));
    node->message = message;
    node->next = NULL;

    pthread_mutex_lock(&mb->lock);
    if (mb->tail) {
        mb->tail->next = node;
    } else {
        mb->head = node;
    }
    mb->tail = node;
    mb->count++;
    pthread_mutex_unlock(&mb->lock);
}

void* intrinsic_mailbox_recv(void* mailbox) {
    if (!mailbox) return NULL;
    Mailbox* mb = (Mailbox*)mailbox;

    pthread_mutex_lock(&mb->lock);
    if (!mb->head) {
        pthread_mutex_unlock(&mb->lock);
        return NULL;
    }

    MailboxNode* node = mb->head;
    mb->head = node->next;
    if (!mb->head) {
        mb->tail = NULL;
    }
    mb->count--;
    pthread_mutex_unlock(&mb->lock);

    void* msg = node->message;
    free(node);
    return msg;
}

int8_t intrinsic_mailbox_empty(void* mailbox) {
    if (!mailbox) return 1;
    Mailbox* mb = (Mailbox*)mailbox;
    pthread_mutex_lock(&mb->lock);
    int8_t result = mb->count == 0;
    pthread_mutex_unlock(&mb->lock);
    return result;
}

int64_t intrinsic_mailbox_len(void* mailbox) {
    if (!mailbox) return 0;
    Mailbox* mb = (Mailbox*)mailbox;
    pthread_mutex_lock(&mb->lock);
    int64_t result = mb->count;
    pthread_mutex_unlock(&mb->lock);
    return result;
}

void intrinsic_mailbox_free(void* mailbox) {
    if (!mailbox) return;
    Mailbox* mb = (Mailbox*)mailbox;

    // Drain remaining messages
    pthread_mutex_lock(&mb->lock);
    MailboxNode* current = mb->head;
    while (current) {
        MailboxNode* next = current->next;
        free(current);
        current = next;
    }
    pthread_mutex_unlock(&mb->lock);
    pthread_mutex_destroy(&mb->lock);
    free(mb);
}

// ========================================
// Actor Registry for spawn/send/ask
// ========================================

typedef struct {
    int64_t id;
    void* state;
    void* mailbox;
    void* handler;      // Function pointer for receive
    void* on_start;     // Lifecycle hook: called after spawn
    void* on_stop;      // Lifecycle hook: called before stop
    void* on_error;     // Lifecycle hook: called on error
    int64_t supervisor; // Supervisor actor ID (0 = none)
    int8_t running;
    int8_t restarting;  // Flag during restart
} ActorHandle;

static int64_t next_actor_id = 1;
static ActorHandle** actor_registry = NULL;
static int64_t actor_registry_cap = 0;
static pthread_mutex_t actor_registry_lock = PTHREAD_MUTEX_INITIALIZER;

void* intrinsic_actor_spawn(void* init_state, void* handler) {
    ActorHandle* actor = malloc(sizeof(ActorHandle));

    pthread_mutex_lock(&actor_registry_lock);
    actor->id = next_actor_id++;
    pthread_mutex_unlock(&actor_registry_lock);

    actor->state = init_state;
    actor->mailbox = intrinsic_mailbox_new();
    actor->handler = handler;
    actor->on_start = NULL;
    actor->on_stop = NULL;
    actor->on_error = NULL;
    actor->supervisor = 0;
    actor->running = 1;
    actor->restarting = 0;

    // Register actor
    pthread_mutex_lock(&actor_registry_lock);
    if (actor->id >= actor_registry_cap) {
        int64_t new_cap = actor_registry_cap == 0 ? 64 : actor_registry_cap * 2;
        while (new_cap <= actor->id) new_cap *= 2;
        actor_registry = realloc(actor_registry, new_cap * sizeof(ActorHandle*));
        for (int64_t i = actor_registry_cap; i < new_cap; i++) {
            actor_registry[i] = NULL;
        }
        actor_registry_cap = new_cap;
    }
    actor_registry[actor->id] = actor;
    pthread_mutex_unlock(&actor_registry_lock);

    // Call on_start hook if set
    if (actor->on_start) {
        typedef void (*StartHook)(void*);
        ((StartHook)actor->on_start)(actor);
    }

    return actor;
}

// Set lifecycle hooks
void intrinsic_actor_set_on_start(void* actor_handle, void* hook) {
    if (!actor_handle) return;
    ((ActorHandle*)actor_handle)->on_start = hook;
}

void intrinsic_actor_set_on_stop(void* actor_handle, void* hook) {
    if (!actor_handle) return;
    ((ActorHandle*)actor_handle)->on_stop = hook;
}

void intrinsic_actor_set_on_error(void* actor_handle, void* hook) {
    if (!actor_handle) return;
    ((ActorHandle*)actor_handle)->on_error = hook;
}

// Stop actor with lifecycle hook
void intrinsic_actor_stop(void* actor_handle) {
    if (!actor_handle) return;
    ActorHandle* actor = (ActorHandle*)actor_handle;

    if (!actor->running) return;

    // Call on_stop hook
    if (actor->on_stop) {
        typedef void (*StopHook)(void*);
        ((StopHook)actor->on_stop)(actor);
    }

    actor->running = 0;

    // Unregister from registry
    pthread_mutex_lock(&actor_registry_lock);
    if (actor->id < actor_registry_cap) {
        actor_registry[actor->id] = NULL;
    }
    pthread_mutex_unlock(&actor_registry_lock);
}

// Check if actor is running
int8_t intrinsic_actor_is_running(void* actor_handle) {
    if (!actor_handle) return 0;
    return ((ActorHandle*)actor_handle)->running;
}

// Note: Supervisor implementation is in Phase 23.1 Supervision Trees section

void intrinsic_actor_send(void* actor_handle, void* message) {
    if (!actor_handle) return;
    ActorHandle* actor = (ActorHandle*)actor_handle;
    intrinsic_mailbox_send(actor->mailbox, message);
}

void* intrinsic_actor_state(void* actor_handle) {
    if (!actor_handle) return NULL;
    return ((ActorHandle*)actor_handle)->state;
}

void intrinsic_actor_set_state(void* actor_handle, void* state) {
    if (!actor_handle) return;
    ((ActorHandle*)actor_handle)->state = state;
}

void* intrinsic_actor_mailbox(void* actor_handle) {
    if (!actor_handle) return NULL;
    return ((ActorHandle*)actor_handle)->mailbox;
}

int64_t intrinsic_actor_id(void* actor_handle) {
    if (!actor_handle) return 0;
    return ((ActorHandle*)actor_handle)->id;
}

// Simple synchronous message processing (for bootstrap)
// Full async processing would use worker threads
void* intrinsic_actor_ask(void* actor_handle, void* message) {
    if (!actor_handle) return NULL;
    ActorHandle* actor = (ActorHandle*)actor_handle;

    // For now, synchronous call - push message and process immediately
    // Real impl would use channels for async response
    intrinsic_mailbox_send(actor->mailbox, message);

    // Process one message
    void* msg = intrinsic_mailbox_recv(actor->mailbox);
    if (msg && actor->handler) {
        // Handler returns new state (or result for ask)
        typedef void* (*ReceiveHandler)(void* state, void* msg);
        ReceiveHandler fn = (ReceiveHandler)actor->handler;
        void* result = fn(actor->state, msg);
        return result;
    }
    return NULL;
}

// Sleep for milliseconds
void intrinsic_sleep_ms(int64_t ms) {
    usleep(ms * 1000);
}

// Yield to other threads
void intrinsic_thread_yield(void) {
    sched_yield();
}

// ========================================
// Phase 4.7: Actor Checkpointing (Persistence)
// ========================================

// Checkpoint file format:
// Header: "SXCP" (4 bytes) + version (4 bytes) + actor_id (8 bytes)
// State size (8 bytes) + state data (variable)
// Mailbox count (8 bytes) + messages (variable)

#define CHECKPOINT_MAGIC 0x50435853  // "SXCP"
#define CHECKPOINT_VERSION 1

typedef struct {
    uint32_t magic;
    uint32_t version;
    int64_t actor_id;
    int64_t state_size;
    int64_t mailbox_count;
} CheckpointHeader;

// Serialize actor state to a byte buffer
// Returns pointer to buffer, sets *out_size to buffer size
// Caller must provide serialize_fn that converts state to bytes
void* actor_serialize_state(int64_t actor_ptr, int64_t serialize_fn, int64_t* out_size) {
    if (!actor_ptr) {
        *out_size = 0;
        return NULL;
    }
    ActorHandle* actor = (ActorHandle*)actor_ptr;

    // Call user-provided serialize function
    typedef void* (*SerializeFn)(void*, int64_t*);
    SerializeFn fn = (SerializeFn)serialize_fn;
    return fn(actor->state, out_size);
}

// Save actor checkpoint to file
int64_t actor_checkpoint_save(int64_t actor_ptr, void* path, int64_t serialize_fn) {
    if (!actor_ptr || !path) return -1;

    ActorHandle* actor = (ActorHandle*)actor_ptr;
    SxString* filepath = (SxString*)path;

    // Serialize state
    int64_t state_size = 0;
    void* state_data = actor_serialize_state(actor_ptr, serialize_fn, &state_size);

    // Count mailbox messages
    int64_t msg_count = 0;
    // Note: In production, we'd iterate mailbox. For now, checkpoint state only.

    // Create checkpoint file
    FILE* f = fopen(filepath->data, "wb");
    if (!f) {
        if (state_data) free(state_data);
        return -1;
    }

    // Write header
    CheckpointHeader header = {
        .magic = CHECKPOINT_MAGIC,
        .version = CHECKPOINT_VERSION,
        .actor_id = actor->id,
        .state_size = state_size,
        .mailbox_count = msg_count
    };
    fwrite(&header, sizeof(CheckpointHeader), 1, f);

    // Write state data
    if (state_size > 0 && state_data) {
        fwrite(state_data, 1, state_size, f);
    }

    fclose(f);
    if (state_data) free(state_data);

    return 0;  // Success
}

// Load actor checkpoint from file
// Returns new actor state pointer, or 0 on failure
// Caller must provide deserialize_fn that converts bytes back to state
int64_t actor_checkpoint_load(void* path, int64_t deserialize_fn) {
    if (!path) return 0;

    SxString* filepath = (SxString*)path;

    FILE* f = fopen(filepath->data, "rb");
    if (!f) return 0;

    // Read header
    CheckpointHeader header;
    if (fread(&header, sizeof(CheckpointHeader), 1, f) != 1) {
        fclose(f);
        return 0;
    }

    // Validate header
    if (header.magic != CHECKPOINT_MAGIC || header.version != CHECKPOINT_VERSION) {
        fclose(f);
        return 0;
    }

    // Read state data
    void* state_data = NULL;
    if (header.state_size > 0) {
        state_data = malloc(header.state_size);
        if (fread(state_data, 1, header.state_size, f) != (size_t)header.state_size) {
            free(state_data);
            fclose(f);
            return 0;
        }
    }

    fclose(f);

    // Deserialize state
    typedef void* (*DeserializeFn)(void*, int64_t);
    DeserializeFn fn = (DeserializeFn)deserialize_fn;
    void* state = fn(state_data, header.state_size);

    if (state_data) free(state_data);

    return (int64_t)state;
}

// Get actor ID from checkpoint file without loading full state
int64_t actor_checkpoint_get_id(void* path) {
    if (!path) return 0;

    SxString* filepath = (SxString*)path;

    FILE* f = fopen(filepath->data, "rb");
    if (!f) return 0;

    CheckpointHeader header;
    if (fread(&header, sizeof(CheckpointHeader), 1, f) != 1) {
        fclose(f);
        return 0;
    }

    fclose(f);

    if (header.magic != CHECKPOINT_MAGIC) return 0;

    return header.actor_id;
}

// Check if checkpoint file exists and is valid
int64_t actor_checkpoint_exists(void* path) {
    if (!path) return 0;

    SxString* filepath = (SxString*)path;

    FILE* f = fopen(filepath->data, "rb");
    if (!f) return 0;

    CheckpointHeader header;
    int valid = 0;
    if (fread(&header, sizeof(CheckpointHeader), 1, f) == 1) {
        valid = (header.magic == CHECKPOINT_MAGIC && header.version == CHECKPOINT_VERSION);
    }

    fclose(f);
    return valid ? 1 : 0;
}

// Delete checkpoint file
int64_t actor_checkpoint_delete(void* path) {
    if (!path) return -1;

    SxString* filepath = (SxString*)path;
    return remove(filepath->data) == 0 ? 0 : -1;
}

// Spawn actor from checkpoint - restores state from checkpoint file
int64_t actor_spawn_from_checkpoint(void* path, void* handler, int64_t deserialize_fn) {
    int64_t state = actor_checkpoint_load(path, deserialize_fn);
    if (!state) return 0;

    return (int64_t)intrinsic_actor_spawn((void*)state, handler);
}

// ========================================
// Phase 23.4: Actor Error Handling
// ========================================

// Forward declaration
int64_t time_now_ms(void);

// Exit reasons
#define EXIT_NORMAL 0
#define EXIT_SHUTDOWN 1
#define EXIT_KILLED 2
#define EXIT_ERROR 3
#define EXIT_TIMEOUT 4
#define EXIT_PANIC 5

// Actor status
#define ACTOR_RUNNING 0
#define ACTOR_STOPPED 1
#define ACTOR_CRASHED 2
#define ACTOR_RESTARTING 3

// Extended actor handle with error handling
typedef struct {
    int64_t id;
    void* state;
    void* mailbox;
    void* handler;
    int8_t running;
    int64_t status;          // ACTOR_RUNNING, ACTOR_STOPPED, etc.
    int64_t exit_reason;     // EXIT_NORMAL, EXIT_ERROR, etc.
    int64_t error_code;      // Application-specific error code
    char* error_message;     // Error description
    int64_t restart_count;   // Number of restarts
    int64_t last_restart_time;
    void* supervisor;        // Parent supervisor (if any)
    void* on_error;          // Error callback function
    void* on_exit;           // Exit callback function
} ActorHandleEx;

// Get actor status
int64_t actor_get_status(int64_t actor_ptr) {
    if (actor_ptr == 0) return ACTOR_STOPPED;
    ActorHandleEx* actor = (ActorHandleEx*)actor_ptr;
    return actor->status;
}

// Get actor exit reason
int64_t actor_get_exit_reason(int64_t actor_ptr) {
    if (actor_ptr == 0) return EXIT_NORMAL;
    ActorHandleEx* actor = (ActorHandleEx*)actor_ptr;
    return actor->exit_reason;
}

// Get actor error code
int64_t actor_get_error_code(int64_t actor_ptr) {
    if (actor_ptr == 0) return 0;
    ActorHandleEx* actor = (ActorHandleEx*)actor_ptr;
    return actor->error_code;
}

// Set actor error
void actor_set_error(int64_t actor_ptr, int64_t code, int64_t message_ptr) {
    if (actor_ptr == 0) return;
    ActorHandleEx* actor = (ActorHandleEx*)actor_ptr;
    actor->status = ACTOR_CRASHED;
    actor->exit_reason = EXIT_ERROR;
    actor->error_code = code;
    actor->error_message = (char*)message_ptr;
}

// Stop actor normally
void actor_stop(int64_t actor_ptr) {
    if (actor_ptr == 0) return;
    ActorHandleEx* actor = (ActorHandleEx*)actor_ptr;
    actor->status = ACTOR_STOPPED;
    actor->exit_reason = EXIT_NORMAL;
    actor->running = 0;

    // Call exit callback if set
    if (actor->on_exit) {
        typedef void (*ExitHandler)(int64_t, int64_t);
        ExitHandler fn = (ExitHandler)actor->on_exit;
        fn(actor_ptr, EXIT_NORMAL);
    }
}

// Kill actor forcefully
void actor_kill(int64_t actor_ptr) {
    if (actor_ptr == 0) return;
    ActorHandleEx* actor = (ActorHandleEx*)actor_ptr;
    actor->status = ACTOR_STOPPED;
    actor->exit_reason = EXIT_KILLED;
    actor->running = 0;

    if (actor->on_exit) {
        typedef void (*ExitHandler)(int64_t, int64_t);
        ExitHandler fn = (ExitHandler)actor->on_exit;
        fn(actor_ptr, EXIT_KILLED);
    }
}

// Crash actor with error
void actor_crash(int64_t actor_ptr, int64_t error_code, int64_t message_ptr) {
    if (actor_ptr == 0) return;
    ActorHandleEx* actor = (ActorHandleEx*)actor_ptr;
    actor->status = ACTOR_CRASHED;
    actor->exit_reason = EXIT_ERROR;
    actor->error_code = error_code;
    actor->error_message = (char*)message_ptr;
    actor->running = 0;

    // Call error callback if set
    if (actor->on_error) {
        typedef void (*ErrorHandler)(int64_t, int64_t, int64_t);
        ErrorHandler fn = (ErrorHandler)actor->on_error;
        fn(actor_ptr, error_code, message_ptr);
    }

    // Call exit callback
    if (actor->on_exit) {
        typedef void (*ExitHandler)(int64_t, int64_t);
        ExitHandler fn = (ExitHandler)actor->on_exit;
        fn(actor_ptr, EXIT_ERROR);
    }
}

// Set error callback
void actor_set_on_error(int64_t actor_ptr, int64_t callback) {
    if (actor_ptr == 0) return;
    ActorHandleEx* actor = (ActorHandleEx*)actor_ptr;
    actor->on_error = (void*)callback;
}

// Set exit callback
void actor_set_on_exit(int64_t actor_ptr, int64_t callback) {
    if (actor_ptr == 0) return;
    ActorHandleEx* actor = (ActorHandleEx*)actor_ptr;
    actor->on_exit = (void*)callback;
}

// Set supervisor
void actor_set_supervisor(int64_t actor_ptr, int64_t supervisor_ptr) {
    if (actor_ptr == 0) return;
    ActorHandleEx* actor = (ActorHandleEx*)actor_ptr;
    actor->supervisor = (void*)supervisor_ptr;
}

// Get supervisor
int64_t actor_get_supervisor(int64_t actor_ptr) {
    if (actor_ptr == 0) return 0;
    ActorHandleEx* actor = (ActorHandleEx*)actor_ptr;
    return (int64_t)actor->supervisor;
}

// Get restart count
int64_t actor_get_restart_count(int64_t actor_ptr) {
    if (actor_ptr == 0) return 0;
    ActorHandleEx* actor = (ActorHandleEx*)actor_ptr;
    return actor->restart_count;
}

// Increment restart count
void actor_increment_restart(int64_t actor_ptr) {
    if (actor_ptr == 0) return;
    ActorHandleEx* actor = (ActorHandleEx*)actor_ptr;
    actor->restart_count++;
    actor->last_restart_time = time_now_ms();
}

// Check if actor is alive
int64_t actor_is_alive(int64_t actor_ptr) {
    if (actor_ptr == 0) return 0;
    ActorHandleEx* actor = (ActorHandleEx*)actor_ptr;
    return actor->status == ACTOR_RUNNING ? 1 : 0;
}

// ========================================
// Phase 23.4: Circuit Breaker Pattern
// ========================================

// Circuit breaker states
#define CB_CLOSED 0     // Normal operation
#define CB_OPEN 1       // Failing, reject requests
#define CB_HALF_OPEN 2  // Testing if recovered

typedef struct {
    int64_t state;
    int64_t failure_count;
    int64_t success_count;
    int64_t failure_threshold;
    int64_t success_threshold;
    int64_t timeout_ms;
    int64_t last_failure_time;
} CircuitBreaker;

// Create circuit breaker
int64_t circuit_breaker_new(int64_t failure_threshold, int64_t success_threshold, int64_t timeout_ms) {
    CircuitBreaker* cb = malloc(sizeof(CircuitBreaker));
    cb->state = CB_CLOSED;
    cb->failure_count = 0;
    cb->success_count = 0;
    cb->failure_threshold = failure_threshold;
    cb->success_threshold = success_threshold;
    cb->timeout_ms = timeout_ms;
    cb->last_failure_time = 0;
    return (int64_t)cb;
}

// Check if request should be allowed
int64_t circuit_breaker_allow(int64_t cb_ptr) {
    CircuitBreaker* cb = (CircuitBreaker*)cb_ptr;

    if (cb->state == CB_CLOSED) {
        return 1;  // Allow
    }

    if (cb->state == CB_OPEN) {
        // Check if timeout expired
        int64_t now = time_now_ms();
        if (now - cb->last_failure_time >= cb->timeout_ms) {
            cb->state = CB_HALF_OPEN;
            cb->success_count = 0;
            return 1;  // Allow test request
        }
        return 0;  // Reject
    }

    // Half-open: allow limited requests
    return 1;
}

// Record success
void circuit_breaker_success(int64_t cb_ptr) {
    CircuitBreaker* cb = (CircuitBreaker*)cb_ptr;

    if (cb->state == CB_HALF_OPEN) {
        cb->success_count++;
        if (cb->success_count >= cb->success_threshold) {
            cb->state = CB_CLOSED;
            cb->failure_count = 0;
        }
    } else if (cb->state == CB_CLOSED) {
        cb->failure_count = 0;  // Reset on success
    }
}

// Record failure
void circuit_breaker_failure(int64_t cb_ptr) {
    CircuitBreaker* cb = (CircuitBreaker*)cb_ptr;

    cb->failure_count++;
    cb->last_failure_time = time_now_ms();

    if (cb->state == CB_HALF_OPEN) {
        cb->state = CB_OPEN;
    } else if (cb->state == CB_CLOSED) {
        if (cb->failure_count >= cb->failure_threshold) {
            cb->state = CB_OPEN;
        }
    }
}

// Get circuit breaker state
int64_t circuit_breaker_state(int64_t cb_ptr) {
    CircuitBreaker* cb = (CircuitBreaker*)cb_ptr;
    return cb->state;
}

// Reset circuit breaker
void circuit_breaker_reset(int64_t cb_ptr) {
    CircuitBreaker* cb = (CircuitBreaker*)cb_ptr;
    cb->state = CB_CLOSED;
    cb->failure_count = 0;
    cb->success_count = 0;
}

// ========================================
// Phase 23.4: Retry Policy
// ========================================

// Retry strategies
#define RETRY_IMMEDIATE 0
#define RETRY_LINEAR 1
#define RETRY_EXPONENTIAL 2

typedef struct {
    int64_t strategy;
    int64_t max_retries;
    int64_t base_delay_ms;
    int64_t max_delay_ms;
    int64_t current_retry;
    int64_t jitter;  // Random jitter percentage (0-100)
} RetryPolicy;

// Create retry policy
int64_t retry_policy_new(int64_t strategy, int64_t max_retries, int64_t base_delay_ms, int64_t max_delay_ms) {
    RetryPolicy* rp = malloc(sizeof(RetryPolicy));
    rp->strategy = strategy;
    rp->max_retries = max_retries;
    rp->base_delay_ms = base_delay_ms;
    rp->max_delay_ms = max_delay_ms;
    rp->current_retry = 0;
    rp->jitter = 10;  // 10% jitter by default
    return (int64_t)rp;
}

// Set jitter percentage
void retry_policy_set_jitter(int64_t rp_ptr, int64_t jitter_percent) {
    RetryPolicy* rp = (RetryPolicy*)rp_ptr;
    rp->jitter = jitter_percent;
}

// Check if should retry
int64_t retry_policy_should_retry(int64_t rp_ptr) {
    RetryPolicy* rp = (RetryPolicy*)rp_ptr;
    return rp->current_retry < rp->max_retries ? 1 : 0;
}

// Get next delay and increment retry count
int64_t retry_policy_next_delay(int64_t rp_ptr) {
    RetryPolicy* rp = (RetryPolicy*)rp_ptr;

    int64_t delay = rp->base_delay_ms;

    if (rp->strategy == RETRY_LINEAR) {
        delay = rp->base_delay_ms * (rp->current_retry + 1);
    } else if (rp->strategy == RETRY_EXPONENTIAL) {
        delay = rp->base_delay_ms;
        for (int64_t i = 0; i < rp->current_retry; i++) {
            delay *= 2;
        }
    }

    // Cap at max delay
    if (delay > rp->max_delay_ms) {
        delay = rp->max_delay_ms;
    }

    // Add jitter
    if (rp->jitter > 0) {
        int64_t jitter_amount = (delay * rp->jitter) / 100;
        delay += (rand() % (jitter_amount * 2 + 1)) - jitter_amount;
        if (delay < 0) delay = 0;
    }

    rp->current_retry++;
    return delay;
}

// Reset retry count
void retry_policy_reset(int64_t rp_ptr) {
    RetryPolicy* rp = (RetryPolicy*)rp_ptr;
    rp->current_retry = 0;
}

// Get current retry count
int64_t retry_policy_count(int64_t rp_ptr) {
    RetryPolicy* rp = (RetryPolicy*)rp_ptr;
    return rp->current_retry;
}

// ========================================
// Phase 23.5: Actor Linking and Monitoring
// ========================================

// Maximum links/monitors per actor
#define MAX_LINKS 64
#define MAX_MONITORS 64

// Link entry - bidirectional failure notification
typedef struct {
    int64_t actor1;
    int64_t actor2;
    int8_t active;
} LinkEntry;

// Monitor entry - unidirectional notification
typedef struct {
    int64_t watcher;    // Actor doing the monitoring
    int64_t target;     // Actor being monitored
    int64_t ref;        // Unique reference for this monitor
    int8_t active;
} MonitorEntry;

// Global link and monitor registries
static LinkEntry* link_registry = NULL;
static int64_t link_registry_count = 0;
static int64_t link_registry_cap = 0;
static pthread_mutex_t link_registry_lock = PTHREAD_MUTEX_INITIALIZER;

static MonitorEntry* monitor_registry = NULL;
static int64_t monitor_registry_count = 0;
static int64_t monitor_registry_cap = 0;
static int64_t next_monitor_ref = 1;
static pthread_mutex_t monitor_registry_lock = PTHREAD_MUTEX_INITIALIZER;

// Create a bidirectional link between two actors
// If one crashes, the other will also crash (unless trapping exits)
int64_t actor_link(int64_t actor1, int64_t actor2) {
    if (actor1 == 0 || actor2 == 0) return 0;
    if (actor1 == actor2) return 0;  // Can't link to self

    pthread_mutex_lock(&link_registry_lock);

    // Check if link already exists
    for (int64_t i = 0; i < link_registry_count; i++) {
        if (link_registry[i].active &&
            ((link_registry[i].actor1 == actor1 && link_registry[i].actor2 == actor2) ||
             (link_registry[i].actor1 == actor2 && link_registry[i].actor2 == actor1))) {
            pthread_mutex_unlock(&link_registry_lock);
            return 1;  // Already linked
        }
    }

    // Expand registry if needed
    if (link_registry_count >= link_registry_cap) {
        int64_t new_cap = link_registry_cap == 0 ? 64 : link_registry_cap * 2;
        link_registry = realloc(link_registry, new_cap * sizeof(LinkEntry));
        link_registry_cap = new_cap;
    }

    // Add link
    link_registry[link_registry_count].actor1 = actor1;
    link_registry[link_registry_count].actor2 = actor2;
    link_registry[link_registry_count].active = 1;
    link_registry_count++;

    pthread_mutex_unlock(&link_registry_lock);
    return 1;
}

// Remove a link between two actors
void actor_unlink(int64_t actor1, int64_t actor2) {
    if (actor1 == 0 || actor2 == 0) return;

    pthread_mutex_lock(&link_registry_lock);

    for (int64_t i = 0; i < link_registry_count; i++) {
        if (link_registry[i].active &&
            ((link_registry[i].actor1 == actor1 && link_registry[i].actor2 == actor2) ||
             (link_registry[i].actor1 == actor2 && link_registry[i].actor2 == actor1))) {
            link_registry[i].active = 0;
            break;
        }
    }

    pthread_mutex_unlock(&link_registry_lock);
}

// Start monitoring an actor (unidirectional)
// Returns a unique monitor reference
int64_t actor_monitor(int64_t watcher, int64_t target) {
    if (watcher == 0 || target == 0) return 0;
    if (watcher == target) return 0;  // Can't monitor self

    pthread_mutex_lock(&monitor_registry_lock);

    // Expand registry if needed
    if (monitor_registry_count >= monitor_registry_cap) {
        int64_t new_cap = monitor_registry_cap == 0 ? 64 : monitor_registry_cap * 2;
        monitor_registry = realloc(monitor_registry, new_cap * sizeof(MonitorEntry));
        monitor_registry_cap = new_cap;
    }

    // Add monitor
    int64_t ref = next_monitor_ref++;
    monitor_registry[monitor_registry_count].watcher = watcher;
    monitor_registry[monitor_registry_count].target = target;
    monitor_registry[monitor_registry_count].ref = ref;
    monitor_registry[monitor_registry_count].active = 1;
    monitor_registry_count++;

    pthread_mutex_unlock(&monitor_registry_lock);
    return ref;
}

// Stop monitoring an actor
void actor_demonitor(int64_t ref) {
    if (ref == 0) return;

    pthread_mutex_lock(&monitor_registry_lock);

    for (int64_t i = 0; i < monitor_registry_count; i++) {
        if (monitor_registry[i].active && monitor_registry[i].ref == ref) {
            monitor_registry[i].active = 0;
            break;
        }
    }

    pthread_mutex_unlock(&monitor_registry_lock);
}

// Down message structure for monitor notifications
typedef struct {
    int64_t type;       // Message type = 0 for Down
    int64_t ref;        // Monitor reference
    int64_t actor;      // Actor that went down
    int64_t reason;     // Exit reason
} DownMessage;

// Send exit signal to linked actors when an actor exits
void actor_propagate_exit(int64_t actor, int64_t reason) {
    if (actor == 0) return;

    pthread_mutex_lock(&link_registry_lock);

    // Find and notify linked actors
    for (int64_t i = 0; i < link_registry_count; i++) {
        if (link_registry[i].active) {
            int64_t linked = 0;
            if (link_registry[i].actor1 == actor) {
                linked = link_registry[i].actor2;
            } else if (link_registry[i].actor2 == actor) {
                linked = link_registry[i].actor1;
            }

            if (linked != 0) {
                // Deactivate link
                link_registry[i].active = 0;

                // If reason is not normal, crash the linked actor
                if (reason != EXIT_NORMAL) {
                    // Note: In a full implementation, we'd check trap_exit flag
                    actor_crash(linked, reason, 0);
                }
            }
        }
    }

    pthread_mutex_unlock(&link_registry_lock);

    // Send Down messages to monitors
    pthread_mutex_lock(&monitor_registry_lock);

    for (int64_t i = 0; i < monitor_registry_count; i++) {
        if (monitor_registry[i].active && monitor_registry[i].target == actor) {
            // Create Down message
            DownMessage* msg = malloc(sizeof(DownMessage));
            msg->type = 0;  // Down message type
            msg->ref = monitor_registry[i].ref;
            msg->actor = actor;
            msg->reason = reason;

            // Send to watcher's mailbox
            // Note: In full implementation, would use actor_send
            // For now, just deactivate the monitor
            monitor_registry[i].active = 0;

            // The watcher would receive this message in their mailbox
            // and handle it in their receive block
        }
    }

    pthread_mutex_unlock(&monitor_registry_lock);
}

// Check if two actors are linked
int64_t actor_is_linked(int64_t actor1, int64_t actor2) {
    if (actor1 == 0 || actor2 == 0) return 0;

    pthread_mutex_lock(&link_registry_lock);

    int64_t result = 0;
    for (int64_t i = 0; i < link_registry_count; i++) {
        if (link_registry[i].active &&
            ((link_registry[i].actor1 == actor1 && link_registry[i].actor2 == actor2) ||
             (link_registry[i].actor1 == actor2 && link_registry[i].actor2 == actor1))) {
            result = 1;
            break;
        }
    }

    pthread_mutex_unlock(&link_registry_lock);
    return result;
}

// Get list of actors linked to this actor
// Returns count and fills array (up to max_count)
int64_t actor_get_links(int64_t actor, int64_t* out_array, int64_t max_count) {
    if (actor == 0 || out_array == NULL) return 0;

    pthread_mutex_lock(&link_registry_lock);

    int64_t count = 0;
    for (int64_t i = 0; i < link_registry_count && count < max_count; i++) {
        if (link_registry[i].active) {
            if (link_registry[i].actor1 == actor) {
                out_array[count++] = link_registry[i].actor2;
            } else if (link_registry[i].actor2 == actor) {
                out_array[count++] = link_registry[i].actor1;
            }
        }
    }

    pthread_mutex_unlock(&link_registry_lock);
    return count;
}

// Spawn and link atomically
int64_t actor_spawn_link(int64_t parent, int64_t init_state, int64_t handler) {
    // This would call intrinsic_actor_spawn then actor_link
    // For now, just return 0 as placeholder
    return 0;
}

// Simple wrapper to get link count (for Simplex binding)
int64_t actor_get_links_count(int64_t actor) {
    if (actor == 0) return 0;

    pthread_mutex_lock(&link_registry_lock);

    int64_t count = 0;
    for (int64_t i = 0; i < link_registry_count; i++) {
        if (link_registry[i].active) {
            if (link_registry[i].actor1 == actor || link_registry[i].actor2 == actor) {
                count++;
            }
        }
    }

    pthread_mutex_unlock(&link_registry_lock);
    return count;
}

// Send a Down message to an actor (monitor notification)
int64_t actor_send_down(int64_t watcher, int64_t target, int64_t reason) {
    // Create Down message
    DownMessage* msg = malloc(sizeof(DownMessage));
    msg->type = 0;  // Down message type
    msg->ref = 0;   // Will be filled by caller
    msg->actor = target;
    msg->reason = reason;

    // In a full implementation, would send to watcher's mailbox
    // For now, just return the message pointer
    return (int64_t)msg;
}

// ========================================
// Phase 23.1: Supervision Trees
// ========================================

// Supervisor restart strategies
#define STRATEGY_ONE_FOR_ONE   0   // Restart only crashed child
#define STRATEGY_ONE_FOR_ALL   1   // Restart all children if one crashes
#define STRATEGY_REST_FOR_ONE  2   // Restart crashed child and all after it

// Child restart modes
#define CHILD_PERMANENT   0   // Always restart
#define CHILD_TEMPORARY   1   // Never restart
#define CHILD_TRANSIENT   2   // Restart only on abnormal exit

// Child spec - defines how to supervise a child
typedef struct {
    int64_t id;               // Child identifier
    void* start_func;         // Start function pointer
    int64_t start_arg;        // Start argument
    int64_t restart_type;     // PERMANENT, TEMPORARY, TRANSIENT
    int64_t shutdown_timeout; // ms to wait for graceful shutdown
    int64_t actor_handle;     // Current actor handle (0 if not running)
} ChildSpec;

// Supervisor state
typedef struct {
    int64_t id;
    int64_t strategy;         // Restart strategy
    int64_t max_restarts;     // Max restarts in intensity period
    int64_t intensity_period; // Period in ms for restart counting
    int64_t restart_count;    // Current restart count
    int64_t period_start;     // Start time of current intensity period

    ChildSpec* children;
    int64_t child_count;
    int64_t child_cap;

    int8_t running;
    pthread_mutex_t lock;
} Supervisor;

// Create a new supervisor
int64_t supervisor_new(int64_t strategy, int64_t max_restarts, int64_t intensity_period) {
    Supervisor* sup = malloc(sizeof(Supervisor));
    static int64_t next_sup_id = 1;
    sup->id = next_sup_id++;
    sup->strategy = strategy;
    sup->max_restarts = max_restarts > 0 ? max_restarts : 3;
    sup->intensity_period = intensity_period > 0 ? intensity_period : 5000;
    sup->restart_count = 0;
    sup->period_start = time_now_ms();
    sup->children = NULL;
    sup->child_count = 0;
    sup->child_cap = 0;
    sup->running = 0;
    pthread_mutex_init(&sup->lock, NULL);
    return (int64_t)sup;
}

// Add a child specification
int64_t supervisor_add_child(int64_t sup_ptr, int64_t start_func, int64_t start_arg,
                             int64_t restart_type, int64_t shutdown_timeout) {
    if (sup_ptr == 0) return 0;
    Supervisor* sup = (Supervisor*)sup_ptr;

    pthread_mutex_lock(&sup->lock);

    // Expand children array if needed
    if (sup->child_count >= sup->child_cap) {
        int64_t new_cap = sup->child_cap == 0 ? 8 : sup->child_cap * 2;
        sup->children = realloc(sup->children, new_cap * sizeof(ChildSpec));
        sup->child_cap = new_cap;
    }

    static int64_t next_child_id = 1;
    int64_t idx = sup->child_count++;
    sup->children[idx].id = next_child_id++;
    sup->children[idx].start_func = (void*)start_func;
    sup->children[idx].start_arg = start_arg;
    sup->children[idx].restart_type = restart_type;
    sup->children[idx].shutdown_timeout = shutdown_timeout > 0 ? shutdown_timeout : 5000;
    sup->children[idx].actor_handle = 0;

    int64_t child_id = sup->children[idx].id;
    pthread_mutex_unlock(&sup->lock);
    return child_id;
}

// Start a single child
static int64_t supervisor_start_child_internal(Supervisor* sup, int64_t idx) {
    if (idx < 0 || idx >= sup->child_count) return 0;

    ChildSpec* child = &sup->children[idx];
    if (child->start_func == NULL) return 0;

    // Call start function with argument
    // In a real implementation, this would call intrinsic_actor_spawn
    typedef int64_t (*StartFunc)(int64_t);
    StartFunc fn = (StartFunc)child->start_func;
    int64_t handle = fn(child->start_arg);
    child->actor_handle = handle;
    return handle;
}

// Start all children
int64_t supervisor_start(int64_t sup_ptr) {
    if (sup_ptr == 0) return 0;
    Supervisor* sup = (Supervisor*)sup_ptr;

    pthread_mutex_lock(&sup->lock);
    sup->running = 1;
    sup->period_start = time_now_ms();
    sup->restart_count = 0;

    for (int64_t i = 0; i < sup->child_count; i++) {
        supervisor_start_child_internal(sup, i);
    }

    pthread_mutex_unlock(&sup->lock);
    return 1;
}

// Stop a single child
static void supervisor_stop_child_internal(Supervisor* sup, int64_t idx) {
    if (idx < 0 || idx >= sup->child_count) return;

    ChildSpec* child = &sup->children[idx];
    if (child->actor_handle != 0) {
        actor_stop(child->actor_handle);
        child->actor_handle = 0;
    }
}

// Stop all children
void supervisor_stop(int64_t sup_ptr) {
    if (sup_ptr == 0) return;
    Supervisor* sup = (Supervisor*)sup_ptr;

    pthread_mutex_lock(&sup->lock);
    sup->running = 0;

    // Stop children in reverse order
    for (int64_t i = sup->child_count - 1; i >= 0; i--) {
        supervisor_stop_child_internal(sup, i);
    }

    pthread_mutex_unlock(&sup->lock);
}

// Check if we've exceeded restart intensity
static int supervisor_check_intensity(Supervisor* sup) {
    int64_t now = time_now_ms();

    // Reset count if we're in a new period
    if (now - sup->period_start > sup->intensity_period) {
        sup->restart_count = 0;
        sup->period_start = now;
    }

    sup->restart_count++;
    return sup->restart_count <= sup->max_restarts;
}

// Handle child exit
int64_t supervisor_handle_exit(int64_t sup_ptr, int64_t child_id, int64_t reason) {
    if (sup_ptr == 0) return 0;
    Supervisor* sup = (Supervisor*)sup_ptr;

    pthread_mutex_lock(&sup->lock);

    if (!sup->running) {
        pthread_mutex_unlock(&sup->lock);
        return 0;
    }

    // Find the child
    int64_t child_idx = -1;
    for (int64_t i = 0; i < sup->child_count; i++) {
        if (sup->children[i].id == child_id) {
            child_idx = i;
            break;
        }
    }

    if (child_idx < 0) {
        pthread_mutex_unlock(&sup->lock);
        return 0;
    }

    ChildSpec* child = &sup->children[child_idx];
    child->actor_handle = 0;

    // Determine if restart is needed
    int should_restart = 0;
    if (child->restart_type == CHILD_PERMANENT) {
        should_restart = 1;
    } else if (child->restart_type == CHILD_TRANSIENT && reason != EXIT_NORMAL) {
        should_restart = 1;
    }
    // CHILD_TEMPORARY never restarts

    if (!should_restart) {
        pthread_mutex_unlock(&sup->lock);
        return 1;
    }

    // Check restart intensity
    if (!supervisor_check_intensity(sup)) {
        // Too many restarts - supervisor should terminate
        sup->running = 0;
        for (int64_t i = sup->child_count - 1; i >= 0; i--) {
            supervisor_stop_child_internal(sup, i);
        }
        pthread_mutex_unlock(&sup->lock);
        return -1;  // Indicate supervisor terminated
    }

    // Apply restart strategy
    switch (sup->strategy) {
        case STRATEGY_ONE_FOR_ONE:
            // Just restart the crashed child
            supervisor_start_child_internal(sup, child_idx);
            break;

        case STRATEGY_ONE_FOR_ALL:
            // Stop all, then restart all
            for (int64_t i = sup->child_count - 1; i >= 0; i--) {
                if (i != child_idx) {
                    supervisor_stop_child_internal(sup, i);
                }
            }
            for (int64_t i = 0; i < sup->child_count; i++) {
                supervisor_start_child_internal(sup, i);
            }
            break;

        case STRATEGY_REST_FOR_ONE:
            // Stop and restart the crashed child and all after it
            for (int64_t i = sup->child_count - 1; i > child_idx; i--) {
                supervisor_stop_child_internal(sup, i);
            }
            for (int64_t i = child_idx; i < sup->child_count; i++) {
                supervisor_start_child_internal(sup, i);
            }
            break;
    }

    pthread_mutex_unlock(&sup->lock);
    return 1;
}

// Get child count
int64_t supervisor_child_count(int64_t sup_ptr) {
    if (sup_ptr == 0) return 0;
    Supervisor* sup = (Supervisor*)sup_ptr;
    return sup->child_count;
}

// Get child status by index
int64_t supervisor_child_status(int64_t sup_ptr, int64_t idx) {
    if (sup_ptr == 0) return -1;
    Supervisor* sup = (Supervisor*)sup_ptr;

    if (idx < 0 || idx >= sup->child_count) return -1;
    return sup->children[idx].actor_handle != 0 ? ACTOR_RUNNING : ACTOR_STOPPED;
}

// Get child handle by index
int64_t supervisor_child_handle(int64_t sup_ptr, int64_t idx) {
    if (sup_ptr == 0) return 0;
    Supervisor* sup = (Supervisor*)sup_ptr;

    if (idx < 0 || idx >= sup->child_count) return 0;
    return sup->children[idx].actor_handle;
}

// Free supervisor
void supervisor_free(int64_t sup_ptr) {
    if (sup_ptr == 0) return;
    Supervisor* sup = (Supervisor*)sup_ptr;

    supervisor_stop(sup_ptr);
    pthread_mutex_destroy(&sup->lock);
    if (sup->children) free(sup->children);
    free(sup);
}

// Strategy constants accessors for Simplex
int64_t strategy_one_for_one(void) { return STRATEGY_ONE_FOR_ONE; }
int64_t strategy_one_for_all(void) { return STRATEGY_ONE_FOR_ALL; }
int64_t strategy_rest_for_one(void) { return STRATEGY_REST_FOR_ONE; }

// Child restart type accessors
int64_t child_permanent(void) { return CHILD_PERMANENT; }
int64_t child_temporary(void) { return CHILD_TEMPORARY; }
int64_t child_transient(void) { return CHILD_TRANSIENT; }

// ========================================
// Phase 23.2: Work-Stealing Scheduler
// ========================================

// Work item for scheduler
typedef struct WorkItem {
    void* task;            // Task/future to execute
    int64_t priority;      // Priority level (0 = highest)
    struct WorkItem* next;
} WorkItem;

// Per-worker deque for work-stealing
typedef struct {
    WorkItem** items;
    volatile int64_t top;     // Index for push/pop (owner)
    volatile int64_t bottom;  // Index for steal (thieves)
    int64_t capacity;
    pthread_mutex_t lock;     // Used for resize
} WorkDeque;

// Worker thread state
typedef struct {
    int64_t id;
    pthread_t thread;
    WorkDeque* local_queue;
    volatile int8_t running;
    volatile int8_t idle;
    void* scheduler;
} Worker;

// Work-stealing scheduler
typedef struct {
    Worker* workers;
    int64_t worker_count;
    volatile int64_t active_workers;
    volatile int8_t shutdown;
    pthread_mutex_t global_lock;
    pthread_cond_t work_available;

    // Global queue for tasks without affinity
    WorkItem* global_queue_head;
    WorkItem* global_queue_tail;
    int64_t global_queue_size;
} WorkStealingScheduler;

// Initialize work deque
static WorkDeque* deque_new(int64_t initial_capacity) {
    WorkDeque* dq = malloc(sizeof(WorkDeque));
    dq->capacity = initial_capacity > 0 ? initial_capacity : 256;
    dq->items = calloc(dq->capacity, sizeof(WorkItem*));
    dq->top = 0;
    dq->bottom = 0;
    pthread_mutex_init(&dq->lock, NULL);
    return dq;
}

// Push to local end (by owner)
static void deque_push(WorkDeque* dq, void* task, int64_t priority) {
    pthread_mutex_lock(&dq->lock);

    // Resize if needed
    int64_t size = dq->bottom - dq->top;
    if (size >= dq->capacity - 1) {
        int64_t new_cap = dq->capacity * 2;
        WorkItem** new_items = calloc(new_cap, sizeof(WorkItem*));
        for (int64_t i = dq->top; i < dq->bottom; i++) {
            new_items[i % new_cap] = dq->items[i % dq->capacity];
        }
        free(dq->items);
        dq->items = new_items;
        dq->capacity = new_cap;
    }

    WorkItem* item = malloc(sizeof(WorkItem));
    item->task = task;
    item->priority = priority;
    item->next = NULL;

    dq->items[dq->bottom % dq->capacity] = item;
    __atomic_store_n(&dq->bottom, dq->bottom + 1, __ATOMIC_RELEASE);

    pthread_mutex_unlock(&dq->lock);
}

// Pop from local end (by owner)
static WorkItem* deque_pop(WorkDeque* dq) {
    int64_t b = __atomic_load_n(&dq->bottom, __ATOMIC_ACQUIRE) - 1;
    __atomic_store_n(&dq->bottom, b, __ATOMIC_RELEASE);

    int64_t t = __atomic_load_n(&dq->top, __ATOMIC_ACQUIRE);

    if (t <= b) {
        WorkItem* item = dq->items[b % dq->capacity];
        if (t == b) {
            // Last item - race with thieves
            if (!__atomic_compare_exchange_n(&dq->top, &t, t + 1,
                                              0, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED)) {
                item = NULL;
            }
            __atomic_store_n(&dq->bottom, t + 1, __ATOMIC_RELEASE);
        }
        return item;
    } else {
        __atomic_store_n(&dq->bottom, t, __ATOMIC_RELEASE);
        return NULL;
    }
}

// Steal from bottom (by thieves)
static WorkItem* deque_steal(WorkDeque* dq) {
    int64_t t = __atomic_load_n(&dq->top, __ATOMIC_ACQUIRE);
    int64_t b = __atomic_load_n(&dq->bottom, __ATOMIC_ACQUIRE);

    if (t < b) {
        WorkItem* item = dq->items[t % dq->capacity];
        if (!__atomic_compare_exchange_n(&dq->top, &t, t + 1,
                                          0, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED)) {
            return NULL;  // Lost race
        }
        return item;
    }
    return NULL;
}

// Free deque
static void deque_free(WorkDeque* dq) {
    if (!dq) return;
    // Free remaining items
    while (dq->top < dq->bottom) {
        WorkItem* item = dq->items[dq->top % dq->capacity];
        if (item) free(item);
        dq->top++;
    }
    free(dq->items);
    pthread_mutex_destroy(&dq->lock);
    free(dq);
}

// Worker thread function
static void* worker_thread(void* arg) {
    Worker* worker = (Worker*)arg;
    WorkStealingScheduler* sched = (WorkStealingScheduler*)worker->scheduler;

    while (!sched->shutdown) {
        WorkItem* item = NULL;

        // 1. Try local queue
        item = deque_pop(worker->local_queue);

        // 2. Try global queue
        if (!item) {
            pthread_mutex_lock(&sched->global_lock);
            if (sched->global_queue_head) {
                item = sched->global_queue_head;
                sched->global_queue_head = item->next;
                if (!sched->global_queue_head) {
                    sched->global_queue_tail = NULL;
                }
                sched->global_queue_size--;
            }
            pthread_mutex_unlock(&sched->global_lock);
        }

        // 3. Try stealing from other workers
        if (!item) {
            for (int64_t i = 0; i < sched->worker_count && !item; i++) {
                if (i != worker->id) {
                    item = deque_steal(sched->workers[i].local_queue);
                }
            }
        }

        // Execute or wait
        if (item) {
            worker->idle = 0;
            // Execute the task
            typedef void (*TaskFunc)(void);
            if (item->task) {
                TaskFunc fn = (TaskFunc)item->task;
                fn();
            }
            free(item);
        } else {
            worker->idle = 1;
            // Wait for work
            pthread_mutex_lock(&sched->global_lock);
            if (!sched->shutdown && !sched->global_queue_head) {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_nsec += 10000000;  // 10ms timeout
                if (ts.tv_nsec >= 1000000000) {
                    ts.tv_sec++;
                    ts.tv_nsec -= 1000000000;
                }
                pthread_cond_timedwait(&sched->work_available, &sched->global_lock, &ts);
            }
            pthread_mutex_unlock(&sched->global_lock);
        }
    }

    return NULL;
}

// Create scheduler with N workers
int64_t scheduler_new(int64_t num_workers) {
    if (num_workers <= 0) num_workers = 4;  // Default

    WorkStealingScheduler* sched = malloc(sizeof(WorkStealingScheduler));
    sched->worker_count = num_workers;
    sched->active_workers = 0;
    sched->shutdown = 0;
    sched->global_queue_head = NULL;
    sched->global_queue_tail = NULL;
    sched->global_queue_size = 0;
    pthread_mutex_init(&sched->global_lock, NULL);
    pthread_cond_init(&sched->work_available, NULL);

    sched->workers = calloc(num_workers, sizeof(Worker));
    for (int64_t i = 0; i < num_workers; i++) {
        sched->workers[i].id = i;
        sched->workers[i].local_queue = deque_new(256);
        sched->workers[i].running = 0;
        sched->workers[i].idle = 1;
        sched->workers[i].scheduler = sched;
    }

    return (int64_t)sched;
}

// Start all workers
int64_t scheduler_start(int64_t sched_ptr) {
    if (sched_ptr == 0) return 0;
    WorkStealingScheduler* sched = (WorkStealingScheduler*)sched_ptr;

    for (int64_t i = 0; i < sched->worker_count; i++) {
        sched->workers[i].running = 1;
        pthread_create(&sched->workers[i].thread, NULL, worker_thread, &sched->workers[i]);
        sched->active_workers++;
    }

    return 1;
}

// Submit task to scheduler
int64_t scheduler_submit(int64_t sched_ptr, int64_t task, int64_t priority) {
    if (sched_ptr == 0) return 0;
    WorkStealingScheduler* sched = (WorkStealingScheduler*)sched_ptr;

    WorkItem* item = malloc(sizeof(WorkItem));
    item->task = (void*)task;
    item->priority = priority;
    item->next = NULL;

    pthread_mutex_lock(&sched->global_lock);
    if (sched->global_queue_tail) {
        sched->global_queue_tail->next = item;
    } else {
        sched->global_queue_head = item;
    }
    sched->global_queue_tail = item;
    sched->global_queue_size++;
    pthread_cond_signal(&sched->work_available);
    pthread_mutex_unlock(&sched->global_lock);

    return 1;
}

// Submit to specific worker (for affinity)
int64_t scheduler_submit_local(int64_t sched_ptr, int64_t worker_id, int64_t task, int64_t priority) {
    if (sched_ptr == 0) return 0;
    WorkStealingScheduler* sched = (WorkStealingScheduler*)sched_ptr;

    if (worker_id < 0 || worker_id >= sched->worker_count) {
        return scheduler_submit(sched_ptr, task, priority);  // Fall back to global
    }

    deque_push(sched->workers[worker_id].local_queue, (void*)task, priority);
    return 1;
}

// Stop scheduler
void scheduler_stop(int64_t sched_ptr) {
    if (sched_ptr == 0) return;
    WorkStealingScheduler* sched = (WorkStealingScheduler*)sched_ptr;

    sched->shutdown = 1;

    // Wake up all workers
    pthread_mutex_lock(&sched->global_lock);
    pthread_cond_broadcast(&sched->work_available);
    pthread_mutex_unlock(&sched->global_lock);

    // Join all workers
    for (int64_t i = 0; i < sched->worker_count; i++) {
        if (sched->workers[i].running) {
            pthread_join(sched->workers[i].thread, NULL);
            sched->workers[i].running = 0;
        }
    }
}

// Free scheduler
void scheduler_free(int64_t sched_ptr) {
    if (sched_ptr == 0) return;
    WorkStealingScheduler* sched = (WorkStealingScheduler*)sched_ptr;

    scheduler_stop(sched_ptr);

    for (int64_t i = 0; i < sched->worker_count; i++) {
        deque_free(sched->workers[i].local_queue);
    }
    free(sched->workers);

    // Free global queue
    while (sched->global_queue_head) {
        WorkItem* item = sched->global_queue_head;
        sched->global_queue_head = item->next;
        free(item);
    }

    pthread_mutex_destroy(&sched->global_lock);
    pthread_cond_destroy(&sched->work_available);
    free(sched);
}

// Get worker count
int64_t scheduler_worker_count(int64_t sched_ptr) {
    if (sched_ptr == 0) return 0;
    WorkStealingScheduler* sched = (WorkStealingScheduler*)sched_ptr;
    return sched->worker_count;
}

// Get global queue size
int64_t scheduler_queue_size(int64_t sched_ptr) {
    if (sched_ptr == 0) return 0;
    WorkStealingScheduler* sched = (WorkStealingScheduler*)sched_ptr;
    return sched->global_queue_size;
}

// Check if worker is idle
int64_t scheduler_worker_idle(int64_t sched_ptr, int64_t worker_id) {
    if (sched_ptr == 0) return 1;
    WorkStealingScheduler* sched = (WorkStealingScheduler*)sched_ptr;
    if (worker_id < 0 || worker_id >= sched->worker_count) return 1;
    return sched->workers[worker_id].idle;
}

// ========================================
// Phase 23.3: Lock-Free Mailbox
// ========================================

// Lock-free MPSC (Multiple Producer Single Consumer) queue node
typedef struct LFNode {
    void* data;
    volatile struct LFNode* next;
} LFNode;

// Lock-free mailbox
typedef struct {
    volatile LFNode* head;   // Consumer reads from head
    volatile LFNode* tail;   // Producers add to tail
    volatile int64_t count;
    volatile int64_t capacity;
    volatile int8_t closed;
} LockFreeMailbox;

// Create a lock-free mailbox
int64_t mailbox_new(int64_t capacity) {
    LockFreeMailbox* mb = malloc(sizeof(LockFreeMailbox));

    // Create sentinel node
    LFNode* sentinel = malloc(sizeof(LFNode));
    sentinel->data = NULL;
    sentinel->next = NULL;

    mb->head = sentinel;
    mb->tail = sentinel;
    mb->count = 0;
    mb->capacity = capacity > 0 ? capacity : 1000;
    mb->closed = 0;

    return (int64_t)mb;
}

// Send message to mailbox (lock-free enqueue)
int64_t mailbox_send(int64_t mb_ptr, int64_t message) {
    if (mb_ptr == 0) return 0;
    LockFreeMailbox* mb = (LockFreeMailbox*)mb_ptr;

    if (mb->closed) return -1;  // Mailbox closed
    if (mb->count >= mb->capacity) return 0;  // Full

    LFNode* node = malloc(sizeof(LFNode));
    node->data = (void*)message;
    node->next = NULL;

    // CAS loop to add to tail
    while (1) {
        volatile LFNode* tail = mb->tail;
        volatile LFNode* next = tail->next;

        if (tail == mb->tail) {
            if (next == NULL) {
                // Try to link new node - use atomic store/exchange properly
                volatile LFNode* expected = NULL;
                if (__atomic_compare_exchange_n(&tail->next, &expected, node,
                                                 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
                    // Success - try to move tail
                    volatile LFNode* old_tail = tail;
                    __atomic_compare_exchange_n(&mb->tail, &old_tail, node,
                                                0, __ATOMIC_RELEASE, __ATOMIC_RELAXED);
                    __atomic_fetch_add(&mb->count, 1, __ATOMIC_RELAXED);
                    return 1;
                }
            } else {
                // Tail is behind, help move it
                volatile LFNode* old_tail = tail;
                __atomic_compare_exchange_n(&mb->tail, &old_tail, (LFNode*)next,
                                            0, __ATOMIC_RELEASE, __ATOMIC_RELAXED);
            }
        }
    }
}

// Receive message from mailbox (single consumer, still lock-free)
int64_t mailbox_recv(int64_t mb_ptr) {
    if (mb_ptr == 0) return 0;
    LockFreeMailbox* mb = (LockFreeMailbox*)mb_ptr;

    while (1) {
        volatile LFNode* head = mb->head;
        volatile LFNode* tail = mb->tail;
        volatile LFNode* next = head->next;

        if (head == mb->head) {
            if (head == tail) {
                if (next == NULL) {
                    return 0;  // Empty
                }
                // Tail is behind
                volatile LFNode* old_tail = tail;
                __atomic_compare_exchange_n(&mb->tail, &old_tail, (LFNode*)next,
                                            0, __ATOMIC_RELEASE, __ATOMIC_RELAXED);
            } else {
                void* data = next->data;
                volatile LFNode* old_head = head;
                if (__atomic_compare_exchange_n(&mb->head, &old_head, (LFNode*)next,
                                                0, __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
                    __atomic_fetch_sub(&mb->count, 1, __ATOMIC_RELAXED);
                    free((void*)head);  // Free old sentinel
                    return (int64_t)data;
                }
            }
        }
    }
}

// Try receive (non-blocking)
int64_t mailbox_try_recv(int64_t mb_ptr) {
    if (mb_ptr == 0) return 0;
    LockFreeMailbox* mb = (LockFreeMailbox*)mb_ptr;

    volatile LFNode* head = mb->head;
    volatile LFNode* next = head->next;

    if (next == NULL) return 0;  // Empty

    void* data = next->data;
    volatile LFNode* old_head = head;
    if (__atomic_compare_exchange_n(&mb->head, &old_head, (LFNode*)next,
                                    0, __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
        __atomic_fetch_sub(&mb->count, 1, __ATOMIC_RELAXED);
        free((void*)head);
        return (int64_t)data;
    }

    return 0;  // Failed, try again
}

// Get mailbox size
int64_t mailbox_size(int64_t mb_ptr) {
    if (mb_ptr == 0) return 0;
    LockFreeMailbox* mb = (LockFreeMailbox*)mb_ptr;
    return mb->count;
}

// Check if empty
int64_t mailbox_empty(int64_t mb_ptr) {
    if (mb_ptr == 0) return 1;
    LockFreeMailbox* mb = (LockFreeMailbox*)mb_ptr;
    return mb->count == 0 ? 1 : 0;
}

// Check if full
int64_t mailbox_full(int64_t mb_ptr) {
    if (mb_ptr == 0) return 0;
    LockFreeMailbox* mb = (LockFreeMailbox*)mb_ptr;
    return mb->count >= mb->capacity ? 1 : 0;
}

// Close mailbox (no more sends)
void mailbox_close(int64_t mb_ptr) {
    if (mb_ptr == 0) return;
    LockFreeMailbox* mb = (LockFreeMailbox*)mb_ptr;
    mb->closed = 1;
}

// Check if closed
int64_t mailbox_is_closed(int64_t mb_ptr) {
    if (mb_ptr == 0) return 1;
    LockFreeMailbox* mb = (LockFreeMailbox*)mb_ptr;
    return mb->closed ? 1 : 0;
}

// Free mailbox
void mailbox_free(int64_t mb_ptr) {
    if (mb_ptr == 0) return;
    LockFreeMailbox* mb = (LockFreeMailbox*)mb_ptr;

    // Drain remaining messages
    while (mb->head != mb->tail) {
        LFNode* node = (LFNode*)mb->head;
        mb->head = node->next;
        free(node);
    }
    // Free final sentinel
    free((void*)mb->head);
    free(mb);
}

// ========================================
// Phase 23.6: Actor Discovery and Registry
// ========================================

// Registry entry
typedef struct {
    char* name;
    int64_t actor_handle;
    int64_t metadata;      // Optional metadata
    int8_t active;
} RegistryEntry;

// Global actor registry
typedef struct {
    RegistryEntry* entries;
    int64_t count;
    int64_t capacity;
    pthread_rwlock_t lock;  // Read-write lock for better concurrency
} ActorRegistry;

static ActorRegistry* global_registry = NULL;

// Initialize global registry
static void registry_init(void) {
    if (global_registry) return;

    global_registry = malloc(sizeof(ActorRegistry));
    global_registry->capacity = 256;
    global_registry->entries = calloc(global_registry->capacity, sizeof(RegistryEntry));
    global_registry->count = 0;
    pthread_rwlock_init(&global_registry->lock, NULL);
}

// Register an actor with a name
int64_t registry_register(int64_t name_ptr, int64_t actor_handle) {
    if (name_ptr == 0 || actor_handle == 0) return 0;

    registry_init();
    // name_ptr is an SxString, extract the data field
    SxString* str = (SxString*)name_ptr;
    char* name = str->data;

    pthread_rwlock_wrlock(&global_registry->lock);

    // Check if name already exists
    for (int64_t i = 0; i < global_registry->count; i++) {
        if (global_registry->entries[i].active &&
            strcmp(global_registry->entries[i].name, name) == 0) {
            pthread_rwlock_unlock(&global_registry->lock);
            return 0;  // Name already registered
        }
    }

    // Find slot
    int64_t slot = -1;
    for (int64_t i = 0; i < global_registry->count; i++) {
        if (!global_registry->entries[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        // Need new slot
        if (global_registry->count >= global_registry->capacity) {
            int64_t new_cap = global_registry->capacity * 2;
            global_registry->entries = realloc(global_registry->entries,
                                               new_cap * sizeof(RegistryEntry));
            memset(&global_registry->entries[global_registry->capacity], 0,
                   (new_cap - global_registry->capacity) * sizeof(RegistryEntry));
            global_registry->capacity = new_cap;
        }
        slot = global_registry->count++;
    }

    global_registry->entries[slot].name = strdup(name);
    global_registry->entries[slot].actor_handle = actor_handle;
    global_registry->entries[slot].metadata = 0;
    global_registry->entries[slot].active = 1;

    pthread_rwlock_unlock(&global_registry->lock);
    return 1;
}

// Unregister an actor
void registry_unregister(int64_t name_ptr) {
    if (name_ptr == 0 || !global_registry) return;
    SxString* str = (SxString*)name_ptr;
    char* name = str->data;

    pthread_rwlock_wrlock(&global_registry->lock);

    for (int64_t i = 0; i < global_registry->count; i++) {
        if (global_registry->entries[i].active &&
            strcmp(global_registry->entries[i].name, name) == 0) {
            free(global_registry->entries[i].name);
            global_registry->entries[i].name = NULL;
            global_registry->entries[i].actor_handle = 0;
            global_registry->entries[i].active = 0;
            break;
        }
    }

    pthread_rwlock_unlock(&global_registry->lock);
}

// Look up an actor by name
int64_t registry_lookup(int64_t name_ptr) {
    if (name_ptr == 0 || !global_registry) return 0;
    SxString* str = (SxString*)name_ptr;
    char* name = str->data;

    pthread_rwlock_rdlock(&global_registry->lock);

    int64_t result = 0;
    for (int64_t i = 0; i < global_registry->count; i++) {
        if (global_registry->entries[i].active &&
            strcmp(global_registry->entries[i].name, name) == 0) {
            result = global_registry->entries[i].actor_handle;
            break;
        }
    }

    pthread_rwlock_unlock(&global_registry->lock);
    return result;
}

// List all registered actors (returns count, fills array)
int64_t registry_list(int64_t* out_handles, int64_t max_count) {
    if (!global_registry || !out_handles) return 0;

    pthread_rwlock_rdlock(&global_registry->lock);

    int64_t count = 0;
    for (int64_t i = 0; i < global_registry->count && count < max_count; i++) {
        if (global_registry->entries[i].active) {
            out_handles[count++] = global_registry->entries[i].actor_handle;
        }
    }

    pthread_rwlock_unlock(&global_registry->lock);
    return count;
}

// Get count of registered actors
int64_t registry_count(void) {
    if (!global_registry) return 0;

    pthread_rwlock_rdlock(&global_registry->lock);

    int64_t count = 0;
    for (int64_t i = 0; i < global_registry->count; i++) {
        if (global_registry->entries[i].active) count++;
    }

    pthread_rwlock_unlock(&global_registry->lock);
    return count;
}

// Set metadata for a registered actor
int64_t registry_set_metadata(int64_t name_ptr, int64_t metadata) {
    if (name_ptr == 0 || !global_registry) return 0;
    SxString* str = (SxString*)name_ptr;
    char* name = str->data;

    pthread_rwlock_wrlock(&global_registry->lock);

    int64_t result = 0;
    for (int64_t i = 0; i < global_registry->count; i++) {
        if (global_registry->entries[i].active &&
            strcmp(global_registry->entries[i].name, name) == 0) {
            global_registry->entries[i].metadata = metadata;
            result = 1;
            break;
        }
    }

    pthread_rwlock_unlock(&global_registry->lock);
    return result;
}

// Get metadata for a registered actor
int64_t registry_get_metadata(int64_t name_ptr) {
    if (name_ptr == 0 || !global_registry) return 0;
    SxString* str = (SxString*)name_ptr;
    char* name = str->data;

    pthread_rwlock_rdlock(&global_registry->lock);

    int64_t result = 0;
    for (int64_t i = 0; i < global_registry->count; i++) {
        if (global_registry->entries[i].active &&
            strcmp(global_registry->entries[i].name, name) == 0) {
            result = global_registry->entries[i].metadata;
            break;
        }
    }

    pthread_rwlock_unlock(&global_registry->lock);
    return result;
}

// ========================================
// Phase 23.7: Backpressure and Flow Control
// ========================================

// Flow control mode
#define FLOW_DROP      0   // Drop messages when full
#define FLOW_BLOCK     1   // Block sender when full
#define FLOW_SIGNAL    2   // Signal backpressure to sender

// Flow controller state
typedef struct {
    int64_t mode;
    int64_t high_watermark;   // Start signaling at this level
    int64_t low_watermark;    // Stop signaling at this level
    volatile int64_t current;
    volatile int8_t signaling;
    pthread_mutex_t lock;
    pthread_cond_t not_full;
} FlowController;

// Create flow controller
int64_t flow_controller_new(int64_t mode, int64_t high_watermark, int64_t low_watermark) {
    FlowController* fc = malloc(sizeof(FlowController));
    fc->mode = mode;
    fc->high_watermark = high_watermark > 0 ? high_watermark : 100;
    fc->low_watermark = low_watermark > 0 ? low_watermark : 50;
    fc->current = 0;
    fc->signaling = 0;
    pthread_mutex_init(&fc->lock, NULL);
    pthread_cond_init(&fc->not_full, NULL);
    return (int64_t)fc;
}

// Check if can send (non-blocking)
int64_t flow_check(int64_t fc_ptr) {
    if (fc_ptr == 0) return 1;
    FlowController* fc = (FlowController*)fc_ptr;

    if (fc->current >= fc->high_watermark) {
        fc->signaling = 1;
        if (fc->mode == FLOW_DROP) {
            return 0;  // Would drop
        }
        return 0;  // Would block or signal
    }
    return 1;  // Can send
}

// Acquire permission to send (may block)
int64_t flow_acquire(int64_t fc_ptr) {
    if (fc_ptr == 0) return 1;
    FlowController* fc = (FlowController*)fc_ptr;

    pthread_mutex_lock(&fc->lock);

    if (fc->mode == FLOW_DROP) {
        if (fc->current >= fc->high_watermark) {
            pthread_mutex_unlock(&fc->lock);
            return 0;  // Drop
        }
    } else if (fc->mode == FLOW_BLOCK) {
        while (fc->current >= fc->high_watermark) {
            pthread_cond_wait(&fc->not_full, &fc->lock);
        }
    }

    fc->current++;
    if (fc->current >= fc->high_watermark) {
        fc->signaling = 1;
    }

    pthread_mutex_unlock(&fc->lock);
    return 1;
}

// Release after processing
void flow_release(int64_t fc_ptr) {
    if (fc_ptr == 0) return;
    FlowController* fc = (FlowController*)fc_ptr;

    pthread_mutex_lock(&fc->lock);

    if (fc->current > 0) {
        fc->current--;
    }

    if (fc->current <= fc->low_watermark) {
        fc->signaling = 0;
        pthread_cond_broadcast(&fc->not_full);
    }

    pthread_mutex_unlock(&fc->lock);
}

// Check if backpressure is signaled
int64_t flow_is_signaling(int64_t fc_ptr) {
    if (fc_ptr == 0) return 0;
    FlowController* fc = (FlowController*)fc_ptr;
    return fc->signaling ? 1 : 0;
}

// Get current level
int64_t flow_current(int64_t fc_ptr) {
    if (fc_ptr == 0) return 0;
    FlowController* fc = (FlowController*)fc_ptr;
    return fc->current;
}

// Get high watermark
int64_t flow_high_watermark(int64_t fc_ptr) {
    if (fc_ptr == 0) return 0;
    FlowController* fc = (FlowController*)fc_ptr;
    return fc->high_watermark;
}

// Get low watermark
int64_t flow_low_watermark(int64_t fc_ptr) {
    if (fc_ptr == 0) return 0;
    FlowController* fc = (FlowController*)fc_ptr;
    return fc->low_watermark;
}

// Reset flow controller
void flow_reset(int64_t fc_ptr) {
    if (fc_ptr == 0) return;
    FlowController* fc = (FlowController*)fc_ptr;

    pthread_mutex_lock(&fc->lock);
    fc->current = 0;
    fc->signaling = 0;
    pthread_cond_broadcast(&fc->not_full);
    pthread_mutex_unlock(&fc->lock);
}

// Free flow controller
void flow_free(int64_t fc_ptr) {
    if (fc_ptr == 0) return;
    FlowController* fc = (FlowController*)fc_ptr;

    pthread_mutex_destroy(&fc->lock);
    pthread_cond_destroy(&fc->not_full);
    free(fc);
}

// Flow mode accessors
int64_t flow_mode_drop(void) { return FLOW_DROP; }
int64_t flow_mode_block(void) { return FLOW_BLOCK; }
int64_t flow_mode_signal(void) { return FLOW_SIGNAL; }

// ========================================
// Phase 8: Async Runtime Support
// ========================================

#include <fcntl.h>

// Poll enum values
#define POLL_READY 0
#define POLL_PENDING 1

#ifdef __APPLE__
// macOS: Use kqueue for async I/O
#include <sys/event.h>

typedef struct {
    int kq;
    struct kevent events[256];
    int num_events;
} IoDriver;

static IoDriver* global_io_driver = NULL;

void* intrinsic_io_driver_new(void) {
    IoDriver* driver = malloc(sizeof(IoDriver));
    driver->kq = kqueue();
    driver->num_events = 0;
    return driver;
}

void intrinsic_io_driver_init(void) {
    if (!global_io_driver) {
        global_io_driver = intrinsic_io_driver_new();
    }
}

void intrinsic_io_driver_register_read(void* driver_ptr, int64_t fd, void* waker) {
    IoDriver* driver = driver_ptr ? driver_ptr : global_io_driver;
    if (!driver) return;

    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, waker);
    kevent(driver->kq, &ev, 1, NULL, 0, NULL);
}

void intrinsic_io_driver_register_write(void* driver_ptr, int64_t fd, void* waker) {
    IoDriver* driver = driver_ptr ? driver_ptr : global_io_driver;
    if (!driver) return;

    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, waker);
    kevent(driver->kq, &ev, 1, NULL, 0, NULL);
}

void intrinsic_io_driver_unregister(void* driver_ptr, int64_t fd) {
    IoDriver* driver = driver_ptr ? driver_ptr : global_io_driver;
    if (!driver) return;

    struct kevent ev[2];
    EV_SET(&ev[0], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    EV_SET(&ev[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    kevent(driver->kq, ev, 2, NULL, 0, NULL);
}

int64_t intrinsic_io_driver_poll(void* driver_ptr, int64_t timeout_ms) {
    IoDriver* driver = driver_ptr ? driver_ptr : global_io_driver;
    if (!driver) return 0;

    struct timespec ts = { timeout_ms / 1000, (timeout_ms % 1000) * 1000000 };
    int n = kevent(driver->kq, NULL, 0, driver->events, 256, &ts);
    driver->num_events = n > 0 ? n : 0;

    // Wake up any registered wakers
    for (int i = 0; i < driver->num_events; i++) {
        // The udata contains the waker callback if set
        // For now, we just count events
    }

    return driver->num_events;
}

void intrinsic_io_driver_free(void* driver_ptr) {
    if (!driver_ptr) return;
    IoDriver* driver = (IoDriver*)driver_ptr;
    close(driver->kq);
    free(driver);
}

#else
// Linux/other: Use epoll for async I/O
#include <sys/epoll.h>

typedef struct {
    int epfd;
    struct epoll_event events[256];
    int num_events;
} IoDriver;

static IoDriver* global_io_driver = NULL;

void* intrinsic_io_driver_new(void) {
    IoDriver* driver = malloc(sizeof(IoDriver));
    driver->epfd = epoll_create1(0);
    driver->num_events = 0;
    return driver;
}

void intrinsic_io_driver_init(void) {
    if (!global_io_driver) {
        global_io_driver = intrinsic_io_driver_new();
    }
}

void intrinsic_io_driver_register_read(void* driver_ptr, int64_t fd, void* waker) {
    IoDriver* driver = driver_ptr ? driver_ptr : global_io_driver;
    if (!driver) return;

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = waker;
    epoll_ctl(driver->epfd, EPOLL_CTL_ADD, fd, &ev);
}

void intrinsic_io_driver_register_write(void* driver_ptr, int64_t fd, void* waker) {
    IoDriver* driver = driver_ptr ? driver_ptr : global_io_driver;
    if (!driver) return;

    struct epoll_event ev;
    ev.events = EPOLLOUT;
    ev.data.ptr = waker;
    epoll_ctl(driver->epfd, EPOLL_CTL_ADD, fd, &ev);
}

void intrinsic_io_driver_unregister(void* driver_ptr, int64_t fd) {
    IoDriver* driver = driver_ptr ? driver_ptr : global_io_driver;
    if (!driver) return;

    epoll_ctl(driver->epfd, EPOLL_CTL_DEL, fd, NULL);
}

int64_t intrinsic_io_driver_poll(void* driver_ptr, int64_t timeout_ms) {
    IoDriver* driver = driver_ptr ? driver_ptr : global_io_driver;
    if (!driver) return 0;

    int n = epoll_wait(driver->epfd, driver->events, 256, (int)timeout_ms);
    driver->num_events = n > 0 ? n : 0;

    return driver->num_events;
}

void intrinsic_io_driver_free(void* driver_ptr) {
    if (!driver_ptr) return;
    IoDriver* driver = (IoDriver*)driver_ptr;
    close(driver->epfd);
    free(driver);
}

#endif

// Set file descriptor non-blocking
int8_t intrinsic_set_nonblocking(int64_t fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

// Timer wheel for efficient timer management
typedef struct TimerEntry {
    int64_t deadline_ms;
    void* waker;
    struct TimerEntry* next;
} TimerEntry;

typedef struct {
    TimerEntry* entries;
    int64_t count;
    pthread_mutex_t lock;
} TimerWheel;

static TimerWheel* global_timer_wheel = NULL;

void* intrinsic_timer_wheel_new(void) {
    TimerWheel* wheel = malloc(sizeof(TimerWheel));
    wheel->entries = NULL;
    wheel->count = 0;
    pthread_mutex_init(&wheel->lock, NULL);
    return wheel;
}

void intrinsic_timer_wheel_init(void) {
    if (!global_timer_wheel) {
        global_timer_wheel = intrinsic_timer_wheel_new();
    }
}

void intrinsic_timer_register(void* wheel_ptr, int64_t deadline_ms, void* waker) {
    TimerWheel* wheel = wheel_ptr ? wheel_ptr : global_timer_wheel;
    if (!wheel) return;

    TimerEntry* entry = malloc(sizeof(TimerEntry));
    entry->deadline_ms = deadline_ms;
    entry->waker = waker;

    pthread_mutex_lock(&wheel->lock);
    entry->next = wheel->entries;
    wheel->entries = entry;
    wheel->count++;
    pthread_mutex_unlock(&wheel->lock);
}

int64_t intrinsic_timer_check(void* wheel_ptr) {
    TimerWheel* wheel = wheel_ptr ? wheel_ptr : global_timer_wheel;
    if (!wheel) return 0;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t now_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    int64_t fired = 0;

    pthread_mutex_lock(&wheel->lock);
    TimerEntry** pp = &wheel->entries;
    while (*pp) {
        TimerEntry* entry = *pp;
        if (entry->deadline_ms <= now_ms) {
            // Timer expired - remove it
            *pp = entry->next;
            wheel->count--;
            free(entry);
            fired++;
        } else {
            pp = &(*pp)->next;
        }
    }
    pthread_mutex_unlock(&wheel->lock);

    return fired;
}

int64_t intrinsic_timer_next_deadline(void* wheel_ptr) {
    TimerWheel* wheel = wheel_ptr ? wheel_ptr : global_timer_wheel;
    if (!wheel) return -1;

    pthread_mutex_lock(&wheel->lock);
    int64_t min_deadline = -1;
    for (TimerEntry* e = wheel->entries; e; e = e->next) {
        if (min_deadline < 0 || e->deadline_ms < min_deadline) {
            min_deadline = e->deadline_ms;
        }
    }
    pthread_mutex_unlock(&wheel->lock);

    return min_deadline;
}

void intrinsic_timer_wheel_free(void* wheel_ptr) {
    if (!wheel_ptr) return;
    TimerWheel* wheel = (TimerWheel*)wheel_ptr;

    pthread_mutex_lock(&wheel->lock);
    TimerEntry* e = wheel->entries;
    while (e) {
        TimerEntry* next = e->next;
        free(e);
        e = next;
    }
    pthread_mutex_unlock(&wheel->lock);
    pthread_mutex_destroy(&wheel->lock);
    free(wheel);
}

// ========================================
// Simple Async Executor
// ========================================

typedef struct TaskNode {
    int64_t id;
    void* future;        // Pointer to future state
    void* poll_fn;       // Poll function pointer
    int8_t completed;
    struct TaskNode* next;
} TaskNode;

typedef struct {
    TaskNode* tasks;
    TaskNode* ready_queue;
    int64_t next_id;
    pthread_mutex_t lock;
    int8_t running;
} Executor;

static Executor* global_executor = NULL;

void* intrinsic_executor_new(void) {
    Executor* exec = malloc(sizeof(Executor));
    exec->tasks = NULL;
    exec->ready_queue = NULL;
    exec->next_id = 1;
    pthread_mutex_init(&exec->lock, NULL);
    exec->running = 0;
    return exec;
}

void intrinsic_executor_init(void) {
    if (!global_executor) {
        global_executor = intrinsic_executor_new();
    }
    intrinsic_io_driver_init();
    intrinsic_timer_wheel_init();
}

int64_t intrinsic_executor_spawn(void* exec_ptr, void* future, void* poll_fn) {
    Executor* exec = exec_ptr ? exec_ptr : global_executor;
    if (!exec) return 0;

    TaskNode* task = malloc(sizeof(TaskNode));

    pthread_mutex_lock(&exec->lock);
    task->id = exec->next_id++;
    task->future = future;
    task->poll_fn = poll_fn;
    task->completed = 0;
    task->next = exec->tasks;
    exec->tasks = task;
    pthread_mutex_unlock(&exec->lock);

    return task->id;
}

void intrinsic_executor_wake(void* exec_ptr, int64_t task_id) {
    Executor* exec = exec_ptr ? exec_ptr : global_executor;
    if (!exec) return;

    pthread_mutex_lock(&exec->lock);
    for (TaskNode* t = exec->tasks; t; t = t->next) {
        if (t->id == task_id && !t->completed) {
            // Mark as ready (add to ready queue if not already)
            // Simplified: just mark for next poll
            break;
        }
    }
    pthread_mutex_unlock(&exec->lock);
}

// Run executor until all tasks complete
void intrinsic_executor_run(void* exec_ptr) {
    Executor* exec = exec_ptr ? exec_ptr : global_executor;
    if (!exec) return;

    exec->running = 1;
    while (exec->running) {
        int active = 0;

        pthread_mutex_lock(&exec->lock);
        for (TaskNode* t = exec->tasks; t; t = t->next) {
            if (!t->completed) {
                active = 1;
                // Would call poll_fn here
            }
        }
        pthread_mutex_unlock(&exec->lock);

        if (!active) break;

        // Check timers
        intrinsic_timer_check(NULL);

        // Poll I/O with small timeout
        intrinsic_io_driver_poll(NULL, 10);
    }
}

void intrinsic_executor_stop(void* exec_ptr) {
    Executor* exec = exec_ptr ? exec_ptr : global_executor;
    if (exec) exec->running = 0;
}

void intrinsic_executor_free(void* exec_ptr) {
    if (!exec_ptr) return;
    Executor* exec = (Executor*)exec_ptr;

    pthread_mutex_lock(&exec->lock);
    TaskNode* t = exec->tasks;
    while (t) {
        TaskNode* next = t->next;
        free(t);
        t = next;
    }
    pthread_mutex_unlock(&exec->lock);
    pthread_mutex_destroy(&exec->lock);
    free(exec);
}

// Get current time in milliseconds (for async timeouts)
int64_t intrinsic_now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// ========================================
// Phase 9: Networking & Platform
// ========================================

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

// Socket creation
int64_t intrinsic_socket_create(int64_t domain, int64_t type) {
    return socket((int)domain, (int)type, 0);
}

// Socket options
void intrinsic_socket_set_nonblocking(int64_t fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void intrinsic_socket_set_reuseaddr(int64_t fd) {
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

// Socket bind
int64_t intrinsic_socket_bind(int64_t fd, int64_t ip, int64_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl((uint32_t)ip);
    return bind(fd, (struct sockaddr*)&addr, sizeof(addr));
}

// Socket listen
int64_t intrinsic_socket_listen(int64_t fd, int64_t backlog) {
    return listen(fd, (int)backlog);
}

// Socket accept (returns fd, sets peer_ip and peer_port via pointers)
int64_t intrinsic_socket_accept(int64_t fd, int64_t* peer_ip, int64_t* peer_port) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int client = accept(fd, (struct sockaddr*)&addr, &len);

    if (client < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -11;  // EAGAIN
        }
        return client;
    }

    if (peer_ip) *peer_ip = ntohl(addr.sin_addr.s_addr);
    if (peer_port) *peer_port = ntohs(addr.sin_port);
    return client;
}

// Socket connect
int64_t intrinsic_socket_connect(int64_t fd, int64_t ip, int64_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl((uint32_t)ip);

    int result = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (result < 0) {
        if (errno == EINPROGRESS) {
            return -115;  // EINPROGRESS
        }
    }
    return result;
}

// Socket read
int64_t intrinsic_socket_read(int64_t fd, void* buf, int64_t len) {
    ssize_t n = read(fd, buf, len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -11;  // EAGAIN
        }
    }
    return n;
}

// Socket write
int64_t intrinsic_socket_write(int64_t fd, void* buf, int64_t len) {
    ssize_t n = write(fd, buf, len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -11;  // EAGAIN
        }
    }
    return n;
}

// Socket close
void intrinsic_socket_close(int64_t fd) {
    close(fd);
}

// Get socket error
int64_t intrinsic_socket_get_error(int64_t fd) {
    int error = 0;
    socklen_t len = sizeof(error);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
    return error;
}

// DNS resolution (blocking - should be called from thread pool)
int64_t intrinsic_dns_resolve(void* hostname_ptr, int64_t* ip_out) {
    SxString* hostname = (SxString*)hostname_ptr;
    if (!hostname || !hostname->data) return -1;

    struct addrinfo hints = {0};
    struct addrinfo* result;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(hostname->data, NULL, &hints, &result);
    if (status != 0) {
        return -1;
    }

    struct sockaddr_in* addr = (struct sockaddr_in*)result->ai_addr;
    *ip_out = ntohl(addr->sin_addr.s_addr);

    freeaddrinfo(result);
    return 0;
}

// IP address to string
void* intrinsic_ip_to_string(int64_t ip) {
    struct in_addr addr;
    addr.s_addr = htonl((uint32_t)ip);
    char* str = inet_ntoa(addr);
    return intrinsic_string_new(str);
}

// String to IP address
int64_t intrinsic_string_to_ip(void* str_ptr) {
    SxString* str = (SxString*)str_ptr;
    if (!str || !str->data) return 0;

    struct in_addr addr;
    if (inet_aton(str->data, &addr) == 0) {
        return 0;  // Invalid IP
    }
    return ntohl(addr.s_addr);
}

// UDP sendto
int64_t intrinsic_socket_sendto(int64_t fd, void* buf, int64_t len, int64_t ip, int64_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl((uint32_t)ip);

    ssize_t n = sendto(fd, buf, len, 0, (struct sockaddr*)&addr, sizeof(addr));
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -11;
        }
    }
    return n;
}

// UDP recvfrom
int64_t intrinsic_socket_recvfrom(int64_t fd, void* buf, int64_t len, int64_t* ip_out, int64_t* port_out) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    ssize_t n = recvfrom(fd, buf, len, 0, (struct sockaddr*)&addr, &addr_len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -11;
        }
    } else {
        if (ip_out) *ip_out = ntohl(addr.sin_addr.s_addr);
        if (port_out) *port_out = ntohs(addr.sin_port);
    }
    return n;
}

// Get errno for last error
int64_t intrinsic_get_errno(void) {
    return errno;
}

// Math intrinsics (Phase 9 stdlib)
#include <math.h>

double intrinsic_sqrt(double x) { return sqrt(x); }
double intrinsic_sin(double x) { return sin(x); }
double intrinsic_cos(double x) { return cos(x); }
double intrinsic_tan(double x) { return tan(x); }
double intrinsic_asin(double x) { return asin(x); }
double intrinsic_acos(double x) { return acos(x); }
double intrinsic_atan(double x) { return atan(x); }
double intrinsic_atan2(double y, double x) { return atan2(y, x); }
double intrinsic_exp(double x) { return exp(x); }
double intrinsic_log(double x) { return log(x); }
double intrinsic_log10(double x) { return log10(x); }
double intrinsic_log2(double x) { return log2(x); }
double intrinsic_pow(double base, double exp) { return pow(base, exp); }
double intrinsic_fabs(double x) { return fabs(x); }
double intrinsic_floor(double x) { return floor(x); }
double intrinsic_ceil(double x) { return ceil(x); }
double intrinsic_round(double x) { return round(x); }
double intrinsic_sinh(double x) { return sinh(x); }
double intrinsic_cosh(double x) { return cosh(x); }
double intrinsic_tanh(double x) { return tanh(x); }

// Random number generator (xorshift64*)
static uint64_t rng_state = 0;

void intrinsic_random_seed(int64_t seed) {
    rng_state = (uint64_t)seed;
    if (rng_state == 0) rng_state = 1;
}

int64_t intrinsic_random_i64(void) {
    if (rng_state == 0) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        rng_state = tv.tv_sec * 1000000 + tv.tv_usec;
        if (rng_state == 0) rng_state = 1;
    }
    rng_state ^= rng_state >> 12;
    rng_state ^= rng_state << 25;
    rng_state ^= rng_state >> 27;
    return (int64_t)(rng_state * 0x2545F4914F6CDD1DULL);
}

double intrinsic_random_f64(void) {
    return (intrinsic_random_i64() >> 11) * (1.0 / 9007199254740992.0);
}

// Get current working directory
void* intrinsic_getcwd(void) {
    char buf[4096];
    if (getcwd(buf, sizeof(buf)) == NULL) {
        return intrinsic_string_new("");
    }
    return intrinsic_string_new(buf);
}

// Get environment variable
void* intrinsic_getenv(void* name_ptr) {
    SxString* name = (SxString*)name_ptr;
    if (!name || !name->data) return NULL;

    char* value = getenv(name->data);
    if (!value) return NULL;
    return intrinsic_string_new(value);
}

// Set environment variable
void intrinsic_setenv(void* name_ptr, void* value_ptr) {
    SxString* name = (SxString*)name_ptr;
    SxString* value = (SxString*)value_ptr;
    if (!name || !name->data || !value || !value->data) return;
    setenv(name->data, value->data, 1);
}

// =============================================================================
// Phase 10: Distribution & Persistence
// =============================================================================

// Serialization Writer (binary format)
typedef struct {
    uint8_t* buffer;
    size_t len;
    size_t cap;
} SerWriter;

void* intrinsic_ser_writer_new(void) {
    SerWriter* w = malloc(sizeof(SerWriter));
    w->cap = 256;
    w->buffer = malloc(w->cap);
    w->len = 0;
    return w;
}

static void writer_ensure_capacity(SerWriter* w, size_t extra) {
    if (w->len + extra > w->cap) {
        while (w->cap < w->len + extra) {
            w->cap *= 2;
        }
        w->buffer = realloc(w->buffer, w->cap);
    }
}

void intrinsic_ser_write_u8(void* writer_ptr, int64_t value) {
    SerWriter* w = (SerWriter*)writer_ptr;
    writer_ensure_capacity(w, 1);
    w->buffer[w->len++] = (uint8_t)value;
}

void intrinsic_ser_write_u16(void* writer_ptr, int64_t value) {
    intrinsic_ser_write_u8(writer_ptr, (value >> 8) & 0xFF);
    intrinsic_ser_write_u8(writer_ptr, value & 0xFF);
}

void intrinsic_ser_write_u32(void* writer_ptr, int64_t value) {
    intrinsic_ser_write_u16(writer_ptr, (value >> 16) & 0xFFFF);
    intrinsic_ser_write_u16(writer_ptr, value & 0xFFFF);
}

void intrinsic_ser_write_i64(void* writer_ptr, int64_t value) {
    intrinsic_ser_write_u32(writer_ptr, (value >> 32) & 0xFFFFFFFF);
    intrinsic_ser_write_u32(writer_ptr, value & 0xFFFFFFFF);
}

void intrinsic_ser_write_bytes(void* writer_ptr, void* data, int64_t len) {
    SerWriter* w = (SerWriter*)writer_ptr;
    intrinsic_ser_write_i64(writer_ptr, len);
    writer_ensure_capacity(w, len);
    memcpy(w->buffer + w->len, data, len);
    w->len += len;
}

void intrinsic_ser_write_string(void* writer_ptr, void* str_ptr) {
    SxString* str = (SxString*)str_ptr;
    if (!str || !str->data) {
        intrinsic_ser_write_i64(writer_ptr, 0);
        return;
    }
    intrinsic_ser_write_bytes(writer_ptr, str->data, str->len);
}

void* intrinsic_ser_writer_bytes(void* writer_ptr) {
    SerWriter* w = (SerWriter*)writer_ptr;
    // Return as a SxVec<u8> - allocated copy
    SxVec* vec = malloc(sizeof(SxVec));
    vec->len = w->len;
    vec->cap = w->len;
    vec->items = malloc(w->len);
    memcpy(vec->items, w->buffer, w->len);
    return vec;
}

int64_t intrinsic_ser_writer_len(void* writer_ptr) {
    SerWriter* w = (SerWriter*)writer_ptr;
    return w->len;
}

void intrinsic_ser_writer_free(void* writer_ptr) {
    SerWriter* w = (SerWriter*)writer_ptr;
    if (w) {
        free(w->buffer);
        free(w);
    }
}

// Serialization Reader
typedef struct {
    uint8_t* buffer;
    size_t pos;
    size_t len;
} SerReader;

void* intrinsic_ser_reader_new(void* data, int64_t len) {
    SerReader* r = malloc(sizeof(SerReader));
    r->buffer = (uint8_t*)data;
    r->pos = 0;
    r->len = len;
    return r;
}

int64_t intrinsic_ser_read_u8(void* reader_ptr) {
    SerReader* r = (SerReader*)reader_ptr;
    if (r->pos >= r->len) return -1;  // Error
    return r->buffer[r->pos++];
}

int64_t intrinsic_ser_read_u16(void* reader_ptr) {
    int64_t hi = intrinsic_ser_read_u8(reader_ptr);
    int64_t lo = intrinsic_ser_read_u8(reader_ptr);
    if (hi < 0 || lo < 0) return -1;
    return (hi << 8) | lo;
}

int64_t intrinsic_ser_read_u32(void* reader_ptr) {
    int64_t hi = intrinsic_ser_read_u16(reader_ptr);
    int64_t lo = intrinsic_ser_read_u16(reader_ptr);
    if (hi < 0 || lo < 0) return -1;
    return (hi << 16) | lo;
}

int64_t intrinsic_ser_read_i64(void* reader_ptr) {
    int64_t hi = intrinsic_ser_read_u32(reader_ptr);
    int64_t lo = intrinsic_ser_read_u32(reader_ptr);
    if (hi < 0 || lo < 0) return -1;
    return (hi << 32) | lo;
}

void* intrinsic_ser_read_bytes(void* reader_ptr) {
    SerReader* r = (SerReader*)reader_ptr;
    int64_t len = intrinsic_ser_read_i64(reader_ptr);
    if (len < 0 || r->pos + len > r->len) return NULL;

    SxVec* vec = malloc(sizeof(SxVec));
    vec->len = len;
    vec->cap = len;
    vec->items = malloc(len);
    memcpy(vec->items, r->buffer + r->pos, len);
    r->pos += len;
    return vec;
}

void* intrinsic_ser_read_string(void* reader_ptr) {
    SerReader* r = (SerReader*)reader_ptr;
    int64_t len = intrinsic_ser_read_i64(reader_ptr);
    if (len < 0 || r->pos + len > r->len) return NULL;

    SxString* str = malloc(sizeof(SxString));
    str->len = len;
    str->cap = len + 1;
    str->data = malloc(str->cap);
    memcpy(str->data, r->buffer + r->pos, len);
    str->data[len] = '\0';
    r->pos += len;
    return str;
}

int64_t intrinsic_ser_reader_remaining(void* reader_ptr) {
    SerReader* r = (SerReader*)reader_ptr;
    return r->len - r->pos;
}

void intrinsic_ser_reader_free(void* reader_ptr) {
    free(reader_ptr);
}

// Simple SHA256 implementation (for content-addressed code)
// Based on public domain implementation

static const uint32_t sha256_k[] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static void sha256_transform(uint32_t state[8], const uint8_t data[64]) {
    uint32_t m[64], a, b, c, d, e, f, g, h, t1, t2;

    for (int i = 0; i < 16; i++) {
        m[i] = ((uint32_t)data[i * 4] << 24) | ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) | ((uint32_t)data[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    }

    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];

    for (int i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e, f, g) + sha256_k[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

void* intrinsic_sha256(void* data, int64_t len) {
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    // Unwrap SxString to get raw data pointer
    SxString* str = (SxString*)data;
    uint8_t* bytes = str ? (uint8_t*)str->data : (uint8_t*)data;
    size_t i = 0;

    // Process full blocks
    for (; i + 64 <= (size_t)len; i += 64) {
        sha256_transform(state, bytes + i);
    }

    // Pad final block
    uint8_t block[64];
    size_t remaining = len - i;
    memcpy(block, bytes + i, remaining);
    block[remaining] = 0x80;

    if (remaining >= 56) {
        memset(block + remaining + 1, 0, 64 - remaining - 1);
        sha256_transform(state, block);
        memset(block, 0, 56);
    } else {
        memset(block + remaining + 1, 0, 56 - remaining - 1);
    }

    // Append length in bits (big-endian)
    uint64_t bits = (uint64_t)len * 8;
    for (int j = 0; j < 8; j++) {
        block[56 + j] = (bits >> (56 - j * 8)) & 0xFF;
    }
    sha256_transform(state, block);

    // Output hash as hex string
    char* hex = malloc(65);
    for (int j = 0; j < 8; j++) {
        sprintf(hex + j * 8, "%08x", state[j]);
    }
    hex[64] = '\0';
    return intrinsic_string_new(hex);
}

// Note: Content-addressed code (Phase 10) is implemented in Phase 25.4 section

// Checkpoint storage (file-based)
int64_t intrinsic_checkpoint_save(void* actor_id_ptr, int64_t version, void* data, int64_t len) {
    SxString* actor_id = (SxString*)actor_id_ptr;
    if (!actor_id || !actor_id->data) return -1;

    // Create checkpoint filename
    char path[4096];
    snprintf(path, sizeof(path), ".simplex_checkpoints/%s.checkpoint", actor_id->data);

    // Ensure directory exists
    mkdir(".simplex_checkpoints", 0755);

    // Write version + data
    FILE* f = fopen(path, "wb");
    if (!f) return -1;

    fwrite(&version, sizeof(version), 1, f);
    fwrite(&len, sizeof(len), 1, f);
    fwrite(data, 1, len, f);
    fclose(f);

    return 0;
}

void* intrinsic_checkpoint_load(void* actor_id_ptr, int64_t* version_out) {
    SxString* actor_id = (SxString*)actor_id_ptr;
    if (!actor_id || !actor_id->data) return NULL;

    char path[4096];
    snprintf(path, sizeof(path), ".simplex_checkpoints/%s.checkpoint", actor_id->data);

    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    int64_t version, len;
    if (fread(&version, sizeof(version), 1, f) != 1) { fclose(f); return NULL; }
    if (fread(&len, sizeof(len), 1, f) != 1) { fclose(f); return NULL; }

    void* data = malloc(len);
    if (fread(data, 1, len, f) != (size_t)len) { fclose(f); free(data); return NULL; }
    fclose(f);

    if (version_out) *version_out = version;

    // Return as SxVec
    SxVec* vec = malloc(sizeof(SxVec));
    vec->len = len;
    vec->cap = len;
    vec->items = data;
    return vec;
}

int64_t intrinsic_checkpoint_delete(void* actor_id_ptr) {
    SxString* actor_id = (SxString*)actor_id_ptr;
    if (!actor_id || !actor_id->data) return -1;

    char path[4096];
    snprintf(path, sizeof(path), ".simplex_checkpoints/%s.checkpoint", actor_id->data);
    return unlink(path);
}

// Node ID generation (for clustering)
int64_t intrinsic_generate_node_id(void) {
    // Use random + time for uniqueness
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t time_part = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
    uint64_t random_part = intrinsic_random_i64();
    return (int64_t)((time_part ^ random_part) & 0x7FFFFFFFFFFFFFFF);
}

// Simple RPC message framing (length-prefixed)
int64_t intrinsic_rpc_send(int64_t fd, void* data, int64_t len) {
    // Write 4-byte length header + data
    uint32_t hdr = htonl((uint32_t)len);
    if (write(fd, &hdr, 4) != 4) return -1;
    if (write(fd, data, len) != len) return -1;
    return 0;
}

void* intrinsic_rpc_recv(int64_t fd) {
    // Read 4-byte length header
    uint32_t hdr;
    if (read(fd, &hdr, 4) != 4) return NULL;
    int64_t len = ntohl(hdr);

    if (len <= 0 || len > 10*1024*1024) return NULL;  // Max 10MB

    void* data = malloc(len);
    int64_t total = 0;
    while (total < len) {
        ssize_t n = read(fd, (char*)data + total, len - total);
        if (n <= 0) { free(data); return NULL; }
        total += n;
    }

    SxVec* vec = malloc(sizeof(SxVec));
    vec->len = len;
    vec->cap = len;
    vec->items = data;
    return vec;
}

// =============================================================================
// Phase 11: Complete Toolchain
// =============================================================================

// Process execution (spawn command and wait for result)
int64_t intrinsic_process_run(void* cmd_ptr) {
    SxString* cmd = (SxString*)cmd_ptr;
    if (!cmd || !cmd->data) return -1;
    return system(cmd->data);
}

// Exit program with status code
void intrinsic_exit(int64_t status) {
    exit((int)status);
}

// Process spawn with output capture
void* intrinsic_process_output(void* cmd_ptr) {
    SxString* cmd = (SxString*)cmd_ptr;
    if (!cmd || !cmd->data) return NULL;

    FILE* fp = popen(cmd->data, "r");
    if (!fp) return NULL;

    // Read all output
    size_t cap = 4096;
    size_t len = 0;
    char* buf = malloc(cap);

    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, fp)) > 0) {
        len += n;
        if (len + 1 >= cap) {
            cap *= 2;
            buf = realloc(buf, cap);
        }
    }
    buf[len] = '\0';

    pclose(fp);
    void* result = intrinsic_string_new(buf);
    free(buf);
    return result;
}

// File existence check
int64_t intrinsic_file_exists(void* path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path || !path->data) return 0;

    struct stat st;
    return stat(path->data, &st) == 0 ? 1 : 0;
}

// Check if path is directory
int64_t intrinsic_is_directory(void* path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path || !path->data) return 0;

    struct stat st;
    if (stat(path->data, &st) != 0) return 0;
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

// Check if path is file
int64_t intrinsic_is_file(void* path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path || !path->data) return 0;

    struct stat st;
    if (stat(path->data, &st) != 0) return 0;
    return S_ISREG(st.st_mode) ? 1 : 0;
}

// Create directory (and parents if needed)
int64_t intrinsic_mkdir_p(void* path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path || !path->data) return -1;

    // mkdir -p equivalent
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path->data);
    size_t len = strlen(tmp);

    if (tmp[len - 1] == '/') tmp[len - 1] = 0;

    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755);
}

// Remove file or empty directory
int64_t intrinsic_remove_path(void* path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path || !path->data) return -1;

    struct stat st;
    if (stat(path->data, &st) != 0) return -1;

    if (S_ISDIR(st.st_mode)) {
        return rmdir(path->data);
    } else {
        return unlink(path->data);
    }
}

// Get file size
int64_t intrinsic_file_size(void* path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path || !path->data) return -1;

    struct stat st;
    if (stat(path->data, &st) != 0) return -1;
    return st.st_size;
}

// Get file modification time (unix timestamp)
int64_t intrinsic_file_mtime(void* path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path || !path->data) return -1;

    struct stat st;
    if (stat(path->data, &st) != 0) return -1;
    return st.st_mtime;
}

// Create temp file and return path
void* intrinsic_temp_file(void* suffix_ptr) {
    SxString* suffix = (SxString*)suffix_ptr;
    const char* suf = (suffix && suffix->data) ? suffix->data : "";

    char template_path[256];
    snprintf(template_path, sizeof(template_path), "/tmp/simplex_XXXXXX%s", suf);

    int fd = mkstemps(template_path, strlen(suf));
    if (fd < 0) return NULL;
    close(fd);

    return intrinsic_string_new(template_path);
}

// Create temp directory and return path
void* intrinsic_temp_dir(void) {
    char template_path[] = "/tmp/simplex_XXXXXX";
    char* result = mkdtemp(template_path);
    if (!result) return NULL;
    return intrinsic_string_new(result);
}

// Join paths
void* intrinsic_path_join(void* a_ptr, void* b_ptr) {
    SxString* a = (SxString*)a_ptr;
    SxString* b = (SxString*)b_ptr;
    if (!a || !a->data || !b || !b->data) return NULL;

    size_t alen = a->len;
    size_t blen = b->len;

    // Remove trailing slash from a if present
    while (alen > 0 && a->data[alen-1] == '/') alen--;

    // Remove leading slash from b if present
    const char* bstart = b->data;
    while (*bstart == '/') { bstart++; blen--; }

    char* buf = malloc(alen + 1 + blen + 1);
    memcpy(buf, a->data, alen);
    buf[alen] = '/';
    memcpy(buf + alen + 1, bstart, blen);
    buf[alen + 1 + blen] = '\0';

    void* result = intrinsic_string_new(buf);
    free(buf);
    return result;
}

// Get directory name (parent path)
void* intrinsic_path_dirname(void* path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path || !path->data || path->len == 0) return intrinsic_string_new(".");

    // Find last slash
    const char* last_slash = strrchr(path->data, '/');
    if (!last_slash) return intrinsic_string_new(".");

    size_t len = last_slash - path->data;
    if (len == 0) return intrinsic_string_new("/");

    char* buf = malloc(len + 1);
    memcpy(buf, path->data, len);
    buf[len] = '\0';

    void* result = intrinsic_string_new(buf);
    free(buf);
    return result;
}

// Get file name (base name)
void* intrinsic_path_basename(void* path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path || !path->data || path->len == 0) return intrinsic_string_new("");

    const char* last_slash = strrchr(path->data, '/');
    if (!last_slash) return intrinsic_string_new(path->data);

    return intrinsic_string_new(last_slash + 1);
}

// Get file extension
void* intrinsic_path_extension(void* path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path || !path->data || path->len == 0) return intrinsic_string_new("");

    const char* basename = strrchr(path->data, '/');
    if (!basename) basename = path->data;
    else basename++;

    const char* dot = strrchr(basename, '.');
    if (!dot || dot == basename) return intrinsic_string_new("");

    return intrinsic_string_new(dot + 1);
}

// Get current working directory
void* intrinsic_get_cwd(void) {
    char buf[4096];
    if (getcwd(buf, sizeof(buf)) == NULL) {
        return intrinsic_string_new("");
    }
    return intrinsic_string_new(buf);
}

// Set current working directory
int64_t intrinsic_set_cwd(void* path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path || !path->data) return -1;
    return chdir(path->data);
}

// List directory contents
void* intrinsic_list_dir(void* path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path || !path->data) return intrinsic_vec_new();

    DIR* dir = opendir(path->data);
    if (!dir) return intrinsic_vec_new();

    void* result = intrinsic_vec_new();
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        void* name = intrinsic_string_new(entry->d_name);
        intrinsic_vec_push(result, name);
    }

    closedir(dir);
    return result;
}

// Stdin/stderr I/O
void* intrinsic_stdin_read_line(void) {
    char buffer[4096];
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return intrinsic_string_new("");
    }
    // Remove trailing newline
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    return intrinsic_string_new(buffer);
}

void intrinsic_stderr_write(void* msg_ptr) {
    SxString* msg = (SxString*)msg_ptr;
    if (msg && msg->data) {
        fprintf(stderr, "%s", msg->data);
    }
}

void intrinsic_stderr_writeln(void* msg_ptr) {
    SxString* msg = (SxString*)msg_ptr;
    if (msg && msg->data) {
        fprintf(stderr, "%s\n", msg->data);
    } else {
        fprintf(stderr, "\n");
    }
}

// File copy
int64_t intrinsic_file_copy(void* src_ptr, void* dst_ptr) {
    SxString* src = (SxString*)src_ptr;
    SxString* dst = (SxString*)dst_ptr;
    if (!src || !src->data || !dst || !dst->data) return -1;

    FILE* fsrc = fopen(src->data, "rb");
    if (!fsrc) return -1;

    FILE* fdst = fopen(dst->data, "wb");
    if (!fdst) {
        fclose(fsrc);
        return -1;
    }

    char buffer[8192];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), fsrc)) > 0) {
        if (fwrite(buffer, 1, n, fdst) != n) {
            fclose(fsrc);
            fclose(fdst);
            return -1;
        }
    }

    fclose(fsrc);
    fclose(fdst);
    return 0;
}

// File rename/move
int64_t intrinsic_file_rename(void* src_ptr, void* dst_ptr) {
    SxString* src = (SxString*)src_ptr;
    SxString* dst = (SxString*)dst_ptr;
    if (!src || !src->data || !dst || !dst->data) return -1;

    return rename(src->data, dst->data);
}

// Assertion helpers
void intrinsic_assert_fail(void* msg_ptr, void* file_ptr, int64_t line) {
    SxString* msg = (SxString*)msg_ptr;
    SxString* file = (SxString*)file_ptr;

    fprintf(stderr, "Assertion failed: %s\n  at %s:%lld\n",
            msg ? msg->data : "unknown",
            file ? file->data : "unknown",
            line);
    exit(1);
}

void intrinsic_assert_eq_i64(int64_t left, int64_t right, void* file_ptr, int64_t line) {
    if (left != right) {
        SxString* file = (SxString*)file_ptr;
        fprintf(stderr, "Assertion failed: %lld != %lld\n  at %s:%lld\n",
                left, right, file ? file->data : "unknown", line);
        exit(1);
    }
}

void intrinsic_assert_eq_str(void* left_ptr, void* right_ptr, void* file_ptr, int64_t line) {
    SxString* left = (SxString*)left_ptr;
    SxString* right = (SxString*)right_ptr;
    SxString* file = (SxString*)file_ptr;

    const char* l = left ? left->data : "";
    const char* r = right ? right->data : "";

    if (strcmp(l, r) != 0) {
        fprintf(stderr, "Assertion failed: \"%s\" != \"%s\"\n  at %s:%lld\n",
                l, r, file ? file->data : "unknown", line);
        exit(1);
    }
}

// Command line argument access
static int g_argc = 0;
static char** g_argv = NULL;

void intrinsic_set_args(int argc, char** argv) {
    g_argc = argc;
    g_argv = argv;
}

int64_t intrinsic_args_count(void) {
    return g_argc;
}

void* intrinsic_args_get(int64_t index) {
    if (index < 0 || index >= g_argc) return NULL;
    return intrinsic_string_new(g_argv[index]);
}

// =============================================================================
// Phase 12: Memory Substrate
// =============================================================================

// Memory entry
typedef struct MemoryEntry {
    int64_t id;
    char* content;
    int64_t importance;
    int64_t created_at;
    int64_t expires_at;  // 0 = never
    int64_t access_count;
    struct MemoryEntry* next;
} MemoryEntry;

static MemoryEntry* g_memory_head = NULL;
static int64_t g_memory_next_id = 1;
static int64_t g_memory_count = 0;

// Remember - store a memory
int64_t intrinsic_remember(void* content_ptr, int64_t importance, int64_t ttl_ms) {
    SxString* content = (SxString*)content_ptr;
    if (!content || !content->data) return -1;

    MemoryEntry* entry = malloc(sizeof(MemoryEntry));
    entry->id = g_memory_next_id++;
    entry->content = strdup(content->data);
    entry->importance = importance;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    entry->created_at = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    entry->expires_at = (ttl_ms > 0) ? entry->created_at + ttl_ms : 0;
    entry->access_count = 0;

    // Add to head of list
    entry->next = g_memory_head;
    g_memory_head = entry;
    g_memory_count++;

    return entry->id;
}

// Recall - retrieve memories (basic substring match for bootstrap)
void* intrinsic_recall(void* query_ptr, int64_t limit) {
    SxString* query = (SxString*)query_ptr;
    const char* q = (query && query->data) ? query->data : "";

    // Create result vector
    SxVec* result = malloc(sizeof(SxVec));
    result->cap = limit > 0 ? limit : 16;
    result->len = 0;
    result->items = malloc(result->cap * sizeof(void*));

    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t now = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    MemoryEntry* entry = g_memory_head;
    while (entry && (limit <= 0 || result->len < (size_t)limit)) {
        // Skip expired entries
        if (entry->expires_at > 0 && entry->expires_at < now) {
            entry = entry->next;
            continue;
        }

        // Basic substring match (semantic search deferred)
        if (strlen(q) == 0 || strstr(entry->content, q) != NULL) {
            // Return memory as struct { id: i64, content: String, importance: i64 }
            // For simplicity, return just the content string
            entry->access_count++;
            ((void**)result->items)[result->len++] = intrinsic_string_new(entry->content);
        }
        entry = entry->next;
    }

    return result;
}

// Recall one - get single memory by ID
void* intrinsic_recall_one(int64_t memory_id) {
    MemoryEntry* entry = g_memory_head;
    while (entry) {
        if (entry->id == memory_id) {
            entry->access_count++;
            return intrinsic_string_new(entry->content);
        }
        entry = entry->next;
    }
    return NULL;
}

// Forget - remove memory by ID
int64_t intrinsic_forget(int64_t memory_id) {
    MemoryEntry* prev = NULL;
    MemoryEntry* entry = g_memory_head;

    while (entry) {
        if (entry->id == memory_id) {
            if (prev) {
                prev->next = entry->next;
            } else {
                g_memory_head = entry->next;
            }
            free(entry->content);
            free(entry);
            g_memory_count--;
            return 1;
        }
        prev = entry;
        entry = entry->next;
    }
    return 0;
}

// Forget all - clear all memories
void intrinsic_forget_all(void) {
    MemoryEntry* entry = g_memory_head;
    while (entry) {
        MemoryEntry* next = entry->next;
        free(entry->content);
        free(entry);
        entry = next;
    }
    g_memory_head = NULL;
    g_memory_count = 0;
}

// Memory count
int64_t intrinsic_memory_count(void) {
    return g_memory_count;
}

// Prune expired memories
int64_t intrinsic_memory_prune(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t now = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    int64_t pruned = 0;
    MemoryEntry* prev = NULL;
    MemoryEntry* entry = g_memory_head;

    while (entry) {
        MemoryEntry* next = entry->next;
        if (entry->expires_at > 0 && entry->expires_at < now) {
            if (prev) {
                prev->next = next;
            } else {
                g_memory_head = next;
            }
            free(entry->content);
            free(entry);
            g_memory_count--;
            pruned++;
        } else {
            prev = entry;
        }
        entry = next;
    }
    return pruned;
}

// Decay importance of all memories
void intrinsic_memory_decay(int64_t factor) {
    MemoryEntry* entry = g_memory_head;
    while (entry) {
        entry->importance *= factor;
        entry = entry->next;
    }
}

// Get memory importance
int64_t intrinsic_memory_importance(int64_t memory_id) {
    MemoryEntry* entry = g_memory_head;
    while (entry) {
        if (entry->id == memory_id) {
            return entry->importance;
        }
        entry = entry->next;
    }
    return -1;
}

// Set memory importance
void intrinsic_memory_set_importance(int64_t memory_id, int64_t importance) {
    MemoryEntry* entry = g_memory_head;
    while (entry) {
        if (entry->id == memory_id) {
            entry->importance = importance;
            return;
        }
        entry = entry->next;
    }
}

// =============================================================================
// Phase 13: Belief System
// =============================================================================

// Truth categories
#define TRUTH_ABSOLUTE 0
#define TRUTH_CONTEXTUAL 1
#define TRUTH_OPINION 2
#define TRUTH_INFERRED 3

// Belief entry
typedef struct BeliefEntry {
    int64_t id;
    char* value;
    int64_t truth_category;
    int64_t confidence;  // 0-100 scale
    int64_t created_at;
    int64_t last_validated;
    int64_t* premise_ids;
    int64_t premise_count;
    struct BeliefEntry* next;
} BeliefEntry;

static BeliefEntry* g_belief_head = NULL;
static int64_t g_belief_next_id = 1;
static int64_t g_belief_count = 0;

// Believe - create a belief
int64_t intrinsic_believe(void* value_ptr, int64_t truth_category, int64_t confidence) {
    SxString* value = (SxString*)value_ptr;
    if (!value || !value->data) return -1;

    BeliefEntry* entry = malloc(sizeof(BeliefEntry));
    entry->id = g_belief_next_id++;
    entry->value = strdup(value->data);
    entry->truth_category = truth_category;
    entry->confidence = confidence;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    entry->created_at = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    entry->last_validated = entry->created_at;
    entry->premise_ids = NULL;
    entry->premise_count = 0;

    entry->next = g_belief_head;
    g_belief_head = entry;
    g_belief_count++;

    return entry->id;
}

// Infer belief - derive from premises
int64_t intrinsic_infer_belief(void* value_ptr, void* premise_ids_ptr, int64_t premise_count) {
    SxString* value = (SxString*)value_ptr;
    if (!value || !value->data) return -1;

    // Calculate confidence as min of premises * 0.9
    int64_t min_confidence = 100;
    int64_t* premise_ids = (int64_t*)premise_ids_ptr;

    for (int64_t i = 0; i < premise_count; i++) {
        BeliefEntry* premise = g_belief_head;
        while (premise) {
            if (premise->id == premise_ids[i]) {
                if (premise->confidence < min_confidence) {
                    min_confidence = premise->confidence;
                }
                break;
            }
            premise = premise->next;
        }
    }

    int64_t inferred_confidence = (min_confidence * 90) / 100;

    BeliefEntry* entry = malloc(sizeof(BeliefEntry));
    entry->id = g_belief_next_id++;
    entry->value = strdup(value->data);
    entry->truth_category = TRUTH_INFERRED;
    entry->confidence = inferred_confidence;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    entry->created_at = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    entry->last_validated = entry->created_at;

    // Store premise references
    entry->premise_count = premise_count;
    if (premise_count > 0) {
        entry->premise_ids = malloc(premise_count * sizeof(int64_t));
        memcpy(entry->premise_ids, premise_ids, premise_count * sizeof(int64_t));
    } else {
        entry->premise_ids = NULL;
    }

    entry->next = g_belief_head;
    g_belief_head = entry;
    g_belief_count++;

    return entry->id;
}

// Query beliefs by truth category and minimum confidence
void* intrinsic_query_beliefs(int64_t truth_category, int64_t min_confidence) {
    SxVec* result = malloc(sizeof(SxVec));
    result->cap = 16;
    result->len = 0;
    result->items = malloc(result->cap * sizeof(void*));

    BeliefEntry* entry = g_belief_head;
    while (entry) {
        if ((truth_category < 0 || entry->truth_category == truth_category) &&
            entry->confidence >= min_confidence) {

            if (result->len >= result->cap) {
                result->cap *= 2;
                result->items = realloc(result->items, result->cap * sizeof(void*));
            }
            ((void**)result->items)[result->len++] = intrinsic_string_new(entry->value);
        }
        entry = entry->next;
    }

    return result;
}

// Get belief by ID
void* intrinsic_get_belief(int64_t belief_id) {
    BeliefEntry* entry = g_belief_head;
    while (entry) {
        if (entry->id == belief_id) {
            return intrinsic_string_new(entry->value);
        }
        entry = entry->next;
    }
    return NULL;
}

// Get belief confidence
int64_t intrinsic_belief_confidence(int64_t belief_id) {
    BeliefEntry* entry = g_belief_head;
    while (entry) {
        if (entry->id == belief_id) {
            return entry->confidence;
        }
        entry = entry->next;
    }
    return -1;
}

// Get belief truth category
int64_t intrinsic_belief_truth(int64_t belief_id) {
    BeliefEntry* entry = g_belief_head;
    while (entry) {
        if (entry->id == belief_id) {
            return entry->truth_category;
        }
        entry = entry->next;
    }
    return -1;
}

// Update belief confidence
void intrinsic_update_belief(int64_t belief_id, int64_t new_confidence) {
    BeliefEntry* entry = g_belief_head;
    while (entry) {
        if (entry->id == belief_id) {
            entry->confidence = new_confidence;
            struct timeval tv;
            gettimeofday(&tv, NULL);
            entry->last_validated = tv.tv_sec * 1000 + tv.tv_usec / 1000;
            return;
        }
        entry = entry->next;
    }
}

// Revoke belief
int64_t intrinsic_revoke_belief(int64_t belief_id) {
    BeliefEntry* prev = NULL;
    BeliefEntry* entry = g_belief_head;

    while (entry) {
        if (entry->id == belief_id) {
            if (prev) {
                prev->next = entry->next;
            } else {
                g_belief_head = entry->next;
            }
            free(entry->value);
            free(entry->premise_ids);
            free(entry);
            g_belief_count--;
            return 1;
        }
        prev = entry;
        entry = entry->next;
    }
    return 0;
}

// Belief count
int64_t intrinsic_belief_count(void) {
    return g_belief_count;
}

// Decay beliefs based on truth category
void intrinsic_decay_beliefs(void) {
    BeliefEntry* prev = NULL;
    BeliefEntry* entry = g_belief_head;

    while (entry) {
        BeliefEntry* next = entry->next;

        // Decay rates: Absolute=0, Contextual=1, Opinion=2, Inferred=5
        int64_t decay = 0;
        switch (entry->truth_category) {
            case TRUTH_ABSOLUTE: decay = 0; break;
            case TRUTH_CONTEXTUAL: decay = 1; break;
            case TRUTH_OPINION: decay = 2; break;
            case TRUTH_INFERRED: decay = 5; break;
        }

        entry->confidence -= decay;
        if (entry->confidence < 10) {
            // Remove decayed belief
            if (prev) {
                prev->next = next;
            } else {
                g_belief_head = next;
            }
            free(entry->value);
            free(entry->premise_ids);
            free(entry);
            g_belief_count--;
        } else {
            prev = entry;
        }
        entry = next;
    }
}

// =============================================================================
// Phase 14: BDI Agent Architecture
// =============================================================================

// Goal entry
typedef struct GoalEntry {
    int64_t id;
    char* name;
    char* description;
    int64_t priority;  // Higher = more important
    int64_t status;    // 0=pending, 1=active, 2=achieved, 3=abandoned
    struct GoalEntry* next;
} GoalEntry;

// Intention entry
typedef struct IntentionEntry {
    int64_t id;
    int64_t goal_id;   // Associated goal
    char* plan;        // Plan description
    int64_t status;    // 0=pending, 1=executing, 2=completed, 3=failed
    int64_t step;      // Current step
    int64_t total_steps;
    struct IntentionEntry* next;
} IntentionEntry;

static GoalEntry* g_goal_head = NULL;
static IntentionEntry* g_intention_head = NULL;
static int64_t g_goal_next_id = 1;
static int64_t g_intention_next_id = 1;

// Add goal
int64_t intrinsic_add_goal(void* name_ptr, void* desc_ptr, int64_t priority) {
    SxString* name = (SxString*)name_ptr;
    SxString* desc = (SxString*)desc_ptr;
    if (!name || !name->data) return -1;

    GoalEntry* entry = malloc(sizeof(GoalEntry));
    entry->id = g_goal_next_id++;
    entry->name = strdup(name->data);
    entry->description = desc && desc->data ? strdup(desc->data) : strdup("");
    entry->priority = priority;
    entry->status = 0;  // pending

    entry->next = g_goal_head;
    g_goal_head = entry;
    return entry->id;
}

// Get goal status
int64_t intrinsic_goal_status(int64_t goal_id) {
    GoalEntry* entry = g_goal_head;
    while (entry) {
        if (entry->id == goal_id) return entry->status;
        entry = entry->next;
    }
    return -1;
}

// Set goal status
void intrinsic_set_goal_status(int64_t goal_id, int64_t status) {
    GoalEntry* entry = g_goal_head;
    while (entry) {
        if (entry->id == goal_id) {
            entry->status = status;
            return;
        }
        entry = entry->next;
    }
}

// Get highest priority pending goal
int64_t intrinsic_select_goal(void) {
    GoalEntry* best = NULL;
    GoalEntry* entry = g_goal_head;
    while (entry) {
        if (entry->status == 0) {  // pending
            if (!best || entry->priority > best->priority) {
                best = entry;
            }
        }
        entry = entry->next;
    }
    return best ? best->id : -1;
}

// Create intention for goal
int64_t intrinsic_create_intention(int64_t goal_id, void* plan_ptr, int64_t total_steps) {
    SxString* plan = (SxString*)plan_ptr;

    IntentionEntry* entry = malloc(sizeof(IntentionEntry));
    entry->id = g_intention_next_id++;
    entry->goal_id = goal_id;
    entry->plan = plan && plan->data ? strdup(plan->data) : strdup("");
    entry->status = 0;  // pending
    entry->step = 0;
    entry->total_steps = total_steps;

    entry->next = g_intention_head;
    g_intention_head = entry;

    // Mark goal as active
    intrinsic_set_goal_status(goal_id, 1);

    return entry->id;
}

// Get intention status
int64_t intrinsic_intention_status(int64_t intention_id) {
    IntentionEntry* entry = g_intention_head;
    while (entry) {
        if (entry->id == intention_id) return entry->status;
        entry = entry->next;
    }
    return -1;
}

// Advance intention step
int64_t intrinsic_intention_step(int64_t intention_id) {
    IntentionEntry* entry = g_intention_head;
    while (entry) {
        if (entry->id == intention_id) {
            if (entry->status != 1) entry->status = 1;  // executing
            entry->step++;
            if (entry->step >= entry->total_steps) {
                entry->status = 2;  // completed
                intrinsic_set_goal_status(entry->goal_id, 2);  // achieved
            }
            return entry->step;
        }
        entry = entry->next;
    }
    return -1;
}

// Fail intention
void intrinsic_fail_intention(int64_t intention_id) {
    IntentionEntry* entry = g_intention_head;
    while (entry) {
        if (entry->id == intention_id) {
            entry->status = 3;  // failed
            intrinsic_set_goal_status(entry->goal_id, 0);  // back to pending
            return;
        }
        entry = entry->next;
    }
}

// Get current step of intention
int64_t intrinsic_intention_current_step(int64_t intention_id) {
    IntentionEntry* entry = g_intention_head;
    while (entry) {
        if (entry->id == intention_id) return entry->step;
        entry = entry->next;
    }
    return -1;
}

// Count pending goals
int64_t intrinsic_pending_goals_count(void) {
    int64_t count = 0;
    GoalEntry* entry = g_goal_head;
    while (entry) {
        if (entry->status == 0) count++;
        entry = entry->next;
    }
    return count;
}

// Count active intentions
int64_t intrinsic_active_intentions_count(void) {
    int64_t count = 0;
    IntentionEntry* entry = g_intention_head;
    while (entry) {
        if (entry->status == 1) count++;
        entry = entry->next;
    }
    return count;
}

// =============================================================================
// Phase 15: Memory-Augmented Specialists
// =============================================================================

// Specialist configuration
typedef struct SpecialistConfig {
    int64_t id;
    char* name;
    char* model;
    char* domain;
    int64_t short_term_limit;
    int64_t long_term_limit;
    int64_t persistent;
    struct SpecialistConfig* next;
} SpecialistConfig;

static SpecialistConfig* g_specialist_head = NULL;
static int64_t g_specialist_next_id = 1;

// Create specialist configuration
int64_t intrinsic_create_specialist(void* name_ptr, void* model_ptr, void* domain_ptr) {
    SxString* name = (SxString*)name_ptr;
    SxString* model = (SxString*)model_ptr;
    SxString* domain = (SxString*)domain_ptr;

    SpecialistConfig* cfg = malloc(sizeof(SpecialistConfig));
    cfg->id = g_specialist_next_id++;
    cfg->name = name && name->data ? strdup(name->data) : strdup("");
    cfg->model = model && model->data ? strdup(model->data) : strdup("default");
    cfg->domain = domain && domain->data ? strdup(domain->data) : strdup("");
    cfg->short_term_limit = 4096;
    cfg->long_term_limit = 100000;
    cfg->persistent = 0;

    cfg->next = g_specialist_head;
    g_specialist_head = cfg;
    return cfg->id;
}

// Configure specialist memory
void intrinsic_specialist_set_memory(int64_t id, int64_t short_term, int64_t long_term, int64_t persistent) {
    SpecialistConfig* cfg = g_specialist_head;
    while (cfg) {
        if (cfg->id == id) {
            cfg->short_term_limit = short_term;
            cfg->long_term_limit = long_term;
            cfg->persistent = persistent;
            return;
        }
        cfg = cfg->next;
    }
}

// Get specialist config
void* intrinsic_specialist_name(int64_t id) {
    SpecialistConfig* cfg = g_specialist_head;
    while (cfg) {
        if (cfg->id == id) return intrinsic_string_new(cfg->name);
        cfg = cfg->next;
    }
    return NULL;
}

void* intrinsic_specialist_model(int64_t id) {
    SpecialistConfig* cfg = g_specialist_head;
    while (cfg) {
        if (cfg->id == id) return intrinsic_string_new(cfg->model);
        cfg = cfg->next;
    }
    return NULL;
}

void* intrinsic_specialist_domain(int64_t id) {
    SpecialistConfig* cfg = g_specialist_head;
    while (cfg) {
        if (cfg->id == id) return intrinsic_string_new(cfg->domain);
        cfg = cfg->next;
    }
    return NULL;
}

// =============================================================================
// Phase 16: Evolution Engine
// =============================================================================

// Trait for evolving entities
typedef struct TraitEntry {
    int64_t id;
    char* name;
    int64_t value;      // Current value (0-100)
    int64_t generation;
    struct TraitEntry* next;
} TraitEntry;

static TraitEntry* g_trait_head = NULL;
static int64_t g_trait_next_id = 1;
static int64_t g_generation = 0;

// Add trait
int64_t intrinsic_add_trait(void* name_ptr, int64_t initial_value) {
    SxString* name = (SxString*)name_ptr;
    if (!name || !name->data) return -1;

    TraitEntry* entry = malloc(sizeof(TraitEntry));
    entry->id = g_trait_next_id++;
    entry->name = strdup(name->data);
    entry->value = initial_value;
    entry->generation = g_generation;

    entry->next = g_trait_head;
    g_trait_head = entry;
    return entry->id;
}

// Get trait value
int64_t intrinsic_trait_value(int64_t trait_id) {
    TraitEntry* entry = g_trait_head;
    while (entry) {
        if (entry->id == trait_id) return entry->value;
        entry = entry->next;
    }
    return -1;
}

// Mutate trait (random variation)
void intrinsic_mutate_trait(int64_t trait_id, int64_t mutation_rate) {
    TraitEntry* entry = g_trait_head;
    while (entry) {
        if (entry->id == trait_id) {
            // Random mutation within rate
            int64_t mutation = (intrinsic_random_i64() % (mutation_rate * 2 + 1)) - mutation_rate;
            entry->value += mutation;
            if (entry->value < 0) entry->value = 0;
            if (entry->value > 100) entry->value = 100;
            entry->generation = g_generation;
            return;
        }
        entry = entry->next;
    }
}

// Advance generation
int64_t intrinsic_next_generation(void) {
    return ++g_generation;
}

// Get current generation
int64_t intrinsic_current_generation(void) {
    return g_generation;
}

// Fitness evaluation (placeholder - returns trait sum)
int64_t intrinsic_evaluate_fitness(void) {
    int64_t fitness = 0;
    TraitEntry* entry = g_trait_head;
    while (entry) {
        fitness += entry->value;
        entry = entry->next;
    }
    return fitness;
}

// =============================================================================
// Phase 17: Collective Intelligence
// =============================================================================

// Swarm message for collective coordination
typedef struct SwarmMessage {
    int64_t id;
    int64_t sender_id;
    int64_t topic;
    char* content;
    int64_t timestamp;
    struct SwarmMessage* next;
} SwarmMessage;

static SwarmMessage* g_swarm_head = NULL;
static int64_t g_swarm_next_id = 1;

// Broadcast message to swarm
int64_t intrinsic_swarm_broadcast(int64_t sender_id, int64_t topic, void* content_ptr) {
    SxString* content = (SxString*)content_ptr;
    if (!content || !content->data) return -1;

    SwarmMessage* msg = malloc(sizeof(SwarmMessage));
    msg->id = g_swarm_next_id++;
    msg->sender_id = sender_id;
    msg->topic = topic;
    msg->content = strdup(content->data);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    msg->timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    msg->next = g_swarm_head;
    g_swarm_head = msg;
    return msg->id;
}

// Get swarm messages by topic
void* intrinsic_swarm_messages(int64_t topic, int64_t since_timestamp) {
    SxVec* result = malloc(sizeof(SxVec));
    result->cap = 16;
    result->len = 0;
    result->items = malloc(result->cap * sizeof(void*));

    SwarmMessage* msg = g_swarm_head;
    while (msg) {
        if (msg->topic == topic && msg->timestamp > since_timestamp) {
            if (result->len >= result->cap) {
                result->cap *= 2;
                result->items = realloc(result->items, result->cap * sizeof(void*));
            }
            ((void**)result->items)[result->len++] = intrinsic_string_new(msg->content);
        }
        msg = msg->next;
    }
    return result;
}

// Consensus voting
typedef struct Vote {
    int64_t voter_id;
    int64_t proposal_id;
    int64_t choice;  // 0=no, 1=yes
} Vote;

static Vote g_votes[1000];
static int64_t g_vote_count = 0;

int64_t intrinsic_cast_vote(int64_t voter_id, int64_t proposal_id, int64_t choice) {
    if (g_vote_count >= 1000) return -1;
    g_votes[g_vote_count].voter_id = voter_id;
    g_votes[g_vote_count].proposal_id = proposal_id;
    g_votes[g_vote_count].choice = choice;
    g_vote_count++;
    return g_vote_count - 1;
}

// Count votes for proposal
int64_t intrinsic_count_votes(int64_t proposal_id, int64_t choice) {
    int64_t count = 0;
    for (int64_t i = 0; i < g_vote_count; i++) {
        if (g_votes[i].proposal_id == proposal_id && g_votes[i].choice == choice) {
            count++;
        }
    }
    return count;
}

// Check if proposal passed (majority)
int64_t intrinsic_proposal_passed(int64_t proposal_id) {
    int64_t yes = intrinsic_count_votes(proposal_id, 1);
    int64_t no = intrinsic_count_votes(proposal_id, 0);
    return yes > no ? 1 : 0;
}

// ========================================
// Phase 20: Toolchain Support
// ========================================

// Read a line from stdin (for REPL)
SxString* intrinsic_read_line(void) {
    char* line = NULL;
    size_t len = 0;
    ssize_t nread = getline(&line, &len, stdin);

    if (nread == -1) {
        free(line);
        return NULL;  // EOF or error
    }

    // Remove trailing newline
    if (nread > 0 && line[nread - 1] == '\n') {
        line[nread - 1] = '\0';
        nread--;
    }

    SxString* result = intrinsic_string_new(line);
    free(line);
    return result;
}

// Print without newline (for REPL prompt)
void intrinsic_print(SxString* str) {
    if (str && str->data) {
        printf("%s", str->data);
        fflush(stdout);
    }
}

// Print an integer (convenience function)
void print_i64(int64_t n) {
    printf("%lld", (long long)n);
    fflush(stdout);
}

// Check if stdin has data (non-blocking check)
int8_t intrinsic_stdin_has_data(void) {
    fd_set readfds;
    struct timeval tv = {0, 0};
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    return select(1, &readfds, NULL, NULL, &tv) > 0;
}

// Check if stdin is interactive (terminal)
int8_t intrinsic_is_tty(void) {
    return isatty(STDIN_FILENO);
}

// Hash a string (for dependency resolution)
int64_t intrinsic_string_hash(SxString* str) {
    if (!str || !str->data) return 0;
    // FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < str->len; i++) {
        hash ^= (unsigned char)str->data[i];
        hash *= 1099511628211ULL;
    }
    return (int64_t)hash;
}

// String find (for parsing TOML)
int64_t intrinsic_string_find(SxString* haystack, SxString* needle, int64_t start) {
    if (!haystack || !needle || !haystack->data || !needle->data) return -1;
    if (start < 0) start = 0;
    if ((size_t)start >= haystack->len) return -1;
    if (needle->len == 0) return start;
    if (needle->len > haystack->len - start) return -1;

    char* found = strstr(haystack->data + start, needle->data);
    if (!found) return -1;
    return found - haystack->data;
}

// String trim whitespace
SxString* intrinsic_string_trim(SxString* str) {
    if (!str || !str->data || str->len == 0) return intrinsic_string_new("");

    size_t start = 0;
    while (start < str->len && (str->data[start] == ' ' || str->data[start] == '\t' ||
           str->data[start] == '\n' || str->data[start] == '\r')) {
        start++;
    }

    if (start >= str->len) return intrinsic_string_new("");

    size_t end = str->len;
    while (end > start && (str->data[end-1] == ' ' || str->data[end-1] == '\t' ||
           str->data[end-1] == '\n' || str->data[end-1] == '\r')) {
        end--;
    }

    return intrinsic_string_slice(str, start, end);
}

// String split by delimiter (returns vector of strings)
SxVec* intrinsic_string_split(SxString* str, SxString* delim) {
    SxVec* result = intrinsic_vec_new();
    if (!str || !str->data || str->len == 0) return result;
    if (!delim || !delim->data || delim->len == 0) {
        intrinsic_vec_push(result, str);
        return result;
    }

    size_t start = 0;
    while (start < str->len) {
        char* found = strstr(str->data + start, delim->data);
        if (!found) {
            // Add remainder
            intrinsic_vec_push(result, intrinsic_string_slice(str, start, str->len));
            break;
        }
        size_t end = found - str->data;
        intrinsic_vec_push(result, intrinsic_string_slice(str, start, end));
        start = end + delim->len;
    }

    return result;
}

// String starts with prefix
int8_t intrinsic_string_starts_with(SxString* str, SxString* prefix) {
    if (!str || !prefix) return 0;
    if (prefix->len > str->len) return 0;
    return memcmp(str->data, prefix->data, prefix->len) == 0;
}

// String ends with suffix
int8_t intrinsic_string_ends_with(SxString* str, SxString* suffix) {
    if (!str || !suffix) return 0;
    if (suffix->len > str->len) return 0;
    return memcmp(str->data + str->len - suffix->len, suffix->data, suffix->len) == 0;
}

// String contains substring
int8_t intrinsic_string_contains(SxString* haystack, SxString* needle) {
    return intrinsic_string_find(haystack, needle, 0) >= 0;
}

// String replace all occurrences
SxString* intrinsic_string_replace(SxString* str, SxString* from, SxString* to) {
    if (!str || !from || !to || str->len == 0 || from->len == 0) {
        return str ? str : intrinsic_string_new("");
    }

    StringBuilder* sb = intrinsic_sb_new();
    size_t pos = 0;

    while (pos < str->len) {
        int64_t found = intrinsic_string_find(str, from, pos);
        if (found < 0) {
            // Append remainder
            intrinsic_sb_append(sb, intrinsic_string_slice(str, pos, str->len));
            break;
        }
        // Append before match
        intrinsic_sb_append(sb, intrinsic_string_slice(str, pos, found));
        // Append replacement
        intrinsic_sb_append(sb, to);
        pos = found + from->len;
    }

    return intrinsic_sb_to_string(sb);
}

// ============================================================================
// Additional String Operations (Phase 1)
// ============================================================================

// Find last occurrence of substring (rfind)
int64_t intrinsic_string_rfind(SxString* haystack, SxString* needle) {
    if (!haystack || !needle || !haystack->data || !needle->data) return -1;
    if (needle->len == 0) return haystack->len;
    if (needle->len > haystack->len) return -1;

    // Search backwards
    for (int64_t i = (int64_t)(haystack->len - needle->len); i >= 0; i--) {
        if (memcmp(haystack->data + i, needle->data, needle->len) == 0) {
            return i;
        }
    }
    return -1;
}

// Count occurrences of substring
int64_t intrinsic_string_count(SxString* str, SxString* substr) {
    if (!str || !substr || !str->data || !substr->data) return 0;
    if (substr->len == 0 || substr->len > str->len) return 0;

    int64_t count = 0;
    size_t pos = 0;
    while (pos <= str->len - substr->len) {
        int64_t found = intrinsic_string_find(str, substr, pos);
        if (found < 0) break;
        count++;
        pos = found + 1;  // Move forward by 1 to allow overlapping matches
    }
    return count;
}

// Split string into at most n parts
SxVec* intrinsic_string_split_n(SxString* str, SxString* delim, int64_t n) {
    SxVec* result = intrinsic_vec_new();
    if (!str || !str->data || str->len == 0 || n <= 0) return result;
    if (!delim || !delim->data || delim->len == 0 || n == 1) {
        intrinsic_vec_push(result, str);
        return result;
    }

    size_t start = 0;
    int64_t parts = 0;
    while (start < str->len && parts < n - 1) {
        char* found = strstr(str->data + start, delim->data);
        if (!found) break;
        size_t end = found - str->data;
        intrinsic_vec_push(result, intrinsic_string_slice(str, start, end));
        start = end + delim->len;
        parts++;
    }
    // Add remainder as final part
    if (start <= str->len) {
        intrinsic_vec_push(result, intrinsic_string_slice(str, start, str->len));
    }

    return result;
}

// Split by whitespace
SxVec* intrinsic_string_split_whitespace(SxString* str) {
    SxVec* result = intrinsic_vec_new();
    if (!str || !str->data || str->len == 0) return result;

    size_t i = 0;
    while (i < str->len) {
        // Skip leading whitespace
        while (i < str->len && (str->data[i] == ' ' || str->data[i] == '\t' ||
               str->data[i] == '\n' || str->data[i] == '\r')) {
            i++;
        }
        if (i >= str->len) break;

        // Find end of word
        size_t start = i;
        while (i < str->len && str->data[i] != ' ' && str->data[i] != '\t' &&
               str->data[i] != '\n' && str->data[i] != '\r') {
            i++;
        }
        intrinsic_vec_push(result, intrinsic_string_slice(str, start, i));
    }

    return result;
}

// Split by newlines
SxVec* intrinsic_string_lines(SxString* str) {
    SxVec* result = intrinsic_vec_new();
    if (!str || !str->data || str->len == 0) return result;

    size_t start = 0;
    for (size_t i = 0; i < str->len; i++) {
        if (str->data[i] == '\n') {
            // Handle \r\n
            size_t end = i;
            if (end > start && str->data[end - 1] == '\r') {
                end--;
            }
            intrinsic_vec_push(result, intrinsic_string_slice(str, start, end));
            start = i + 1;
        }
    }
    // Add final line if exists
    if (start < str->len) {
        size_t end = str->len;
        if (end > start && str->data[end - 1] == '\r') {
            end--;
        }
        intrinsic_vec_push(result, intrinsic_string_slice(str, start, end));
    }

    return result;
}

// Join strings with separator
SxString* intrinsic_string_join(SxVec* parts, SxString* sep) {
    if (!parts || parts->len == 0) return intrinsic_string_new("");

    StringBuilder* sb = intrinsic_sb_new();
    for (size_t i = 0; i < parts->len; i++) {
        if (i > 0 && sep && sep->data) {
            intrinsic_sb_append(sb, sep);
        }
        SxString* part = (SxString*)parts->items[i];
        if (part) {
            intrinsic_sb_append(sb, part);
        }
    }

    return intrinsic_sb_to_string(sb);
}

// Replace first n occurrences
SxString* intrinsic_string_replace_n(SxString* str, SxString* from, SxString* to, int64_t n) {
    if (!str || !from || !to || str->len == 0 || from->len == 0 || n <= 0) {
        return str ? str : intrinsic_string_new("");
    }

    StringBuilder* sb = intrinsic_sb_new();
    size_t pos = 0;
    int64_t replaced = 0;

    while (pos < str->len && replaced < n) {
        int64_t found = intrinsic_string_find(str, from, pos);
        if (found < 0) {
            intrinsic_sb_append(sb, intrinsic_string_slice(str, pos, str->len));
            pos = str->len;
            break;
        }
        intrinsic_sb_append(sb, intrinsic_string_slice(str, pos, found));
        intrinsic_sb_append(sb, to);
        pos = found + from->len;
        replaced++;
    }

    // Append remainder
    if (pos < str->len) {
        intrinsic_sb_append(sb, intrinsic_string_slice(str, pos, str->len));
    }

    return intrinsic_sb_to_string(sb);
}

// Convert to lowercase
SxString* intrinsic_string_to_lowercase(SxString* str) {
    if (!str || !str->data || str->len == 0) return intrinsic_string_new("");

    char* buf = (char*)malloc(str->len + 1);
    for (size_t i = 0; i < str->len; i++) {
        unsigned char c = (unsigned char)str->data[i];
        if (c >= 'A' && c <= 'Z') {
            buf[i] = c + 32;
        } else {
            buf[i] = c;
        }
    }
    buf[str->len] = '\0';

    SxString* result = intrinsic_string_new(buf);
    free(buf);
    return result;
}

// Convert to uppercase
SxString* intrinsic_string_to_uppercase(SxString* str) {
    if (!str || !str->data || str->len == 0) return intrinsic_string_new("");

    char* buf = (char*)malloc(str->len + 1);
    for (size_t i = 0; i < str->len; i++) {
        unsigned char c = (unsigned char)str->data[i];
        if (c >= 'a' && c <= 'z') {
            buf[i] = c - 32;
        } else {
            buf[i] = c;
        }
    }
    buf[str->len] = '\0';

    SxString* result = intrinsic_string_new(buf);
    free(buf);
    return result;
}

// Reverse string
SxString* intrinsic_string_reverse(SxString* str) {
    if (!str || !str->data || str->len == 0) return intrinsic_string_new("");

    char* buf = (char*)malloc(str->len + 1);
    for (size_t i = 0; i < str->len; i++) {
        buf[i] = str->data[str->len - 1 - i];
    }
    buf[str->len] = '\0';

    SxString* result = intrinsic_string_new(buf);
    free(buf);
    return result;
}

// Trim leading whitespace only
SxString* intrinsic_string_trim_start(SxString* str) {
    if (!str || !str->data || str->len == 0) return intrinsic_string_new("");

    size_t start = 0;
    while (start < str->len && (str->data[start] == ' ' || str->data[start] == '\t' ||
           str->data[start] == '\n' || str->data[start] == '\r')) {
        start++;
    }

    if (start >= str->len) return intrinsic_string_new("");
    return intrinsic_string_slice(str, start, str->len);
}

// Trim trailing whitespace only
SxString* intrinsic_string_trim_end(SxString* str) {
    if (!str || !str->data || str->len == 0) return intrinsic_string_new("");

    size_t end = str->len;
    while (end > 0 && (str->data[end-1] == ' ' || str->data[end-1] == '\t' ||
           str->data[end-1] == '\n' || str->data[end-1] == '\r')) {
        end--;
    }

    if (end == 0) return intrinsic_string_new("");
    return intrinsic_string_slice(str, 0, end);
}

// Left pad string to specified length
SxString* intrinsic_string_pad_left(SxString* str, int64_t len, int64_t pad_char) {
    if (!str || len <= 0) return str ? str : intrinsic_string_new("");
    if ((size_t)len <= str->len) return str;

    size_t pad_count = len - str->len;
    char* buf = (char*)malloc(len + 1);

    char c = (char)(pad_char > 0 && pad_char < 128 ? pad_char : ' ');
    memset(buf, c, pad_count);
    if (str->data) {
        memcpy(buf + pad_count, str->data, str->len);
    }
    buf[len] = '\0';

    SxString* result = intrinsic_string_new(buf);
    free(buf);
    return result;
}

// Right pad string to specified length
SxString* intrinsic_string_pad_right(SxString* str, int64_t len, int64_t pad_char) {
    if (!str || len <= 0) return str ? str : intrinsic_string_new("");
    if ((size_t)len <= str->len) return str;

    size_t pad_count = len - str->len;
    char* buf = (char*)malloc(len + 1);

    if (str->data) {
        memcpy(buf, str->data, str->len);
    }
    char c = (char)(pad_char > 0 && pad_char < 128 ? pad_char : ' ');
    memset(buf + str->len, c, pad_count);
    buf[len] = '\0';

    SxString* result = intrinsic_string_new(buf);
    free(buf);
    return result;
}

// Character classification functions
int8_t intrinsic_char_is_digit(int64_t c) {
    return (c >= '0' && c <= '9') ? 1 : 0;
}

int8_t intrinsic_char_is_alpha(int64_t c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) ? 1 : 0;
}

int8_t intrinsic_char_is_alphanumeric(int64_t c) {
    return (intrinsic_char_is_alpha(c) || intrinsic_char_is_digit(c)) ? 1 : 0;
}

int8_t intrinsic_char_is_whitespace(int64_t c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') ? 1 : 0;
}

// ============================================================================
// Iterator Combinators (Phase 1)
// ============================================================================

// Forward declarations for functions used by collect_hashmap/hashset
int64_t hashmap_new(void);
int64_t hashmap_insert(int64_t map_ptr, int64_t key_ptr, int64_t value);
int64_t hashset_new(void);
int8_t hashset_insert(int64_t set_ptr, int64_t value_ptr);
// Integer versions
int64_t int_hashmap_new(void);
int64_t int_hashmap_insert(int64_t map_ptr, int64_t key, int64_t value);
int64_t int_hashset_new(void);
int8_t int_hashset_insert(int64_t set_ptr, int64_t value);

// Iterator type enum
typedef enum {
    SXITER_VEC,
    SXITER_MAP,
    SXITER_FILTER,
    SXITER_FILTER_MAP,
    SXITER_ENUMERATE,
    SXITER_SKIP,
    SXITER_TAKE,
    SXITER_CHAIN,
    SXITER_ZIP,
    SXITER_RANGE
} SxIterType;

// Function pointer types
typedef int64_t (*SxMapFn)(int64_t);
typedef int8_t (*SxPredicateFn)(int64_t);
typedef int64_t (*SxFilterMapFn)(int64_t, int8_t*);  // Returns value and sets valid flag
typedef int64_t (*SxFoldFn)(int64_t, int64_t);  // (acc, elem) -> acc

// Forward declaration
struct SxIter;

// Iterator struct
typedef struct SxIter {
    SxIterType type;
    union {
        struct { SxVec* vec; size_t index; } vec;
        struct { struct SxIter* source; SxMapFn fn; } map;
        struct { struct SxIter* source; SxPredicateFn fn; } filter;
        struct { struct SxIter* source; SxFilterMapFn fn; } filter_map;
        struct { struct SxIter* source; size_t index; } enumerate;
        struct { struct SxIter* source; size_t remaining; } skip;
        struct { struct SxIter* source; size_t remaining; } take;
        struct { struct SxIter* first; struct SxIter* second; int8_t on_second; } chain;
        struct { struct SxIter* a; struct SxIter* b; } zip;
        struct { int64_t current; int64_t end; int64_t step; } range;
    } data;
} SxIter;

// Option-like return for iterator next
typedef struct {
    int64_t value;
    int8_t has_value;
} SxIterOption;

// Enumerate pair
typedef struct {
    size_t index;
    int64_t value;
} SxEnumeratePair;

// Zip pair
typedef struct {
    int64_t first;
    int64_t second;
} SxZipPair;

// Create iterator from Vec
SxIter* sxiter_from_vec(SxVec* vec) {
    SxIter* iter = (SxIter*)malloc(sizeof(SxIter));
    iter->type = SXITER_VEC;
    iter->data.vec.vec = vec;
    iter->data.vec.index = 0;
    return iter;
}

// Create range iterator
SxIter* sxiter_range(int64_t start, int64_t end) {
    SxIter* iter = (SxIter*)malloc(sizeof(SxIter));
    iter->type = SXITER_RANGE;
    iter->data.range.current = start;
    iter->data.range.end = end;
    iter->data.range.step = start <= end ? 1 : -1;
    return iter;
}

SxIter* sxiter_range_step(int64_t start, int64_t end, int64_t step) {
    SxIter* iter = (SxIter*)malloc(sizeof(SxIter));
    iter->type = SXITER_RANGE;
    iter->data.range.current = start;
    iter->data.range.end = end;
    iter->data.range.step = step != 0 ? step : 1;
    return iter;
}

// Forward declaration of sxiter_next
SxIterOption sxiter_next(SxIter* iter);

// Map combinator
SxIter* sxiter_map(SxIter* source, SxMapFn fn) {
    SxIter* iter = (SxIter*)malloc(sizeof(SxIter));
    iter->type = SXITER_MAP;
    iter->data.map.source = source;
    iter->data.map.fn = fn;
    return iter;
}

// Filter combinator
SxIter* sxiter_filter(SxIter* source, SxPredicateFn fn) {
    SxIter* iter = (SxIter*)malloc(sizeof(SxIter));
    iter->type = SXITER_FILTER;
    iter->data.filter.source = source;
    iter->data.filter.fn = fn;
    return iter;
}

// Filter-map combinator
SxIter* sxiter_filter_map(SxIter* source, SxFilterMapFn fn) {
    SxIter* iter = (SxIter*)malloc(sizeof(SxIter));
    iter->type = SXITER_FILTER_MAP;
    iter->data.filter_map.source = source;
    iter->data.filter_map.fn = fn;
    return iter;
}

// Enumerate combinator
SxIter* sxiter_enumerate(SxIter* source) {
    SxIter* iter = (SxIter*)malloc(sizeof(SxIter));
    iter->type = SXITER_ENUMERATE;
    iter->data.enumerate.source = source;
    iter->data.enumerate.index = 0;
    return iter;
}

// Skip combinator
SxIter* sxiter_skip(SxIter* source, int64_t n) {
    SxIter* iter = (SxIter*)malloc(sizeof(SxIter));
    iter->type = SXITER_SKIP;
    iter->data.skip.source = source;
    iter->data.skip.remaining = n > 0 ? (size_t)n : 0;
    return iter;
}

// Take combinator
SxIter* sxiter_take(SxIter* source, int64_t n) {
    SxIter* iter = (SxIter*)malloc(sizeof(SxIter));
    iter->type = SXITER_TAKE;
    iter->data.take.source = source;
    iter->data.take.remaining = n > 0 ? (size_t)n : 0;
    return iter;
}

// Chain combinator
SxIter* sxiter_chain(SxIter* first, SxIter* second) {
    SxIter* iter = (SxIter*)malloc(sizeof(SxIter));
    iter->type = SXITER_CHAIN;
    iter->data.chain.first = first;
    iter->data.chain.second = second;
    iter->data.chain.on_second = 0;
    return iter;
}

// Zip combinator
SxIter* sxiter_zip(SxIter* a, SxIter* b) {
    SxIter* iter = (SxIter*)malloc(sizeof(SxIter));
    iter->type = SXITER_ZIP;
    iter->data.zip.a = a;
    iter->data.zip.b = b;
    return iter;
}

// Core next function
SxIterOption sxiter_next(SxIter* iter) {
    SxIterOption result = {0, 0};
    if (!iter) return result;

    switch (iter->type) {
        case SXITER_VEC: {
            if (iter->data.vec.index < iter->data.vec.vec->len) {
                result.value = (int64_t)iter->data.vec.vec->items[iter->data.vec.index++];
                result.has_value = 1;
            }
            break;
        }
        case SXITER_RANGE: {
            if ((iter->data.range.step > 0 && iter->data.range.current < iter->data.range.end) ||
                (iter->data.range.step < 0 && iter->data.range.current > iter->data.range.end)) {
                result.value = iter->data.range.current;
                result.has_value = 1;
                iter->data.range.current += iter->data.range.step;
            }
            break;
        }
        case SXITER_MAP: {
            SxIterOption src = sxiter_next(iter->data.map.source);
            if (src.has_value) {
                result.value = iter->data.map.fn(src.value);
                result.has_value = 1;
            }
            break;
        }
        case SXITER_FILTER: {
            while (1) {
                SxIterOption src = sxiter_next(iter->data.filter.source);
                if (!src.has_value) break;
                if (iter->data.filter.fn(src.value)) {
                    result = src;
                    break;
                }
            }
            break;
        }
        case SXITER_FILTER_MAP: {
            while (1) {
                SxIterOption src = sxiter_next(iter->data.filter_map.source);
                if (!src.has_value) break;
                int8_t valid = 0;
                int64_t mapped = iter->data.filter_map.fn(src.value, &valid);
                if (valid) {
                    result.value = mapped;
                    result.has_value = 1;
                    break;
                }
            }
            break;
        }
        case SXITER_ENUMERATE: {
            SxIterOption src = sxiter_next(iter->data.enumerate.source);
            if (src.has_value) {
                // Pack index and value into a heap-allocated pair
                SxEnumeratePair* pair = (SxEnumeratePair*)malloc(sizeof(SxEnumeratePair));
                pair->index = iter->data.enumerate.index++;
                pair->value = src.value;
                result.value = (int64_t)pair;
                result.has_value = 1;
            }
            break;
        }
        case SXITER_SKIP: {
            // Skip remaining elements first
            while (iter->data.skip.remaining > 0) {
                SxIterOption src = sxiter_next(iter->data.skip.source);
                if (!src.has_value) return result;
                iter->data.skip.remaining--;
            }
            result = sxiter_next(iter->data.skip.source);
            break;
        }
        case SXITER_TAKE: {
            if (iter->data.take.remaining > 0) {
                SxIterOption src = sxiter_next(iter->data.take.source);
                if (src.has_value) {
                    iter->data.take.remaining--;
                    result = src;
                }
            }
            break;
        }
        case SXITER_CHAIN: {
            if (!iter->data.chain.on_second) {
                result = sxiter_next(iter->data.chain.first);
                if (!result.has_value) {
                    iter->data.chain.on_second = 1;
                    result = sxiter_next(iter->data.chain.second);
                }
            } else {
                result = sxiter_next(iter->data.chain.second);
            }
            break;
        }
        case SXITER_ZIP: {
            SxIterOption a = sxiter_next(iter->data.zip.a);
            SxIterOption b = sxiter_next(iter->data.zip.b);
            if (a.has_value && b.has_value) {
                SxZipPair* pair = (SxZipPair*)malloc(sizeof(SxZipPair));
                pair->first = a.value;
                pair->second = b.value;
                result.value = (int64_t)pair;
                result.has_value = 1;
            }
            break;
        }
    }
    return result;
}

// Free iterator (recursively frees sources)
void sxiter_free(SxIter* iter) {
    if (!iter) return;
    switch (iter->type) {
        case SXITER_MAP:
            sxiter_free(iter->data.map.source);
            break;
        case SXITER_FILTER:
            sxiter_free(iter->data.filter.source);
            break;
        case SXITER_FILTER_MAP:
            sxiter_free(iter->data.filter_map.source);
            break;
        case SXITER_ENUMERATE:
            sxiter_free(iter->data.enumerate.source);
            break;
        case SXITER_SKIP:
            sxiter_free(iter->data.skip.source);
            break;
        case SXITER_TAKE:
            sxiter_free(iter->data.take.source);
            break;
        case SXITER_CHAIN:
            sxiter_free(iter->data.chain.first);
            sxiter_free(iter->data.chain.second);
            break;
        case SXITER_ZIP:
            sxiter_free(iter->data.zip.a);
            sxiter_free(iter->data.zip.b);
            break;
        default:
            break;
    }
    free(iter);
}

// ============================================================================
// Consuming Combinators
// ============================================================================

// Collect iterator into Vec
SxVec* sxiter_collect_vec(SxIter* iter) {
    SxVec* result = intrinsic_vec_new();
    while (1) {
        SxIterOption opt = sxiter_next(iter);
        if (!opt.has_value) break;
        intrinsic_vec_push(result, (void*)opt.value);
    }
    return result;
}

// Fold with initial value and accumulator function
int64_t sxiter_fold(SxIter* iter, int64_t init, SxFoldFn fn) {
    int64_t acc = init;
    while (1) {
        SxIterOption opt = sxiter_next(iter);
        if (!opt.has_value) break;
        acc = fn(acc, opt.value);
    }
    return acc;
}

// Reduce (fold without initial value, uses first element)
SxIterOption sxiter_reduce(SxIter* iter, SxFoldFn fn) {
    SxIterOption result = {0, 0};
    SxIterOption first = sxiter_next(iter);
    if (!first.has_value) return result;

    int64_t acc = first.value;
    while (1) {
        SxIterOption opt = sxiter_next(iter);
        if (!opt.has_value) break;
        acc = fn(acc, opt.value);
    }
    result.value = acc;
    result.has_value = 1;
    return result;
}

// Helper function for sum
static int64_t sxiter_sum_fn(int64_t acc, int64_t elem) {
    return acc + elem;
}

// Sum all elements
int64_t sxiter_sum(SxIter* iter) {
    return sxiter_fold(iter, 0, sxiter_sum_fn);
}

// Helper function for product
static int64_t sxiter_product_fn(int64_t acc, int64_t elem) {
    return acc * elem;
}

// Product of all elements
int64_t sxiter_product(SxIter* iter) {
    return sxiter_fold(iter, 1, sxiter_product_fn);
}

// Count elements
int64_t sxiter_count(SxIter* iter) {
    int64_t count = 0;
    while (1) {
        SxIterOption opt = sxiter_next(iter);
        if (!opt.has_value) break;
        count++;
    }
    return count;
}

// Execute function for each element
void sxiter_for_each(SxIter* iter, SxMapFn fn) {
    while (1) {
        SxIterOption opt = sxiter_next(iter);
        if (!opt.has_value) break;
        fn(opt.value);
    }
}

// Find first element matching predicate
SxIterOption sxiter_find(SxIter* iter, SxPredicateFn predicate) {
    while (1) {
        SxIterOption opt = sxiter_next(iter);
        if (!opt.has_value) return opt;
        if (predicate(opt.value)) return opt;
    }
}

// Find position of first element matching predicate
int64_t sxiter_position(SxIter* iter, SxPredicateFn predicate) {
    int64_t pos = 0;
    while (1) {
        SxIterOption opt = sxiter_next(iter);
        if (!opt.has_value) return -1;
        if (predicate(opt.value)) return pos;
        pos++;
    }
}

// Check if any element matches predicate
int8_t sxiter_any(SxIter* iter, SxPredicateFn predicate) {
    while (1) {
        SxIterOption opt = sxiter_next(iter);
        if (!opt.has_value) return 0;
        if (predicate(opt.value)) return 1;
    }
}

// Check if all elements match predicate
int8_t sxiter_all(SxIter* iter, SxPredicateFn predicate) {
    while (1) {
        SxIterOption opt = sxiter_next(iter);
        if (!opt.has_value) return 1;
        if (!predicate(opt.value)) return 0;
    }
}

// Find minimum element
SxIterOption sxiter_min(SxIter* iter) {
    SxIterOption result = {0, 0};
    SxIterOption first = sxiter_next(iter);
    if (!first.has_value) return result;

    int64_t min = first.value;
    while (1) {
        SxIterOption opt = sxiter_next(iter);
        if (!opt.has_value) break;
        if (opt.value < min) min = opt.value;
    }
    result.value = min;
    result.has_value = 1;
    return result;
}

// Find maximum element
SxIterOption sxiter_max(SxIter* iter) {
    SxIterOption result = {0, 0};
    SxIterOption first = sxiter_next(iter);
    if (!first.has_value) return result;

    int64_t max = first.value;
    while (1) {
        SxIterOption opt = sxiter_next(iter);
        if (!opt.has_value) break;
        if (opt.value > max) max = opt.value;
    }
    result.value = max;
    result.has_value = 1;
    return result;
}

// Get nth element
SxIterOption sxiter_nth(SxIter* iter, int64_t n) {
    SxIterOption result = {0, 0};
    if (n < 0) return result;

    for (int64_t i = 0; i <= n; i++) {
        result = sxiter_next(iter);
        if (!result.has_value) return result;
    }
    return result;
}

// Get last element
SxIterOption sxiter_last(SxIter* iter) {
    SxIterOption result = {0, 0};
    while (1) {
        SxIterOption opt = sxiter_next(iter);
        if (!opt.has_value) break;
        result = opt;
    }
    return result;
}

// Collect into HashMap (assumes elements are key-value pairs with SxString* keys)
int64_t sxiter_collect_hashmap(SxIter* iter) {
    int64_t map = hashmap_new();
    while (1) {
        SxIterOption opt = sxiter_next(iter);
        if (!opt.has_value) break;
        SxZipPair* pair = (SxZipPair*)opt.value;
        hashmap_insert(map, pair->first, pair->second);
    }
    return map;
}

// Collect into Integer HashMap (for int key-value pairs)
int64_t sxiter_collect_int_hashmap(SxIter* iter) {
    int64_t map = int_hashmap_new();
    while (1) {
        SxIterOption opt = sxiter_next(iter);
        if (!opt.has_value) break;
        SxZipPair* pair = (SxZipPair*)opt.value;
        int_hashmap_insert(map, pair->first, pair->second);
    }
    return map;
}

// Collect into HashSet (for SxString* values)
int64_t sxiter_collect_hashset(SxIter* iter) {
    int64_t set = hashset_new();
    while (1) {
        SxIterOption opt = sxiter_next(iter);
        if (!opt.has_value) break;
        hashset_insert(set, opt.value);
    }
    return set;
}

// Collect into Integer HashSet (for int64_t values)
int64_t sxiter_collect_int_hashset(SxIter* iter) {
    int64_t set = int_hashset_new();
    while (1) {
        SxIterOption opt = sxiter_next(iter);
        if (!opt.has_value) break;
        int_hashset_insert(set, opt.value);
    }
    return set;
}

// ============================================================================
// Convenience functions for common patterns
// ============================================================================

// Create iterator from Vec (alias)
int64_t sxiter_vec(int64_t vec_ptr) {
    return (int64_t)sxiter_from_vec((SxVec*)vec_ptr);
}

// Get next value from iterator (returns 0 if exhausted)
int64_t sxiter_next_value(int64_t iter_ptr) {
    SxIterOption opt = sxiter_next((SxIter*)iter_ptr);
    return opt.has_value ? opt.value : 0;
}

// Get SxEnumeratePair fields
int64_t sxiter_enumerate_pair_index(int64_t pair_ptr) {
    SxEnumeratePair* pair = (SxEnumeratePair*)pair_ptr;
    return pair ? (int64_t)pair->index : 0;
}

int64_t sxiter_enumerate_pair_value(int64_t pair_ptr) {
    SxEnumeratePair* pair = (SxEnumeratePair*)pair_ptr;
    return pair ? pair->value : 0;
}

void sxiter_enumerate_pair_free(int64_t pair_ptr) {
    free((void*)pair_ptr);
}

// Get SxZipPair fields
int64_t sxiter_zip_pair_first(int64_t pair_ptr) {
    SxZipPair* pair = (SxZipPair*)pair_ptr;
    return pair ? pair->first : 0;
}

int64_t sxiter_zip_pair_second(int64_t pair_ptr) {
    SxZipPair* pair = (SxZipPair*)pair_ptr;
    return pair ? pair->second : 0;
}

void sxiter_zip_pair_free(int64_t pair_ptr) {
    free((void*)pair_ptr);
}

// ============================================================================
// Option<T> Implementation
// ============================================================================

// Option is represented as a struct: { tag, value }
// tag: 0 = None, 1 = Some
// value: the contained value (only valid when tag = 1)

typedef struct {
    int8_t tag;     // 0 = None, 1 = Some
    int64_t value;  // The contained value
} SxOption;

// Create Some(value)
SxOption* option_some(int64_t value) {
    SxOption* opt = (SxOption*)malloc(sizeof(SxOption));
    opt->tag = 1;
    opt->value = value;
    return opt;
}

// Create None
SxOption* option_none(void) {
    SxOption* opt = (SxOption*)malloc(sizeof(SxOption));
    opt->tag = 0;
    opt->value = 0;
    return opt;
}

// Free an Option
void option_free(int64_t opt_ptr) {
    if (opt_ptr) free((void*)opt_ptr);
}

// Check if Some
int8_t option_is_some(int64_t opt_ptr) {
    SxOption* opt = (SxOption*)opt_ptr;
    return opt && opt->tag == 1;
}

// Check if None
int8_t option_is_none(int64_t opt_ptr) {
    SxOption* opt = (SxOption*)opt_ptr;
    return !opt || opt->tag == 0;
}

// unwrap() - Get value or panic
int64_t option_unwrap(int64_t opt_ptr) {
    SxOption* opt = (SxOption*)opt_ptr;
    if (!opt || opt->tag == 0) {
        fprintf(stderr, "PANIC: called unwrap() on a None value\n");
        exit(1);
    }
    return opt->value;
}

// expect(msg) - Get value or panic with message
int64_t option_expect(int64_t opt_ptr, int64_t msg_ptr) {
    SxOption* opt = (SxOption*)opt_ptr;
    if (!opt || opt->tag == 0) {
        SxString* msg = (SxString*)msg_ptr;
        if (msg && msg->data) {
            fprintf(stderr, "PANIC: %s\n", msg->data);
        } else {
            fprintf(stderr, "PANIC: called expect() on a None value\n");
        }
        exit(1);
    }
    return opt->value;
}

// unwrap_or(default) - Get value or default
int64_t option_unwrap_or(int64_t opt_ptr, int64_t default_val) {
    SxOption* opt = (SxOption*)opt_ptr;
    if (!opt || opt->tag == 0) {
        return default_val;
    }
    return opt->value;
}

// unwrap_or_else(fn) - Get value or compute default
typedef int64_t (*OptionDefaultFn)(void);
int64_t option_unwrap_or_else(int64_t opt_ptr, OptionDefaultFn fn) {
    SxOption* opt = (SxOption*)opt_ptr;
    if (!opt || opt->tag == 0) {
        return fn ? fn() : 0;
    }
    return opt->value;
}

// map(fn) - Transform inner value
int64_t option_map(int64_t opt_ptr, SxMapFn fn) {
    SxOption* opt = (SxOption*)opt_ptr;
    if (!opt || opt->tag == 0 || !fn) {
        return (int64_t)option_none();
    }
    return (int64_t)option_some(fn(opt->value));
}

// map_or(default, fn) - Map or return default
int64_t option_map_or(int64_t opt_ptr, int64_t default_val, SxMapFn fn) {
    SxOption* opt = (SxOption*)opt_ptr;
    if (!opt || opt->tag == 0) {
        return default_val;
    }
    return fn ? fn(opt->value) : opt->value;
}

// map_or_else(default_fn, fn) - Map or compute default
int64_t option_map_or_else(int64_t opt_ptr, OptionDefaultFn default_fn, SxMapFn fn) {
    SxOption* opt = (SxOption*)opt_ptr;
    if (!opt || opt->tag == 0) {
        return default_fn ? default_fn() : 0;
    }
    return fn ? fn(opt->value) : opt->value;
}

// and(other) - Return other if Some, else None
int64_t option_and(int64_t opt_ptr, int64_t other_ptr) {
    SxOption* opt = (SxOption*)opt_ptr;
    if (!opt || opt->tag == 0) {
        return (int64_t)option_none();
    }
    // If Some, return other (clone it to be safe)
    SxOption* other = (SxOption*)other_ptr;
    if (!other) return (int64_t)option_none();
    if (other->tag == 0) return (int64_t)option_none();
    return (int64_t)option_some(other->value);
}

// and_then(fn) - Flatmap: returns Option from fn
typedef int64_t (*OptionAndThenFn)(int64_t);  // Returns Option ptr
int64_t option_and_then(int64_t opt_ptr, OptionAndThenFn fn) {
    SxOption* opt = (SxOption*)opt_ptr;
    if (!opt || opt->tag == 0 || !fn) {
        return (int64_t)option_none();
    }
    return fn(opt->value);
}

// or(other) - Return self if Some, else other
int64_t option_or(int64_t opt_ptr, int64_t other_ptr) {
    SxOption* opt = (SxOption*)opt_ptr;
    if (opt && opt->tag == 1) {
        return (int64_t)option_some(opt->value);
    }
    SxOption* other = (SxOption*)other_ptr;
    if (!other) return (int64_t)option_none();
    if (other->tag == 0) return (int64_t)option_none();
    return (int64_t)option_some(other->value);
}

// or_else(fn) - Return self if Some, else compute
typedef int64_t (*OptionOrElseFn)(void);  // Returns Option ptr
int64_t option_or_else(int64_t opt_ptr, OptionOrElseFn fn) {
    SxOption* opt = (SxOption*)opt_ptr;
    if (opt && opt->tag == 1) {
        return (int64_t)option_some(opt->value);
    }
    return fn ? fn() : (int64_t)option_none();
}

// filter(predicate) - Keep if predicate true
int64_t option_filter(int64_t opt_ptr, SxPredicateFn predicate) {
    SxOption* opt = (SxOption*)opt_ptr;
    if (!opt || opt->tag == 0 || !predicate) {
        return (int64_t)option_none();
    }
    if (predicate(opt->value)) {
        return (int64_t)option_some(opt->value);
    }
    return (int64_t)option_none();
}

// Get value (unchecked - for internal use)
int64_t option_get_value(int64_t opt_ptr) {
    SxOption* opt = (SxOption*)opt_ptr;
    return opt ? opt->value : 0;
}

// Get tag
int8_t option_get_tag(int64_t opt_ptr) {
    SxOption* opt = (SxOption*)opt_ptr;
    return opt ? opt->tag : 0;
}

// Clone an Option
int64_t option_clone(int64_t opt_ptr) {
    SxOption* opt = (SxOption*)opt_ptr;
    if (!opt) return (int64_t)option_none();
    if (opt->tag == 0) return (int64_t)option_none();
    return (int64_t)option_some(opt->value);
}

// ============================================================================
// Result<T, E> Implementation
// ============================================================================

// Result is represented as a struct: { tag, value, error }
// tag: 0 = Err, 1 = Ok
// value: the success value (only valid when tag = 1)
// error: the error value (only valid when tag = 0)

typedef struct {
    int8_t tag;     // 0 = Err, 1 = Ok
    int64_t value;  // The success value
    int64_t error;  // The error value
} SxResult;

// Create Ok(value)
SxResult* result_ok(int64_t value) {
    SxResult* res = (SxResult*)malloc(sizeof(SxResult));
    res->tag = 1;
    res->value = value;
    res->error = 0;
    return res;
}

// Create Err(error)
SxResult* result_err(int64_t error) {
    SxResult* res = (SxResult*)malloc(sizeof(SxResult));
    res->tag = 0;
    res->value = 0;
    res->error = error;
    return res;
}

// Free a Result
void result_free(int64_t res_ptr) {
    if (res_ptr) free((void*)res_ptr);
}

// Check if Ok
int8_t result_is_ok(int64_t res_ptr) {
    SxResult* res = (SxResult*)res_ptr;
    return res && res->tag == 1;
}

// Check if Err
int8_t result_is_err(int64_t res_ptr) {
    SxResult* res = (SxResult*)res_ptr;
    return !res || res->tag == 0;
}

// ok() - Convert to Option<T>
int64_t result_ok_option(int64_t res_ptr) {
    SxResult* res = (SxResult*)res_ptr;
    if (!res || res->tag == 0) {
        return (int64_t)option_none();
    }
    return (int64_t)option_some(res->value);
}

// err() - Convert to Option<E>
int64_t result_err_option(int64_t res_ptr) {
    SxResult* res = (SxResult*)res_ptr;
    if (!res || res->tag == 1) {
        return (int64_t)option_none();
    }
    return (int64_t)option_some(res->error);
}

// unwrap() - Get value or panic
int64_t result_unwrap(int64_t res_ptr) {
    SxResult* res = (SxResult*)res_ptr;
    if (!res || res->tag == 0) {
        if (res) {
            // Try to print error if it's a string
            SxString* err_str = (SxString*)res->error;
            if (err_str && err_str->data) {
                fprintf(stderr, "PANIC: called unwrap() on an Err value: %s\n", err_str->data);
            } else {
                fprintf(stderr, "PANIC: called unwrap() on an Err value: %lld\n", (long long)res->error);
            }
        } else {
            fprintf(stderr, "PANIC: called unwrap() on a null Result\n");
        }
        exit(1);
    }
    return res->value;
}

// unwrap_err() - Get error or panic
int64_t result_unwrap_err(int64_t res_ptr) {
    SxResult* res = (SxResult*)res_ptr;
    if (!res || res->tag == 1) {
        fprintf(stderr, "PANIC: called unwrap_err() on an Ok value\n");
        exit(1);
    }
    return res->error;
}

// expect(msg) - Get value or panic with message
int64_t result_expect(int64_t res_ptr, int64_t msg_ptr) {
    SxResult* res = (SxResult*)res_ptr;
    if (!res || res->tag == 0) {
        SxString* msg = (SxString*)msg_ptr;
        if (msg && msg->data) {
            fprintf(stderr, "PANIC: %s\n", msg->data);
        } else {
            fprintf(stderr, "PANIC: called expect() on an Err value\n");
        }
        exit(1);
    }
    return res->value;
}

// expect_err(msg) - Get error or panic with message
int64_t result_expect_err(int64_t res_ptr, int64_t msg_ptr) {
    SxResult* res = (SxResult*)res_ptr;
    if (!res || res->tag == 1) {
        SxString* msg = (SxString*)msg_ptr;
        if (msg && msg->data) {
            fprintf(stderr, "PANIC: %s\n", msg->data);
        } else {
            fprintf(stderr, "PANIC: called expect_err() on an Ok value\n");
        }
        exit(1);
    }
    return res->error;
}

// unwrap_or(default) - Get value or default
int64_t result_unwrap_or(int64_t res_ptr, int64_t default_val) {
    SxResult* res = (SxResult*)res_ptr;
    if (!res || res->tag == 0) {
        return default_val;
    }
    return res->value;
}

// unwrap_or_else(fn) - Get value or compute from error
typedef int64_t (*ResultErrFn)(int64_t);  // Takes error, returns value
int64_t result_unwrap_or_else(int64_t res_ptr, ResultErrFn fn) {
    SxResult* res = (SxResult*)res_ptr;
    if (!res || res->tag == 0) {
        return fn ? fn(res ? res->error : 0) : 0;
    }
    return res->value;
}

// map(fn) - Transform Ok value
int64_t result_map(int64_t res_ptr, SxMapFn fn) {
    SxResult* res = (SxResult*)res_ptr;
    if (!res || res->tag == 0) {
        return (int64_t)result_err(res ? res->error : 0);
    }
    return (int64_t)result_ok(fn ? fn(res->value) : res->value);
}

// map_err(fn) - Transform Err value
int64_t result_map_err(int64_t res_ptr, SxMapFn fn) {
    SxResult* res = (SxResult*)res_ptr;
    if (!res || res->tag == 0) {
        return (int64_t)result_err(fn && res ? fn(res->error) : (res ? res->error : 0));
    }
    return (int64_t)result_ok(res->value);
}

// and(other) - Return other if Ok, else propagate Err
int64_t result_and(int64_t res_ptr, int64_t other_ptr) {
    SxResult* res = (SxResult*)res_ptr;
    if (!res || res->tag == 0) {
        return (int64_t)result_err(res ? res->error : 0);
    }
    // If Ok, return other (clone it)
    SxResult* other = (SxResult*)other_ptr;
    if (!other) return (int64_t)result_err(0);
    if (other->tag == 0) return (int64_t)result_err(other->error);
    return (int64_t)result_ok(other->value);
}

// and_then(fn) - Flatmap Ok: fn returns Result
typedef int64_t (*ResultAndThenFn)(int64_t);  // Returns Result ptr
int64_t result_and_then(int64_t res_ptr, ResultAndThenFn fn) {
    SxResult* res = (SxResult*)res_ptr;
    if (!res || res->tag == 0 || !fn) {
        return (int64_t)result_err(res ? res->error : 0);
    }
    return fn(res->value);
}

// or(other) - Return self if Ok, else other
int64_t result_or(int64_t res_ptr, int64_t other_ptr) {
    SxResult* res = (SxResult*)res_ptr;
    if (res && res->tag == 1) {
        return (int64_t)result_ok(res->value);
    }
    SxResult* other = (SxResult*)other_ptr;
    if (!other) return (int64_t)result_err(res ? res->error : 0);
    if (other->tag == 0) return (int64_t)result_err(other->error);
    return (int64_t)result_ok(other->value);
}

// or_else(fn) - Return self if Ok, else compute
typedef int64_t (*ResultOrElseFn)(int64_t);  // Takes error, returns Result
int64_t result_or_else(int64_t res_ptr, ResultOrElseFn fn) {
    SxResult* res = (SxResult*)res_ptr;
    if (res && res->tag == 1) {
        return (int64_t)result_ok(res->value);
    }
    return fn ? fn(res ? res->error : 0) : (int64_t)result_err(res ? res->error : 0);
}

// Get value (unchecked - for internal use)
int64_t result_get_value(int64_t res_ptr) {
    SxResult* res = (SxResult*)res_ptr;
    return res ? res->value : 0;
}

// Get error (unchecked - for internal use)
int64_t result_get_error(int64_t res_ptr) {
    SxResult* res = (SxResult*)res_ptr;
    return res ? res->error : 0;
}

// Get tag
int8_t result_get_tag(int64_t res_ptr) {
    SxResult* res = (SxResult*)res_ptr;
    return res ? res->tag : 0;
}

// Clone a Result
int64_t result_clone(int64_t res_ptr) {
    SxResult* res = (SxResult*)res_ptr;
    if (!res) return (int64_t)result_err(0);
    if (res->tag == 0) return (int64_t)result_err(res->error);
    return (int64_t)result_ok(res->value);
}

// ============================================================================
// Option/Result Conversion Functions
// ============================================================================

// ok_or(err) - Convert Option to Result
int64_t option_ok_or(int64_t opt_ptr, int64_t err_val) {
    SxOption* opt = (SxOption*)opt_ptr;
    if (!opt || opt->tag == 0) {
        return (int64_t)result_err(err_val);
    }
    return (int64_t)result_ok(opt->value);
}

// ok_or_else(fn) - Convert Option to Result with computed error
typedef int64_t (*OptionErrorFn)(void);  // Returns error value
int64_t option_ok_or_else(int64_t opt_ptr, OptionErrorFn fn) {
    SxOption* opt = (SxOption*)opt_ptr;
    if (!opt || opt->tag == 0) {
        return (int64_t)result_err(fn ? fn() : 0);
    }
    return (int64_t)result_ok(opt->value);
}

// transpose - Convert Option<Result<T,E>> to Result<Option<T>, E>
// Input: Option containing a Result
// Output: Result containing an Option
int64_t option_transpose(int64_t opt_ptr) {
    SxOption* opt = (SxOption*)opt_ptr;
    if (!opt || opt->tag == 0) {
        // None -> Ok(None)
        return (int64_t)result_ok((int64_t)option_none());
    }
    // Some(result)
    SxResult* inner = (SxResult*)opt->value;
    if (!inner) {
        return (int64_t)result_ok((int64_t)option_none());
    }
    if (inner->tag == 0) {
        // Some(Err(e)) -> Err(e)
        return (int64_t)result_err(inner->error);
    }
    // Some(Ok(v)) -> Ok(Some(v))
    return (int64_t)result_ok((int64_t)option_some(inner->value));
}

// transpose - Convert Result<Option<T>, E> to Option<Result<T, E>>
// Input: Result containing an Option
// Output: Option containing a Result
int64_t result_transpose(int64_t res_ptr) {
    SxResult* res = (SxResult*)res_ptr;
    if (!res || res->tag == 0) {
        // Err(e) -> Some(Err(e))
        return (int64_t)option_some((int64_t)result_err(res ? res->error : 0));
    }
    // Ok(option)
    SxOption* inner = (SxOption*)res->value;
    if (!inner || inner->tag == 0) {
        // Ok(None) -> None
        return (int64_t)option_none();
    }
    // Ok(Some(v)) -> Some(Ok(v))
    return (int64_t)option_some((int64_t)result_ok(inner->value));
}

// flatten - Convert Option<Option<T>> to Option<T>
int64_t option_flatten(int64_t opt_ptr) {
    SxOption* opt = (SxOption*)opt_ptr;
    if (!opt || opt->tag == 0) {
        return (int64_t)option_none();
    }
    SxOption* inner = (SxOption*)opt->value;
    if (!inner || inner->tag == 0) {
        return (int64_t)option_none();
    }
    return (int64_t)option_some(inner->value);
}

// flatten - Convert Result<Result<T,E>, E> to Result<T, E>
int64_t result_flatten(int64_t res_ptr) {
    SxResult* res = (SxResult*)res_ptr;
    if (!res || res->tag == 0) {
        return (int64_t)result_err(res ? res->error : 0);
    }
    SxResult* inner = (SxResult*)res->value;
    if (!inner || inner->tag == 0) {
        return (int64_t)result_err(inner ? inner->error : 0);
    }
    return (int64_t)result_ok(inner->value);
}

// ============================================================================
// Convenience wrapper functions (int64_t interface)
// ============================================================================

int64_t option_some_i64(int64_t value) {
    return (int64_t)option_some(value);
}

int64_t option_none_i64(void) {
    return (int64_t)option_none();
}

int64_t result_ok_i64(int64_t value) {
    return (int64_t)result_ok(value);
}

int64_t result_err_i64(int64_t error) {
    return (int64_t)result_err(error);
}

// Create Err with string message
int64_t result_err_msg(const char* msg) {
    SxString* s = intrinsic_string_new(msg);
    return (int64_t)result_err((int64_t)s);
}

// ============================================================================
// JSON Value Type Implementation
// ============================================================================

// JSON Value types
typedef enum {
    JSON_NULL = 0,
    JSON_BOOL = 1,
    JSON_NUMBER = 2,
    JSON_STRING = 3,
    JSON_ARRAY = 4,
    JSON_OBJECT = 5
} JsonType;

// Forward declarations
typedef struct JsonValue JsonValue;
typedef struct JsonArray JsonArray;
typedef struct JsonObject JsonObject;
typedef struct JsonObjectEntry JsonObjectEntry;

// JSON Array - dynamic array of JsonValue pointers
struct JsonArray {
    JsonValue** items;
    size_t len;
    size_t cap;
};

// JSON Object entry (key-value pair)
struct JsonObjectEntry {
    char* key;
    JsonValue* value;
};

// JSON Object - array of key-value pairs
struct JsonObject {
    JsonObjectEntry* entries;
    size_t len;
    size_t cap;
};

// JSON Value - tagged union
struct JsonValue {
    JsonType type;
    union {
        int8_t bool_val;
        double number_val;
        char* string_val;
        JsonArray* array_val;
        JsonObject* object_val;
    } data;
};

// ============================================================================
// JSON Value Constructors
// ============================================================================

JsonValue* json_null(void) {
    JsonValue* val = (JsonValue*)malloc(sizeof(JsonValue));
    val->type = JSON_NULL;
    return val;
}

JsonValue* json_bool(int8_t b) {
    JsonValue* val = (JsonValue*)malloc(sizeof(JsonValue));
    val->type = JSON_BOOL;
    val->data.bool_val = b ? 1 : 0;
    return val;
}

JsonValue* json_number(double n) {
    JsonValue* val = (JsonValue*)malloc(sizeof(JsonValue));
    val->type = JSON_NUMBER;
    val->data.number_val = n;
    return val;
}

JsonValue* json_number_i64(int64_t n) {
    return json_number((double)n);
}

JsonValue* json_string(const char* s) {
    JsonValue* val = (JsonValue*)malloc(sizeof(JsonValue));
    val->type = JSON_STRING;
    val->data.string_val = s ? strdup(s) : strdup("");
    return val;
}

JsonValue* json_string_sx(int64_t sx_str_ptr) {
    SxString* s = (SxString*)sx_str_ptr;
    if (!s || !s->data) {
        return json_string("");
    }
    return json_string(s->data);
}

JsonValue* json_array(void) {
    JsonValue* val = (JsonValue*)malloc(sizeof(JsonValue));
    val->type = JSON_ARRAY;
    val->data.array_val = (JsonArray*)malloc(sizeof(JsonArray));
    val->data.array_val->items = NULL;
    val->data.array_val->len = 0;
    val->data.array_val->cap = 0;
    return val;
}

JsonValue* json_object(void) {
    JsonValue* val = (JsonValue*)malloc(sizeof(JsonValue));
    val->type = JSON_OBJECT;
    val->data.object_val = (JsonObject*)malloc(sizeof(JsonObject));
    val->data.object_val->entries = NULL;
    val->data.object_val->len = 0;
    val->data.object_val->cap = 0;
    return val;
}

// ============================================================================
// JSON Value Type Checks
// ============================================================================

int8_t json_is_null(int64_t val_ptr) {
    JsonValue* val = (JsonValue*)val_ptr;
    return val && val->type == JSON_NULL;
}

int8_t json_is_bool(int64_t val_ptr) {
    JsonValue* val = (JsonValue*)val_ptr;
    return val && val->type == JSON_BOOL;
}

int8_t json_is_number(int64_t val_ptr) {
    JsonValue* val = (JsonValue*)val_ptr;
    return val && val->type == JSON_NUMBER;
}

int8_t json_is_string(int64_t val_ptr) {
    JsonValue* val = (JsonValue*)val_ptr;
    return val && val->type == JSON_STRING;
}

int8_t json_is_array(int64_t val_ptr) {
    JsonValue* val = (JsonValue*)val_ptr;
    return val && val->type == JSON_ARRAY;
}

int8_t json_is_object(int64_t val_ptr) {
    JsonValue* val = (JsonValue*)val_ptr;
    return val && val->type == JSON_OBJECT;
}

int64_t json_type(int64_t val_ptr) {
    JsonValue* val = (JsonValue*)val_ptr;
    return val ? (int64_t)val->type : -1;
}

// ============================================================================
// JSON Value Accessors
// ============================================================================

int8_t json_as_bool(int64_t val_ptr) {
    JsonValue* val = (JsonValue*)val_ptr;
    if (!val) return 0;
    switch (val->type) {
        case JSON_BOOL: return val->data.bool_val;
        case JSON_NUMBER: return val->data.number_val != 0.0;
        case JSON_NULL: return 0;
        default: return 1;  // Non-null values are truthy
    }
}

double json_as_f64(int64_t val_ptr) {
    JsonValue* val = (JsonValue*)val_ptr;
    if (!val) return 0.0;
    switch (val->type) {
        case JSON_NUMBER: return val->data.number_val;
        case JSON_BOOL: return val->data.bool_val ? 1.0 : 0.0;
        case JSON_STRING: return atof(val->data.string_val);
        default: return 0.0;
    }
}

int64_t json_as_i64(int64_t val_ptr) {
    return (int64_t)json_as_f64(val_ptr);
}

int64_t json_as_string(int64_t val_ptr) {
    JsonValue* val = (JsonValue*)val_ptr;
    if (!val) return (int64_t)intrinsic_string_new("");
    switch (val->type) {
        case JSON_STRING:
            return (int64_t)intrinsic_string_new(val->data.string_val);
        case JSON_NULL:
            return (int64_t)intrinsic_string_new("null");
        case JSON_BOOL:
            return (int64_t)intrinsic_string_new(val->data.bool_val ? "true" : "false");
        case JSON_NUMBER: {
            char buf[64];
            double n = val->data.number_val;
            if (n == (int64_t)n) {
                snprintf(buf, sizeof(buf), "%lld", (long long)(int64_t)n);
            } else {
                snprintf(buf, sizeof(buf), "%g", n);
            }
            return (int64_t)intrinsic_string_new(buf);
        }
        default:
            return (int64_t)intrinsic_string_new("");
    }
}

// Get raw C string (internal use)
const char* json_get_string_raw(int64_t val_ptr) {
    JsonValue* val = (JsonValue*)val_ptr;
    if (!val || val->type != JSON_STRING) return NULL;
    return val->data.string_val;
}

// ============================================================================
// JSON Array Operations
// ============================================================================

void json_array_push(int64_t arr_ptr, int64_t val_ptr) {
    JsonValue* arr = (JsonValue*)arr_ptr;
    JsonValue* val = (JsonValue*)val_ptr;
    if (!arr || arr->type != JSON_ARRAY || !val) return;

    JsonArray* array = arr->data.array_val;
    if (array->len >= array->cap) {
        size_t new_cap = array->cap == 0 ? 8 : array->cap * 2;
        array->items = (JsonValue**)realloc(array->items, new_cap * sizeof(JsonValue*));
        array->cap = new_cap;
    }
    array->items[array->len++] = val;
}

int64_t json_get_index(int64_t arr_ptr, int64_t index) {
    JsonValue* arr = (JsonValue*)arr_ptr;
    if (!arr || arr->type != JSON_ARRAY) return 0;

    JsonArray* array = arr->data.array_val;
    if (index < 0 || (size_t)index >= array->len) return 0;
    return (int64_t)array->items[index];
}

int64_t json_array_len(int64_t arr_ptr) {
    JsonValue* arr = (JsonValue*)arr_ptr;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    return (int64_t)arr->data.array_val->len;
}

// ============================================================================
// JSON Object Operations
// ============================================================================

void json_object_set(int64_t obj_ptr, const char* key, int64_t val_ptr) {
    JsonValue* obj = (JsonValue*)obj_ptr;
    JsonValue* val = (JsonValue*)val_ptr;
    if (!obj || obj->type != JSON_OBJECT || !key || !val) return;

    JsonObject* object = obj->data.object_val;

    // Check if key exists
    for (size_t i = 0; i < object->len; i++) {
        if (strcmp(object->entries[i].key, key) == 0) {
            // Replace existing value (note: old value is not freed, caller should manage)
            object->entries[i].value = val;
            return;
        }
    }

    // Add new entry
    if (object->len >= object->cap) {
        size_t new_cap = object->cap == 0 ? 8 : object->cap * 2;
        object->entries = (JsonObjectEntry*)realloc(object->entries, new_cap * sizeof(JsonObjectEntry));
        object->cap = new_cap;
    }
    object->entries[object->len].key = strdup(key);
    object->entries[object->len].value = val;
    object->len++;
}

void json_object_set_sx(int64_t obj_ptr, int64_t key_ptr, int64_t val_ptr) {
    SxString* key = (SxString*)key_ptr;
    if (!key || !key->data) return;
    json_object_set(obj_ptr, key->data, val_ptr);
}

int64_t json_get(int64_t obj_ptr, const char* key) {
    JsonValue* obj = (JsonValue*)obj_ptr;
    if (!obj || obj->type != JSON_OBJECT || !key) return 0;

    JsonObject* object = obj->data.object_val;
    for (size_t i = 0; i < object->len; i++) {
        if (strcmp(object->entries[i].key, key) == 0) {
            return (int64_t)object->entries[i].value;
        }
    }
    return 0;
}

int64_t json_get_sx(int64_t obj_ptr, int64_t key_ptr) {
    SxString* key = (SxString*)key_ptr;
    if (!key || !key->data) return 0;
    return json_get(obj_ptr, key->data);
}

int64_t json_object_len(int64_t obj_ptr) {
    JsonValue* obj = (JsonValue*)obj_ptr;
    if (!obj || obj->type != JSON_OBJECT) return 0;
    return (int64_t)obj->data.object_val->len;
}

int8_t json_object_has(int64_t obj_ptr, const char* key) {
    return json_get(obj_ptr, key) != 0;
}

int8_t json_object_has_sx(int64_t obj_ptr, int64_t key_ptr) {
    SxString* key = (SxString*)key_ptr;
    if (!key || !key->data) return 0;
    return json_object_has(obj_ptr, key->data);
}

// Get key at index (for iteration)
int64_t json_object_key_at(int64_t obj_ptr, int64_t index) {
    JsonValue* obj = (JsonValue*)obj_ptr;
    if (!obj || obj->type != JSON_OBJECT) return 0;
    JsonObject* object = obj->data.object_val;
    if (index < 0 || (size_t)index >= object->len) return 0;
    return (int64_t)intrinsic_string_new(object->entries[index].key);
}

// Get value at index (for iteration)
int64_t json_object_value_at(int64_t obj_ptr, int64_t index) {
    JsonValue* obj = (JsonValue*)obj_ptr;
    if (!obj || obj->type != JSON_OBJECT) return 0;
    JsonObject* object = obj->data.object_val;
    if (index < 0 || (size_t)index >= object->len) return 0;
    return (int64_t)object->entries[index].value;
}

// Create a new empty JSON object (returns i64 pointer)
int64_t json_object_new(void) {
    return (int64_t)json_object();
}

// Create a new empty JSON array (returns i64 pointer)
int64_t json_array_new(void) {
    return (int64_t)json_array();
}

// Get all keys of a JSON object as a Vec of strings
int64_t json_keys(int64_t obj_ptr) {
    SxVec* keys = intrinsic_vec_new();
    JsonValue* obj = (JsonValue*)obj_ptr;
    if (!obj || obj->type != JSON_OBJECT) return (int64_t)keys;

    JsonObject* object = obj->data.object_val;
    for (size_t i = 0; i < object->len; i++) {
        SxString* key = intrinsic_string_new(object->entries[i].key);
        intrinsic_vec_push(keys, key);
    }
    return (int64_t)keys;
}

// ============================================================================
// JSON Free
// ============================================================================

void json_free(int64_t val_ptr) {
    JsonValue* val = (JsonValue*)val_ptr;
    if (!val) return;

    switch (val->type) {
        case JSON_STRING:
            if (val->data.string_val) free(val->data.string_val);
            break;
        case JSON_ARRAY: {
            JsonArray* arr = val->data.array_val;
            if (arr) {
                for (size_t i = 0; i < arr->len; i++) {
                    json_free((int64_t)arr->items[i]);
                }
                if (arr->items) free(arr->items);
                free(arr);
            }
            break;
        }
        case JSON_OBJECT: {
            JsonObject* obj = val->data.object_val;
            if (obj) {
                for (size_t i = 0; i < obj->len; i++) {
                    if (obj->entries[i].key) free(obj->entries[i].key);
                    json_free((int64_t)obj->entries[i].value);
                }
                if (obj->entries) free(obj->entries);
                free(obj);
            }
            break;
        }
        default:
            break;
    }
    free(val);
}

// ============================================================================
// JSON Serialization (Stringify)
// ============================================================================

// Forward declaration
static void json_stringify_value(JsonValue* val, char** buf, size_t* len, size_t* cap);

static void json_buf_append(char** buf, size_t* len, size_t* cap, const char* str) {
    size_t slen = strlen(str);
    while (*len + slen + 1 > *cap) {
        *cap = *cap == 0 ? 256 : *cap * 2;
        *buf = (char*)realloc(*buf, *cap);
    }
    memcpy(*buf + *len, str, slen);
    *len += slen;
    (*buf)[*len] = '\0';
}

static void json_buf_append_char(char** buf, size_t* len, size_t* cap, char c) {
    if (*len + 2 > *cap) {
        *cap = *cap == 0 ? 256 : *cap * 2;
        *buf = (char*)realloc(*buf, *cap);
    }
    (*buf)[(*len)++] = c;
    (*buf)[*len] = '\0';
}

static void json_stringify_string(const char* str, char** buf, size_t* len, size_t* cap) {
    json_buf_append_char(buf, len, cap, '"');
    for (const char* p = str; *p; p++) {
        switch (*p) {
            case '"':  json_buf_append(buf, len, cap, "\\\""); break;
            case '\\': json_buf_append(buf, len, cap, "\\\\"); break;
            case '\b': json_buf_append(buf, len, cap, "\\b"); break;
            case '\f': json_buf_append(buf, len, cap, "\\f"); break;
            case '\n': json_buf_append(buf, len, cap, "\\n"); break;
            case '\r': json_buf_append(buf, len, cap, "\\r"); break;
            case '\t': json_buf_append(buf, len, cap, "\\t"); break;
            default:
                if ((unsigned char)*p < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof(esc), "\\u%04x", (unsigned char)*p);
                    json_buf_append(buf, len, cap, esc);
                } else {
                    json_buf_append_char(buf, len, cap, *p);
                }
                break;
        }
    }
    json_buf_append_char(buf, len, cap, '"');
}

static void json_stringify_value(JsonValue* val, char** buf, size_t* len, size_t* cap) {
    if (!val) {
        json_buf_append(buf, len, cap, "null");
        return;
    }

    switch (val->type) {
        case JSON_NULL:
            json_buf_append(buf, len, cap, "null");
            break;

        case JSON_BOOL:
            json_buf_append(buf, len, cap, val->data.bool_val ? "true" : "false");
            break;

        case JSON_NUMBER: {
            char num[64];
            double n = val->data.number_val;
            if (n == (int64_t)n && n >= -9007199254740992.0 && n <= 9007199254740992.0) {
                snprintf(num, sizeof(num), "%lld", (long long)(int64_t)n);
            } else {
                snprintf(num, sizeof(num), "%.17g", n);
            }
            json_buf_append(buf, len, cap, num);
            break;
        }

        case JSON_STRING:
            json_stringify_string(val->data.string_val ? val->data.string_val : "", buf, len, cap);
            break;

        case JSON_ARRAY: {
            json_buf_append_char(buf, len, cap, '[');
            JsonArray* arr = val->data.array_val;
            for (size_t i = 0; i < arr->len; i++) {
                if (i > 0) json_buf_append_char(buf, len, cap, ',');
                json_stringify_value(arr->items[i], buf, len, cap);
            }
            json_buf_append_char(buf, len, cap, ']');
            break;
        }

        case JSON_OBJECT: {
            json_buf_append_char(buf, len, cap, '{');
            JsonObject* obj = val->data.object_val;
            for (size_t i = 0; i < obj->len; i++) {
                if (i > 0) json_buf_append_char(buf, len, cap, ',');
                json_stringify_string(obj->entries[i].key, buf, len, cap);
                json_buf_append_char(buf, len, cap, ':');
                json_stringify_value(obj->entries[i].value, buf, len, cap);
            }
            json_buf_append_char(buf, len, cap, '}');
            break;
        }
    }
}

int64_t json_stringify(int64_t val_ptr) {
    char* buf = NULL;
    size_t len = 0, cap = 0;
    json_stringify_value((JsonValue*)val_ptr, &buf, &len, &cap);
    SxString* result = intrinsic_string_new(buf ? buf : "null");
    if (buf) free(buf);
    return (int64_t)result;
}

// ============================================================================
// JSON Pretty Print
// ============================================================================

static void json_stringify_pretty_value(JsonValue* val, char** buf, size_t* len, size_t* cap, int indent, int depth);

static void json_append_indent(char** buf, size_t* len, size_t* cap, int indent, int depth) {
    for (int i = 0; i < indent * depth; i++) {
        json_buf_append_char(buf, len, cap, ' ');
    }
}

static void json_stringify_pretty_value(JsonValue* val, char** buf, size_t* len, size_t* cap, int indent, int depth) {
    if (!val) {
        json_buf_append(buf, len, cap, "null");
        return;
    }

    switch (val->type) {
        case JSON_NULL:
            json_buf_append(buf, len, cap, "null");
            break;

        case JSON_BOOL:
            json_buf_append(buf, len, cap, val->data.bool_val ? "true" : "false");
            break;

        case JSON_NUMBER: {
            char num[64];
            double n = val->data.number_val;
            if (n == (int64_t)n && n >= -9007199254740992.0 && n <= 9007199254740992.0) {
                snprintf(num, sizeof(num), "%lld", (long long)(int64_t)n);
            } else {
                snprintf(num, sizeof(num), "%.17g", n);
            }
            json_buf_append(buf, len, cap, num);
            break;
        }

        case JSON_STRING:
            json_stringify_string(val->data.string_val ? val->data.string_val : "", buf, len, cap);
            break;

        case JSON_ARRAY: {
            JsonArray* arr = val->data.array_val;
            if (arr->len == 0) {
                json_buf_append(buf, len, cap, "[]");
            } else {
                json_buf_append(buf, len, cap, "[\n");
                for (size_t i = 0; i < arr->len; i++) {
                    json_append_indent(buf, len, cap, indent, depth + 1);
                    json_stringify_pretty_value(arr->items[i], buf, len, cap, indent, depth + 1);
                    if (i < arr->len - 1) json_buf_append_char(buf, len, cap, ',');
                    json_buf_append_char(buf, len, cap, '\n');
                }
                json_append_indent(buf, len, cap, indent, depth);
                json_buf_append_char(buf, len, cap, ']');
            }
            break;
        }

        case JSON_OBJECT: {
            JsonObject* obj = val->data.object_val;
            if (obj->len == 0) {
                json_buf_append(buf, len, cap, "{}");
            } else {
                json_buf_append(buf, len, cap, "{\n");
                for (size_t i = 0; i < obj->len; i++) {
                    json_append_indent(buf, len, cap, indent, depth + 1);
                    json_stringify_string(obj->entries[i].key, buf, len, cap);
                    json_buf_append(buf, len, cap, ": ");
                    json_stringify_pretty_value(obj->entries[i].value, buf, len, cap, indent, depth + 1);
                    if (i < obj->len - 1) json_buf_append_char(buf, len, cap, ',');
                    json_buf_append_char(buf, len, cap, '\n');
                }
                json_append_indent(buf, len, cap, indent, depth);
                json_buf_append_char(buf, len, cap, '}');
            }
            break;
        }
    }
}

int64_t json_stringify_pretty(int64_t val_ptr, int64_t indent) {
    char* buf = NULL;
    size_t len = 0, cap = 0;
    json_stringify_pretty_value((JsonValue*)val_ptr, &buf, &len, &cap, (int)indent, 0);
    SxString* result = intrinsic_string_new(buf ? buf : "null");
    if (buf) free(buf);
    return (int64_t)result;
}

// ============================================================================
// JSON Parsing
// ============================================================================

typedef struct {
    const char* str;
    size_t pos;
    size_t len;
    char* error;
} JsonParser;

static void json_parser_skip_ws(JsonParser* p) {
    while (p->pos < p->len) {
        char c = p->str[p->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            p->pos++;
        } else {
            break;
        }
    }
}

static void json_parser_error(JsonParser* p, const char* msg) {
    if (!p->error) {
        size_t elen = strlen(msg) + 64;
        p->error = (char*)malloc(elen);
        snprintf(p->error, elen, "%s at position %zu", msg, p->pos);
    }
}

static JsonValue* json_parse_value(JsonParser* p);

static JsonValue* json_parse_string(JsonParser* p) {
    if (p->str[p->pos] != '"') {
        json_parser_error(p, "Expected '\"'");
        return NULL;
    }
    p->pos++;  // Skip opening quote

    size_t start = p->pos;
    size_t cap = 64;
    char* str = (char*)malloc(cap);
    size_t len = 0;

    while (p->pos < p->len && p->str[p->pos] != '"') {
        char c = p->str[p->pos];
        if (c == '\\') {
            p->pos++;
            if (p->pos >= p->len) {
                free(str);
                json_parser_error(p, "Unexpected end of string");
                return NULL;
            }
            char esc = p->str[p->pos];
            char decoded;
            switch (esc) {
                case '"': decoded = '"'; break;
                case '\\': decoded = '\\'; break;
                case '/': decoded = '/'; break;
                case 'b': decoded = '\b'; break;
                case 'f': decoded = '\f'; break;
                case 'n': decoded = '\n'; break;
                case 'r': decoded = '\r'; break;
                case 't': decoded = '\t'; break;
                case 'u': {
                    // Parse \uXXXX
                    if (p->pos + 4 >= p->len) {
                        free(str);
                        json_parser_error(p, "Invalid unicode escape");
                        return NULL;
                    }
                    char hex[5] = {p->str[p->pos+1], p->str[p->pos+2], p->str[p->pos+3], p->str[p->pos+4], 0};
                    unsigned int code;
                    if (sscanf(hex, "%x", &code) != 1) {
                        free(str);
                        json_parser_error(p, "Invalid unicode escape");
                        return NULL;
                    }
                    p->pos += 4;
                    // Simple UTF-8 encoding for BMP characters
                    if (code < 0x80) {
                        decoded = (char)code;
                    } else if (code < 0x800) {
                        if (len + 2 >= cap) { cap *= 2; str = (char*)realloc(str, cap); }
                        str[len++] = (char)(0xC0 | (code >> 6));
                        decoded = (char)(0x80 | (code & 0x3F));
                    } else {
                        if (len + 3 >= cap) { cap *= 2; str = (char*)realloc(str, cap); }
                        str[len++] = (char)(0xE0 | (code >> 12));
                        str[len++] = (char)(0x80 | ((code >> 6) & 0x3F));
                        decoded = (char)(0x80 | (code & 0x3F));
                    }
                    break;
                }
                default:
                    free(str);
                    json_parser_error(p, "Invalid escape sequence");
                    return NULL;
            }
            if (len + 1 >= cap) { cap *= 2; str = (char*)realloc(str, cap); }
            str[len++] = decoded;
            p->pos++;
        } else if ((unsigned char)c < 0x20) {
            free(str);
            json_parser_error(p, "Invalid control character in string");
            return NULL;
        } else {
            if (len + 1 >= cap) { cap *= 2; str = (char*)realloc(str, cap); }
            str[len++] = c;
            p->pos++;
        }
    }

    if (p->pos >= p->len) {
        free(str);
        json_parser_error(p, "Unterminated string");
        return NULL;
    }

    p->pos++;  // Skip closing quote
    str[len] = '\0';

    JsonValue* val = (JsonValue*)malloc(sizeof(JsonValue));
    val->type = JSON_STRING;
    val->data.string_val = str;
    return val;
}

static JsonValue* json_parse_number(JsonParser* p) {
    size_t start = p->pos;

    // Optional minus
    if (p->pos < p->len && p->str[p->pos] == '-') p->pos++;

    // Integer part
    if (p->pos < p->len && p->str[p->pos] == '0') {
        p->pos++;
    } else if (p->pos < p->len && p->str[p->pos] >= '1' && p->str[p->pos] <= '9') {
        while (p->pos < p->len && p->str[p->pos] >= '0' && p->str[p->pos] <= '9') p->pos++;
    } else {
        json_parser_error(p, "Invalid number");
        return NULL;
    }

    // Fractional part
    if (p->pos < p->len && p->str[p->pos] == '.') {
        p->pos++;
        if (p->pos >= p->len || p->str[p->pos] < '0' || p->str[p->pos] > '9') {
            json_parser_error(p, "Invalid number");
            return NULL;
        }
        while (p->pos < p->len && p->str[p->pos] >= '0' && p->str[p->pos] <= '9') p->pos++;
    }

    // Exponent part
    if (p->pos < p->len && (p->str[p->pos] == 'e' || p->str[p->pos] == 'E')) {
        p->pos++;
        if (p->pos < p->len && (p->str[p->pos] == '+' || p->str[p->pos] == '-')) p->pos++;
        if (p->pos >= p->len || p->str[p->pos] < '0' || p->str[p->pos] > '9') {
            json_parser_error(p, "Invalid number");
            return NULL;
        }
        while (p->pos < p->len && p->str[p->pos] >= '0' && p->str[p->pos] <= '9') p->pos++;
    }

    // Parse the number
    size_t num_len = p->pos - start;
    char* num_str = (char*)malloc(num_len + 1);
    memcpy(num_str, p->str + start, num_len);
    num_str[num_len] = '\0';

    double num = atof(num_str);
    free(num_str);

    return json_number(num);
}

static JsonValue* json_parse_array(JsonParser* p) {
    p->pos++;  // Skip '['
    json_parser_skip_ws(p);

    JsonValue* arr = json_array();

    if (p->pos < p->len && p->str[p->pos] == ']') {
        p->pos++;
        return arr;
    }

    while (1) {
        json_parser_skip_ws(p);
        JsonValue* item = json_parse_value(p);
        if (!item) {
            json_free((int64_t)arr);
            return NULL;
        }
        json_array_push((int64_t)arr, (int64_t)item);

        json_parser_skip_ws(p);
        if (p->pos >= p->len) {
            json_free((int64_t)arr);
            json_parser_error(p, "Unterminated array");
            return NULL;
        }

        if (p->str[p->pos] == ']') {
            p->pos++;
            return arr;
        }

        if (p->str[p->pos] != ',') {
            json_free((int64_t)arr);
            json_parser_error(p, "Expected ',' or ']'");
            return NULL;
        }
        p->pos++;
    }
}

static JsonValue* json_parse_object(JsonParser* p) {
    p->pos++;  // Skip '{'
    json_parser_skip_ws(p);

    JsonValue* obj = json_object();

    if (p->pos < p->len && p->str[p->pos] == '}') {
        p->pos++;
        return obj;
    }

    while (1) {
        json_parser_skip_ws(p);

        // Parse key
        if (p->pos >= p->len || p->str[p->pos] != '"') {
            json_free((int64_t)obj);
            json_parser_error(p, "Expected string key");
            return NULL;
        }

        JsonValue* key_val = json_parse_string(p);
        if (!key_val) {
            json_free((int64_t)obj);
            return NULL;
        }
        char* key = strdup(key_val->data.string_val);
        json_free((int64_t)key_val);

        json_parser_skip_ws(p);

        // Expect ':'
        if (p->pos >= p->len || p->str[p->pos] != ':') {
            free(key);
            json_free((int64_t)obj);
            json_parser_error(p, "Expected ':'");
            return NULL;
        }
        p->pos++;

        json_parser_skip_ws(p);

        // Parse value
        JsonValue* val = json_parse_value(p);
        if (!val) {
            free(key);
            json_free((int64_t)obj);
            return NULL;
        }

        json_object_set((int64_t)obj, key, (int64_t)val);
        free(key);

        json_parser_skip_ws(p);

        if (p->pos >= p->len) {
            json_free((int64_t)obj);
            json_parser_error(p, "Unterminated object");
            return NULL;
        }

        if (p->str[p->pos] == '}') {
            p->pos++;
            return obj;
        }

        if (p->str[p->pos] != ',') {
            json_free((int64_t)obj);
            json_parser_error(p, "Expected ',' or '}'");
            return NULL;
        }
        p->pos++;
    }
}

static JsonValue* json_parse_value(JsonParser* p) {
    json_parser_skip_ws(p);

    if (p->pos >= p->len) {
        json_parser_error(p, "Unexpected end of input");
        return NULL;
    }

    char c = p->str[p->pos];

    // null
    if (c == 'n') {
        if (p->pos + 4 <= p->len && strncmp(p->str + p->pos, "null", 4) == 0) {
            p->pos += 4;
            return json_null();
        }
        json_parser_error(p, "Invalid value");
        return NULL;
    }

    // true
    if (c == 't') {
        if (p->pos + 4 <= p->len && strncmp(p->str + p->pos, "true", 4) == 0) {
            p->pos += 4;
            return json_bool(1);
        }
        json_parser_error(p, "Invalid value");
        return NULL;
    }

    // false
    if (c == 'f') {
        if (p->pos + 5 <= p->len && strncmp(p->str + p->pos, "false", 5) == 0) {
            p->pos += 5;
            return json_bool(0);
        }
        json_parser_error(p, "Invalid value");
        return NULL;
    }

    // string
    if (c == '"') {
        return json_parse_string(p);
    }

    // number
    if (c == '-' || (c >= '0' && c <= '9')) {
        return json_parse_number(p);
    }

    // array
    if (c == '[') {
        return json_parse_array(p);
    }

    // object
    if (c == '{') {
        return json_parse_object(p);
    }

    json_parser_error(p, "Invalid value");
    return NULL;
}

// Parse JSON string to JsonValue
// Returns Result: Ok(JsonValue*) or Err(error_string)
int64_t json_parse(int64_t str_ptr) {
    SxString* str = (SxString*)str_ptr;
    if (!str || !str->data) {
        return (int64_t)result_err_msg("null input");
    }

    JsonParser parser = {
        .str = str->data,
        .pos = 0,
        .len = str->len,
        .error = NULL
    };

    JsonValue* val = json_parse_value(&parser);

    if (parser.error) {
        SxString* err_str = intrinsic_string_new(parser.error);
        free(parser.error);
        return (int64_t)result_err((int64_t)err_str);
    }

    if (!val) {
        return (int64_t)result_err_msg("parse failed");
    }

    // Check for trailing content
    json_parser_skip_ws(&parser);
    if (parser.pos < parser.len) {
        json_free((int64_t)val);
        return (int64_t)result_err_msg("trailing content after JSON value");
    }

    return (int64_t)result_ok((int64_t)val);
}

// Parse JSON from C string (convenience)
int64_t json_parse_cstr(const char* str) {
    if (!str) {
        return (int64_t)result_err_msg("null input");
    }
    SxString* s = intrinsic_string_new(str);
    int64_t result = json_parse((int64_t)s);
    // Note: s is not freed as it might be needed for error messages
    return result;
}

// Simple json_parse that returns value directly (0 on error)
// This is the version used by Simplex code
int64_t json_parse_simple(int64_t str_ptr) {
    int64_t result = json_parse(str_ptr);
    if (!result) return 0;
    SxResult* res = (SxResult*)result;
    if (res->tag == 0) {
        // Error - return 0
        return 0;
    }
    // Ok - return the value
    return res->value;
}

// ============================================================================
// JSON Convenience Functions
// ============================================================================

// Clone a JSON value (deep copy)
int64_t json_clone(int64_t val_ptr) {
    JsonValue* val = (JsonValue*)val_ptr;
    if (!val) return (int64_t)json_null();

    switch (val->type) {
        case JSON_NULL:
            return (int64_t)json_null();

        case JSON_BOOL:
            return (int64_t)json_bool(val->data.bool_val);

        case JSON_NUMBER:
            return (int64_t)json_number(val->data.number_val);

        case JSON_STRING:
            return (int64_t)json_string(val->data.string_val);

        case JSON_ARRAY: {
            JsonValue* arr = json_array();
            JsonArray* src = val->data.array_val;
            for (size_t i = 0; i < src->len; i++) {
                json_array_push((int64_t)arr, json_clone((int64_t)src->items[i]));
            }
            return (int64_t)arr;
        }

        case JSON_OBJECT: {
            JsonValue* obj = json_object();
            JsonObject* src = val->data.object_val;
            for (size_t i = 0; i < src->len; i++) {
                json_object_set((int64_t)obj, src->entries[i].key,
                               json_clone((int64_t)src->entries[i].value));
            }
            return (int64_t)obj;
        }
    }

    return (int64_t)json_null();
}

// Compare two JSON values for equality
int8_t json_equals(int64_t a_ptr, int64_t b_ptr) {
    JsonValue* a = (JsonValue*)a_ptr;
    JsonValue* b = (JsonValue*)b_ptr;

    if (!a && !b) return 1;
    if (!a || !b) return 0;
    if (a->type != b->type) return 0;

    switch (a->type) {
        case JSON_NULL:
            return 1;

        case JSON_BOOL:
            return a->data.bool_val == b->data.bool_val;

        case JSON_NUMBER:
            return a->data.number_val == b->data.number_val;

        case JSON_STRING:
            return strcmp(a->data.string_val ? a->data.string_val : "",
                         b->data.string_val ? b->data.string_val : "") == 0;

        case JSON_ARRAY: {
            JsonArray* arr_a = a->data.array_val;
            JsonArray* arr_b = b->data.array_val;
            if (arr_a->len != arr_b->len) return 0;
            for (size_t i = 0; i < arr_a->len; i++) {
                if (!json_equals((int64_t)arr_a->items[i], (int64_t)arr_b->items[i])) {
                    return 0;
                }
            }
            return 1;
        }

        case JSON_OBJECT: {
            JsonObject* obj_a = a->data.object_val;
            JsonObject* obj_b = b->data.object_val;
            if (obj_a->len != obj_b->len) return 0;
            for (size_t i = 0; i < obj_a->len; i++) {
                int64_t val_b = json_get((int64_t)b, obj_a->entries[i].key);
                if (!val_b) return 0;
                if (!json_equals((int64_t)obj_a->entries[i].value, val_b)) {
                    return 0;
                }
            }
            return 1;
        }
    }

    return 0;
}

// int64_t wrappers for constructors
int64_t json_null_i64(void) {
    return (int64_t)json_null();
}

int64_t json_bool_i64(int8_t b) {
    return (int64_t)json_bool(b);
}

int64_t json_number_f64(double n) {
    return (int64_t)json_number(n);
}

int64_t json_string_i64(const char* s) {
    return (int64_t)json_string(s);
}

int64_t json_array_i64(void) {
    return (int64_t)json_array();
}

int64_t json_object_i64(void) {
    return (int64_t)json_object();
}

// ============================================================================

// Copy file
int64_t intrinsic_copy_file(SxString* src, SxString* dst) {
    if (!src || !dst || !src->data || !dst->data) return -1;

    FILE* in = fopen(src->data, "rb");
    if (!in) return -1;

    FILE* out = fopen(dst->data, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

// Get user home directory
SxString* intrinsic_get_home_dir(void) {
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    return intrinsic_string_new(home);
}

// ============================================================================
// Phase 22: Async State Machine Runtime
// ============================================================================

// Poll result encoding: 0 = Pending, (value << 1) | 1 = Ready(value)
// This is different from Phase 8's POLL_READY/POLL_PENDING which are just enum values
#define ASYNC_PENDING 0
#define ASYNC_READY(val) (((int64_t)(val) << 1) | 1)
#define ASYNC_IS_READY(poll) ((poll) & 1)
#define ASYNC_GET_VALUE(poll) ((poll) >> 1)

// Future structure (managed by generated async fn code)
// Layout: poll_fn(0), state(1), inner_future(2), result(3), params..., locals...
typedef struct {
    int64_t (*poll_fn)(int64_t);  // Pointer to the generated poll function
    int64_t state;          // Current state in state machine
    int64_t inner_future;   // Future being awaited (if any)
    int64_t result;         // Cached result
    // Followed by: params and locals (variable size)
} AsyncFuture;

// Poll a future - calls the generated poll function
// Returns: 0 = Pending, (value << 1) | 1 = Ready(value)
int64_t future_poll(int64_t future_ptr) {
    if (future_ptr == 0) return ASYNC_READY(0);

    AsyncFuture* f = (AsyncFuture*)future_ptr;

    // State -1 means completed
    if (f->state == -1) {
        return ASYNC_READY(f->result);
    }

    // Call the generated poll function if available
    if (f->poll_fn != NULL) {
        int64_t result = f->poll_fn(future_ptr);
        // Cache the result if ready
        if (ASYNC_IS_READY(result)) {
            f->state = -1;
            f->result = ASYNC_GET_VALUE(result);
        }
        return result;
    }

    // No poll function - return ready with result 0
    f->state = -1;
    return ASYNC_READY(f->result);
}

// Create a ready future
int64_t future_ready(int64_t value) {
    AsyncFuture* f = malloc(sizeof(AsyncFuture));
    f->poll_fn = NULL;  // No poll function - already complete
    f->state = -1;  // Completed
    f->inner_future = 0;
    f->result = value;
    return (int64_t)f;
}

// Create a pending future (for testing)
int64_t future_pending(void) {
    return 0;  // NULL = pending forever
}

// Simple executor_spawn (just returns the future)
int64_t executor_spawn(int64_t future_ptr) {
    return future_ptr;
}

// Simple executor_run (polls until complete)
void executor_run(int64_t main_future) {
    if (main_future == 0) return;

    while (1) {
        int64_t result = future_poll(main_future);
        if (ASYNC_IS_READY(result)) break;
    }
}

// block_on - polls a future until ready and returns the result value
int64_t block_on(int64_t future_ptr) {
    if (future_ptr == 0) return 0;

    while (1) {
        int64_t result = future_poll(future_ptr);
        if (ASYNC_IS_READY(result)) {
            // Extract value from ASYNC_READY encoding (value << 1 | 1)
            return result >> 1;
        }
    }
}

// ============================================================================
// Phase 22.2: Async Combinators
// ============================================================================

// JoinFuture - polls two futures and completes when both are ready
typedef struct {
    int64_t (*poll_fn)(int64_t);
    int64_t state;
    int64_t future1;
    int64_t future2;
    int64_t result1;
    int64_t result2;
} JoinFuture;

int64_t join_poll(int64_t self_ptr) {
    JoinFuture* jf = (JoinFuture*)self_ptr;

    // State: 0=both pending, 1=f1 done, 2=f2 done, -1=both done
    if (jf->state == -1) {
        // Pack both results: result1 in lower 32 bits, result2 in upper 32 bits
        return ASYNC_READY((jf->result2 << 32) | (jf->result1 & 0xFFFFFFFF));
    }

    int f1_ready = (jf->state == 1 || jf->state == -1);
    int f2_ready = (jf->state == 2 || jf->state == -1);

    // Poll future1 if not ready
    if (!f1_ready && jf->future1 != 0) {
        int64_t r1 = future_poll(jf->future1);
        if (ASYNC_IS_READY(r1)) {
            jf->result1 = ASYNC_GET_VALUE(r1);
            f1_ready = 1;
        }
    }

    // Poll future2 if not ready
    if (!f2_ready && jf->future2 != 0) {
        int64_t r2 = future_poll(jf->future2);
        if (ASYNC_IS_READY(r2)) {
            jf->result2 = ASYNC_GET_VALUE(r2);
            f2_ready = 1;
        }
    }

    // Update state
    if (f1_ready && f2_ready) {
        jf->state = -1;
        return ASYNC_READY((jf->result2 << 32) | (jf->result1 & 0xFFFFFFFF));
    } else if (f1_ready) {
        jf->state = 1;
    } else if (f2_ready) {
        jf->state = 2;
    }

    return ASYNC_PENDING;
}

// Create a join future from two futures
int64_t async_join(int64_t future1, int64_t future2) {
    JoinFuture* jf = malloc(sizeof(JoinFuture));
    jf->poll_fn = join_poll;
    jf->state = 0;
    jf->future1 = future1;
    jf->future2 = future2;
    jf->result1 = 0;
    jf->result2 = 0;
    return (int64_t)jf;
}

// Extract result1 from join result
int64_t join_result1(int64_t packed) {
    return (int64_t)(int32_t)(packed & 0xFFFFFFFF);
}

// Extract result2 from join result
int64_t join_result2(int64_t packed) {
    return (int64_t)(packed >> 32);
}

// SelectFuture - polls two futures and completes when first is ready
typedef struct {
    int64_t (*poll_fn)(int64_t);
    int64_t state;
    int64_t future1;
    int64_t future2;
    int64_t result;
    int64_t which;  // 0 = neither, 1 = f1 won, 2 = f2 won
} SelectFuture;

int64_t select_poll(int64_t self_ptr) {
    SelectFuture* sf = (SelectFuture*)self_ptr;

    if (sf->state == -1) {
        // Pack: result in lower 32 bits, which (1 or 2) in upper 32 bits
        return ASYNC_READY((sf->which << 32) | (sf->result & 0xFFFFFFFF));
    }

    // Poll future1
    if (sf->future1 != 0) {
        int64_t r1 = future_poll(sf->future1);
        if (ASYNC_IS_READY(r1)) {
            sf->result = ASYNC_GET_VALUE(r1);
            sf->which = 1;
            sf->state = -1;
            return ASYNC_READY((sf->which << 32) | (sf->result & 0xFFFFFFFF));
        }
    }

    // Poll future2
    if (sf->future2 != 0) {
        int64_t r2 = future_poll(sf->future2);
        if (ASYNC_IS_READY(r2)) {
            sf->result = ASYNC_GET_VALUE(r2);
            sf->which = 2;
            sf->state = -1;
            return ASYNC_READY((sf->which << 32) | (sf->result & 0xFFFFFFFF));
        }
    }

    return ASYNC_PENDING;
}

// Create a select future from two futures
int64_t async_select(int64_t future1, int64_t future2) {
    SelectFuture* sf = malloc(sizeof(SelectFuture));
    sf->poll_fn = select_poll;
    sf->state = 0;
    sf->future1 = future1;
    sf->future2 = future2;
    sf->result = 0;
    sf->which = 0;
    return (int64_t)sf;
}

// Extract result from select result
int64_t select_result(int64_t packed) {
    return (int64_t)(int32_t)(packed & 0xFFFFFFFF);
}

// Extract which future completed (1 or 2)
int64_t select_which(int64_t packed) {
    return (int64_t)(packed >> 32);
}

// TimeoutFuture - races a future against a deadline
typedef struct {
    int64_t (*poll_fn)(int64_t);
    int64_t state;
    int64_t inner_future;
    int64_t deadline_ms;
    int64_t start_time;
    int64_t result;
    int64_t timed_out;  // 0 = not timed out, 1 = timed out
} TimeoutFuture;

// Get current time in milliseconds
int64_t time_now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

int64_t timeout_poll(int64_t self_ptr) {
    TimeoutFuture* tf = (TimeoutFuture*)self_ptr;

    if (tf->state == -1) {
        // Pack: result in lower 32 bits, timed_out flag in bit 32
        return ASYNC_READY((tf->timed_out << 32) | (tf->result & 0xFFFFFFFF));
    }

    // Check timeout
    int64_t now = time_now_ms();
    if (now - tf->start_time >= tf->deadline_ms) {
        tf->timed_out = 1;
        tf->result = 0;
        tf->state = -1;
        return ASYNC_READY((tf->timed_out << 32) | (tf->result & 0xFFFFFFFF));
    }

    // Poll inner future
    if (tf->inner_future != 0) {
        int64_t r = future_poll(tf->inner_future);
        if (ASYNC_IS_READY(r)) {
            tf->result = ASYNC_GET_VALUE(r);
            tf->timed_out = 0;
            tf->state = -1;
            return ASYNC_READY((tf->timed_out << 32) | (tf->result & 0xFFFFFFFF));
        }
    }

    return ASYNC_PENDING;
}

// Create a timeout future
int64_t async_timeout(int64_t inner_future, int64_t timeout_ms) {
    TimeoutFuture* tf = malloc(sizeof(TimeoutFuture));
    tf->poll_fn = timeout_poll;
    tf->state = 0;
    tf->inner_future = inner_future;
    tf->deadline_ms = timeout_ms;
    tf->start_time = time_now_ms();
    tf->result = 0;
    tf->timed_out = 0;
    return (int64_t)tf;
}

// Check if timeout result indicates timeout occurred
int64_t timeout_expired(int64_t packed) {
    return (int64_t)(packed >> 32);
}

// Extract result from timeout result
int64_t timeout_result(int64_t packed) {
    return (int64_t)(int32_t)(packed & 0xFFFFFFFF);
}

// ============================================================================
// Phase 22.3: Pin<T> and Self-Referential Futures
// ============================================================================

// Pin is a marker type that guarantees the pinned value won't move.
// For our purposes, Pin<T> is just a wrapper around a heap-allocated T.
// Once pinned, the value must not be moved in memory.

// PinHeader structure - placed before pinned data
typedef struct {
    int64_t pinned;      // 1 if pinned, 0 if not
    int64_t ref_count;   // Reference count for safety
    int64_t data_size;   // Size of the pinned data
} PinHeader;

// Create a Pin<T> by allocating and copying data
// Returns pointer to the data (PinHeader is just before it)
int64_t pin_new(int64_t data_ptr, int64_t size) {
    // Allocate header + data
    PinHeader* header = malloc(sizeof(PinHeader) + size);
    header->pinned = 1;
    header->ref_count = 1;
    header->data_size = size;

    // Copy data after header
    int8_t* data = (int8_t*)(header + 1);
    if (data_ptr != 0) {
        memcpy(data, (void*)data_ptr, size);
    } else {
        memset(data, 0, size);
    }

    return (int64_t)data;
}

// Create an uninitialized Pin<T> (for in-place construction)
int64_t pin_new_uninit(int64_t size) {
    PinHeader* header = malloc(sizeof(PinHeader) + size);
    header->pinned = 1;
    header->ref_count = 1;
    header->data_size = size;
    int8_t* data = (int8_t*)(header + 1);
    memset(data, 0, size);
    return (int64_t)data;
}

// Get the raw pointer from a Pin<T> (for read access)
int64_t pin_get(int64_t pin_ptr) {
    return pin_ptr;  // Just returns the data pointer
}

// Get mutable access to pinned data (unsafe - caller must ensure no moves)
int64_t pin_get_mut(int64_t pin_ptr) {
    return pin_ptr;
}

// Check if a pointer is pinned
int64_t pin_is_pinned(int64_t pin_ptr) {
    if (pin_ptr == 0) return 0;
    PinHeader* header = ((PinHeader*)pin_ptr) - 1;
    return header->pinned;
}

// Increment pin reference count
void pin_ref(int64_t pin_ptr) {
    if (pin_ptr == 0) return;
    PinHeader* header = ((PinHeader*)pin_ptr) - 1;
    header->ref_count++;
}

// Decrement pin reference count and free if zero
void pin_unref(int64_t pin_ptr) {
    if (pin_ptr == 0) return;
    PinHeader* header = ((PinHeader*)pin_ptr) - 1;
    header->ref_count--;
    if (header->ref_count <= 0) {
        header->pinned = 0;
        free(header);
    }
}

// Self-referential future support
// A self-referential future stores a pointer to itself at a known offset

// Set a self-reference within a pinned future
// pin_ptr: the pinned future
// offset: byte offset where the self-pointer should be stored
void pin_set_self_ref(int64_t pin_ptr, int64_t offset) {
    if (pin_ptr == 0) return;
    int64_t* self_ref = (int64_t*)(pin_ptr + offset);
    *self_ref = pin_ptr;  // Store pointer to self
}

// Validate that a self-reference is still valid
int64_t pin_check_self_ref(int64_t pin_ptr, int64_t offset) {
    if (pin_ptr == 0) return 0;
    int64_t* self_ref = (int64_t*)(pin_ptr + offset);
    return (*self_ref == pin_ptr) ? 1 : 0;
}

// ============================================================================
// Phase 22.5: Function pointer call helpers
// ============================================================================

// Call a function pointer with no arguments
int64_t intrinsic_call0(int64_t fn_ptr) {
    typedef int64_t (*fn_type)(void);
    fn_type fn = (fn_type)fn_ptr;
    return fn();
}

// Call a function pointer with one argument
int64_t intrinsic_call1(int64_t fn_ptr, int64_t arg) {
    typedef int64_t (*fn_type)(int64_t);
    fn_type fn = (fn_type)fn_ptr;
    return fn(arg);
}

// Call a function pointer with two arguments
int64_t intrinsic_call2(int64_t fn_ptr, int64_t a, int64_t b) {
    typedef int64_t (*fn_type)(int64_t, int64_t);
    fn_type fn = (fn_type)fn_ptr;
    return fn(a, b);
}

// Call a function pointer with three arguments
int64_t intrinsic_call3(int64_t fn_ptr, int64_t a, int64_t b, int64_t c) {
    typedef int64_t (*fn_type)(int64_t, int64_t, int64_t);
    fn_type fn = (fn_type)fn_ptr;
    return fn(a, b, c);
}

// ============================================================================
// Phase 22.6: Structured Concurrency
// ============================================================================

// Maximum tasks per scope
#define MAX_SCOPE_TASKS 64

// TaskScope - a container for spawned tasks that must all complete
typedef struct {
    int64_t tasks[MAX_SCOPE_TASKS];  // Array of future pointers
    int64_t results[MAX_SCOPE_TASKS]; // Results of completed tasks
    int64_t count;                   // Number of spawned tasks
    int64_t completed;               // Number of completed tasks
    int64_t cancelled;               // 1 if scope has been cancelled
} TaskScope;

// Create a new task scope
int64_t scope_new(void) {
    TaskScope* scope = malloc(sizeof(TaskScope));
    memset(scope, 0, sizeof(TaskScope));
    return (int64_t)scope;
}

// Spawn a future within a scope
// Returns the task index within the scope
int64_t scope_spawn(int64_t scope_ptr, int64_t future_ptr) {
    TaskScope* scope = (TaskScope*)scope_ptr;
    if (scope->cancelled) return -1;
    if (scope->count >= MAX_SCOPE_TASKS) return -1;

    int64_t idx = scope->count;
    scope->tasks[idx] = future_ptr;
    scope->count++;
    return idx;
}

// Poll all tasks in the scope once
// Returns: 0 = some pending, 1 = all complete, -1 = cancelled
int64_t scope_poll(int64_t scope_ptr) {
    TaskScope* scope = (TaskScope*)scope_ptr;
    if (scope->cancelled) return -1;

    int all_ready = 1;
    for (int64_t i = 0; i < scope->count; i++) {
        if (scope->tasks[i] == 0) continue;  // Already completed

        int64_t result = future_poll(scope->tasks[i]);
        if (ASYNC_IS_READY(result)) {
            scope->results[i] = ASYNC_GET_VALUE(result);
            scope->tasks[i] = 0;  // Mark as completed
            scope->completed++;
        } else {
            all_ready = 0;
        }
    }

    return all_ready ? 1 : 0;
}

// Wait for all tasks in the scope to complete
// Returns: 1 = success, -1 = cancelled
int64_t scope_join(int64_t scope_ptr) {
    TaskScope* scope = (TaskScope*)scope_ptr;

    while (1) {
        int64_t result = scope_poll(scope_ptr);
        if (result == 1) return 1;  // All done
        if (result == -1) return -1; // Cancelled
        // Otherwise continue polling
    }
}

// Get result of a specific task by index
int64_t scope_get_result(int64_t scope_ptr, int64_t idx) {
    TaskScope* scope = (TaskScope*)scope_ptr;
    if (idx < 0 || idx >= scope->count) return 0;
    return scope->results[idx];
}

// Cancel all pending tasks in the scope
void scope_cancel(int64_t scope_ptr) {
    TaskScope* scope = (TaskScope*)scope_ptr;
    scope->cancelled = 1;
}

// Get number of tasks in scope
int64_t scope_count(int64_t scope_ptr) {
    TaskScope* scope = (TaskScope*)scope_ptr;
    return scope->count;
}

// Get number of completed tasks
int64_t scope_completed(int64_t scope_ptr) {
    TaskScope* scope = (TaskScope*)scope_ptr;
    return scope->completed;
}

// Free the scope (tasks should be joined first)
void scope_free(int64_t scope_ptr) {
    free((void*)scope_ptr);
}

// Nursery pattern: create scope, run callback with scope, join all
// callback takes (scope_ptr, user_data) and returns i64
int64_t nursery_run(int64_t callback_fn, int64_t user_data) {
    int64_t scope = scope_new();

    // Call callback with scope
    typedef int64_t (*cb_type)(int64_t, int64_t);
    cb_type cb = (cb_type)callback_fn;
    int64_t result = cb(scope, user_data);

    // Join all spawned tasks
    scope_join(scope);

    // Free scope
    scope_free(scope);

    return result;
}

// =============================================================================
// Phase 24.5: f64 Math Intrinsics
// =============================================================================

#include <math.h>

// Basic arithmetic
double f64_add(double a, double b) { return a + b; }
double f64_sub(double a, double b) { return a - b; }
double f64_mul(double a, double b) { return a * b; }
double f64_div(double a, double b) { return a / b; }
double f64_neg(double x) { return -x; }
double f64_abs(double x) { return fabs(x); }

// Comparison (return i64 boolean)
int64_t f64_eq(double a, double b) { return a == b ? 1 : 0; }
int64_t f64_ne(double a, double b) { return a != b ? 1 : 0; }
int64_t f64_lt(double a, double b) { return a < b ? 1 : 0; }
int64_t f64_le(double a, double b) { return a <= b ? 1 : 0; }
int64_t f64_gt(double a, double b) { return a > b ? 1 : 0; }
int64_t f64_ge(double a, double b) { return a >= b ? 1 : 0; }

// Math functions
double f64_sqrt(double x) { return sqrt(x); }
double f64_pow(double base, double exp) { return pow(base, exp); }
double f64_sin(double x) { return sin(x); }
double f64_cos(double x) { return cos(x); }
double f64_tan(double x) { return tan(x); }
double f64_asin(double x) { return asin(x); }
double f64_acos(double x) { return acos(x); }
double f64_atan(double x) { return atan(x); }
double f64_atan2(double y, double x) { return atan2(y, x); }
double f64_exp(double x) { return exp(x); }
double f64_log(double x) { return log(x); }
double f64_log10(double x) { return log10(x); }
double f64_log2(double x) { return log2(x); }
double f64_floor(double x) { return floor(x); }
double f64_ceil(double x) { return ceil(x); }
double f64_round(double x) { return round(x); }
double f64_trunc(double x) { return trunc(x); }
double f64_min(double a, double b) { return a < b ? a : b; }
double f64_max(double a, double b) { return a > b ? a : b; }

// Conversion
double f64_from_i64(int64_t x) { return (double)x; }
int64_t f64_to_i64(double x) { return (int64_t)x; }

SxString* f64_to_string(double x) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%g", x);
    return intrinsic_string_new(buf);
}

double f64_from_string(SxString* str) {
    if (!str || !str->data) return 0.0;
    return atof(str->data);
}

// Parse float from raw string pointer (for literal support)
int64_t f64_parse(const char* str) {
    if (!str) return 0;
    double val = atof(str);
    // Return f64 bits as i64
    union { double d; int64_t i; } u;
    u.d = val;
    return u.i;
}

// Constants
double f64_pi(void) { return 3.14159265358979323846; }
double f64_e(void) { return 2.71828182845904523536; }
double f64_nan(void) { return NAN; }
double f64_inf(void) { return INFINITY; }
int64_t f64_is_nan(double x) { return isnan(x) ? 1 : 0; }
int64_t f64_is_inf(double x) { return isinf(x) ? 1 : 0; }
int64_t f64_is_finite(double x) { return isfinite(x) ? 1 : 0; }

// =============================================================================
// Phase 24.1: TLS/SSL Support
// =============================================================================

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>

// TLS context wrapper
typedef struct {
    SSL_CTX* ctx;
    int is_server;
} TlsContext;

// TLS connection wrapper
typedef struct {
    SSL* ssl;
    int fd;
    TlsContext* ctx;
} TlsConnection;

// Global initialization (called once)
static int tls_initialized = 0;

static void tls_init(void) {
    if (tls_initialized) return;
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    tls_initialized = 1;
}

// Create a new TLS context for client connections
int64_t tls_context_new_client(void) {
    tls_init();
    
    TlsContext* ctx = malloc(sizeof(TlsContext));
    ctx->ctx = SSL_CTX_new(TLS_client_method());
    ctx->is_server = 0;
    
    if (!ctx->ctx) {
        free(ctx);
        return 0;
    }
    
    // Set reasonable defaults
    SSL_CTX_set_options(ctx->ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
    SSL_CTX_set_mode(ctx->ctx, SSL_MODE_AUTO_RETRY);
    
    return (int64_t)ctx;
}

// Create a new TLS context for server connections
int64_t tls_context_new_server(void) {
    tls_init();
    
    TlsContext* ctx = malloc(sizeof(TlsContext));
    ctx->ctx = SSL_CTX_new(TLS_server_method());
    ctx->is_server = 1;
    
    if (!ctx->ctx) {
        free(ctx);
        return 0;
    }
    
    SSL_CTX_set_options(ctx->ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
    
    return (int64_t)ctx;
}

// Load certificate file (PEM format)
int64_t tls_context_load_cert(int64_t ctx_ptr, int64_t cert_path_ptr) {
    TlsContext* ctx = (TlsContext*)ctx_ptr;
    SxString* path = (SxString*)cert_path_ptr;
    
    if (!ctx || !path || !path->data) return 0;
    
    if (SSL_CTX_use_certificate_file(ctx->ctx, path->data, SSL_FILETYPE_PEM) != 1) {
        return 0;
    }
    return 1;
}

// Load private key file (PEM format)
int64_t tls_context_load_key(int64_t ctx_ptr, int64_t key_path_ptr) {
    TlsContext* ctx = (TlsContext*)ctx_ptr;
    SxString* path = (SxString*)key_path_ptr;
    
    if (!ctx || !path || !path->data) return 0;
    
    if (SSL_CTX_use_PrivateKey_file(ctx->ctx, path->data, SSL_FILETYPE_PEM) != 1) {
        return 0;
    }
    return 1;
}

// Load CA certificates for verification
int64_t tls_context_load_ca(int64_t ctx_ptr, int64_t ca_path_ptr) {
    TlsContext* ctx = (TlsContext*)ctx_ptr;
    SxString* path = (SxString*)ca_path_ptr;
    
    if (!ctx || !path || !path->data) return 0;
    
    if (SSL_CTX_load_verify_locations(ctx->ctx, path->data, NULL) != 1) {
        return 0;
    }
    return 1;
}

// Use system CA certificates
int64_t tls_context_use_system_ca(int64_t ctx_ptr) {
    TlsContext* ctx = (TlsContext*)ctx_ptr;
    if (!ctx) return 0;
    
    if (SSL_CTX_set_default_verify_paths(ctx->ctx) != 1) {
        return 0;
    }
    return 1;
}

// Set verification mode (0=none, 1=peer, 2=fail if no peer cert)
void tls_context_set_verify(int64_t ctx_ptr, int64_t mode) {
    TlsContext* ctx = (TlsContext*)ctx_ptr;
    if (!ctx) return;
    
    int ssl_mode = SSL_VERIFY_NONE;
    if (mode == 1) ssl_mode = SSL_VERIFY_PEER;
    if (mode == 2) ssl_mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    
    SSL_CTX_set_verify(ctx->ctx, ssl_mode, NULL);
}

// Free TLS context
void tls_context_free(int64_t ctx_ptr) {
    TlsContext* ctx = (TlsContext*)ctx_ptr;
    if (!ctx) return;
    
    if (ctx->ctx) SSL_CTX_free(ctx->ctx);
    free(ctx);
}

// Connect to a TLS server (wraps existing socket)
int64_t tls_connect(int64_t ctx_ptr, int64_t fd, int64_t hostname_ptr) {
    TlsContext* ctx = (TlsContext*)ctx_ptr;
    SxString* hostname = (SxString*)hostname_ptr;
    
    if (!ctx || fd < 0) return 0;
    
    TlsConnection* conn = malloc(sizeof(TlsConnection));
    conn->ssl = SSL_new(ctx->ctx);
    conn->fd = (int)fd;
    conn->ctx = ctx;
    
    if (!conn->ssl) {
        free(conn);
        return 0;
    }
    
    // Set hostname for SNI
    if (hostname && hostname->data) {
        SSL_set_tlsext_host_name(conn->ssl, hostname->data);
        // Also set for certificate verification
        SSL_set1_host(conn->ssl, hostname->data);
    }
    
    SSL_set_fd(conn->ssl, conn->fd);
    
    int ret = SSL_connect(conn->ssl);
    if (ret != 1) {
        int err = SSL_get_error(conn->ssl, ret);
        SSL_free(conn->ssl);
        free(conn);
        return -err;  // Return negative error code
    }
    
    return (int64_t)conn;
}

// Accept a TLS connection (server side)
int64_t tls_accept(int64_t ctx_ptr, int64_t fd) {
    TlsContext* ctx = (TlsContext*)ctx_ptr;
    
    if (!ctx || fd < 0) return 0;
    
    TlsConnection* conn = malloc(sizeof(TlsConnection));
    conn->ssl = SSL_new(ctx->ctx);
    conn->fd = (int)fd;
    conn->ctx = ctx;
    
    if (!conn->ssl) {
        free(conn);
        return 0;
    }
    
    SSL_set_fd(conn->ssl, conn->fd);
    
    int ret = SSL_accept(conn->ssl);
    if (ret != 1) {
        int err = SSL_get_error(conn->ssl, ret);
        SSL_free(conn->ssl);
        free(conn);
        return -err;
    }
    
    return (int64_t)conn;
}

// Read from TLS connection
int64_t tls_read(int64_t conn_ptr, int64_t buf_ptr, int64_t len) {
    TlsConnection* conn = (TlsConnection*)conn_ptr;
    if (!conn || !buf_ptr) return -1;
    
    int ret = SSL_read(conn->ssl, (void*)buf_ptr, (int)len);
    if (ret <= 0) {
        int err = SSL_get_error(conn->ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return -11;  // EAGAIN equivalent
        }
        if (err == SSL_ERROR_ZERO_RETURN) {
            return 0;  // Connection closed
        }
        return -1;
    }
    return ret;
}

// Write to TLS connection
int64_t tls_write(int64_t conn_ptr, int64_t buf_ptr, int64_t len) {
    TlsConnection* conn = (TlsConnection*)conn_ptr;
    if (!conn || !buf_ptr) return -1;
    
    int ret = SSL_write(conn->ssl, (void*)buf_ptr, (int)len);
    if (ret <= 0) {
        int err = SSL_get_error(conn->ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return -11;  // EAGAIN equivalent
        }
        return -1;
    }
    return ret;
}

// Shutdown TLS connection gracefully
void tls_shutdown(int64_t conn_ptr) {
    TlsConnection* conn = (TlsConnection*)conn_ptr;
    if (!conn) return;
    
    SSL_shutdown(conn->ssl);
}

// Close and free TLS connection
void tls_close(int64_t conn_ptr) {
    TlsConnection* conn = (TlsConnection*)conn_ptr;
    if (!conn) return;
    
    SSL_shutdown(conn->ssl);
    SSL_free(conn->ssl);
    close(conn->fd);
    free(conn);
}

// Get peer certificate info (returns subject as string)
int64_t tls_peer_cert_subject(int64_t conn_ptr) {
    TlsConnection* conn = (TlsConnection*)conn_ptr;
    if (!conn) return 0;
    
    X509* cert = SSL_get_peer_certificate(conn->ssl);
    if (!cert) return 0;
    
    char buf[256];
    X509_NAME_oneline(X509_get_subject_name(cert), buf, sizeof(buf));
    X509_free(cert);
    
    return (int64_t)intrinsic_string_new(buf);
}

// Check if peer certificate is verified
int64_t tls_peer_verified(int64_t conn_ptr) {
    TlsConnection* conn = (TlsConnection*)conn_ptr;
    if (!conn) return 0;
    
    return SSL_get_verify_result(conn->ssl) == X509_V_OK ? 1 : 0;
}

// Get TLS version string
int64_t tls_version(int64_t conn_ptr) {
    TlsConnection* conn = (TlsConnection*)conn_ptr;
    if (!conn) return 0;
    
    return (int64_t)intrinsic_string_new(SSL_get_version(conn->ssl));
}

// Get cipher name
int64_t tls_cipher(int64_t conn_ptr) {
    TlsConnection* conn = (TlsConnection*)conn_ptr;
    if (!conn) return 0;
    
    return (int64_t)intrinsic_string_new(SSL_get_cipher(conn->ssl));
}

// Get last error message
int64_t tls_error_string(void) {
    char buf[256];
    ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
    return (int64_t)intrinsic_string_new(buf);
}

// Get underlying socket fd from TLS connection
int64_t tls_get_fd(int64_t conn_ptr) {
    TlsConnection* conn = (TlsConnection*)conn_ptr;
    if (!conn) return -1;
    return conn->fd;
}

// =============================================================================
// Phase 24.2: HTTP Client
// =============================================================================

// HTTP Header structure
typedef struct HttpHeader {
    char* name;
    char* value;
    struct HttpHeader* next;
} HttpHeader;

// HTTP Request structure
typedef struct {
    char* method;
    char* url;
    char* host;
    char* path;
    int port;
    int use_tls;
    HttpHeader* headers;
    char* body;
    size_t body_len;
} HttpRequest;

// HTTP Response structure
typedef struct {
    int status_code;
    char* status_text;
    HttpHeader* headers;
    char* body;
    size_t body_len;
    size_t body_cap;
} HttpResponse;

// Create new HTTP request
int64_t http_request_new(int64_t method_ptr, int64_t url_ptr) {
    SxString* method = (SxString*)method_ptr;
    SxString* url = (SxString*)url_ptr;
    
    if (!method || !url || !method->data || !url->data) return 0;
    
    HttpRequest* req = calloc(1, sizeof(HttpRequest));
    req->method = strdup(method->data);
    req->url = strdup(url->data);
    
    // Parse URL: http(s)://host(:port)/path
    char* p = req->url;
    if (strncmp(p, "https://", 8) == 0) {
        req->use_tls = 1;
        req->port = 443;
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        req->use_tls = 0;
        req->port = 80;
        p += 7;
    } else {
        req->use_tls = 0;
        req->port = 80;
    }
    
    // Find host end (either : or / or end)
    char* host_start = p;
    char* port_start = NULL;
    char* path_start = NULL;
    
    while (*p && *p != ':' && *p != '/') p++;
    
    size_t host_len = p - host_start;
    req->host = malloc(host_len + 1);
    memcpy(req->host, host_start, host_len);
    req->host[host_len] = '\0';
    
    if (*p == ':') {
        p++;
        port_start = p;
        while (*p && *p != '/') p++;
        req->port = atoi(port_start);
    }
    
    if (*p == '/') {
        req->path = strdup(p);
    } else {
        req->path = strdup("/");
    }
    
    // Add default headers
    return (int64_t)req;
}

// Add header to request
void http_request_header(int64_t req_ptr, int64_t name_ptr, int64_t value_ptr) {
    HttpRequest* req = (HttpRequest*)req_ptr;
    SxString* name = (SxString*)name_ptr;
    SxString* value = (SxString*)value_ptr;
    
    if (!req || !name || !value || !name->data || !value->data) return;
    
    HttpHeader* h = malloc(sizeof(HttpHeader));
    h->name = strdup(name->data);
    h->value = strdup(value->data);
    h->next = req->headers;
    req->headers = h;
}

// Set request body
void http_request_body(int64_t req_ptr, int64_t body_ptr) {
    HttpRequest* req = (HttpRequest*)req_ptr;
    SxString* body = (SxString*)body_ptr;
    
    if (!req || !body || !body->data) return;
    
    if (req->body) free(req->body);
    req->body = strdup(body->data);
    req->body_len = body->len;
}

// Build HTTP request string
static char* http_build_request(HttpRequest* req) {
    // Calculate size needed
    size_t size = 0;
    size += strlen(req->method) + 1 + strlen(req->path) + 11; // "METHOD path HTTP/1.1\r\n"
    size += 6 + strlen(req->host) + 2; // "Host: host\r\n"
    size += 24; // "Connection: close\r\n"
    
    HttpHeader* h = req->headers;
    while (h) {
        size += strlen(h->name) + 2 + strlen(h->value) + 2;
        h = h->next;
    }
    
    if (req->body && req->body_len > 0) {
        size += 32; // Content-Length header
        size += req->body_len;
    }
    size += 2; // Final \r\n
    size += 1; // null terminator
    
    char* buf = malloc(size + 256); // Extra buffer for safety
    char* p = buf;
    
    // Request line
    p += sprintf(p, "%s %s HTTP/1.1\r\n", req->method, req->path);
    
    // Host header
    p += sprintf(p, "Host: %s\r\n", req->host);
    
    // Connection close (simple, no keep-alive)
    p += sprintf(p, "Connection: close\r\n");
    
    // User headers
    h = req->headers;
    while (h) {
        p += sprintf(p, "%s: %s\r\n", h->name, h->value);
        h = h->next;
    }
    
    // Content-Length if body
    if (req->body && req->body_len > 0) {
        p += sprintf(p, "Content-Length: %zu\r\n", req->body_len);
    }
    
    // End headers
    p += sprintf(p, "\r\n");
    
    // Body
    if (req->body && req->body_len > 0) {
        memcpy(p, req->body, req->body_len);
        p += req->body_len;
    }
    
    *p = '\0';
    return buf;
}

// Parse HTTP response
static HttpResponse* http_parse_response(char* data, size_t len) {
    HttpResponse* resp = calloc(1, sizeof(HttpResponse));
    
    char* p = data;
    char* end = data + len;
    
    // Parse status line: HTTP/1.1 200 OK\r\n
    if (strncmp(p, "HTTP/1.", 7) != 0) {
        free(resp);
        return NULL;
    }
    p += 9; // Skip "HTTP/1.x "
    
    resp->status_code = atoi(p);
    while (p < end && *p != ' ' && *p != '\r') p++;
    if (*p == ' ') p++;
    
    char* status_start = p;
    while (p < end && *p != '\r') p++;
    size_t status_len = p - status_start;
    resp->status_text = malloc(status_len + 1);
    memcpy(resp->status_text, status_start, status_len);
    resp->status_text[status_len] = '\0';
    
    if (p + 2 <= end && *p == '\r' && *(p+1) == '\n') p += 2;
    
    // Parse headers
    while (p < end && !(*p == '\r' && *(p+1) == '\n')) {
        char* name_start = p;
        while (p < end && *p != ':') p++;
        size_t name_len = p - name_start;
        
        if (*p == ':') p++;
        while (p < end && *p == ' ') p++;
        
        char* value_start = p;
        while (p < end && *p != '\r') p++;
        size_t value_len = p - value_start;
        
        HttpHeader* h = malloc(sizeof(HttpHeader));
        h->name = malloc(name_len + 1);
        memcpy(h->name, name_start, name_len);
        h->name[name_len] = '\0';
        
        h->value = malloc(value_len + 1);
        memcpy(h->value, value_start, value_len);
        h->value[value_len] = '\0';
        
        h->next = resp->headers;
        resp->headers = h;
        
        if (p + 2 <= end && *p == '\r' && *(p+1) == '\n') p += 2;
    }
    
    // Skip final \r\n
    if (p + 2 <= end && *p == '\r' && *(p+1) == '\n') p += 2;
    
    // Body is the rest
    if (p < end) {
        resp->body_len = end - p;
        resp->body = malloc(resp->body_len + 1);
        memcpy(resp->body, p, resp->body_len);
        resp->body[resp->body_len] = '\0';
    }
    
    return resp;
}

// Send HTTP request and get response
int64_t http_request_send(int64_t req_ptr) {
    HttpRequest* req = (HttpRequest*)req_ptr;
    if (!req) return 0;
    
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;
    
    // Resolve hostname
    struct hostent* host = gethostbyname(req->host);
    if (!host) {
        close(sock);
        return 0;
    }
    
    // Connect
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(req->port);
    memcpy(&addr.sin_addr, host->h_addr_list[0], host->h_length);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return 0;
    }
    
    // Build request
    char* request_str = http_build_request(req);
    size_t request_len = strlen(request_str);
    
    // Send (with or without TLS)
    int64_t tls_conn = 0;
    if (req->use_tls) {
        int64_t ctx = tls_context_new_client();
        tls_context_use_system_ca(ctx);
        SxString hostname = { .data = req->host, .len = strlen(req->host), .cap = strlen(req->host) + 1 };
        tls_conn = tls_connect(ctx, sock, (int64_t)&hostname);
        if (tls_conn <= 0) {
            free(request_str);
            close(sock);
            tls_context_free(ctx);
            return 0;
        }
        tls_write(tls_conn, (int64_t)request_str, request_len);
    } else {
        if (write(sock, request_str, request_len) < 0) {
            free(request_str);
            close(sock);
            return 0;
        }
    }
    free(request_str);
    
    // Read response
    size_t buf_size = 4096;
    size_t buf_used = 0;
    char* buf = malloc(buf_size);
    
    while (1) {
        if (buf_used + 1024 > buf_size) {
            buf_size *= 2;
            buf = realloc(buf, buf_size);
        }
        
        ssize_t n;
        if (req->use_tls) {
            n = tls_read(tls_conn, (int64_t)(buf + buf_used), 1024);
        } else {
            n = read(sock, buf + buf_used, 1024);
        }
        
        if (n <= 0) break;
        buf_used += n;
    }
    buf[buf_used] = '\0';
    
    // Cleanup connection
    if (req->use_tls && tls_conn > 0) {
        tls_close(tls_conn);
    } else {
        close(sock);
    }
    
    // Parse response
    HttpResponse* resp = http_parse_response(buf, buf_used);
    free(buf);
    
    return (int64_t)resp;
}

// Get response status code
int64_t http_response_status(int64_t resp_ptr) {
    HttpResponse* resp = (HttpResponse*)resp_ptr;
    return resp ? resp->status_code : 0;
}

// Get response status text
int64_t http_response_status_text(int64_t resp_ptr) {
    HttpResponse* resp = (HttpResponse*)resp_ptr;
    if (!resp || !resp->status_text) return 0;
    return (int64_t)intrinsic_string_new(resp->status_text);
}

// Get response header by name
int64_t http_response_header(int64_t resp_ptr, int64_t name_ptr) {
    HttpResponse* resp = (HttpResponse*)resp_ptr;
    SxString* name = (SxString*)name_ptr;
    
    if (!resp || !name || !name->data) return 0;
    
    HttpHeader* h = resp->headers;
    while (h) {
        if (strcasecmp(h->name, name->data) == 0) {
            return (int64_t)intrinsic_string_new(h->value);
        }
        h = h->next;
    }
    return 0;
}

// Get response body
int64_t http_response_body(int64_t resp_ptr) {
    HttpResponse* resp = (HttpResponse*)resp_ptr;
    if (!resp || !resp->body) return 0;
    return (int64_t)intrinsic_string_new(resp->body);
}

// Get response body length
int64_t http_response_body_len(int64_t resp_ptr) {
    HttpResponse* resp = (HttpResponse*)resp_ptr;
    return resp ? (int64_t)resp->body_len : 0;
}

// Free request
void http_request_free(int64_t req_ptr) {
    HttpRequest* req = (HttpRequest*)req_ptr;
    if (!req) return;
    
    free(req->method);
    free(req->url);
    free(req->host);
    free(req->path);
    free(req->body);
    
    HttpHeader* h = req->headers;
    while (h) {
        HttpHeader* next = h->next;
        free(h->name);
        free(h->value);
        free(h);
        h = next;
    }
    free(req);
}

// Free response
void http_response_free(int64_t resp_ptr) {
    HttpResponse* resp = (HttpResponse*)resp_ptr;
    if (!resp) return;
    
    free(resp->status_text);
    free(resp->body);
    
    HttpHeader* h = resp->headers;
    while (h) {
        HttpHeader* next = h->next;
        free(h->name);
        free(h->value);
        free(h);
        h = next;
    }
    free(resp);
}

// Convenience: Simple GET request
int64_t http_get(int64_t url_ptr) {
    SxString method = { .data = "GET", .len = 3, .cap = 4 };
    int64_t req = http_request_new((int64_t)&method, url_ptr);
    if (!req) return 0;
    
    int64_t resp = http_request_send(req);
    http_request_free(req);
    return resp;
}

// Convenience: Simple POST request
int64_t http_post(int64_t url_ptr, int64_t body_ptr) {
    SxString method = { .data = "POST", .len = 4, .cap = 5 };
    int64_t req = http_request_new((int64_t)&method, url_ptr);
    if (!req) return 0;
    
    http_request_body(req, body_ptr);
    
    SxString ct_name = { .data = "Content-Type", .len = 12, .cap = 13 };
    SxString ct_value = { .data = "application/x-www-form-urlencoded", .len = 33, .cap = 34 };
    http_request_header(req, (int64_t)&ct_name, (int64_t)&ct_value);
    
    int64_t resp = http_request_send(req);
    http_request_free(req);
    return resp;
}

// =============================================================================
// Phase 24.3: HTTP Server
// =============================================================================

// Route handler function type
typedef int64_t (*HttpHandler)(int64_t req_ptr, int64_t resp_ptr);

// Route entry
typedef struct HttpRoute {
    char* method;
    char* path;
    int64_t handler_fn;  // Function pointer as i64
    struct HttpRoute* next;
} HttpRoute;

// HTTP Server structure
typedef struct {
    int listen_fd;
    int port;
    HttpRoute* routes;
    int running;
    int64_t tls_ctx;  // Optional TLS context
} HttpServer;

// Server response builder
typedef struct {
    int status_code;
    char* status_text;
    HttpHeader* headers;
    char* body;
    size_t body_len;
} HttpServerResponse;

// Create new HTTP server
int64_t http_server_new(int64_t port) {
    HttpServer* server = calloc(1, sizeof(HttpServer));
    server->port = (int)port;
    server->listen_fd = -1;
    return (int64_t)server;
}

// Enable TLS on server
int64_t http_server_tls(int64_t server_ptr, int64_t cert_path_ptr, int64_t key_path_ptr) {
    HttpServer* server = (HttpServer*)server_ptr;
    if (!server) return 0;
    
    server->tls_ctx = tls_context_new_server();
    if (!server->tls_ctx) return 0;
    
    if (!tls_context_load_cert(server->tls_ctx, cert_path_ptr)) {
        tls_context_free(server->tls_ctx);
        server->tls_ctx = 0;
        return 0;
    }
    
    if (!tls_context_load_key(server->tls_ctx, key_path_ptr)) {
        tls_context_free(server->tls_ctx);
        server->tls_ctx = 0;
        return 0;
    }
    
    return 1;
}

// Add route to server
int64_t http_server_route(int64_t server_ptr, int64_t method_ptr, int64_t path_ptr, int64_t handler_fn) {
    HttpServer* server = (HttpServer*)server_ptr;
    SxString* method = (SxString*)method_ptr;
    SxString* path = (SxString*)path_ptr;

    if (!server || !method || !path || !method->data || !path->data) return 0;

    HttpRoute* route = malloc(sizeof(HttpRoute));
    route->method = strdup(method->data);
    route->path = strdup(path->data);
    route->handler_fn = handler_fn;
    route->next = server->routes;
    server->routes = route;
    return 1;
}

// Create server response
int64_t http_server_response_new(void) {
    HttpServerResponse* resp = calloc(1, sizeof(HttpServerResponse));
    resp->status_code = 200;
    resp->status_text = strdup("OK");
    return (int64_t)resp;
}

// Set response status
int64_t http_server_response_status(int64_t resp_ptr, int64_t code, int64_t text_ptr) {
    HttpServerResponse* resp = (HttpServerResponse*)resp_ptr;
    SxString* text = (SxString*)text_ptr;

    if (!resp) return 0;

    resp->status_code = (int)code;
    if (resp->status_text) free(resp->status_text);
    resp->status_text = text && text->data ? strdup(text->data) : strdup("OK");
    return 1;
}

// Add response header
int64_t http_server_response_header(int64_t resp_ptr, int64_t name_ptr, int64_t value_ptr) {
    HttpServerResponse* resp = (HttpServerResponse*)resp_ptr;
    SxString* name = (SxString*)name_ptr;
    SxString* value = (SxString*)value_ptr;

    if (!resp || !name || !value || !name->data || !value->data) return 0;

    HttpHeader* h = malloc(sizeof(HttpHeader));
    h->name = strdup(name->data);
    h->value = strdup(value->data);
    h->next = resp->headers;
    resp->headers = h;
    return 1;
}

// Set response body
int64_t http_server_response_body(int64_t resp_ptr, int64_t body_ptr) {
    HttpServerResponse* resp = (HttpServerResponse*)resp_ptr;
    SxString* body = (SxString*)body_ptr;

    if (!resp) return 0;

    if (resp->body) free(resp->body);
    if (body && body->data) {
        resp->body = strdup(body->data);
        resp->body_len = body->len;
    } else {
        resp->body = NULL;
        resp->body_len = 0;
    }
    return 1;
}

// Build HTTP response string
static char* http_build_response(HttpServerResponse* resp) {
    size_t size = 64;  // Status line
    size += 32;  // Content-Length
    size += 24;  // Connection header
    
    HttpHeader* h = resp->headers;
    while (h) {
        size += strlen(h->name) + strlen(h->value) + 4;
        h = h->next;
    }
    size += resp->body_len + 4;
    
    char* buf = malloc(size + 256);
    char* p = buf;
    
    // Status line
    p += sprintf(p, "HTTP/1.1 %d %s\r\n", resp->status_code, resp->status_text);
    
    // Headers
    h = resp->headers;
    while (h) {
        p += sprintf(p, "%s: %s\r\n", h->name, h->value);
        h = h->next;
    }
    
    // Content-Length
    p += sprintf(p, "Content-Length: %zu\r\n", resp->body_len);
    p += sprintf(p, "Connection: close\r\n");
    
    // End headers
    p += sprintf(p, "\r\n");
    
    // Body
    if (resp->body && resp->body_len > 0) {
        memcpy(p, resp->body, resp->body_len);
        p += resp->body_len;
    }
    *p = '\0';
    
    return buf;
}

// Parse incoming HTTP request
static HttpRequest* http_parse_request(char* data, size_t len) {
    HttpRequest* req = calloc(1, sizeof(HttpRequest));
    
    char* p = data;
    char* end = data + len;
    
    // Parse request line: METHOD /path HTTP/1.1\r\n
    char* method_start = p;
    while (p < end && *p != ' ') p++;
    size_t method_len = p - method_start;
    req->method = malloc(method_len + 1);
    memcpy(req->method, method_start, method_len);
    req->method[method_len] = '\0';
    
    if (*p == ' ') p++;
    
    char* path_start = p;
    while (p < end && *p != ' ' && *p != '?') p++;
    size_t path_len = p - path_start;
    req->path = malloc(path_len + 1);
    memcpy(req->path, path_start, path_len);
    req->path[path_len] = '\0';
    
    // Skip to end of line
    while (p < end && *p != '\r') p++;
    if (p + 2 <= end) p += 2;
    
    // Parse headers
    while (p < end && !(*p == '\r' && *(p+1) == '\n')) {
        char* name_start = p;
        while (p < end && *p != ':') p++;
        size_t name_len = p - name_start;
        
        if (*p == ':') p++;
        while (p < end && *p == ' ') p++;
        
        char* value_start = p;
        while (p < end && *p != '\r') p++;
        size_t value_len = p - value_start;
        
        HttpHeader* h = malloc(sizeof(HttpHeader));
        h->name = malloc(name_len + 1);
        memcpy(h->name, name_start, name_len);
        h->name[name_len] = '\0';
        
        h->value = malloc(value_len + 1);
        memcpy(h->value, value_start, value_len);
        h->value[value_len] = '\0';
        
        // Check for Host header
        if (strcasecmp(h->name, "Host") == 0) {
            req->host = strdup(h->value);
        }
        
        h->next = req->headers;
        req->headers = h;
        
        if (p + 2 <= end) p += 2;
    }
    
    // Skip final \r\n
    if (p + 2 <= end) p += 2;
    
    // Body
    if (p < end) {
        req->body_len = end - p;
        req->body = malloc(req->body_len + 1);
        memcpy(req->body, p, req->body_len);
        req->body[req->body_len] = '\0';
    }
    
    return req;
}

// Match route (simple exact match for now)
static HttpRoute* http_match_route(HttpServer* server, const char* method, const char* path) {
    HttpRoute* route = server->routes;
    while (route) {
        if (strcmp(route->method, method) == 0 && strcmp(route->path, path) == 0) {
            return route;
        }
        // Also try wildcard "*" method
        if (strcmp(route->method, "*") == 0 && strcmp(route->path, path) == 0) {
            return route;
        }
        route = route->next;
    }
    return NULL;
}

// Handle single client connection
static void http_handle_client(HttpServer* server, int client_fd) {
    // Read request
    char buf[8192];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    buf[n] = '\0';
    
    // Parse request
    HttpRequest* req = http_parse_request(buf, n);
    if (!req) {
        close(client_fd);
        return;
    }
    
    // Create response
    HttpServerResponse* resp = (HttpServerResponse*)http_server_response_new();
    
    // Find route
    HttpRoute* route = http_match_route(server, req->method, req->path);
    
    if (route) {
        // Call handler: handler(req, resp) -> status
        typedef int64_t (*HandlerFn)(int64_t, int64_t);
        HandlerFn handler = (HandlerFn)route->handler_fn;
        handler((int64_t)req, (int64_t)resp);
    } else {
        // 404 Not Found
        resp->status_code = 404;
        free(resp->status_text);
        resp->status_text = strdup("Not Found");
        resp->body = strdup("404 Not Found");
        resp->body_len = 13;
    }
    
    // Build and send response
    char* response_str = http_build_response(resp);
    write(client_fd, response_str, strlen(response_str));
    free(response_str);
    
    // Cleanup
    http_request_free((int64_t)req);
    
    // Free response
    free(resp->status_text);
    free(resp->body);
    HttpHeader* h = resp->headers;
    while (h) {
        HttpHeader* next = h->next;
        free(h->name);
        free(h->value);
        free(h);
        h = next;
    }
    free(resp);
    
    close(client_fd);
}

// Bind server to port
int64_t http_server_bind(int64_t server_ptr) {
    HttpServer* server = (HttpServer*)server_ptr;
    if (!server) return 0;
    
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) return 0;
    
    int opt = 1;
    setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(server->port);
    
    if (bind(server->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server->listen_fd);
        server->listen_fd = -1;
        return 0;
    }
    
    if (listen(server->listen_fd, 128) < 0) {
        close(server->listen_fd);
        server->listen_fd = -1;
        return 0;
    }
    
    return 1;
}

// Accept one connection and handle it (non-blocking style)
int64_t http_server_accept_one(int64_t server_ptr) {
    HttpServer* server = (HttpServer*)server_ptr;
    if (!server || server->listen_fd < 0) return 0;
    
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(server->listen_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) return 0;
    
    http_handle_client(server, client_fd);
    return 1;
}

// Run server (blocking, handles N requests then returns)
int64_t http_server_run(int64_t server_ptr, int64_t max_requests) {
    HttpServer* server = (HttpServer*)server_ptr;
    if (!server || server->listen_fd < 0) return 0;
    
    server->running = 1;
    int64_t handled = 0;
    
    while (server->running && (max_requests == 0 || handled < max_requests)) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server->listen_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) continue;
        
        http_handle_client(server, client_fd);
        handled++;
    }
    
    return handled;
}

// Stop server
int64_t http_server_stop(int64_t server_ptr) {
    HttpServer* server = (HttpServer*)server_ptr;
    if (server) server->running = 0;
    return 1;
}

// Close server
int64_t http_server_close(int64_t server_ptr) {
    HttpServer* server = (HttpServer*)server_ptr;
    if (!server) return 0;

    if (server->listen_fd >= 0) {
        close(server->listen_fd);
    }

    // Free routes
    HttpRoute* route = server->routes;
    while (route) {
        HttpRoute* next = route->next;
        free(route->method);
        free(route->path);
        free(route);
        route = next;
    }

    if (server->tls_ctx) {
        tls_context_free(server->tls_ctx);
    }

    free(server);
    return 1;
}

// Get server port
int64_t http_server_port(int64_t server_ptr) {
    HttpServer* server = (HttpServer*)server_ptr;
    return server ? server->port : 0;
}

// Get request method
int64_t http_server_request_method(int64_t req_ptr) {
    HttpRequest* req = (HttpRequest*)req_ptr;
    if (!req || !req->method) return 0;
    return (int64_t)intrinsic_string_new(req->method);
}

// Get request path
int64_t http_server_request_path(int64_t req_ptr) {
    HttpRequest* req = (HttpRequest*)req_ptr;
    if (!req || !req->path) return 0;
    return (int64_t)intrinsic_string_new(req->path);
}

// Get request header
int64_t http_server_request_header(int64_t req_ptr, int64_t name_ptr) {
    HttpRequest* req = (HttpRequest*)req_ptr;
    SxString* name = (SxString*)name_ptr;
    
    if (!req || !name || !name->data) return 0;
    
    HttpHeader* h = req->headers;
    while (h) {
        if (strcasecmp(h->name, name->data) == 0) {
            return (int64_t)intrinsic_string_new(h->value);
        }
        h = h->next;
    }
    return 0;
}

// Get request body
int64_t http_server_request_body(int64_t req_ptr) {
    HttpRequest* req = (HttpRequest*)req_ptr;
    if (!req || !req->body) return 0;
    return (int64_t)intrinsic_string_new(req->body);
}

// =============================================================================
// Phase 24.4: WebSocket Support
// =============================================================================

#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

// WebSocket opcodes
#define WS_OPCODE_CONTINUATION 0x0
#define WS_OPCODE_TEXT         0x1
#define WS_OPCODE_BINARY       0x2
#define WS_OPCODE_CLOSE        0x8
#define WS_OPCODE_PING         0x9
#define WS_OPCODE_PONG         0xA

// WebSocket connection state
typedef struct {
    int fd;
    int is_client;
    int connected;
    int64_t tls_conn;  // Optional TLS
} WebSocketConn;

// Base64 encode
static char* base64_encode(const unsigned char* data, size_t len) {
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);
    BIO_write(bio, data, len);
    BIO_flush(bio);
    
    BUF_MEM* bufferPtr;
    BIO_get_mem_ptr(bio, &bufferPtr);
    
    char* result = malloc(bufferPtr->length + 1);
    memcpy(result, bufferPtr->data, bufferPtr->length);
    result[bufferPtr->length] = '\0';
    
    BIO_free_all(bio);
    return result;
}

// Generate WebSocket accept key from client key
static char* ws_generate_accept_key(const char* client_key) {
    const char* magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    
    size_t key_len = strlen(client_key);
    size_t magic_len = strlen(magic);
    char* concat = malloc(key_len + magic_len + 1);
    memcpy(concat, client_key, key_len);
    memcpy(concat + key_len, magic, magic_len);
    concat[key_len + magic_len] = '\0';
    
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)concat, key_len + magic_len, hash);
    free(concat);
    
    return base64_encode(hash, SHA_DIGEST_LENGTH);
}

// Generate random WebSocket key for client
static char* ws_generate_client_key(void) {
    unsigned char bytes[16];
    for (int i = 0; i < 16; i++) {
        bytes[i] = rand() % 256;
    }
    return base64_encode(bytes, 16);
}

// Create WebSocket client connection
int64_t ws_connect(int64_t url_ptr) {
    SxString* url = (SxString*)url_ptr;
    if (!url || !url->data) return 0;
    
    // Parse URL: ws(s)://host(:port)/path
    char* p = url->data;
    int use_tls = 0;
    int port = 80;
    
    if (strncmp(p, "wss://", 6) == 0) {
        use_tls = 1;
        port = 443;
        p += 6;
    } else if (strncmp(p, "ws://", 5) == 0) {
        use_tls = 0;
        port = 80;
        p += 5;
    } else {
        return 0;
    }
    
    // Extract host
    char* host_start = p;
    while (*p && *p != ':' && *p != '/') p++;
    size_t host_len = p - host_start;
    char* host = malloc(host_len + 1);
    memcpy(host, host_start, host_len);
    host[host_len] = '\0';
    
    // Optional port
    if (*p == ':') {
        p++;
        port = atoi(p);
        while (*p && *p != '/') p++;
    }
    
    // Path (default to /)
    char* path = (*p == '/') ? strdup(p) : strdup("/");
    
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        free(host);
        free(path);
        return 0;
    }
    
    // Resolve and connect
    struct hostent* he = gethostbyname(host);
    if (!he) {
        close(sock);
        free(host);
        free(path);
        return 0;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        free(host);
        free(path);
        return 0;
    }
    
    // Create connection object
    WebSocketConn* ws = calloc(1, sizeof(WebSocketConn));
    ws->fd = sock;
    ws->is_client = 1;
    
    // TLS if needed
    if (use_tls) {
        int64_t ctx = tls_context_new_client();
        tls_context_use_system_ca(ctx);
        SxString hostname = { .data = host, .len = host_len, .cap = host_len + 1 };
        ws->tls_conn = tls_connect(ctx, sock, (int64_t)&hostname);
        if (ws->tls_conn <= 0) {
            close(sock);
            free(ws);
            free(host);
            free(path);
            return 0;
        }
    }
    
    // Generate WebSocket key
    char* key = ws_generate_client_key();
    
    // Send upgrade request
    char request[1024];
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        path, host, key);
    
    if (ws->tls_conn) {
        tls_write(ws->tls_conn, (int64_t)request, strlen(request));
    } else {
        write(sock, request, strlen(request));
    }
    
    // Read response
    char response[1024];
    ssize_t n;
    if (ws->tls_conn) {
        n = tls_read(ws->tls_conn, (int64_t)response, sizeof(response) - 1);
    } else {
        n = read(sock, response, sizeof(response) - 1);
    }
    
    if (n <= 0) {
        if (ws->tls_conn) tls_close(ws->tls_conn);
        else close(sock);
        free(ws);
        free(host);
        free(path);
        free(key);
        return 0;
    }
    response[n] = '\0';
    
    // Verify response (simple check for 101)
    if (strstr(response, "101") == NULL) {
        if (ws->tls_conn) tls_close(ws->tls_conn);
        else close(sock);
        free(ws);
        free(host);
        free(path);
        free(key);
        return 0;
    }
    
    ws->connected = 1;
    free(host);
    free(path);
    free(key);
    
    return (int64_t)ws;
}

// Accept WebSocket upgrade from HTTP request
int64_t ws_accept(int64_t fd, int64_t key_ptr) {
    SxString* key = (SxString*)key_ptr;
    if (fd < 0 || !key || !key->data) return 0;
    
    // Generate accept key
    char* accept_key = ws_generate_accept_key(key->data);
    
    // Send upgrade response
    char response[512];
    snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept_key);
    free(accept_key);
    
    if (write((int)fd, response, strlen(response)) < 0) {
        return 0;
    }
    
    WebSocketConn* ws = calloc(1, sizeof(WebSocketConn));
    ws->fd = (int)fd;
    ws->is_client = 0;
    ws->connected = 1;
    
    return (int64_t)ws;
}

// Send WebSocket frame
static int ws_send_frame(WebSocketConn* ws, int opcode, const void* data, size_t len, int mask) {
    unsigned char header[14];
    size_t header_len = 2;
    
    // FIN + opcode
    header[0] = 0x80 | (opcode & 0x0F);
    
    // Mask bit + payload length
    if (len < 126) {
        header[1] = (mask ? 0x80 : 0) | len;
    } else if (len < 65536) {
        header[1] = (mask ? 0x80 : 0) | 126;
        header[2] = (len >> 8) & 0xFF;
        header[3] = len & 0xFF;
        header_len = 4;
    } else {
        header[1] = (mask ? 0x80 : 0) | 127;
        header[2] = 0;
        header[3] = 0;
        header[4] = 0;
        header[5] = 0;
        header[6] = (len >> 24) & 0xFF;
        header[7] = (len >> 16) & 0xFF;
        header[8] = (len >> 8) & 0xFF;
        header[9] = len & 0xFF;
        header_len = 10;
    }
    
    // Masking key (for client)
    unsigned char mask_key[4] = {0, 0, 0, 0};
    if (mask) {
        for (int i = 0; i < 4; i++) {
            mask_key[i] = rand() % 256;
            header[header_len++] = mask_key[i];
        }
    }
    
    // Send header
    ssize_t sent;
    if (ws->tls_conn) {
        sent = tls_write(ws->tls_conn, (int64_t)header, header_len);
    } else {
        sent = write(ws->fd, header, header_len);
    }
    if (sent < 0) return -1;
    
    // Send payload (masked if client)
    if (len > 0) {
        if (mask) {
            unsigned char* masked = malloc(len);
            for (size_t i = 0; i < len; i++) {
                masked[i] = ((unsigned char*)data)[i] ^ mask_key[i % 4];
            }
            if (ws->tls_conn) {
                sent = tls_write(ws->tls_conn, (int64_t)masked, len);
            } else {
                sent = write(ws->fd, masked, len);
            }
            free(masked);
        } else {
            if (ws->tls_conn) {
                sent = tls_write(ws->tls_conn, (int64_t)data, len);
            } else {
                sent = write(ws->fd, data, len);
            }
        }
        if (sent < 0) return -1;
    }
    
    return 0;
}

// Send text message
int64_t ws_send_text(int64_t ws_ptr, int64_t msg_ptr) {
    WebSocketConn* ws = (WebSocketConn*)ws_ptr;
    SxString* msg = (SxString*)msg_ptr;
    
    if (!ws || !ws->connected || !msg || !msg->data) return 0;
    
    int mask = ws->is_client ? 1 : 0;
    if (ws_send_frame(ws, WS_OPCODE_TEXT, msg->data, msg->len, mask) < 0) {
        return 0;
    }
    return 1;
}

// Send binary message
int64_t ws_send_binary(int64_t ws_ptr, int64_t data_ptr, int64_t len) {
    WebSocketConn* ws = (WebSocketConn*)ws_ptr;
    
    if (!ws || !ws->connected || !data_ptr) return 0;
    
    int mask = ws->is_client ? 1 : 0;
    if (ws_send_frame(ws, WS_OPCODE_BINARY, (void*)data_ptr, len, mask) < 0) {
        return 0;
    }
    return 1;
}

// Send ping
int64_t ws_ping(int64_t ws_ptr) {
    WebSocketConn* ws = (WebSocketConn*)ws_ptr;
    if (!ws || !ws->connected) return 0;
    
    int mask = ws->is_client ? 1 : 0;
    return ws_send_frame(ws, WS_OPCODE_PING, NULL, 0, mask) == 0 ? 1 : 0;
}

// Send pong
int64_t ws_pong(int64_t ws_ptr) {
    WebSocketConn* ws = (WebSocketConn*)ws_ptr;
    if (!ws || !ws->connected) return 0;
    
    int mask = ws->is_client ? 1 : 0;
    return ws_send_frame(ws, WS_OPCODE_PONG, NULL, 0, mask) == 0 ? 1 : 0;
}

// Receive WebSocket frame (returns message type in high bits, data in string)
// Returns: (opcode << 56) | string_ptr, or 0 on error/close
int64_t ws_recv(int64_t ws_ptr) {
    WebSocketConn* ws = (WebSocketConn*)ws_ptr;
    if (!ws || !ws->connected) return 0;
    
    unsigned char header[2];
    ssize_t n;
    
    // Read first 2 bytes
    if (ws->tls_conn) {
        n = tls_read(ws->tls_conn, (int64_t)header, 2);
    } else {
        n = read(ws->fd, header, 2);
    }
    if (n < 2) return 0;
    
    int fin = (header[0] >> 7) & 1;
    int opcode = header[0] & 0x0F;
    int masked = (header[1] >> 7) & 1;
    size_t payload_len = header[1] & 0x7F;
    
    // Extended payload length
    if (payload_len == 126) {
        unsigned char ext[2];
        if (ws->tls_conn) {
            n = tls_read(ws->tls_conn, (int64_t)ext, 2);
        } else {
            n = read(ws->fd, ext, 2);
        }
        if (n < 2) return 0;
        payload_len = (ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        unsigned char ext[8];
        if (ws->tls_conn) {
            n = tls_read(ws->tls_conn, (int64_t)ext, 8);
        } else {
            n = read(ws->fd, ext, 8);
        }
        if (n < 8) return 0;
        payload_len = ((size_t)ext[4] << 24) | ((size_t)ext[5] << 16) | 
                      ((size_t)ext[6] << 8) | ext[7];
    }
    
    // Read masking key if present
    unsigned char mask_key[4] = {0, 0, 0, 0};
    if (masked) {
        if (ws->tls_conn) {
            n = tls_read(ws->tls_conn, (int64_t)mask_key, 4);
        } else {
            n = read(ws->fd, mask_key, 4);
        }
        if (n < 4) return 0;
    }
    
    // Read payload
    char* payload = NULL;
    if (payload_len > 0) {
        payload = malloc(payload_len + 1);
        size_t read_total = 0;
        while (read_total < payload_len) {
            if (ws->tls_conn) {
                n = tls_read(ws->tls_conn, (int64_t)(payload + read_total), payload_len - read_total);
            } else {
                n = read(ws->fd, payload + read_total, payload_len - read_total);
            }
            if (n <= 0) {
                free(payload);
                return 0;
            }
            read_total += n;
        }
        
        // Unmask if needed
        if (masked) {
            for (size_t i = 0; i < payload_len; i++) {
                payload[i] ^= mask_key[i % 4];
            }
        }
        payload[payload_len] = '\0';
    }
    
    // Handle control frames
    if (opcode == WS_OPCODE_CLOSE) {
        ws->connected = 0;
        if (payload) free(payload);
        return 0;
    }
    
    if (opcode == WS_OPCODE_PING) {
        // Auto-respond with pong
        ws_pong(ws_ptr);
        if (payload) free(payload);
        // Continue reading
        return ws_recv(ws_ptr);
    }
    
    if (opcode == WS_OPCODE_PONG) {
        if (payload) free(payload);
        // Continue reading
        return ws_recv(ws_ptr);
    }
    
    // Return data message
    SxString* str = NULL;
    if (payload) {
        str = intrinsic_string_new(payload);
        free(payload);
    } else {
        str = intrinsic_string_new("");
    }
    
    // Encode opcode in high bits
    return ((int64_t)opcode << 56) | (int64_t)str;
}

// Extract opcode from ws_recv result
int64_t ws_msg_opcode(int64_t result) {
    return (result >> 56) & 0xFF;
}

// Extract string from ws_recv result
int64_t ws_msg_data(int64_t result) {
    return result & 0x00FFFFFFFFFFFFFF;
}

// Close WebSocket connection
void ws_close(int64_t ws_ptr) {
    WebSocketConn* ws = (WebSocketConn*)ws_ptr;
    if (!ws) return;
    
    if (ws->connected) {
        // Send close frame
        int mask = ws->is_client ? 1 : 0;
        ws_send_frame(ws, WS_OPCODE_CLOSE, NULL, 0, mask);
        ws->connected = 0;
    }
    
    if (ws->tls_conn) {
        tls_close(ws->tls_conn);
    } else {
        close(ws->fd);
    }
    free(ws);
}

// Check if connected
int64_t ws_is_connected(int64_t ws_ptr) {
    WebSocketConn* ws = (WebSocketConn*)ws_ptr;
    return ws && ws->connected ? 1 : 0;
}

// Get underlying fd
int64_t ws_get_fd(int64_t ws_ptr) {
    WebSocketConn* ws = (WebSocketConn*)ws_ptr;
    return ws ? ws->fd : -1;
}

// WebSocket opcode constants
int64_t ws_opcode_text(void) { return WS_OPCODE_TEXT; }
int64_t ws_opcode_binary(void) { return WS_OPCODE_BINARY; }
int64_t ws_opcode_close(void) { return WS_OPCODE_CLOSE; }
int64_t ws_opcode_ping(void) { return WS_OPCODE_PING; }
int64_t ws_opcode_pong(void) { return WS_OPCODE_PONG; }

// ============================================================================
// Phase 25: Distribution & Clustering
// ============================================================================

// --------------------------------------------------------------------------
// 25.1 Gossip-Based Cluster Membership (SWIM Protocol)
// --------------------------------------------------------------------------

typedef enum {
    MEMBER_ALIVE = 0,
    MEMBER_SUSPECT = 1,
    MEMBER_DEAD = 2
} MemberState;

typedef struct ClusterMember {
    char* node_id;
    char* host;
    int port;
    MemberState state;
    uint64_t incarnation;
    uint64_t last_seen;
    struct ClusterMember* next;
} ClusterMember;

typedef struct ClusterNode {
    char* self_id;
    char* self_host;
    int self_port;
    ClusterMember* members;
    int member_count;
    int fd;  // UDP socket for gossip
    int tcp_fd;  // TCP socket for state transfer
    uint64_t incarnation;
    pthread_mutex_t lock;
    int running;
    pthread_t gossip_thread;
    pthread_t failure_thread;
    // Callback for membership changes
    int64_t (*on_join)(const char* node_id);
    int64_t (*on_leave)(const char* node_id);
    int64_t (*on_suspect)(const char* node_id);
} ClusterNode;

static ClusterNode* g_cluster = NULL;

// Get current time in milliseconds
static uint64_t cluster_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// Create a new cluster node
int64_t cluster_new(int64_t node_id_ptr, int64_t host_ptr, int64_t port) {
    SxString* node_id = (SxString*)node_id_ptr;
    SxString* host = (SxString*)host_ptr;
    
    ClusterNode* node = (ClusterNode*)malloc(sizeof(ClusterNode));
    if (!node) return 0;
    
    node->self_id = strdup(node_id->data);
    node->self_host = strdup(host->data);
    node->self_port = (int)port;
    node->members = NULL;
    node->member_count = 0;
    node->incarnation = 1;
    node->running = 0;
    node->on_join = NULL;
    node->on_leave = NULL;
    node->on_suspect = NULL;
    pthread_mutex_init(&node->lock, NULL);
    
    // Create UDP socket for gossip
    node->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (node->fd < 0) {
        free(node->self_id);
        free(node->self_host);
        free(node);
        return 0;
    }
    
    // Bind UDP socket
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);
    
    if (bind(node->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(node->fd);
        free(node->self_id);
        free(node->self_host);
        free(node);
        return 0;
    }
    
    // Create TCP socket for state transfer
    node->tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (node->tcp_fd >= 0) {
        int opt = 1;
        setsockopt(node->tcp_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in tcp_addr;
        memset(&tcp_addr, 0, sizeof(tcp_addr));
        tcp_addr.sin_family = AF_INET;
        tcp_addr.sin_addr.s_addr = INADDR_ANY;
        tcp_addr.sin_port = htons((uint16_t)(port + 1));
        
        if (bind(node->tcp_fd, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)) == 0) {
            listen(node->tcp_fd, 10);
        }
    }
    
    g_cluster = node;
    return (int64_t)node;
}

// Add a seed node to bootstrap cluster
int64_t cluster_add_seed(int64_t cluster_ptr, int64_t host_ptr, int64_t port) {
    ClusterNode* node = (ClusterNode*)cluster_ptr;
    SxString* host = (SxString*)host_ptr;
    if (!node || !host) return 0;
    
    pthread_mutex_lock(&node->lock);
    
    // Create member entry
    ClusterMember* member = (ClusterMember*)malloc(sizeof(ClusterMember));
    if (!member) {
        pthread_mutex_unlock(&node->lock);
        return 0;
    }
    
    char id[256];
    snprintf(id, sizeof(id), "%s:%d", host->data, (int)port);
    member->node_id = strdup(id);
    member->host = strdup(host->data);
    member->port = (int)port;
    member->state = MEMBER_ALIVE;
    member->incarnation = 0;
    member->last_seen = cluster_time_ms();
    member->next = node->members;
    node->members = member;
    node->member_count++;
    
    pthread_mutex_unlock(&node->lock);
    return 1;
}

// SWIM message types
#define SWIM_PING 1
#define SWIM_PING_REQ 2
#define SWIM_ACK 3
#define SWIM_SUSPECT 4
#define SWIM_ALIVE 5
#define SWIM_DEAD 6
#define SWIM_JOIN 7
#define SWIM_SYNC 8

typedef struct {
    uint8_t type;
    char sender_id[64];
    char target_id[64];
    uint64_t incarnation;
    uint8_t member_count;
    // Piggyback member updates follow
} SwimMessage;

// Send SWIM message
static int swim_send(ClusterNode* node, const char* host, int port, SwimMessage* msg, size_t len) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, host, &addr.sin_addr);
    
    return sendto(node->fd, msg, len, 0, (struct sockaddr*)&addr, sizeof(addr));
}

// Find member by ID
static ClusterMember* find_member(ClusterNode* node, const char* id) {
    for (ClusterMember* m = node->members; m; m = m->next) {
        if (strcmp(m->node_id, id) == 0) return m;
    }
    return NULL;
}

// Process incoming SWIM message
static void swim_process(ClusterNode* node, SwimMessage* msg, struct sockaddr_in* from) {
    pthread_mutex_lock(&node->lock);
    
    switch (msg->type) {
        case SWIM_PING: {
            // Respond with ACK
            SwimMessage ack;
            memset(&ack, 0, sizeof(ack));
            ack.type = SWIM_ACK;
            strncpy(ack.sender_id, node->self_id, sizeof(ack.sender_id) - 1);
            strncpy(ack.target_id, msg->sender_id, sizeof(ack.target_id) - 1);
            ack.incarnation = node->incarnation;
            
            sendto(node->fd, &ack, sizeof(ack), 0, (struct sockaddr*)from, sizeof(*from));
            break;
        }
        
        case SWIM_ACK: {
            // Update member as alive
            ClusterMember* m = find_member(node, msg->sender_id);
            if (m) {
                m->state = MEMBER_ALIVE;
                m->last_seen = cluster_time_ms();
                if (msg->incarnation > m->incarnation) {
                    m->incarnation = msg->incarnation;
                }
            }
            break;
        }
        
        case SWIM_SUSPECT: {
            // Handle suspicion
            if (strcmp(msg->target_id, node->self_id) == 0) {
                // We're suspected, increase incarnation
                node->incarnation++;
                // Broadcast ALIVE message
            } else {
                ClusterMember* m = find_member(node, msg->target_id);
                if (m && m->state == MEMBER_ALIVE) {
                    m->state = MEMBER_SUSPECT;
                    if (node->on_suspect) {
                        node->on_suspect(m->node_id);
                    }
                }
            }
            break;
        }
        
        case SWIM_ALIVE: {
            ClusterMember* m = find_member(node, msg->sender_id);
            if (m) {
                if (msg->incarnation > m->incarnation) {
                    m->state = MEMBER_ALIVE;
                    m->incarnation = msg->incarnation;
                    m->last_seen = cluster_time_ms();
                }
            }
            break;
        }
        
        case SWIM_DEAD: {
            ClusterMember* m = find_member(node, msg->target_id);
            if (m && m->state != MEMBER_DEAD) {
                m->state = MEMBER_DEAD;
                if (node->on_leave) {
                    node->on_leave(m->node_id);
                }
            }
            break;
        }
        
        case SWIM_JOIN: {
            // New node joining
            ClusterMember* existing = find_member(node, msg->sender_id);
            if (!existing) {
                ClusterMember* member = (ClusterMember*)malloc(sizeof(ClusterMember));
                if (member) {
                    char host[64];
                    inet_ntop(AF_INET, &from->sin_addr, host, sizeof(host));
                    member->node_id = strdup(msg->sender_id);
                    member->host = strdup(host);
                    member->port = ntohs(from->sin_port);
                    member->state = MEMBER_ALIVE;
                    member->incarnation = msg->incarnation;
                    member->last_seen = cluster_time_ms();
                    member->next = node->members;
                    node->members = member;
                    node->member_count++;
                    
                    if (node->on_join) {
                        node->on_join(member->node_id);
                    }
                }
            }
            break;
        }
        
        default:
            break;
    }
    
    pthread_mutex_unlock(&node->lock);
}

// Gossip thread function
static void* gossip_thread_func(void* arg) {
    ClusterNode* node = (ClusterNode*)arg;
    
    // Set socket timeout
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;  // 500ms
    setsockopt(node->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    while (node->running) {
        // Receive messages
        SwimMessage msg;
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        
        ssize_t n = recvfrom(node->fd, &msg, sizeof(msg), 0, 
                            (struct sockaddr*)&from, &from_len);
        if (n > 0) {
            swim_process(node, &msg, &from);
        }
        
        // Periodically ping a random member
        pthread_mutex_lock(&node->lock);
        if (node->member_count > 0) {
            int idx = rand() % node->member_count;
            ClusterMember* target = node->members;
            for (int i = 0; i < idx && target; i++) {
                target = target->next;
            }
            
            if (target && target->state != MEMBER_DEAD) {
                SwimMessage ping;
                memset(&ping, 0, sizeof(ping));
                ping.type = SWIM_PING;
                strncpy(ping.sender_id, node->self_id, sizeof(ping.sender_id) - 1);
                strncpy(ping.target_id, target->node_id, sizeof(ping.target_id) - 1);
                ping.incarnation = node->incarnation;
                
                swim_send(node, target->host, target->port, &ping, sizeof(ping));
            }
        }
        pthread_mutex_unlock(&node->lock);
        
        usleep(100000);  // 100ms between rounds
    }
    
    return NULL;
}

// Failure detection thread
static void* failure_thread_func(void* arg) {
    ClusterNode* node = (ClusterNode*)arg;
    
    while (node->running) {
        uint64_t now = cluster_time_ms();
        
        pthread_mutex_lock(&node->lock);
        for (ClusterMember* m = node->members; m; m = m->next) {
            if (m->state == MEMBER_DEAD) continue;
            
            uint64_t age = now - m->last_seen;
            
            if (m->state == MEMBER_ALIVE && age > 5000) {
                // Move to suspect
                m->state = MEMBER_SUSPECT;
                if (node->on_suspect) {
                    node->on_suspect(m->node_id);
                }
            } else if (m->state == MEMBER_SUSPECT && age > 10000) {
                // Mark as dead
                m->state = MEMBER_DEAD;
                if (node->on_leave) {
                    node->on_leave(m->node_id);
                }
            }
        }
        pthread_mutex_unlock(&node->lock);
        
        usleep(1000000);  // Check every second
    }
    
    return NULL;
}

// Start cluster gossip
int64_t cluster_start(int64_t cluster_ptr) {
    ClusterNode* node = (ClusterNode*)cluster_ptr;
    if (!node || node->running) return 0;
    
    node->running = 1;
    
    // Start gossip thread
    pthread_create(&node->gossip_thread, NULL, gossip_thread_func, node);
    
    // Start failure detection thread
    pthread_create(&node->failure_thread, NULL, failure_thread_func, node);
    
    // Send JOIN to all seeds
    SwimMessage join;
    memset(&join, 0, sizeof(join));
    join.type = SWIM_JOIN;
    strncpy(join.sender_id, node->self_id, sizeof(join.sender_id) - 1);
    join.incarnation = node->incarnation;
    
    pthread_mutex_lock(&node->lock);
    for (ClusterMember* m = node->members; m; m = m->next) {
        swim_send(node, m->host, m->port, &join, sizeof(join));
    }
    pthread_mutex_unlock(&node->lock);
    
    return 1;
}

// Stop cluster gossip
int64_t cluster_stop(int64_t cluster_ptr) {
    ClusterNode* node = (ClusterNode*)cluster_ptr;
    if (!node || !node->running) return 0;
    
    node->running = 0;
    pthread_join(node->gossip_thread, NULL);
    pthread_join(node->failure_thread, NULL);
    
    return 1;
}

// Get member count
int64_t cluster_member_count(int64_t cluster_ptr) {
    ClusterNode* node = (ClusterNode*)cluster_ptr;
    if (!node) return 0;
    
    pthread_mutex_lock(&node->lock);
    int count = 0;
    for (ClusterMember* m = node->members; m; m = m->next) {
        if (m->state != MEMBER_DEAD) count++;
    }
    pthread_mutex_unlock(&node->lock);
    
    return count;
}

// Get member state
int64_t cluster_member_state(int64_t cluster_ptr, int64_t node_id_ptr) {
    ClusterNode* node = (ClusterNode*)cluster_ptr;
    SxString* id = (SxString*)node_id_ptr;
    if (!node || !id) return -1;
    
    pthread_mutex_lock(&node->lock);
    ClusterMember* m = find_member(node, id->data);
    int state = m ? (int)m->state : -1;
    pthread_mutex_unlock(&node->lock);
    
    return state;
}

// Check if node is alive
int64_t cluster_is_alive(int64_t cluster_ptr, int64_t node_id_ptr) {
    return cluster_member_state(cluster_ptr, node_id_ptr) == MEMBER_ALIVE ? 1 : 0;
}

// Get self node ID
int64_t cluster_self_id(int64_t cluster_ptr) {
    ClusterNode* node = (ClusterNode*)cluster_ptr;
    if (!node) return 0;
    return (int64_t)intrinsic_string_new(node->self_id);
}

// Close cluster
void cluster_close(int64_t cluster_ptr) {
    ClusterNode* node = (ClusterNode*)cluster_ptr;
    if (!node) return;
    
    if (node->running) {
        cluster_stop(cluster_ptr);
    }
    
    close(node->fd);
    if (node->tcp_fd >= 0) close(node->tcp_fd);
    
    // Free members
    ClusterMember* m = node->members;
    while (m) {
        ClusterMember* next = m->next;
        free(m->node_id);
        free(m->host);
        free(m);
        m = next;
    }
    
    pthread_mutex_destroy(&node->lock);
    free(node->self_id);
    free(node->self_host);
    free(node);
    
    if (g_cluster == node) g_cluster = NULL;
}

// --------------------------------------------------------------------------
// 25.2 Actor Location Registry (Distributed Hash Table)
// --------------------------------------------------------------------------

#define DHT_RING_SIZE 65536
#define DHT_VIRTUAL_NODES 150
#define DHT_REPLICATION 3

typedef struct DHTEntry {
    uint64_t hash;
    char* key;
    char* node_id;
    void* value;
    size_t value_len;
    uint64_t version;
    struct DHTEntry* next;
} DHTEntry;

typedef struct VirtualNode {
    uint64_t hash;
    char* node_id;
} VirtualNode;

typedef struct DHTNode {
    VirtualNode* ring;
    int ring_size;
    DHTEntry** table;
    int table_size;
    pthread_mutex_t lock;
    ClusterNode* cluster;
} DHTNode;

// FNV-1a hash
static uint64_t dht_hash(const char* key) {
    uint64_t hash = 14695981039346656037ULL;
    while (*key) {
        hash ^= (uint8_t)*key++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

// Create DHT
int64_t dht_new(int64_t cluster_ptr) {
    DHTNode* dht = (DHTNode*)malloc(sizeof(DHTNode));
    if (!dht) return 0;
    
    dht->cluster = (ClusterNode*)cluster_ptr;
    dht->table_size = 4096;
    dht->table = (DHTEntry**)calloc(dht->table_size, sizeof(DHTEntry*));
    dht->ring = NULL;
    dht->ring_size = 0;
    pthread_mutex_init(&dht->lock, NULL);
    
    return (int64_t)dht;
}

// Compare virtual nodes for sorting
static int vnode_compare(const void* a, const void* b) {
    const VirtualNode* va = (const VirtualNode*)a;
    const VirtualNode* vb = (const VirtualNode*)b;
    if (va->hash < vb->hash) return -1;
    if (va->hash > vb->hash) return 1;
    return 0;
}

// Rebuild consistent hash ring
int64_t dht_rebuild_ring(int64_t dht_ptr) {
    DHTNode* dht = (DHTNode*)dht_ptr;
    if (!dht || !dht->cluster) return 0;
    
    pthread_mutex_lock(&dht->lock);
    
    // Free old ring
    if (dht->ring) {
        for (int i = 0; i < dht->ring_size; i++) {
            free(dht->ring[i].node_id);
        }
        free(dht->ring);
    }
    
    // Count alive members
    ClusterNode* cluster = dht->cluster;
    pthread_mutex_lock(&cluster->lock);
    
    int alive_count = 1;  // Include self
    for (ClusterMember* m = cluster->members; m; m = m->next) {
        if (m->state == MEMBER_ALIVE) alive_count++;
    }
    
    // Allocate ring
    dht->ring_size = alive_count * DHT_VIRTUAL_NODES;
    dht->ring = (VirtualNode*)malloc(dht->ring_size * sizeof(VirtualNode));
    
    int idx = 0;
    
    // Add self virtual nodes
    for (int v = 0; v < DHT_VIRTUAL_NODES; v++) {
        char vkey[256];
        snprintf(vkey, sizeof(vkey), "%s#%d", cluster->self_id, v);
        dht->ring[idx].hash = dht_hash(vkey);
        dht->ring[idx].node_id = strdup(cluster->self_id);
        idx++;
    }
    
    // Add member virtual nodes
    for (ClusterMember* m = cluster->members; m; m = m->next) {
        if (m->state != MEMBER_ALIVE) continue;
        for (int v = 0; v < DHT_VIRTUAL_NODES; v++) {
            char vkey[256];
            snprintf(vkey, sizeof(vkey), "%s#%d", m->node_id, v);
            dht->ring[idx].hash = dht_hash(vkey);
            dht->ring[idx].node_id = strdup(m->node_id);
            idx++;
        }
    }
    
    pthread_mutex_unlock(&cluster->lock);
    
    // Sort ring
    qsort(dht->ring, dht->ring_size, sizeof(VirtualNode), vnode_compare);
    
    pthread_mutex_unlock(&dht->lock);
    return 1;
}

// Find responsible node for key
int64_t dht_find_node(int64_t dht_ptr, int64_t key_ptr) {
    DHTNode* dht = (DHTNode*)dht_ptr;
    SxString* key = (SxString*)key_ptr;
    if (!dht || !key || dht->ring_size == 0) return 0;
    
    pthread_mutex_lock(&dht->lock);
    
    uint64_t hash = dht_hash(key->data);
    
    // Binary search for first node >= hash
    int lo = 0, hi = dht->ring_size;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (dht->ring[mid].hash < hash) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    
    // Wrap around
    if (lo >= dht->ring_size) lo = 0;
    
    char* node_id = dht->ring[lo].node_id;
    SxString* result = intrinsic_string_new(node_id);
    
    pthread_mutex_unlock(&dht->lock);
    return (int64_t)result;
}

// Store value in DHT
int64_t dht_put(int64_t dht_ptr, int64_t key_ptr, int64_t value_ptr) {
    DHTNode* dht = (DHTNode*)dht_ptr;
    SxString* key = (SxString*)key_ptr;
    SxString* value = (SxString*)value_ptr;
    if (!dht || !key || !value) return 0;
    
    pthread_mutex_lock(&dht->lock);
    
    uint64_t hash = dht_hash(key->data);
    int bucket = hash % dht->table_size;
    
    // Check for existing entry
    DHTEntry* entry = dht->table[bucket];
    while (entry) {
        if (strcmp(entry->key, key->data) == 0) {
            // Update existing
            free(entry->value);
            entry->value = strdup(value->data);
            entry->value_len = value->len;
            entry->version++;
            pthread_mutex_unlock(&dht->lock);
            return 1;
        }
        entry = entry->next;
    }
    
    // Create new entry
    entry = (DHTEntry*)malloc(sizeof(DHTEntry));
    entry->hash = hash;
    entry->key = strdup(key->data);
    entry->node_id = dht->cluster ? strdup(dht->cluster->self_id) : strdup("local");
    entry->value = strdup(value->data);
    entry->value_len = value->len;
    entry->version = 1;
    entry->next = dht->table[bucket];
    dht->table[bucket] = entry;
    
    pthread_mutex_unlock(&dht->lock);
    return 1;
}

// Get value from DHT
int64_t dht_get(int64_t dht_ptr, int64_t key_ptr) {
    DHTNode* dht = (DHTNode*)dht_ptr;
    SxString* key = (SxString*)key_ptr;
    if (!dht || !key) return 0;
    
    pthread_mutex_lock(&dht->lock);
    
    uint64_t hash = dht_hash(key->data);
    int bucket = hash % dht->table_size;
    
    DHTEntry* entry = dht->table[bucket];
    while (entry) {
        if (strcmp(entry->key, key->data) == 0) {
            SxString* result = intrinsic_string_new((char*)entry->value);
            pthread_mutex_unlock(&dht->lock);
            return (int64_t)result;
        }
        entry = entry->next;
    }
    
    pthread_mutex_unlock(&dht->lock);
    return 0;
}

// Delete from DHT
int64_t dht_delete(int64_t dht_ptr, int64_t key_ptr) {
    DHTNode* dht = (DHTNode*)dht_ptr;
    SxString* key = (SxString*)key_ptr;
    if (!dht || !key) return 0;
    
    pthread_mutex_lock(&dht->lock);
    
    uint64_t hash = dht_hash(key->data);
    int bucket = hash % dht->table_size;
    
    DHTEntry** pp = &dht->table[bucket];
    while (*pp) {
        if (strcmp((*pp)->key, key->data) == 0) {
            DHTEntry* entry = *pp;
            *pp = entry->next;
            free(entry->key);
            free(entry->node_id);
            free(entry->value);
            free(entry);
            pthread_mutex_unlock(&dht->lock);
            return 1;
        }
        pp = &(*pp)->next;
    }
    
    pthread_mutex_unlock(&dht->lock);
    return 0;
}

// Close DHT
void dht_close(int64_t dht_ptr) {
    DHTNode* dht = (DHTNode*)dht_ptr;
    if (!dht) return;
    
    pthread_mutex_lock(&dht->lock);
    
    // Free ring
    if (dht->ring) {
        for (int i = 0; i < dht->ring_size; i++) {
            free(dht->ring[i].node_id);
        }
        free(dht->ring);
    }
    
    // Free table
    for (int i = 0; i < dht->table_size; i++) {
        DHTEntry* entry = dht->table[i];
        while (entry) {
            DHTEntry* next = entry->next;
            free(entry->key);
            free(entry->node_id);
            free(entry->value);
            free(entry);
            entry = next;
        }
    }
    free(dht->table);
    
    pthread_mutex_unlock(&dht->lock);
    pthread_mutex_destroy(&dht->lock);
    free(dht);
}

// --------------------------------------------------------------------------
// 25.3 Actor Migration Protocol
// --------------------------------------------------------------------------

typedef struct MigrationState {
    char* actor_id;
    void* state_data;
    size_t state_len;
    char* source_node;
    char* target_node;
    int status;  // 0=pending, 1=transferring, 2=complete, -1=failed
    pthread_mutex_t lock;
} MigrationState;

typedef struct MigrationBuffer {
    void** messages;
    int count;
    int capacity;
} MigrationBuffer;

// Serialize actor state (placeholder - actual serialization depends on actor type)
int64_t migration_serialize_actor(int64_t actor_ptr) {
    // In a real implementation, this would serialize the actor's full state
    // For now, return a placeholder
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "{\"actor\":%lld,\"state\":\"serialized\"}", (long long)actor_ptr);
    return (int64_t)intrinsic_string_new(buffer);
}

// Deserialize actor state
int64_t migration_deserialize_actor(int64_t data_ptr) {
    // In a real implementation, this would deserialize and recreate the actor
    // For now, return placeholder
    return 0;
}

// Create migration coordinator
int64_t migration_new(int64_t actor_id_ptr, int64_t target_node_ptr) {
    SxString* actor_id = (SxString*)actor_id_ptr;
    SxString* target_node = (SxString*)target_node_ptr;
    if (!actor_id || !target_node) return 0;
    
    MigrationState* mig = (MigrationState*)malloc(sizeof(MigrationState));
    if (!mig) return 0;
    
    mig->actor_id = strdup(actor_id->data);
    mig->state_data = NULL;
    mig->state_len = 0;
    mig->source_node = g_cluster ? strdup(g_cluster->self_id) : strdup("local");
    mig->target_node = strdup(target_node->data);
    mig->status = 0;
    pthread_mutex_init(&mig->lock, NULL);
    
    return (int64_t)mig;
}

// Start migration
int64_t migration_start(int64_t migration_ptr, int64_t state_data_ptr) {
    MigrationState* mig = (MigrationState*)migration_ptr;
    SxString* state_data = (SxString*)state_data_ptr;
    if (!mig || !state_data) return 0;
    
    pthread_mutex_lock(&mig->lock);
    
    mig->state_data = strdup(state_data->data);
    mig->state_len = state_data->len;
    mig->status = 1;  // Transferring
    
    // In a real implementation, this would:
    // 1. Buffer incoming messages for this actor
    // 2. Send state to target node
    // 3. Wait for acknowledgment
    // 4. Forward buffered messages
    // 5. Update DHT with new location
    
    // Simulate successful transfer
    mig->status = 2;  // Complete
    
    pthread_mutex_unlock(&mig->lock);
    return 1;
}

// Check migration status
int64_t migration_status(int64_t migration_ptr) {
    MigrationState* mig = (MigrationState*)migration_ptr;
    if (!mig) return -1;
    
    pthread_mutex_lock(&mig->lock);
    int status = mig->status;
    pthread_mutex_unlock(&mig->lock);
    
    return status;
}

// Rollback migration on failure
int64_t migration_rollback(int64_t migration_ptr) {
    MigrationState* mig = (MigrationState*)migration_ptr;
    if (!mig) return 0;
    
    pthread_mutex_lock(&mig->lock);
    mig->status = -1;  // Failed
    pthread_mutex_unlock(&mig->lock);
    
    return 1;
}

// Close migration
void migration_close(int64_t migration_ptr) {
    MigrationState* mig = (MigrationState*)migration_ptr;
    if (!mig) return;
    
    free(mig->actor_id);
    free(mig->source_node);
    free(mig->target_node);
    if (mig->state_data) free(mig->state_data);
    pthread_mutex_destroy(&mig->lock);
    free(mig);
}

// --------------------------------------------------------------------------
// 25.4 Content-Addressed Code
// --------------------------------------------------------------------------

typedef struct CodeEntry {
    char hash[65];  // SHA-256 hex
    char* code;
    size_t code_len;
    char* ast_json;
    uint64_t version;
    struct CodeEntry* next;
} CodeEntry;

typedef struct CodeStore {
    CodeEntry** table;
    int table_size;
    pthread_mutex_t lock;
} CodeStore;

static CodeStore* g_code_store = NULL;

// Simple SHA-256 placeholder (real implementation would use OpenSSL)
static void code_hash(const char* data, size_t len, char* out) {
    // Using FNV-1a as placeholder since we already have OpenSSL linked
    uint64_t h1 = 14695981039346656037ULL;
    uint64_t h2 = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        if (i % 2 == 0) {
            h1 ^= (uint8_t)data[i];
            h1 *= 1099511628211ULL;
        } else {
            h2 ^= (uint8_t)data[i];
            h2 *= 1099511628211ULL;
        }
    }
    snprintf(out, 65, "%016llx%016llx%016llx%016llx",
             (unsigned long long)h1, (unsigned long long)h2,
             (unsigned long long)(h1 ^ h2), (unsigned long long)(h1 + h2));
}

// Create code store
int64_t code_store_new(void) {
    CodeStore* store = (CodeStore*)malloc(sizeof(CodeStore));
    if (!store) return 0;
    
    store->table_size = 1024;
    store->table = (CodeEntry**)calloc(store->table_size, sizeof(CodeEntry*));
    pthread_mutex_init(&store->lock, NULL);
    
    g_code_store = store;
    return (int64_t)store;
}

// Store code and return hash
int64_t code_store_put(int64_t store_ptr, int64_t code_ptr) {
    CodeStore* store = (CodeStore*)store_ptr;
    SxString* code = (SxString*)code_ptr;
    if (!store || !code) return 0;
    
    char hash[65];
    code_hash(code->data, code->len, hash);
    
    pthread_mutex_lock(&store->lock);
    
    // Check if already exists
    int bucket = (int)(dht_hash(hash) % store->table_size);
    CodeEntry* entry = store->table[bucket];
    while (entry) {
        if (strcmp(entry->hash, hash) == 0) {
            pthread_mutex_unlock(&store->lock);
            return (int64_t)intrinsic_string_new(hash);
        }
        entry = entry->next;
    }
    
    // Create new entry
    entry = (CodeEntry*)malloc(sizeof(CodeEntry));
    strncpy(entry->hash, hash, 64);
    entry->hash[64] = '\0';
    entry->code = strdup(code->data);
    entry->code_len = code->len;
    entry->ast_json = NULL;
    entry->version = 1;
    entry->next = store->table[bucket];
    store->table[bucket] = entry;
    
    pthread_mutex_unlock(&store->lock);
    return (int64_t)intrinsic_string_new(hash);
}

// Get code by hash
int64_t code_store_get(int64_t store_ptr, int64_t hash_ptr) {
    CodeStore* store = (CodeStore*)store_ptr;
    SxString* hash = (SxString*)hash_ptr;
    if (!store || !hash) return 0;
    
    pthread_mutex_lock(&store->lock);
    
    int bucket = (int)(dht_hash(hash->data) % store->table_size);
    CodeEntry* entry = store->table[bucket];
    while (entry) {
        if (strcmp(entry->hash, hash->data) == 0) {
            SxString* result = intrinsic_string_new(entry->code);
            pthread_mutex_unlock(&store->lock);
            return (int64_t)result;
        }
        entry = entry->next;
    }
    
    pthread_mutex_unlock(&store->lock);
    return 0;
}

// Store AST JSON for code
int64_t code_store_put_ast(int64_t store_ptr, int64_t hash_ptr, int64_t ast_ptr) {
    CodeStore* store = (CodeStore*)store_ptr;
    SxString* hash = (SxString*)hash_ptr;
    SxString* ast = (SxString*)ast_ptr;
    if (!store || !hash || !ast) return 0;
    
    pthread_mutex_lock(&store->lock);
    
    int bucket = (int)(dht_hash(hash->data) % store->table_size);
    CodeEntry* entry = store->table[bucket];
    while (entry) {
        if (strcmp(entry->hash, hash->data) == 0) {
            if (entry->ast_json) free(entry->ast_json);
            entry->ast_json = strdup(ast->data);
            pthread_mutex_unlock(&store->lock);
            return 1;
        }
        entry = entry->next;
    }
    
    pthread_mutex_unlock(&store->lock);
    return 0;
}

// Get AST JSON by hash
int64_t code_store_get_ast(int64_t store_ptr, int64_t hash_ptr) {
    CodeStore* store = (CodeStore*)store_ptr;
    SxString* hash = (SxString*)hash_ptr;
    if (!store || !hash) return 0;
    
    pthread_mutex_lock(&store->lock);
    
    int bucket = (int)(dht_hash(hash->data) % store->table_size);
    CodeEntry* entry = store->table[bucket];
    while (entry) {
        if (strcmp(entry->hash, hash->data) == 0 && entry->ast_json) {
            SxString* result = intrinsic_string_new(entry->ast_json);
            pthread_mutex_unlock(&store->lock);
            return (int64_t)result;
        }
        entry = entry->next;
    }
    
    pthread_mutex_unlock(&store->lock);
    return 0;
}

// Close code store
void code_store_close(int64_t store_ptr) {
    CodeStore* store = (CodeStore*)store_ptr;
    if (!store) return;
    
    pthread_mutex_lock(&store->lock);
    
    for (int i = 0; i < store->table_size; i++) {
        CodeEntry* entry = store->table[i];
        while (entry) {
            CodeEntry* next = entry->next;
            free(entry->code);
            if (entry->ast_json) free(entry->ast_json);
            free(entry);
            entry = next;
        }
    }
    free(store->table);
    
    pthread_mutex_unlock(&store->lock);
    pthread_mutex_destroy(&store->lock);
    free(store);
    
    if (g_code_store == store) g_code_store = NULL;
}

// --------------------------------------------------------------------------
// 25.5 Network Partition Detection
// --------------------------------------------------------------------------

typedef struct PartitionDetector {
    ClusterNode* cluster;
    int quorum_size;
    int current_partition;  // 0=healthy, 1=minority, 2=majority
    pthread_mutex_t lock;
    int64_t (*on_partition)(int partition_type);
    int64_t (*on_heal)(void);
} PartitionDetector;

// Create partition detector
int64_t partition_detector_new(int64_t cluster_ptr) {
    ClusterNode* cluster = (ClusterNode*)cluster_ptr;
    if (!cluster) return 0;
    
    PartitionDetector* pd = (PartitionDetector*)malloc(sizeof(PartitionDetector));
    if (!pd) return 0;
    
    pd->cluster = cluster;
    pd->quorum_size = 0;
    pd->current_partition = 0;
    pd->on_partition = NULL;
    pd->on_heal = NULL;
    pthread_mutex_init(&pd->lock, NULL);
    
    return (int64_t)pd;
}

// Set expected quorum size
int64_t partition_set_quorum(int64_t pd_ptr, int64_t quorum) {
    PartitionDetector* pd = (PartitionDetector*)pd_ptr;
    if (!pd) return 0;
    
    pthread_mutex_lock(&pd->lock);
    pd->quorum_size = (int)quorum;
    pthread_mutex_unlock(&pd->lock);
    
    return 1;
}

// Check partition status
int64_t partition_check(int64_t pd_ptr) {
    PartitionDetector* pd = (PartitionDetector*)pd_ptr;
    if (!pd) return -1;
    
    pthread_mutex_lock(&pd->lock);
    
    // Count reachable members
    int reachable = 1;  // Count self
    ClusterNode* cluster = pd->cluster;
    
    pthread_mutex_lock(&cluster->lock);
    for (ClusterMember* m = cluster->members; m; m = m->next) {
        if (m->state == MEMBER_ALIVE) reachable++;
    }
    pthread_mutex_unlock(&cluster->lock);
    
    int old_partition = pd->current_partition;
    
    if (pd->quorum_size > 0) {
        if (reachable >= pd->quorum_size) {
            pd->current_partition = 2;  // Majority
        } else if (reachable > 0) {
            pd->current_partition = 1;  // Minority
        } else {
            pd->current_partition = 0;  // Isolated
        }
    } else {
        pd->current_partition = reachable > 0 ? 2 : 0;
    }
    
    // Fire callbacks on state change
    if (old_partition != pd->current_partition) {
        if (pd->current_partition == 1 && pd->on_partition) {
            pd->on_partition(1);
        } else if (old_partition == 1 && pd->current_partition == 2 && pd->on_heal) {
            pd->on_heal();
        }
    }
    
    int result = pd->current_partition;
    pthread_mutex_unlock(&pd->lock);
    
    return result;
}

// Check if we have quorum
int64_t partition_has_quorum(int64_t pd_ptr) {
    PartitionDetector* pd = (PartitionDetector*)pd_ptr;
    if (!pd) return 0;
    
    pthread_mutex_lock(&pd->lock);
    int result = (pd->current_partition == 2) ? 1 : 0;
    pthread_mutex_unlock(&pd->lock);
    
    return result;
}

// Get reachable count
int64_t partition_reachable_count(int64_t pd_ptr) {
    PartitionDetector* pd = (PartitionDetector*)pd_ptr;
    if (!pd) return 0;
    
    int count = 1;  // Self
    ClusterNode* cluster = pd->cluster;
    
    pthread_mutex_lock(&cluster->lock);
    for (ClusterMember* m = cluster->members; m; m = m->next) {
        if (m->state == MEMBER_ALIVE) count++;
    }
    pthread_mutex_unlock(&cluster->lock);
    
    return count;
}

// Close partition detector
void partition_detector_close(int64_t pd_ptr) {
    PartitionDetector* pd = (PartitionDetector*)pd_ptr;
    if (!pd) return;
    
    pthread_mutex_destroy(&pd->lock);
    free(pd);
}

// --------------------------------------------------------------------------
// 25.6 Split-Brain Resolution
// --------------------------------------------------------------------------

typedef struct VectorClock {
    char** node_ids;
    uint64_t* counters;
    int count;
    int capacity;
    pthread_mutex_t lock;
} VectorClock;

// Create vector clock
int64_t vclock_new(void) {
    VectorClock* vc = (VectorClock*)malloc(sizeof(VectorClock));
    if (!vc) return 0;
    
    vc->capacity = 16;
    vc->count = 0;
    vc->node_ids = (char**)calloc(vc->capacity, sizeof(char*));
    vc->counters = (uint64_t*)calloc(vc->capacity, sizeof(uint64_t));
    pthread_mutex_init(&vc->lock, NULL);
    
    return (int64_t)vc;
}

// Increment counter for node
int64_t vclock_increment(int64_t vc_ptr, int64_t node_id_ptr) {
    VectorClock* vc = (VectorClock*)vc_ptr;
    SxString* node_id = (SxString*)node_id_ptr;
    if (!vc || !node_id) return 0;
    
    pthread_mutex_lock(&vc->lock);
    
    // Find or create entry
    int idx = -1;
    for (int i = 0; i < vc->count; i++) {
        if (strcmp(vc->node_ids[i], node_id->data) == 0) {
            idx = i;
            break;
        }
    }
    
    if (idx < 0) {
        // Create new entry
        if (vc->count >= vc->capacity) {
            vc->capacity *= 2;
            vc->node_ids = (char**)realloc(vc->node_ids, vc->capacity * sizeof(char*));
            vc->counters = (uint64_t*)realloc(vc->counters, vc->capacity * sizeof(uint64_t));
        }
        idx = vc->count++;
        vc->node_ids[idx] = strdup(node_id->data);
        vc->counters[idx] = 0;
    }
    
    vc->counters[idx]++;
    uint64_t result = vc->counters[idx];
    
    pthread_mutex_unlock(&vc->lock);
    return result;
}

// Get counter for node
int64_t vclock_get(int64_t vc_ptr, int64_t node_id_ptr) {
    VectorClock* vc = (VectorClock*)vc_ptr;
    SxString* node_id = (SxString*)node_id_ptr;
    if (!vc || !node_id) return 0;
    
    pthread_mutex_lock(&vc->lock);
    
    for (int i = 0; i < vc->count; i++) {
        if (strcmp(vc->node_ids[i], node_id->data) == 0) {
            uint64_t result = vc->counters[i];
            pthread_mutex_unlock(&vc->lock);
            return result;
        }
    }
    
    pthread_mutex_unlock(&vc->lock);
    return 0;
}

// Compare two vector clocks: -1 = a < b, 0 = concurrent, 1 = a > b
int64_t vclock_compare(int64_t vc_a_ptr, int64_t vc_b_ptr) {
    VectorClock* a = (VectorClock*)vc_a_ptr;
    VectorClock* b = (VectorClock*)vc_b_ptr;
    if (!a || !b) return 0;
    
    pthread_mutex_lock(&a->lock);
    pthread_mutex_lock(&b->lock);
    
    int a_greater = 0;
    int b_greater = 0;
    
    // Check all entries in a
    for (int i = 0; i < a->count; i++) {
        uint64_t a_val = a->counters[i];
        uint64_t b_val = 0;
        
        for (int j = 0; j < b->count; j++) {
            if (strcmp(a->node_ids[i], b->node_ids[j]) == 0) {
                b_val = b->counters[j];
                break;
            }
        }
        
        if (a_val > b_val) a_greater = 1;
        if (a_val < b_val) b_greater = 1;
    }
    
    // Check entries in b that might not be in a
    for (int i = 0; i < b->count; i++) {
        int found = 0;
        for (int j = 0; j < a->count; j++) {
            if (strcmp(b->node_ids[i], a->node_ids[j]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found && b->counters[i] > 0) {
            b_greater = 1;
        }
    }
    
    pthread_mutex_unlock(&b->lock);
    pthread_mutex_unlock(&a->lock);
    
    if (a_greater && !b_greater) return 1;
    if (b_greater && !a_greater) return -1;
    return 0;  // Concurrent
}

// Merge two vector clocks (take max of each)
int64_t vclock_merge(int64_t vc_a_ptr, int64_t vc_b_ptr) {
    VectorClock* a = (VectorClock*)vc_a_ptr;
    VectorClock* b = (VectorClock*)vc_b_ptr;
    if (!a || !b) return 0;
    
    pthread_mutex_lock(&a->lock);
    pthread_mutex_lock(&b->lock);
    
    for (int i = 0; i < b->count; i++) {
        int found = -1;
        for (int j = 0; j < a->count; j++) {
            if (strcmp(a->node_ids[j], b->node_ids[i]) == 0) {
                found = j;
                break;
            }
        }
        
        if (found >= 0) {
            if (b->counters[i] > a->counters[found]) {
                a->counters[found] = b->counters[i];
            }
        } else {
            // Add new entry
            if (a->count >= a->capacity) {
                a->capacity *= 2;
                a->node_ids = (char**)realloc(a->node_ids, a->capacity * sizeof(char*));
                a->counters = (uint64_t*)realloc(a->counters, a->capacity * sizeof(uint64_t));
            }
            a->node_ids[a->count] = strdup(b->node_ids[i]);
            a->counters[a->count] = b->counters[i];
            a->count++;
        }
    }
    
    pthread_mutex_unlock(&b->lock);
    pthread_mutex_unlock(&a->lock);
    
    return 1;
}

// Close vector clock
void vclock_close(int64_t vc_ptr) {
    VectorClock* vc = (VectorClock*)vc_ptr;
    if (!vc) return;
    
    pthread_mutex_lock(&vc->lock);
    for (int i = 0; i < vc->count; i++) {
        free(vc->node_ids[i]);
    }
    free(vc->node_ids);
    free(vc->counters);
    pthread_mutex_unlock(&vc->lock);
    pthread_mutex_destroy(&vc->lock);
    free(vc);
}

// --------------------------------------------------------------------------
// 25.7 Node Authentication
// --------------------------------------------------------------------------

typedef struct NodeAuth {
    SSL_CTX* ctx;
    char* cert_path;
    char* key_path;
    char* ca_path;
    int verify_peer;
} NodeAuth;

// Create node authenticator with TLS
int64_t node_auth_new(void) {
    NodeAuth* auth = (NodeAuth*)malloc(sizeof(NodeAuth));
    if (!auth) return 0;
    
    auth->ctx = SSL_CTX_new(TLS_method());
    if (!auth->ctx) {
        free(auth);
        return 0;
    }
    
    auth->cert_path = NULL;
    auth->key_path = NULL;
    auth->ca_path = NULL;
    auth->verify_peer = 1;
    
    // Set secure defaults
    SSL_CTX_set_min_proto_version(auth->ctx, TLS1_2_VERSION);
    
    return (int64_t)auth;
}

// Load certificate
int64_t node_auth_load_cert(int64_t auth_ptr, int64_t cert_path_ptr) {
    NodeAuth* auth = (NodeAuth*)auth_ptr;
    SxString* path = (SxString*)cert_path_ptr;
    if (!auth || !path) return 0;
    
    if (auth->cert_path) free(auth->cert_path);
    auth->cert_path = strdup(path->data);
    
    if (SSL_CTX_use_certificate_file(auth->ctx, path->data, SSL_FILETYPE_PEM) != 1) {
        return 0;
    }
    
    return 1;
}

// Load private key
int64_t node_auth_load_key(int64_t auth_ptr, int64_t key_path_ptr) {
    NodeAuth* auth = (NodeAuth*)auth_ptr;
    SxString* path = (SxString*)key_path_ptr;
    if (!auth || !path) return 0;
    
    if (auth->key_path) free(auth->key_path);
    auth->key_path = strdup(path->data);
    
    if (SSL_CTX_use_PrivateKey_file(auth->ctx, path->data, SSL_FILETYPE_PEM) != 1) {
        return 0;
    }
    
    return 1;
}

// Load CA certificate for peer verification
int64_t node_auth_load_ca(int64_t auth_ptr, int64_t ca_path_ptr) {
    NodeAuth* auth = (NodeAuth*)auth_ptr;
    SxString* path = (SxString*)ca_path_ptr;
    if (!auth || !path) return 0;
    
    if (auth->ca_path) free(auth->ca_path);
    auth->ca_path = strdup(path->data);
    
    if (SSL_CTX_load_verify_locations(auth->ctx, path->data, NULL) != 1) {
        return 0;
    }
    
    SSL_CTX_set_verify(auth->ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    
    return 1;
}

// Set shared secret (pre-shared key)
int64_t node_auth_set_secret(int64_t auth_ptr, int64_t secret_ptr) {
    NodeAuth* auth = (NodeAuth*)auth_ptr;
    SxString* secret = (SxString*)secret_ptr;
    if (!auth || !secret) return 0;
    
    // PSK is more complex to implement properly
    // For now, just mark as configured
    return 1;
}

// Create authenticated connection to peer
int64_t node_auth_connect(int64_t auth_ptr, int64_t host_ptr, int64_t port) {
    NodeAuth* auth = (NodeAuth*)auth_ptr;
    SxString* host = (SxString*)host_ptr;
    if (!auth || !host) return 0;
    
    // Create socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return 0;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, host->data, &addr.sin_addr);
    
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return 0;
    }
    
    // Wrap with TLS
    SSL* ssl = SSL_new(auth->ctx);
    if (!ssl) {
        close(fd);
        return 0;
    }
    
    SSL_set_fd(ssl, fd);
    
    if (SSL_connect(ssl) != 1) {
        SSL_free(ssl);
        close(fd);
        return 0;
    }
    
    // Verify peer certificate if required
    if (auth->verify_peer) {
        X509* cert = SSL_get_peer_certificate(ssl);
        if (!cert) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(fd);
            return 0;
        }
        X509_free(cert);
        
        if (SSL_get_verify_result(ssl) != X509_V_OK) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(fd);
            return 0;
        }
    }
    
    return (int64_t)ssl;
}

// Accept authenticated connection
int64_t node_auth_accept(int64_t auth_ptr, int64_t listen_fd) {
    NodeAuth* auth = (NodeAuth*)auth_ptr;
    if (!auth) return 0;
    
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    int fd = accept((int)listen_fd, (struct sockaddr*)&addr, &addr_len);
    if (fd < 0) return 0;
    
    SSL* ssl = SSL_new(auth->ctx);
    if (!ssl) {
        close(fd);
        return 0;
    }
    
    SSL_set_fd(ssl, fd);
    
    if (SSL_accept(ssl) != 1) {
        SSL_free(ssl);
        close(fd);
        return 0;
    }
    
    return (int64_t)ssl;
}

// Close authenticated connection
void node_auth_close_conn(int64_t ssl_ptr) {
    SSL* ssl = (SSL*)ssl_ptr;
    if (!ssl) return;
    
    int fd = SSL_get_fd(ssl);
    SSL_shutdown(ssl);
    SSL_free(ssl);
    if (fd >= 0) close(fd);
}

// Close authenticator
void node_auth_close(int64_t auth_ptr) {
    NodeAuth* auth = (NodeAuth*)auth_ptr;
    if (!auth) return;
    
    if (auth->ctx) SSL_CTX_free(auth->ctx);
    if (auth->cert_path) free(auth->cert_path);
    if (auth->key_path) free(auth->key_path);
    if (auth->ca_path) free(auth->ca_path);
    free(auth);
}


// ============================================================================
// Phase 26: Semantic Memory
// ============================================================================

// --------------------------------------------------------------------------
// 26.1 Vector Embeddings
// --------------------------------------------------------------------------

#define EMBEDDING_DIM 384  // Default dimension for small models

typedef struct Embedding {
    double* values;
    int dim;
} Embedding;

typedef struct EmbeddingModel {
    char* model_path;
    int dim;
    int loaded;
    pthread_mutex_t lock;
} EmbeddingModel;

// Create embedding model (simplified - real impl would use ONNX)
int64_t embedding_model_new(int64_t dim) {
    EmbeddingModel* model = (EmbeddingModel*)malloc(sizeof(EmbeddingModel));
    if (!model) return 0;
    
    model->model_path = NULL;
    model->dim = dim > 0 ? (int)dim : EMBEDDING_DIM;
    model->loaded = 1;  // Simplified - always "loaded"
    pthread_mutex_init(&model->lock, NULL);
    
    return (int64_t)model;
}

// Load model from path
int64_t embedding_model_load(int64_t model_ptr, int64_t path_ptr) {
    EmbeddingModel* model = (EmbeddingModel*)model_ptr;
    SxString* path = (SxString*)path_ptr;
    if (!model || !path) return 0;
    
    pthread_mutex_lock(&model->lock);
    if (model->model_path) free(model->model_path);
    model->model_path = strdup(path->data);
    model->loaded = 1;
    pthread_mutex_unlock(&model->lock);
    
    return 1;
}

// Simple hash-based embedding (placeholder for real model)
static void compute_simple_embedding(const char* text, double* out, int dim) {
    // Initialize with zeros
    memset(out, 0, dim * sizeof(double));
    
    // Simple character-based embedding
    size_t len = strlen(text);
    for (size_t i = 0; i < len; i++) {
        int idx = (int)(((unsigned char)text[i] * 7 + i * 13) % dim);
        out[idx] += 1.0;
    }
    
    // Normalize to unit vector
    double sum = 0;
    for (int i = 0; i < dim; i++) {
        sum += out[i] * out[i];
    }
    if (sum > 0) {
        sum = sqrt(sum);
        for (int i = 0; i < dim; i++) {
            out[i] /= sum;
        }
    }
}

// Embed single text
int64_t embedding_embed(int64_t model_ptr, int64_t text_ptr) {
    EmbeddingModel* model = (EmbeddingModel*)model_ptr;
    SxString* text = (SxString*)text_ptr;
    if (!model || !text) return 0;
    
    pthread_mutex_lock(&model->lock);
    
    Embedding* emb = (Embedding*)malloc(sizeof(Embedding));
    if (!emb) {
        pthread_mutex_unlock(&model->lock);
        return 0;
    }
    
    emb->dim = model->dim;
    emb->values = (double*)malloc(emb->dim * sizeof(double));
    if (!emb->values) {
        free(emb);
        pthread_mutex_unlock(&model->lock);
        return 0;
    }
    
    compute_simple_embedding(text->data, emb->values, emb->dim);
    
    pthread_mutex_unlock(&model->lock);
    return (int64_t)emb;
}

// Batch embed texts
int64_t embedding_batch_embed(int64_t model_ptr, int64_t texts_ptr) {
    EmbeddingModel* model = (EmbeddingModel*)model_ptr;
    SxVec* texts = (SxVec*)texts_ptr;
    if (!model || !texts) return 0;
    
    SxVec* results = intrinsic_vec_new();
    
    for (int i = 0; i < texts->len; i++) {
        SxString* text = (SxString*)texts->items[i];
        int64_t emb = embedding_embed(model_ptr, (int64_t)text);
        intrinsic_vec_push(results, (void*)emb);
    }
    
    return (int64_t)results;
}

// Get embedding dimension
int64_t embedding_dim(int64_t emb_ptr) {
    Embedding* emb = (Embedding*)emb_ptr;
    return emb ? emb->dim : 0;
}

// Get embedding value at index
double embedding_get(int64_t emb_ptr, int64_t idx) {
    Embedding* emb = (Embedding*)emb_ptr;
    if (!emb || idx < 0 || idx >= emb->dim) return 0.0;
    return emb->values[idx];
}

// Cosine similarity between embeddings
double embedding_cosine_similarity(int64_t emb1_ptr, int64_t emb2_ptr) {
    Embedding* e1 = (Embedding*)emb1_ptr;
    Embedding* e2 = (Embedding*)emb2_ptr;
    if (!e1 || !e2 || e1->dim != e2->dim) return 0.0;
    
    double dot = 0, norm1 = 0, norm2 = 0;
    for (int i = 0; i < e1->dim; i++) {
        dot += e1->values[i] * e2->values[i];
        norm1 += e1->values[i] * e1->values[i];
        norm2 += e2->values[i] * e2->values[i];
    }
    
    if (norm1 <= 0 || norm2 <= 0) return 0.0;
    return dot / (sqrt(norm1) * sqrt(norm2));
}

// Free embedding
void embedding_free(int64_t emb_ptr) {
    Embedding* emb = (Embedding*)emb_ptr;
    if (!emb) return;
    if (emb->values) free(emb->values);
    free(emb);
}

// Close model
void embedding_model_close(int64_t model_ptr) {
    EmbeddingModel* model = (EmbeddingModel*)model_ptr;
    if (!model) return;
    if (model->model_path) free(model->model_path);
    pthread_mutex_destroy(&model->lock);
    free(model);
}

// --------------------------------------------------------------------------
// 26.2 HNSW Index (Hierarchical Navigable Small World)
// --------------------------------------------------------------------------

#define HNSW_M 16           // Max connections per layer
#define HNSW_EF_CONSTRUCTION 200  // Size during construction
#define HNSW_MAX_LEVEL 16

typedef struct HNSWNode {
    int id;
    Embedding* embedding;
    void* data;
    int level;
    int* neighbors[HNSW_MAX_LEVEL];  // Neighbors at each level
    int neighbor_count[HNSW_MAX_LEVEL];
} HNSWNode;

typedef struct HNSW {
    HNSWNode** nodes;
    int node_count;
    int node_capacity;
    int entry_point;
    int max_level;
    int M;
    int ef_construction;
    pthread_mutex_t lock;
} HNSW;

// Create HNSW index
int64_t hnsw_new(void) {
    HNSW* idx = (HNSW*)malloc(sizeof(HNSW));
    if (!idx) return 0;
    
    idx->node_capacity = 1024;
    idx->nodes = (HNSWNode**)calloc(idx->node_capacity, sizeof(HNSWNode*));
    idx->node_count = 0;
    idx->entry_point = -1;
    idx->max_level = 0;
    idx->M = HNSW_M;
    idx->ef_construction = HNSW_EF_CONSTRUCTION;
    pthread_mutex_init(&idx->lock, NULL);
    
    return (int64_t)idx;
}

// Random level for new node
static int hnsw_random_level(int M) {
    double r = (double)rand() / RAND_MAX;
    double ml = 1.0 / log((double)M);
    return (int)(-log(r) * ml);
}

// Distance between embeddings (1 - cosine for similarity search)
static double hnsw_distance(Embedding* e1, Embedding* e2) {
    return 1.0 - embedding_cosine_similarity((int64_t)e1, (int64_t)e2);
}

// Insert into HNSW
int64_t hnsw_insert(int64_t idx_ptr, int64_t emb_ptr, int64_t data_ptr) {
    HNSW* idx = (HNSW*)idx_ptr;
    Embedding* emb = (Embedding*)emb_ptr;
    if (!idx || !emb) return -1;
    
    pthread_mutex_lock(&idx->lock);
    
    // Resize if needed
    if (idx->node_count >= idx->node_capacity) {
        idx->node_capacity *= 2;
        idx->nodes = (HNSWNode**)realloc(idx->nodes, 
                                         idx->node_capacity * sizeof(HNSWNode*));
    }
    
    // Create node
    HNSWNode* node = (HNSWNode*)malloc(sizeof(HNSWNode));
    node->id = idx->node_count;
    node->embedding = emb;
    node->data = (void*)data_ptr;
    node->level = hnsw_random_level(idx->M);
    
    if (node->level > HNSW_MAX_LEVEL - 1) node->level = HNSW_MAX_LEVEL - 1;
    
    // Initialize neighbor lists
    for (int l = 0; l <= node->level; l++) {
        node->neighbors[l] = (int*)calloc(idx->M * 2, sizeof(int));
        node->neighbor_count[l] = 0;
    }
    for (int l = node->level + 1; l < HNSW_MAX_LEVEL; l++) {
        node->neighbors[l] = NULL;
        node->neighbor_count[l] = 0;
    }
    
    idx->nodes[idx->node_count] = node;
    
    // If first node
    if (idx->entry_point < 0) {
        idx->entry_point = node->id;
        idx->max_level = node->level;
        idx->node_count++;
        pthread_mutex_unlock(&idx->lock);
        return node->id;
    }
    
    // Find entry point at top level
    int curr = idx->entry_point;
    
    // Search down from top level
    for (int level = idx->max_level; level > node->level; level--) {
        // Greedy search at this level
        int changed = 1;
        while (changed) {
            changed = 0;
            HNSWNode* curr_node = idx->nodes[curr];
            double curr_dist = hnsw_distance(emb, curr_node->embedding);
            
            for (int i = 0; i < curr_node->neighbor_count[level]; i++) {
                int neighbor = curr_node->neighbors[level][i];
                double neighbor_dist = hnsw_distance(emb, idx->nodes[neighbor]->embedding);
                if (neighbor_dist < curr_dist) {
                    curr = neighbor;
                    curr_dist = neighbor_dist;
                    changed = 1;
                }
            }
        }
    }
    
    // Insert at each level
    for (int level = (node->level < idx->max_level ? node->level : idx->max_level); level >= 0; level--) {
        // Simple insertion: just add as neighbor if space
        HNSWNode* curr_node = idx->nodes[curr];
        
        if (curr_node->neighbor_count[level] < idx->M * 2) {
            curr_node->neighbors[level][curr_node->neighbor_count[level]++] = node->id;
        }
        
        if (node->neighbor_count[level] < idx->M * 2) {
            node->neighbors[level][node->neighbor_count[level]++] = curr;
        }
    }
    
    // Update entry point if new node has higher level
    if (node->level > idx->max_level) {
        idx->entry_point = node->id;
        idx->max_level = node->level;
    }
    
    idx->node_count++;
    pthread_mutex_unlock(&idx->lock);
    
    return node->id;
}

// Search HNSW
int64_t hnsw_search(int64_t idx_ptr, int64_t query_emb_ptr, int64_t k) {
    HNSW* idx = (HNSW*)idx_ptr;
    Embedding* query = (Embedding*)query_emb_ptr;
    if (!idx || !query || idx->node_count == 0) return 0;
    
    pthread_mutex_lock(&idx->lock);
    
    // Find entry point
    int curr = idx->entry_point;
    
    // Descend to level 0
    for (int level = idx->max_level; level > 0; level--) {
        int changed = 1;
        while (changed) {
            changed = 0;
            HNSWNode* curr_node = idx->nodes[curr];
            double curr_dist = hnsw_distance(query, curr_node->embedding);
            
            for (int i = 0; i < curr_node->neighbor_count[level]; i++) {
                int neighbor = curr_node->neighbors[level][i];
                double neighbor_dist = hnsw_distance(query, idx->nodes[neighbor]->embedding);
                if (neighbor_dist < curr_dist) {
                    curr = neighbor;
                    changed = 1;
                    break;
                }
            }
        }
    }
    
    // Collect k nearest at level 0
    SxVec* results = intrinsic_vec_new();
    
    // Simple: collect all candidates from current node's neighbors
    int* candidates = (int*)malloc((k + idx->M * 2) * sizeof(int));
    double* distances = (double*)malloc((k + idx->M * 2) * sizeof(double));
    int candidate_count = 0;
    
    // Add current and neighbors
    candidates[candidate_count] = curr;
    distances[candidate_count] = hnsw_distance(query, idx->nodes[curr]->embedding);
    candidate_count++;
    
    HNSWNode* curr_node = idx->nodes[curr];
    for (int i = 0; i < curr_node->neighbor_count[0] && candidate_count < k + idx->M * 2; i++) {
        int neighbor = curr_node->neighbors[0][i];
        candidates[candidate_count] = neighbor;
        distances[candidate_count] = hnsw_distance(query, idx->nodes[neighbor]->embedding);
        candidate_count++;
    }
    
    // Sort by distance (simple bubble sort for small k)
    for (int i = 0; i < candidate_count - 1; i++) {
        for (int j = 0; j < candidate_count - i - 1; j++) {
            if (distances[j] > distances[j + 1]) {
                double td = distances[j];
                distances[j] = distances[j + 1];
                distances[j + 1] = td;
                int tc = candidates[j];
                candidates[j] = candidates[j + 1];
                candidates[j + 1] = tc;
            }
        }
    }
    
    // Return top k
    for (int i = 0; i < k && i < candidate_count; i++) {
        intrinsic_vec_push(results, (void*)(int64_t)candidates[i]);
    }
    
    free(candidates);
    free(distances);
    
    pthread_mutex_unlock(&idx->lock);
    return (int64_t)results;
}

// Get node count
int64_t hnsw_count(int64_t idx_ptr) {
    HNSW* idx = (HNSW*)idx_ptr;
    return idx ? idx->node_count : 0;
}

// Get node data by ID
int64_t hnsw_get_data(int64_t idx_ptr, int64_t node_id) {
    HNSW* idx = (HNSW*)idx_ptr;
    if (!idx || node_id < 0 || node_id >= idx->node_count) return 0;
    return (int64_t)idx->nodes[node_id]->data;
}

// Close HNSW
void hnsw_close(int64_t idx_ptr) {
    HNSW* idx = (HNSW*)idx_ptr;
    if (!idx) return;
    
    pthread_mutex_lock(&idx->lock);
    
    for (int i = 0; i < idx->node_count; i++) {
        HNSWNode* node = idx->nodes[i];
        for (int l = 0; l <= node->level; l++) {
            if (node->neighbors[l]) free(node->neighbors[l]);
        }
        // Note: don't free embedding as it may be shared
        free(node);
    }
    free(idx->nodes);
    
    pthread_mutex_unlock(&idx->lock);
    pthread_mutex_destroy(&idx->lock);
    free(idx);
}

// ==========================================================================
// Phase 3: simplex-sql - REMOVED (SQLite legacy data store)
// Simplex is AI-first: the SLM is the data layer, not SQLite.
// These stubs preserve ABI compatibility for any compiled .ll files.
// ==========================================================================

// Stub implementations - all return error/zero
int64_t sql_open(int64_t path_ptr) { (void)path_ptr; return 0; }
int64_t sql_open_memory(void) { return 0; }
void sql_close(int64_t db_ptr) { (void)db_ptr; }
int64_t sql_execute(int64_t db_ptr, int64_t query_ptr) { (void)db_ptr; (void)query_ptr; return -1; }
int64_t sql_error(int64_t db_ptr) { (void)db_ptr; return (int64_t)intrinsic_string_new("SQLite removed: use SLM data layer"); }
int64_t sql_prepare(int64_t db_ptr, int64_t query_ptr) { (void)db_ptr; (void)query_ptr; return 0; }
int64_t sql_bind_int(int64_t stmt_ptr, int64_t index, int64_t value) { (void)stmt_ptr; (void)index; (void)value; return -1; }
int64_t sql_bind_text(int64_t stmt_ptr, int64_t index, int64_t text_ptr) { (void)stmt_ptr; (void)index; (void)text_ptr; return -1; }
int64_t sql_bind_double(int64_t stmt_ptr, int64_t index, double value) { (void)stmt_ptr; (void)index; (void)value; return -1; }
int64_t sql_bind_null(int64_t stmt_ptr, int64_t index) { (void)stmt_ptr; (void)index; return -1; }
int64_t sql_step(int64_t stmt_ptr) { (void)stmt_ptr; return -1; }
int64_t sql_reset(int64_t stmt_ptr) { (void)stmt_ptr; return -1; }
int64_t sql_column_count(int64_t stmt_ptr) { (void)stmt_ptr; return 0; }
int64_t sql_column_type(int64_t stmt_ptr, int64_t index) { (void)stmt_ptr; (void)index; return 0; }
int64_t sql_column_name(int64_t stmt_ptr, int64_t index) { (void)stmt_ptr; (void)index; return (int64_t)intrinsic_string_new(""); }
int64_t sql_column_int(int64_t stmt_ptr, int64_t index) { (void)stmt_ptr; (void)index; return 0; }
int64_t sql_column_text(int64_t stmt_ptr, int64_t index) { (void)stmt_ptr; (void)index; return (int64_t)intrinsic_string_new(""); }
double sql_column_double(int64_t stmt_ptr, int64_t index) { (void)stmt_ptr; (void)index; return 0.0; }
int64_t sql_column_blob(int64_t stmt_ptr, int64_t index) { (void)stmt_ptr; (void)index; return 0; }
int64_t sql_column_blob_len(int64_t stmt_ptr, int64_t index) { (void)stmt_ptr; (void)index; return 0; }
void sql_finalize(int64_t stmt_ptr) { (void)stmt_ptr; }
int64_t sql_begin(int64_t db_ptr) { (void)db_ptr; return -1; }
int64_t sql_commit(int64_t db_ptr) { (void)db_ptr; return -1; }
int64_t sql_rollback(int64_t db_ptr) { (void)db_ptr; return -1; }
int64_t sql_last_insert_id(int64_t db_ptr) { (void)db_ptr; return 0; }
int64_t sql_changes(int64_t db_ptr) { (void)db_ptr; return 0; }
int64_t sql_column_is_null(int64_t stmt_ptr, int64_t index) { (void)stmt_ptr; (void)index; return 1; }
int64_t sql_total_changes(int64_t db_ptr) { (void)db_ptr; return 0; }

// ==========================================================================
// Phase 3: simplex-regex - POSIX Regex API
// ==========================================================================

#include <regex.h>

// Compiled regex structure
typedef struct SxRegex {
    regex_t compiled;
    int flags;
    int compiled_ok;
} SxRegex;

// Compile a regex pattern
// flags: 0 = default, 1 = case insensitive
int64_t regex_new(int64_t pattern_ptr, int64_t flags) {
    SxString* pattern = (SxString*)pattern_ptr;
    if (!pattern || !pattern->data) return 0;

    SxRegex* rx = (SxRegex*)malloc(sizeof(SxRegex));
    if (!rx) return 0;

    int cflags = REG_EXTENDED;
    if (flags & 1) cflags |= REG_ICASE;
    if (flags & 2) cflags |= REG_NEWLINE;

    rx->flags = (int)flags;
    int result = regcomp(&rx->compiled, pattern->data, cflags);
    rx->compiled_ok = (result == 0);

    if (result != 0) {
        free(rx);
        return 0;
    }

    return (int64_t)rx;
}

// Free a compiled regex
void regex_free(int64_t rx_ptr) {
    SxRegex* rx = (SxRegex*)rx_ptr;
    if (rx) {
        if (rx->compiled_ok) {
            regfree(&rx->compiled);
        }
        free(rx);
    }
}

// Check if pattern matches string (returns 1 if match, 0 if no match)
int64_t regex_is_match(int64_t rx_ptr, int64_t text_ptr) {
    SxRegex* rx = (SxRegex*)rx_ptr;
    SxString* text = (SxString*)text_ptr;
    if (!rx || !rx->compiled_ok || !text || !text->data) return 0;

    return regexec(&rx->compiled, text->data, 0, NULL, 0) == 0 ? 1 : 0;
}

// Find first match position
// Returns: [start, end] as packed i64 (start << 32 | end), or -1 if no match
int64_t regex_find(int64_t rx_ptr, int64_t text_ptr) {
    SxRegex* rx = (SxRegex*)rx_ptr;
    SxString* text = (SxString*)text_ptr;
    if (!rx || !rx->compiled_ok || !text || !text->data) return -1;

    regmatch_t match;
    if (regexec(&rx->compiled, text->data, 1, &match, 0) != 0) {
        return -1;
    }

    // Pack start and end into single i64
    return ((int64_t)match.rm_so << 32) | (match.rm_eo & 0xFFFFFFFF);
}

// Get the matched substring
int64_t regex_find_str(int64_t rx_ptr, int64_t text_ptr) {
    SxRegex* rx = (SxRegex*)rx_ptr;
    SxString* text = (SxString*)text_ptr;
    if (!rx || !rx->compiled_ok || !text || !text->data) {
        return (int64_t)intrinsic_string_new("");
    }

    regmatch_t match;
    if (regexec(&rx->compiled, text->data, 1, &match, 0) != 0) {
        return (int64_t)intrinsic_string_new("");
    }

    int len = match.rm_eo - match.rm_so;
    char* result = (char*)malloc(len + 1);
    strncpy(result, text->data + match.rm_so, len);
    result[len] = '\0';

    SxString* s = intrinsic_string_new(result);
    free(result);
    return (int64_t)s;
}

// Count number of matches
int64_t regex_count(int64_t rx_ptr, int64_t text_ptr) {
    SxRegex* rx = (SxRegex*)rx_ptr;
    SxString* text = (SxString*)text_ptr;
    if (!rx || !rx->compiled_ok || !text || !text->data) return 0;

    int64_t count = 0;
    const char* ptr = text->data;
    regmatch_t match;

    while (regexec(&rx->compiled, ptr, 1, &match, 0) == 0) {
        count++;
        ptr += match.rm_eo;
        if (match.rm_eo == 0) ptr++;  // Avoid infinite loop on empty match
        if (*ptr == '\0') break;
    }

    return count;
}

// Replace all matches with replacement string
int64_t regex_replace(int64_t rx_ptr, int64_t text_ptr, int64_t replacement_ptr) {
    SxRegex* rx = (SxRegex*)rx_ptr;
    SxString* text = (SxString*)text_ptr;
    SxString* replacement = (SxString*)replacement_ptr;
    if (!rx || !rx->compiled_ok || !text || !text->data || !replacement) {
        return text_ptr;
    }

    // Build result string
    size_t result_capacity = text->len * 2 + 1;
    char* result = (char*)malloc(result_capacity);
    size_t result_len = 0;

    const char* ptr = text->data;
    regmatch_t match;

    while (regexec(&rx->compiled, ptr, 1, &match, 0) == 0) {
        // Copy text before match
        size_t before_len = match.rm_so;
        if (result_len + before_len + replacement->len >= result_capacity) {
            result_capacity = (result_len + before_len + replacement->len) * 2;
            result = (char*)realloc(result, result_capacity);
        }
        memcpy(result + result_len, ptr, before_len);
        result_len += before_len;

        // Copy replacement
        memcpy(result + result_len, replacement->data, replacement->len);
        result_len += replacement->len;

        ptr += match.rm_eo;
        if (match.rm_eo == 0) ptr++;
        if (*ptr == '\0') break;
    }

    // Copy remaining text
    size_t remaining = strlen(ptr);
    if (result_len + remaining >= result_capacity) {
        result_capacity = result_len + remaining + 1;
        result = (char*)realloc(result, result_capacity);
    }
    memcpy(result + result_len, ptr, remaining);
    result_len += remaining;
    result[result_len] = '\0';

    SxString* s = intrinsic_string_new(result);
    free(result);
    return (int64_t)s;
}

// Replace first match only
int64_t regex_replace_first(int64_t rx_ptr, int64_t text_ptr, int64_t replacement_ptr) {
    SxRegex* rx = (SxRegex*)rx_ptr;
    SxString* text = (SxString*)text_ptr;
    SxString* replacement = (SxString*)replacement_ptr;
    if (!rx || !rx->compiled_ok || !text || !text->data || !replacement) {
        return text_ptr;
    }

    regmatch_t match;
    if (regexec(&rx->compiled, text->data, 1, &match, 0) != 0) {
        return text_ptr;  // No match, return original
    }

    size_t new_len = text->len - (match.rm_eo - match.rm_so) + replacement->len;
    char* result = (char*)malloc(new_len + 1);

    // Copy before match
    memcpy(result, text->data, match.rm_so);
    // Copy replacement
    memcpy(result + match.rm_so, replacement->data, replacement->len);
    // Copy after match
    strcpy(result + match.rm_so + replacement->len, text->data + match.rm_eo);

    SxString* s = intrinsic_string_new(result);
    free(result);
    return (int64_t)s;
}

// Split string by regex pattern
int64_t regex_split(int64_t rx_ptr, int64_t text_ptr) {
    SxRegex* rx = (SxRegex*)rx_ptr;
    SxString* text = (SxString*)text_ptr;
    if (!rx || !rx->compiled_ok || !text || !text->data) {
        return (int64_t)intrinsic_vec_new();
    }

    // Create a Vec<String> to hold results
    SxVec* list = intrinsic_vec_new();

    const char* ptr = text->data;
    const char* start = ptr;
    regmatch_t match;

    while (regexec(&rx->compiled, ptr, 1, &match, 0) == 0) {
        // Extract text before match
        int len = (ptr - start) + match.rm_so;
        if (len > 0) {
            char* part = (char*)malloc(len + 1);
            strncpy(part, start, len);
            part[len] = '\0';
            SxString* s = intrinsic_string_new(part);
            free(part);
            intrinsic_vec_push(list, s);
        }

        ptr += match.rm_eo;
        start = ptr;
        if (match.rm_eo == 0) { ptr++; start++; }
        if (*ptr == '\0') break;
    }

    // Add remaining text
    if (*start != '\0') {
        SxString* s = intrinsic_string_new(start);
        intrinsic_vec_push(list, s);
    }

    return (int64_t)list;
}

// Get error message for failed regex compilation
int64_t regex_error(int64_t pattern_ptr) {
    SxString* pattern = (SxString*)pattern_ptr;
    if (!pattern || !pattern->data) {
        return (int64_t)intrinsic_string_new("Invalid pattern");
    }

    regex_t rx;
    int result = regcomp(&rx, pattern->data, REG_EXTENDED);
    if (result == 0) {
        regfree(&rx);
        return (int64_t)intrinsic_string_new("");
    }

    char errbuf[256];
    regerror(result, &rx, errbuf, sizeof(errbuf));
    return (int64_t)intrinsic_string_new(errbuf);
}

// Capture group support - get number of capture groups
int64_t regex_group_count(int64_t rx_ptr) {
    SxRegex* rx = (SxRegex*)rx_ptr;
    if (!rx || !rx->compiled_ok) return 0;
    return rx->compiled.re_nsub;
}

// Capture groups - returns array of matched groups
int64_t regex_captures(int64_t rx_ptr, int64_t text_ptr) {
    SxRegex* rx = (SxRegex*)rx_ptr;
    SxString* text = (SxString*)text_ptr;
    if (!rx || !rx->compiled_ok || !text || !text->data) return 0;

    size_t nmatch = rx->compiled.re_nsub + 1;
    regmatch_t* matches = (regmatch_t*)malloc(nmatch * sizeof(regmatch_t));

    if (regexec(&rx->compiled, text->data, nmatch, matches, 0) != 0) {
        free(matches);
        return 0;
    }

    SxVec* list = intrinsic_vec_new();

    for (size_t i = 0; i < nmatch; i++) {
        if (matches[i].rm_so >= 0) {
            int len = matches[i].rm_eo - matches[i].rm_so;
            char* part = (char*)malloc(len + 1);
            strncpy(part, text->data + matches[i].rm_so, len);
            part[len] = '\0';
            SxString* s = intrinsic_string_new(part);
            free(part);
            intrinsic_vec_push(list, s);
        } else {
            intrinsic_vec_push(list, intrinsic_string_new(""));
        }
    }

    free(matches);
    return (int64_t)list;
}

// ==========================================================================
// Phase 3: simplex-crypto - Cryptography API
// ==========================================================================

#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>

// Generate cryptographically secure random bytes as hex string
int64_t crypto_random_bytes(int64_t length) {
    if (length <= 0 || length > 1024 * 1024) return (int64_t)intrinsic_string_new("");

    unsigned char* buf = (unsigned char*)malloc(length);
    if (!buf) return (int64_t)intrinsic_string_new("");

    if (RAND_bytes(buf, (int)length) != 1) {
        free(buf);
        return (int64_t)intrinsic_string_new("");
    }

    // Convert to hex string
    char* hex = (char*)malloc(length * 2 + 1);
    if (!hex) {
        free(buf);
        return (int64_t)intrinsic_string_new("");
    }

    for (int64_t i = 0; i < length; i++) {
        sprintf(hex + i * 2, "%02x", buf[i]);
    }
    hex[length * 2] = '\0';

    SxString* result = intrinsic_string_new(hex);
    free(hex);
    free(buf);
    return (int64_t)result;
}

// SHA-256 hash (using OpenSSL)
int64_t crypto_sha256(int64_t data_ptr) {
    SxString* data = (SxString*)data_ptr;
    if (!data || !data->data) return (int64_t)intrinsic_string_new("");

    unsigned char hash[SHA256_DIGEST_LENGTH];
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return (int64_t)intrinsic_string_new("");

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
        EVP_DigestUpdate(ctx, data->data, data->len) != 1 ||
        EVP_DigestFinal_ex(ctx, hash, NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return (int64_t)intrinsic_string_new("");
    }

    EVP_MD_CTX_free(ctx);

    char hex[SHA256_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(hex + i * 2, "%02x", hash[i]);
    }
    hex[SHA256_DIGEST_LENGTH * 2] = '\0';

    return (int64_t)intrinsic_string_new(hex);
}

// SHA-512 hash
int64_t crypto_sha512(int64_t data_ptr) {
    SxString* data = (SxString*)data_ptr;
    if (!data || !data->data) return (int64_t)intrinsic_string_new("");

    unsigned char hash[SHA512_DIGEST_LENGTH];
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return (int64_t)intrinsic_string_new("");

    if (EVP_DigestInit_ex(ctx, EVP_sha512(), NULL) != 1 ||
        EVP_DigestUpdate(ctx, data->data, data->len) != 1 ||
        EVP_DigestFinal_ex(ctx, hash, NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return (int64_t)intrinsic_string_new("");
    }

    EVP_MD_CTX_free(ctx);

    char hex[SHA512_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA512_DIGEST_LENGTH; i++) {
        sprintf(hex + i * 2, "%02x", hash[i]);
    }
    hex[SHA512_DIGEST_LENGTH * 2] = '\0';

    return (int64_t)intrinsic_string_new(hex);
}

// HMAC-SHA256
int64_t crypto_hmac_sha256(int64_t key_ptr, int64_t data_ptr) {
    SxString* key = (SxString*)key_ptr;
    SxString* data = (SxString*)data_ptr;
    if (!key || !key->data || !data || !data->data) {
        return (int64_t)intrinsic_string_new("");
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;

    HMAC(EVP_sha256(), key->data, key->len,
         (unsigned char*)data->data, data->len, hash, &hash_len);

    char hex[65];
    for (unsigned int i = 0; i < hash_len; i++) {
        sprintf(hex + i * 2, "%02x", hash[i]);
    }
    hex[hash_len * 2] = '\0';

    return (int64_t)intrinsic_string_new(hex);
}

// Base64 encoding
int64_t crypto_base64_encode(int64_t data_ptr) {
    SxString* data = (SxString*)data_ptr;
    if (!data || !data->data || data->len == 0) {
        return (int64_t)intrinsic_string_new("");
    }

    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    b64 = BIO_push(b64, mem);

    BIO_write(b64, data->data, data->len);
    BIO_flush(b64);

    BUF_MEM* bptr;
    BIO_get_mem_ptr(b64, &bptr);

    char* result = (char*)malloc(bptr->length + 1);
    memcpy(result, bptr->data, bptr->length);
    result[bptr->length] = '\0';

    BIO_free_all(b64);

    SxString* s = intrinsic_string_new(result);
    free(result);
    return (int64_t)s;
}

// Base64 decoding
int64_t crypto_base64_decode(int64_t encoded_ptr) {
    SxString* encoded = (SxString*)encoded_ptr;
    if (!encoded || !encoded->data || encoded->len == 0) {
        return (int64_t)intrinsic_string_new("");
    }

    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new_mem_buf(encoded->data, encoded->len);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    mem = BIO_push(b64, mem);

    char* result = (char*)malloc(encoded->len + 1);
    int len = BIO_read(mem, result, encoded->len);
    BIO_free_all(mem);

    if (len < 0) {
        free(result);
        return (int64_t)intrinsic_string_new("");
    }

    result[len] = '\0';
    SxString* s = intrinsic_string_new(result);
    free(result);
    return (int64_t)s;
}

// Hex encoding
int64_t crypto_hex_encode(int64_t data_ptr) {
    SxString* data = (SxString*)data_ptr;
    if (!data || !data->data) return (int64_t)intrinsic_string_new("");

    char* hex = (char*)malloc(data->len * 2 + 1);
    for (int64_t i = 0; i < data->len; i++) {
        sprintf(hex + i * 2, "%02x", (unsigned char)data->data[i]);
    }
    hex[data->len * 2] = '\0';

    SxString* s = intrinsic_string_new(hex);
    free(hex);
    return (int64_t)s;
}

// Hex decoding
int64_t crypto_hex_decode(int64_t hex_ptr) {
    SxString* hex = (SxString*)hex_ptr;
    if (!hex || !hex->data || hex->len == 0 || hex->len % 2 != 0) {
        return (int64_t)intrinsic_string_new("");
    }

    int64_t out_len = hex->len / 2;
    char* result = (char*)malloc(out_len + 1);

    for (int64_t i = 0; i < out_len; i++) {
        unsigned int byte;
        sscanf(hex->data + i * 2, "%2x", &byte);
        result[i] = (char)byte;
    }
    result[out_len] = '\0';

    SxString* s = intrinsic_string_new(result);
    free(result);
    return (int64_t)s;
}

// Constant-time comparison (for MAC verification)
int64_t crypto_compare(int64_t a_ptr, int64_t b_ptr) {
    SxString* a = (SxString*)a_ptr;
    SxString* b = (SxString*)b_ptr;
    if (!a || !a->data || !b || !b->data) return 0;
    if (a->len != b->len) return 0;

    volatile int result = 0;
    for (int64_t i = 0; i < a->len; i++) {
        result |= a->data[i] ^ b->data[i];
    }
    return result == 0 ? 1 : 0;
}

// --------------------------------------------------------------------------
// Phase 3: CLI API
// --------------------------------------------------------------------------

// Get number of command-line arguments
int64_t cli_arg_count(void) {
    return program_argc;
}

// Get command-line argument by index
int64_t cli_get_arg(int64_t index) {
    if (index < 0 || index >= program_argc) {
        return (int64_t)intrinsic_string_new("");
    }
    return (int64_t)intrinsic_string_new(program_argv[index]);
}

// Get all arguments as a vector
int64_t cli_args(void) {
    SxVec* args = intrinsic_vec_new();
    for (int i = 0; i < program_argc; i++) {
        intrinsic_vec_push(args, intrinsic_string_new(program_argv[i]));
    }
    return (int64_t)args;
}

// Get environment variable
int64_t cli_getenv(int64_t name_ptr) {
    SxString* name = (SxString*)name_ptr;
    if (!name || !name->data) return (int64_t)intrinsic_string_new("");

    const char* value = getenv(name->data);
    if (!value) return (int64_t)intrinsic_string_new("");
    return (int64_t)intrinsic_string_new(value);
}

// Alias for env_get (used by sxc.sx and other toolchain files)
int64_t env_get(int64_t name_ptr) {
    return cli_getenv(name_ptr);
}

// Set environment variable
int64_t cli_setenv(int64_t name_ptr, int64_t value_ptr) {
    SxString* name = (SxString*)name_ptr;
    SxString* value = (SxString*)value_ptr;
    if (!name || !name->data || !value || !value->data) return -1;

    return setenv(name->data, value->data, 1);
}

// Read file contents (wrapper for intrinsic_read_file with i64 signature)
int64_t file_read(int64_t path_ptr) {
    SxString* path = (SxString*)path_ptr;
    SxString* result = intrinsic_read_file(path);
    return (int64_t)result;
}

// Write file contents (wrapper for intrinsic_write_file with i64 signature)
void file_write(int64_t path_ptr, int64_t content_ptr) {
    SxString* path = (SxString*)path_ptr;
    SxString* content = (SxString*)content_ptr;
    intrinsic_write_file(path, content);
}

// Check if file exists
int64_t file_exists(int64_t path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path || !path->data) return 0;
    struct stat st;
    return stat(path->data, &st) == 0 ? 1 : 0;
}

// Delete a file
int64_t file_delete(int64_t path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path || !path->data) return -1;
    return unlink(path->data);
}

// String length wrapper
int64_t string_len(int64_t str_ptr) {
    return intrinsic_string_len((SxString*)str_ptr);
}

// String concatenation wrapper
int64_t string_concat(int64_t a_ptr, int64_t b_ptr) {
    return (int64_t)intrinsic_string_concat((SxString*)a_ptr, (SxString*)b_ptr);
}

// Remove path (file or directory)
int64_t remove_path(int64_t path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path || !path->data) return -1;
    return remove(path->data);
}

// Get current working directory
int64_t cli_cwd(void) {
    char buf[4096];
    if (getcwd(buf, sizeof(buf)) == NULL) {
        return (int64_t)intrinsic_string_new("");
    }
    return (int64_t)intrinsic_string_new(buf);
}

// Exit with code
void cli_exit(int64_t code) {
    exit((int)code);
}

// Check if a flag exists (--flag or -f)
int64_t cli_has_flag(int64_t flag_ptr) {
    SxString* flag = (SxString*)flag_ptr;
    if (!flag || !flag->data) return 0;

    for (int i = 1; i < program_argc; i++) {
        if (strcmp(program_argv[i], flag->data) == 0) {
            return 1;
        }
    }
    return 0;
}

// Get option value (--name=value or --name value)
int64_t cli_get_option(int64_t name_ptr) {
    SxString* name = (SxString*)name_ptr;
    if (!name || !name->data) return (int64_t)intrinsic_string_new("");

    for (int i = 1; i < program_argc; i++) {
        // Check for --name=value format
        size_t name_len = strlen(name->data);
        if (strncmp(program_argv[i], name->data, name_len) == 0) {
            if (program_argv[i][name_len] == '=') {
                return (int64_t)intrinsic_string_new(program_argv[i] + name_len + 1);
            }
            // Check for --name value format (next argument)
            if (program_argv[i][name_len] == '\0' && i + 1 < program_argc) {
                // Make sure next arg is not a flag
                if (program_argv[i + 1][0] != '-') {
                    return (int64_t)intrinsic_string_new(program_argv[i + 1]);
                }
            }
        }
    }
    return (int64_t)intrinsic_string_new("");
}

// Get positional arguments (non-flag arguments)
int64_t cli_positional_args(void) {
    SxVec* args = intrinsic_vec_new();
    for (int i = 1; i < program_argc; i++) {
        if (program_argv[i][0] != '-') {
            intrinsic_vec_push(args, intrinsic_string_new(program_argv[i]));
        } else {
            // Skip option values (--name value)
            if (i + 1 < program_argc && program_argv[i + 1][0] != '-' &&
                strchr(program_argv[i], '=') == NULL) {
                i++; // Skip the value
            }
        }
    }
    return (int64_t)args;
}

// --------------------------------------------------------------------------
// Phase 3: Simple Log API (slog_ prefix to avoid conflict with Logger API)
// --------------------------------------------------------------------------

// Log levels: 0=TRACE, 1=DEBUG, 2=INFO, 3=WARN, 4=ERROR
static int64_t slog_min_level = 2; // Default: INFO

// Set minimum log level
void slog_set_level(int64_t level) {
    slog_min_level = level;
}

// Get current log level
int64_t slog_get_level(void) {
    return slog_min_level;
}

// Internal: format and print log message
static void slog_print_msg(const char* level_str, int64_t msg_ptr) {
    SxString* msg = (SxString*)msg_ptr;
    if (!msg || !msg->data) return;

    // Get timestamp
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    // Print: [LEVEL] timestamp: message
    printf("[%s] %s: %s\n", level_str, timestamp, msg->data);
    fflush(stdout);
}

// Log at TRACE level (0)
void slog_trace(int64_t msg_ptr) {
    if (slog_min_level <= 0) {
        slog_print_msg("TRACE", msg_ptr);
    }
}

// Log at DEBUG level (1)
void slog_debug(int64_t msg_ptr) {
    if (slog_min_level <= 1) {
        slog_print_msg("DEBUG", msg_ptr);
    }
}

// Log at INFO level (2)
void slog_info(int64_t msg_ptr) {
    if (slog_min_level <= 2) {
        slog_print_msg("INFO", msg_ptr);
    }
}

// Log at WARN level (3)
void slog_warn(int64_t msg_ptr) {
    if (slog_min_level <= 3) {
        slog_print_msg("WARN", msg_ptr);
    }
}

// Log at ERROR level (4)
void slog_error(int64_t msg_ptr) {
    if (slog_min_level <= 4) {
        slog_print_msg("ERROR", msg_ptr);
    }
}

// Log with key-value context
void slog_info_ctx(int64_t msg_ptr, int64_t key_ptr, int64_t val_ptr) {
    if (slog_min_level > 2) return;

    SxString* msg = (SxString*)msg_ptr;
    SxString* key = (SxString*)key_ptr;
    SxString* val = (SxString*)val_ptr;
    if (!msg || !msg->data) return;

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    if (key && key->data && val && val->data) {
        printf("[INFO] %s: %s {%s=%s}\n", timestamp, msg->data, key->data, val->data);
    } else {
        printf("[INFO] %s: %s\n", timestamp, msg->data);
    }
    fflush(stdout);
}

// Log formatted message (simple string interpolation)
void slog_fmt(int64_t level, int64_t fmt_ptr, int64_t arg_ptr) {
    if (slog_min_level > level) return;

    SxString* fmt = (SxString*)fmt_ptr;
    SxString* arg = (SxString*)arg_ptr;
    if (!fmt || !fmt->data) return;

    const char* level_str = "INFO";
    switch ((int)level) {
        case 0: level_str = "TRACE"; break;
        case 1: level_str = "DEBUG"; break;
        case 2: level_str = "INFO"; break;
        case 3: level_str = "WARN"; break;
        case 4: level_str = "ERROR"; break;
    }

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    // Simple format: replace {} with arg
    char* result = (char*)malloc(strlen(fmt->data) + (arg ? arg->len : 0) + 100);
    if (!result) return;

    const char* placeholder = strstr(fmt->data, "{}");
    if (placeholder && arg && arg->data) {
        size_t prefix_len = placeholder - fmt->data;
        memcpy(result, fmt->data, prefix_len);
        strcpy(result + prefix_len, arg->data);
        strcpy(result + prefix_len + arg->len, placeholder + 2);
    } else {
        strcpy(result, fmt->data);
    }

    printf("[%s] %s: %s\n", level_str, timestamp, result);
    fflush(stdout);
    free(result);
}

// --------------------------------------------------------------------------
// Phase 3: Test Framework API (tfw_ prefix to avoid conflicts)
// --------------------------------------------------------------------------

static int64_t tfw_passed = 0;
static int64_t tfw_failed = 0;

// Reset test counters
void tfw_reset(void) {
    tfw_passed = 0;
    tfw_failed = 0;
}

// Get passed count
int64_t tfw_passed_count(void) {
    return tfw_passed;
}

// Get failed count
int64_t tfw_failed_count(void) {
    return tfw_failed;
}

// Assert condition is true
int64_t tfw_assert(int64_t condition, int64_t msg_ptr) {
    SxString* msg = (SxString*)msg_ptr;
    if (condition) {
        tfw_passed++;
        return 1;
    } else {
        tfw_failed++;
        if (msg && msg->data) {
            printf("ASSERT FAILED: %s\n", msg->data);
        } else {
            printf("ASSERT FAILED\n");
        }
        return 0;
    }
}

// Assert two integers are equal
int64_t tfw_assert_eq_i64(int64_t a, int64_t b, int64_t msg_ptr) {
    SxString* msg = (SxString*)msg_ptr;
    if (a == b) {
        tfw_passed++;
        return 1;
    } else {
        tfw_failed++;
        if (msg && msg->data) {
            printf("ASSERT EQ FAILED: %s (expected %lld, got %lld)\n", msg->data, (long long)a, (long long)b);
        } else {
            printf("ASSERT EQ FAILED: expected %lld, got %lld\n", (long long)a, (long long)b);
        }
        return 0;
    }
}

// Assert two strings are equal
int64_t tfw_assert_eq_str(int64_t a_ptr, int64_t b_ptr, int64_t msg_ptr) {
    SxString* a = (SxString*)a_ptr;
    SxString* b = (SxString*)b_ptr;
    SxString* msg = (SxString*)msg_ptr;

    int equal = (a && b && a->data && b->data && strcmp(a->data, b->data) == 0);
    if (equal) {
        tfw_passed++;
        return 1;
    } else {
        tfw_failed++;
        const char* a_str = (a && a->data) ? a->data : "(null)";
        const char* b_str = (b && b->data) ? b->data : "(null)";
        if (msg && msg->data) {
            printf("ASSERT EQ FAILED: %s (expected '%s', got '%s')\n", msg->data, a_str, b_str);
        } else {
            printf("ASSERT EQ FAILED: expected '%s', got '%s'\n", a_str, b_str);
        }
        return 0;
    }
}

// Assert two integers are not equal
int64_t tfw_assert_ne_i64(int64_t a, int64_t b, int64_t msg_ptr) {
    SxString* msg = (SxString*)msg_ptr;
    if (a != b) {
        tfw_passed++;
        return 1;
    } else {
        tfw_failed++;
        if (msg && msg->data) {
            printf("ASSERT NE FAILED: %s (both are %lld)\n", msg->data, (long long)a);
        } else {
            printf("ASSERT NE FAILED: both values are %lld\n", (long long)a);
        }
        return 0;
    }
}

// Explicitly fail a test
void tfw_fail(int64_t msg_ptr) {
    SxString* msg = (SxString*)msg_ptr;
    tfw_failed++;
    if (msg && msg->data) {
        printf("TEST FAILED: %s\n", msg->data);
    } else {
        printf("TEST FAILED\n");
    }
}

// Print test summary
void tfw_summary(void) {
    int64_t total = tfw_passed + tfw_failed;
    printf("\n=== Test Summary ===\n");
    printf("Passed: %lld / %lld\n", (long long)tfw_passed, (long long)total);
    printf("Failed: %lld / %lld\n", (long long)tfw_failed, (long long)total);
    if (tfw_failed == 0) {
        printf("All tests passed!\n");
    }
}

// --------------------------------------------------------------------------
// Phase 3: UUID API
// --------------------------------------------------------------------------

// Generate a UUID v4 (random)
int64_t uuid_v4(void) {
    unsigned char bytes[16];
    if (RAND_bytes(bytes, 16) != 1) {
        return (int64_t)intrinsic_string_new("");
    }

    // Set version (4) and variant (RFC4122)
    bytes[6] = (bytes[6] & 0x0F) | 0x40;  // Version 4
    bytes[8] = (bytes[8] & 0x3F) | 0x80;  // Variant 1

    // Format as string: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    char uuid[37];
    snprintf(uuid, sizeof(uuid),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5], bytes[6], bytes[7],
        bytes[8], bytes[9], bytes[10], bytes[11],
        bytes[12], bytes[13], bytes[14], bytes[15]);

    return (int64_t)intrinsic_string_new(uuid);
}

// Generate a nil UUID (all zeros)
int64_t uuid_nil(void) {
    return (int64_t)intrinsic_string_new("00000000-0000-0000-0000-000000000000");
}

// Check if a UUID is nil
int64_t uuid_is_nil(int64_t uuid_ptr) {
    SxString* uuid = (SxString*)uuid_ptr;
    if (!uuid || !uuid->data) return 1;
    return strcmp(uuid->data, "00000000-0000-0000-0000-000000000000") == 0 ? 1 : 0;
}

// Validate UUID format
int64_t uuid_is_valid(int64_t uuid_ptr) {
    SxString* uuid = (SxString*)uuid_ptr;
    if (!uuid || !uuid->data || uuid->len != 36) return 0;

    // Check format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    const char* s = uuid->data;
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (s[i] != '-') return 0;
        } else {
            char c = s[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                return 0;
            }
        }
    }
    return 1;
}

// --------------------------------------------------------------------------
// 26.3 Persistent Storage - REMOVED (SQLite legacy data store)
// Simplex is AI-first: the SLM is the data layer, not SQLite.
// These stubs preserve ABI compatibility.
// --------------------------------------------------------------------------

typedef struct MemoryDB {
    void* reserved;  // No longer uses sqlite3
    char* path;
    pthread_mutex_t lock;
} MemoryDB;

// Stub implementations - all return error/zero
int64_t memdb_new(int64_t path_ptr) { (void)path_ptr; return 0; }
int64_t memdb_store(int64_t mdb_ptr, int64_t content_ptr, int64_t emb_ptr, double importance) { (void)mdb_ptr; (void)content_ptr; (void)emb_ptr; (void)importance; return 0; }
int64_t memdb_get(int64_t mdb_ptr, int64_t id) { (void)mdb_ptr; (void)id; return 0; }
double memdb_get_importance(int64_t mdb_ptr, int64_t id) { (void)mdb_ptr; (void)id; return 0.0; }
int64_t memdb_set_importance(int64_t mdb_ptr, int64_t id, double importance) { (void)mdb_ptr; (void)id; (void)importance; return 0; }
int64_t memdb_count(int64_t mdb_ptr) { (void)mdb_ptr; return 0; }
void memdb_close(int64_t mdb_ptr) { (void)mdb_ptr; }

// --------------------------------------------------------------------------
// 26.4 Memory Clustering
// --------------------------------------------------------------------------

typedef struct MemoryCluster {
    int id;
    int64_t* members;
    int member_count;
    int member_capacity;
    Embedding* centroid;
} MemoryCluster;

typedef struct ClusterManager {
    MemoryCluster** clusters;
    int cluster_count;
    int cluster_capacity;
    double similarity_threshold;
    pthread_mutex_t lock;
} ClusterManager;

// Create cluster manager
int64_t cluster_manager_new(double threshold) {
    ClusterManager* cm = (ClusterManager*)malloc(sizeof(ClusterManager));
    if (!cm) return 0;
    
    cm->cluster_capacity = 64;
    cm->clusters = (MemoryCluster**)calloc(cm->cluster_capacity, sizeof(MemoryCluster*));
    cm->cluster_count = 0;
    cm->similarity_threshold = threshold > 0 ? threshold : 0.7;
    pthread_mutex_init(&cm->lock, NULL);
    
    return (int64_t)cm;
}

// Add memory to clusters
int64_t cluster_add_memory(int64_t cm_ptr, int64_t memory_id, int64_t emb_ptr) {
    ClusterManager* cm = (ClusterManager*)cm_ptr;
    Embedding* emb = (Embedding*)emb_ptr;
    if (!cm || !emb) return -1;
    
    pthread_mutex_lock(&cm->lock);
    
    // Find best matching cluster
    int best_cluster = -1;
    double best_similarity = 0;
    
    for (int i = 0; i < cm->cluster_count; i++) {
        if (cm->clusters[i]->centroid) {
            double sim = embedding_cosine_similarity((int64_t)emb, 
                                                     (int64_t)cm->clusters[i]->centroid);
            if (sim > best_similarity && sim >= cm->similarity_threshold) {
                best_similarity = sim;
                best_cluster = i;
            }
        }
    }
    
    if (best_cluster < 0) {
        // Create new cluster
        if (cm->cluster_count >= cm->cluster_capacity) {
            cm->cluster_capacity *= 2;
            cm->clusters = (MemoryCluster**)realloc(cm->clusters,
                                                    cm->cluster_capacity * sizeof(MemoryCluster*));
        }
        
        MemoryCluster* cluster = (MemoryCluster*)malloc(sizeof(MemoryCluster));
        cluster->id = cm->cluster_count;
        cluster->member_capacity = 64;
        cluster->members = (int64_t*)malloc(cluster->member_capacity * sizeof(int64_t));
        cluster->member_count = 0;
        
        // Copy embedding as centroid
        cluster->centroid = (Embedding*)malloc(sizeof(Embedding));
        cluster->centroid->dim = emb->dim;
        cluster->centroid->values = (double*)malloc(emb->dim * sizeof(double));
        memcpy(cluster->centroid->values, emb->values, emb->dim * sizeof(double));
        
        cm->clusters[cm->cluster_count] = cluster;
        best_cluster = cm->cluster_count;
        cm->cluster_count++;
    }
    
    // Add to cluster
    MemoryCluster* cluster = cm->clusters[best_cluster];
    if (cluster->member_count >= cluster->member_capacity) {
        cluster->member_capacity *= 2;
        cluster->members = (int64_t*)realloc(cluster->members,
                                             cluster->member_capacity * sizeof(int64_t));
    }
    cluster->members[cluster->member_count++] = memory_id;
    
    pthread_mutex_unlock(&cm->lock);
    return best_cluster;
}

// Get cluster for memory
int64_t cluster_get_for_memory(int64_t cm_ptr, int64_t memory_id) {
    ClusterManager* cm = (ClusterManager*)cm_ptr;
    if (!cm) return -1;
    
    pthread_mutex_lock(&cm->lock);
    
    for (int i = 0; i < cm->cluster_count; i++) {
        for (int j = 0; j < cm->clusters[i]->member_count; j++) {
            if (cm->clusters[i]->members[j] == memory_id) {
                pthread_mutex_unlock(&cm->lock);
                return i;
            }
        }
    }
    
    pthread_mutex_unlock(&cm->lock);
    return -1;
}

// Get cluster count
int64_t cluster_count(int64_t cm_ptr) {
    ClusterManager* cm = (ClusterManager*)cm_ptr;
    return cm ? cm->cluster_count : 0;
}

// Get cluster member count
int64_t cluster_member_count_cm(int64_t cm_ptr, int64_t cluster_id) {
    ClusterManager* cm = (ClusterManager*)cm_ptr;
    if (!cm || cluster_id < 0 || cluster_id >= cm->cluster_count) return 0;
    return cm->clusters[cluster_id]->member_count;
}

// Close cluster manager
void cluster_manager_close(int64_t cm_ptr) {
    ClusterManager* cm = (ClusterManager*)cm_ptr;
    if (!cm) return;
    
    pthread_mutex_lock(&cm->lock);
    
    for (int i = 0; i < cm->cluster_count; i++) {
        MemoryCluster* cluster = cm->clusters[i];
        if (cluster->members) free(cluster->members);
        if (cluster->centroid) {
            if (cluster->centroid->values) free(cluster->centroid->values);
            free(cluster->centroid);
        }
        free(cluster);
    }
    free(cm->clusters);
    
    pthread_mutex_unlock(&cm->lock);
    pthread_mutex_destroy(&cm->lock);
    free(cm);
}

// --------------------------------------------------------------------------
// 26.5 Importance-Based Pruning - stubs (SQLite removed)
// --------------------------------------------------------------------------

typedef struct PruneConfig {
    double min_importance;
    int64_t max_age_seconds;
    int64_t max_memories;
    int preserve_clusters;
} PruneConfig;

int64_t prune_config_new(void) {
    PruneConfig* cfg = (PruneConfig*)malloc(sizeof(PruneConfig));
    if (!cfg) return 0;
    cfg->min_importance = 0.1;
    cfg->max_age_seconds = 86400 * 30;
    cfg->max_memories = 10000;
    cfg->preserve_clusters = 1;
    return (int64_t)cfg;
}
int64_t prune_set_min_importance(int64_t cfg_ptr, double min_importance) { PruneConfig* cfg = (PruneConfig*)cfg_ptr; if (!cfg) return 0; cfg->min_importance = min_importance; return 1; }
int64_t prune_set_max_age(int64_t cfg_ptr, int64_t max_age_seconds) { PruneConfig* cfg = (PruneConfig*)cfg_ptr; if (!cfg) return 0; cfg->max_age_seconds = max_age_seconds; return 1; }
int64_t prune_set_max_memories(int64_t cfg_ptr, int64_t max_memories) { PruneConfig* cfg = (PruneConfig*)cfg_ptr; if (!cfg) return 0; cfg->max_memories = max_memories; return 1; }
int64_t prune_execute(int64_t mdb_ptr, int64_t cfg_ptr) { (void)mdb_ptr; (void)cfg_ptr; return 0; }
void prune_config_free(int64_t cfg_ptr) { PruneConfig* cfg = (PruneConfig*)cfg_ptr; if (cfg) free(cfg); }

// --------------------------------------------------------------------------
// 26.6 f64 Importance Scoring
// --------------------------------------------------------------------------

// Calculate importance score using f64
double importance_calculate(double base_score, double recency_factor, 
                           double access_factor, double relevance_factor) {
    // Weighted combination
    double score = base_score * 0.3 + 
                   recency_factor * 0.2 + 
                   access_factor * 0.2 + 
                   relevance_factor * 0.3;
    
    // Clamp to [0, 1]
    if (score < 0.0) score = 0.0;
    if (score > 1.0) score = 1.0;
    
    return score;
}

// Decay importance over time
double importance_decay(double current, double decay_rate, double time_delta) {
    return current * exp(-decay_rate * time_delta);
}

// Boost importance on access
double importance_boost(double current, double boost_amount) {
    double boosted = current + boost_amount * (1.0 - current);
    if (boosted > 1.0) boosted = 1.0;
    return boosted;
}


// ============================================================================
// Phase 27: Advanced Belief System
// ============================================================================

// --------------------------------------------------------------------------
// 27.1 Contradiction Detection
// --------------------------------------------------------------------------

typedef struct Belief {
    int64_t id;
    char* content;
    Embedding* embedding;
    double confidence;
    int64_t created_at;
    char* source;
    char* provenance_json;
    int64_t* derived_from;
    int derived_count;
} Belief;

typedef struct BeliefStore {
    Belief** beliefs;
    int count;
    int capacity;
    EmbeddingModel* model;
    double contradiction_threshold;
    pthread_mutex_t lock;
} BeliefStore;

// Create belief store
int64_t belief_store_new(void) {
    BeliefStore* bs = (BeliefStore*)malloc(sizeof(BeliefStore));
    if (!bs) return 0;

    bs->capacity = 256;
    bs->beliefs = (Belief**)calloc(bs->capacity, sizeof(Belief*));
    bs->count = 0;
    bs->model = (EmbeddingModel*)embedding_model_new(64);
    bs->contradiction_threshold = 0.85;
    pthread_mutex_init(&bs->lock, NULL);

    return (int64_t)bs;
}

// Add belief
int64_t belief_add(int64_t bs_ptr, int64_t content_ptr, double confidence) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    SxString* content = (SxString*)content_ptr;
    if (!bs || !content) return -1;
    
    pthread_mutex_lock(&bs->lock);
    
    if (bs->count >= bs->capacity) {
        bs->capacity *= 2;
        bs->beliefs = (Belief**)realloc(bs->beliefs, bs->capacity * sizeof(Belief*));
    }
    
    Belief* belief = (Belief*)malloc(sizeof(Belief));
    belief->id = bs->count;
    belief->content = strdup(content->data);
    belief->embedding = (Embedding*)embedding_embed((int64_t)bs->model, content_ptr);
    belief->confidence = confidence;
    belief->created_at = (int64_t)time(NULL);
    belief->source = NULL;
    belief->provenance_json = NULL;
    belief->derived_from = NULL;
    belief->derived_count = 0;
    
    bs->beliefs[bs->count] = belief;
    int64_t id = bs->count;
    bs->count++;
    
    pthread_mutex_unlock(&bs->lock);
    return id;
}

// Check for contradictions (returns vector of contradicting belief IDs)
int64_t belief_check_contradictions(int64_t bs_ptr, int64_t belief_id) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    if (!bs || belief_id < 0 || belief_id >= bs->count) return 0;
    
    pthread_mutex_lock(&bs->lock);
    
    SxVec* contradictions = intrinsic_vec_new();
    Belief* target = bs->beliefs[belief_id];
    
    for (int i = 0; i < bs->count; i++) {
        if (i == belief_id) continue;
        
        Belief* other = bs->beliefs[i];
        double sim = embedding_cosine_similarity((int64_t)target->embedding, 
                                                  (int64_t)other->embedding);
        
        // High similarity might indicate contradiction (same topic, different stance)
        // Check for negation patterns in content
        if (sim > bs->contradiction_threshold) {
            // Check for negation words
            if ((strstr(target->content, "not") && !strstr(other->content, "not")) ||
                (!strstr(target->content, "not") && strstr(other->content, "not")) ||
                (strstr(target->content, "never") && !strstr(other->content, "never")) ||
                (!strstr(target->content, "never") && strstr(other->content, "never"))) {
                intrinsic_vec_push(contradictions, (void*)(int64_t)i);
            }
        }
    }
    
    pthread_mutex_unlock(&bs->lock);
    return (int64_t)contradictions;
}

// Get belief confidence
double belief_get_confidence(int64_t bs_ptr, int64_t belief_id) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    if (!bs || belief_id < 0 || belief_id >= bs->count) return 0.0;
    return bs->beliefs[belief_id]->confidence;
}

// Set belief confidence
int64_t belief_set_confidence(int64_t bs_ptr, int64_t belief_id, double confidence) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    if (!bs || belief_id < 0 || belief_id >= bs->count) return 0;
    bs->beliefs[belief_id]->confidence = confidence;
    return 1;
}

// Get belief content
int64_t belief_get_content(int64_t bs_ptr, int64_t belief_id) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    if (!bs || belief_id < 0 || belief_id >= bs->count) return 0;
    return (int64_t)intrinsic_string_new(bs->beliefs[belief_id]->content);
}

// --------------------------------------------------------------------------
// 27.2 Resolution Strategies
// --------------------------------------------------------------------------

#define STRATEGY_CONFIDENCE_WINS 0
#define STRATEGY_RECENCY_WINS 1
#define STRATEGY_MERGE 2

// Resolve contradiction
int64_t belief_resolve(int64_t bs_ptr, int64_t belief1_id, int64_t belief2_id, int64_t strategy) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    if (!bs || belief1_id < 0 || belief2_id < 0) return -1;
    
    pthread_mutex_lock(&bs->lock);
    
    Belief* b1 = bs->beliefs[belief1_id];
    Belief* b2 = bs->beliefs[belief2_id];
    int64_t winner = -1;
    
    switch (strategy) {
        case STRATEGY_CONFIDENCE_WINS:
            winner = (b1->confidence >= b2->confidence) ? belief1_id : belief2_id;
            break;
            
        case STRATEGY_RECENCY_WINS:
            winner = (b1->created_at >= b2->created_at) ? belief1_id : belief2_id;
            break;
            
        case STRATEGY_MERGE:
            // Merge: average confidence, keep more recent content
            b1->confidence = (b1->confidence + b2->confidence) / 2.0;
            winner = belief1_id;
            break;
    }
    
    pthread_mutex_unlock(&bs->lock);
    return winner;
}

// Get strategy constant
int64_t belief_strategy_confidence(void) { return STRATEGY_CONFIDENCE_WINS; }
int64_t belief_strategy_recency(void) { return STRATEGY_RECENCY_WINS; }
int64_t belief_strategy_merge(void) { return STRATEGY_MERGE; }

// --------------------------------------------------------------------------
// 27.3 Semantic Belief Queries
// --------------------------------------------------------------------------

// Query beliefs by similarity
int64_t belief_query(int64_t bs_ptr, int64_t query_ptr, int64_t k) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    SxString* query = (SxString*)query_ptr;
    if (!bs || !query) return 0;
    
    pthread_mutex_lock(&bs->lock);
    
    Embedding* query_emb = (Embedding*)embedding_embed((int64_t)bs->model, query_ptr);
    
    // Score all beliefs
    double* scores = (double*)malloc(bs->count * sizeof(double));
    int* indices = (int*)malloc(bs->count * sizeof(int));
    
    for (int i = 0; i < bs->count; i++) {
        scores[i] = embedding_cosine_similarity((int64_t)query_emb, 
                                                 (int64_t)bs->beliefs[i]->embedding);
        indices[i] = i;
    }
    
    // Sort by score (bubble sort for simplicity)
    for (int i = 0; i < bs->count - 1; i++) {
        for (int j = 0; j < bs->count - i - 1; j++) {
            if (scores[j] < scores[j + 1]) {
                double ts = scores[j];
                scores[j] = scores[j + 1];
                scores[j + 1] = ts;
                int ti = indices[j];
                indices[j] = indices[j + 1];
                indices[j + 1] = ti;
            }
        }
    }
    
    // Return top k
    SxVec* results = intrinsic_vec_new();
    for (int i = 0; i < k && i < bs->count; i++) {
        intrinsic_vec_push(results, (void*)(int64_t)indices[i]);
    }
    
    free(scores);
    free(indices);
    embedding_free((int64_t)query_emb);
    
    pthread_mutex_unlock(&bs->lock);
    return (int64_t)results;
}

// --------------------------------------------------------------------------
// 27.5 Belief Provenance Tracking
// --------------------------------------------------------------------------

// Set belief source
int64_t belief_set_source(int64_t bs_ptr, int64_t belief_id, int64_t source_ptr) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    SxString* source = (SxString*)source_ptr;
    if (!bs || belief_id < 0 || belief_id >= bs->count) return 0;
    
    pthread_mutex_lock(&bs->lock);
    if (bs->beliefs[belief_id]->source) free(bs->beliefs[belief_id]->source);
    bs->beliefs[belief_id]->source = strdup(source->data);
    pthread_mutex_unlock(&bs->lock);
    
    return 1;
}

// Get belief source
int64_t belief_get_source(int64_t bs_ptr, int64_t belief_id) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    if (!bs || belief_id < 0 || belief_id >= bs->count) return 0;
    if (!bs->beliefs[belief_id]->source) return 0;
    return (int64_t)intrinsic_string_new(bs->beliefs[belief_id]->source);
}

// Add derivation link
int64_t belief_add_derivation(int64_t bs_ptr, int64_t belief_id, int64_t source_belief_id) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    if (!bs || belief_id < 0 || belief_id >= bs->count) return 0;
    
    pthread_mutex_lock(&bs->lock);
    
    Belief* b = bs->beliefs[belief_id];
    b->derived_from = (int64_t*)realloc(b->derived_from, 
                                        (b->derived_count + 1) * sizeof(int64_t));
    b->derived_from[b->derived_count++] = source_belief_id;
    
    pthread_mutex_unlock(&bs->lock);
    return 1;
}

// Get belief count
int64_t belief_count(int64_t bs_ptr) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    return bs ? bs->count : 0;
}

// Close belief store
void belief_store_close(int64_t bs_ptr) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    if (!bs) return;
    
    pthread_mutex_lock(&bs->lock);
    
    for (int i = 0; i < bs->count; i++) {
        Belief* b = bs->beliefs[i];
        free(b->content);
        if (b->embedding) embedding_free((int64_t)b->embedding);
        if (b->source) free(b->source);
        if (b->provenance_json) free(b->provenance_json);
        if (b->derived_from) free(b->derived_from);
        free(b);
    }
    free(bs->beliefs);
    if (bs->model) embedding_model_close((int64_t)bs->model);
    
    pthread_mutex_unlock(&bs->lock);
    pthread_mutex_destroy(&bs->lock);
    free(bs);
}

// Wrapper functions to match declarations
int64_t belief_store_add(int64_t bs_ptr, int64_t content_ptr, double confidence, int64_t source_id) {
    (void)source_id;  // Ignored for now
    return belief_add(bs_ptr, content_ptr, confidence);
}

int64_t belief_store_get(int64_t bs_ptr, int64_t belief_id) {
    return belief_get_content(bs_ptr, belief_id);
}

double belief_store_confidence(int64_t bs_ptr, int64_t belief_id) {
    return belief_get_confidence(bs_ptr, belief_id);
}

int64_t belief_store_remove(int64_t bs_ptr, int64_t belief_id) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    if (!bs || belief_id < 0 || belief_id >= bs->count) return 0;
    // Mark as removed by setting content to NULL
    if (bs->beliefs[belief_id]) {
        free(bs->beliefs[belief_id]->content);
        bs->beliefs[belief_id]->content = NULL;
    }
    return 1;
}

int64_t belief_store_count(int64_t bs_ptr) {
    return belief_count(bs_ptr);
}

int64_t belief_check_contradiction(int64_t bs_ptr, int64_t belief1_id, int64_t belief2_id) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    if (!bs || belief1_id < 0 || belief1_id >= bs->count ||
        belief2_id < 0 || belief2_id >= bs->count) return 0;

    Belief* b1 = bs->beliefs[belief1_id];
    Belief* b2 = bs->beliefs[belief2_id];
    if (!b1 || !b2) return 0;

    double sim = embedding_cosine_similarity((int64_t)b1->embedding, (int64_t)b2->embedding);
    // High similarity with different content = potential contradiction
    return sim > 0.7 ? 1 : 0;
}

int64_t belief_find_contradictions(int64_t bs_ptr) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    if (!bs) return 0;

    SxVec* contras = intrinsic_vec_new();
    for (int i = 0; i < bs->count; i++) {
        SxVec* c = (SxVec*)belief_check_contradictions(bs_ptr, i);
        if (c && intrinsic_vec_len(c) > 0) {
            intrinsic_vec_push(contras, (void*)(int64_t)i);
        }
    }
    return (int64_t)contras;
}

int64_t belief_contradiction_count(int64_t bs_ptr) {
    SxVec* contras = (SxVec*)belief_find_contradictions(bs_ptr);
    return contras ? intrinsic_vec_len(contras) : 0;
}

int64_t belief_resolve_by_confidence(int64_t bs_ptr, int64_t belief1_id, int64_t belief2_id) {
    return belief_resolve(bs_ptr, belief1_id, belief2_id, STRATEGY_CONFIDENCE_WINS);
}

int64_t belief_resolve_by_recency(int64_t bs_ptr, int64_t belief1_id, int64_t belief2_id) {
    return belief_resolve(bs_ptr, belief1_id, belief2_id, STRATEGY_RECENCY_WINS);
}

int64_t belief_resolve_by_source(int64_t bs_ptr, int64_t belief1_id, int64_t belief2_id) {
    // Use confidence strategy as default for source-based
    return belief_resolve(bs_ptr, belief1_id, belief2_id, STRATEGY_CONFIDENCE_WINS);
}

int64_t belief_query_related(int64_t bs_ptr, int64_t belief_id) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    if (!bs || belief_id < 0 || belief_id >= bs->count) return 0;
    Belief* b = bs->beliefs[belief_id];
    if (!b) return 0;
    SxString query = { .data = b->content, .len = strlen(b->content) };
    return belief_query(bs_ptr, (int64_t)&query, 5);
}

int64_t belief_query_by_source(int64_t bs_ptr, int64_t source_id) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    if (!bs) return 0;

    SxVec* results = intrinsic_vec_new();
    for (int i = 0; i < bs->count; i++) {
        Belief* b = bs->beliefs[i];
        if (b && b->source) {
            // Simple match - could be improved
            intrinsic_vec_push(results, (void*)(int64_t)i);
        }
    }
    return (int64_t)results;
}

int64_t belief_query_by_confidence(int64_t bs_ptr, double min_confidence) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    if (!bs) return 0;

    SxVec* results = intrinsic_vec_new();
    for (int i = 0; i < bs->count; i++) {
        Belief* b = bs->beliefs[i];
        if (b && b->confidence >= min_confidence) {
            intrinsic_vec_push(results, (void*)(int64_t)i);
        }
    }
    return (int64_t)results;
}

int64_t belief_set_timestamp(int64_t bs_ptr, int64_t belief_id, int64_t timestamp) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    if (!bs || belief_id < 0 || belief_id >= bs->count) return 0;
    bs->beliefs[belief_id]->created_at = timestamp;
    return 1;
}

int64_t belief_get_timestamp(int64_t bs_ptr, int64_t belief_id) {
    BeliefStore* bs = (BeliefStore*)bs_ptr;
    if (!bs || belief_id < 0 || belief_id >= bs->count) return 0;
    return bs->beliefs[belief_id]->created_at;
}

// ============================================================================
// Phase 28: Full BDI Reasoning
// ============================================================================

// --------------------------------------------------------------------------
// 28.1-28.7 BDI System
// --------------------------------------------------------------------------

typedef enum {
    GOAL_PENDING = 0,
    GOAL_ACTIVE = 1,
    GOAL_ACHIEVED = 2,
    GOAL_FAILED = 3,
    GOAL_SUSPENDED = 4
} GoalStatus;

typedef struct Goal {
    int64_t id;
    char* description;
    GoalStatus status;
    double priority;
    int64_t* subgoals;
    int subgoal_count;
    int64_t parent_goal;
} Goal;

typedef struct PlanStep {
    char* action;
    char** preconditions;
    int precond_count;
    char** effects;
    int effect_count;
} PlanStep;

typedef struct Plan {
    int64_t id;
    char* name;
    int64_t goal_id;
    PlanStep** steps;
    int step_count;
    int current_step;
    int status;  // 0=pending, 1=executing, 2=complete, -1=failed
} Plan;

typedef struct Intention {
    int64_t id;
    Plan* plan;
    Goal* goal;
    int64_t created_at;
    int committed;
    int suspended;
} Intention;

// Standalone goal counter
static int64_t g_goal_counter = 0;
static int64_t g_plan_counter = 0;
static int64_t g_intention_counter = 0;

// --------------------------------------------------------------------------
// Standalone Goal Functions
// --------------------------------------------------------------------------

int64_t goal_new(int64_t desc_ptr, int64_t flags) {
    (void)flags;
    SxString* desc = (SxString*)desc_ptr;
    if (!desc) return 0;

    Goal* goal = (Goal*)malloc(sizeof(Goal));
    if (!goal) return 0;

    goal->id = g_goal_counter++;
    goal->description = strdup(desc->data);
    goal->status = GOAL_PENDING;
    goal->priority = 0.5;
    goal->subgoals = NULL;
    goal->subgoal_count = 0;
    goal->parent_goal = -1;

    return (int64_t)goal;
}

int64_t goal_set_priority(int64_t goal_ptr, double priority) {
    Goal* goal = (Goal*)goal_ptr;
    if (!goal) return 0;
    goal->priority = priority;
    return 1;
}

double goal_get_priority(int64_t goal_ptr) {
    Goal* goal = (Goal*)goal_ptr;
    if (!goal) return 0.0;
    return goal->priority;
}

int64_t goal_set_deadline(int64_t goal_ptr, int64_t deadline) {
    (void)deadline;
    Goal* goal = (Goal*)goal_ptr;
    if (!goal) return 0;
    // Deadline stored elsewhere in a more complete implementation
    return 1;
}

int64_t goal_is_achieved(int64_t goal_ptr) {
    Goal* goal = (Goal*)goal_ptr;
    if (!goal) return 0;
    return goal->status == GOAL_ACHIEVED ? 1 : 0;
}

void goal_free(int64_t goal_ptr) {
    Goal* goal = (Goal*)goal_ptr;
    if (!goal) return;
    if (goal->description) free(goal->description);
    if (goal->subgoals) free(goal->subgoals);
    free(goal);
}

// --------------------------------------------------------------------------
// Standalone Plan Functions
// --------------------------------------------------------------------------

int64_t plan_new(int64_t goal_ptr) {
    Goal* goal = (Goal*)goal_ptr;
    if (!goal) return 0;

    Plan* plan = (Plan*)malloc(sizeof(Plan));
    if (!plan) return 0;

    plan->id = g_plan_counter++;
    plan->name = NULL;
    plan->goal_id = goal->id;
    plan->steps = NULL;
    plan->step_count = 0;
    plan->current_step = 0;
    plan->status = 0;

    return (int64_t)plan;
}

int64_t plan_add_step(int64_t plan_ptr, int64_t action_ptr) {
    Plan* plan = (Plan*)plan_ptr;
    SxString* action = (SxString*)action_ptr;
    if (!plan || !action) return 0;

    plan->steps = (PlanStep**)realloc(plan->steps, (plan->step_count + 1) * sizeof(PlanStep*));
    PlanStep* step = (PlanStep*)malloc(sizeof(PlanStep));
    step->action = strdup(action->data);
    step->preconditions = NULL;
    step->precond_count = 0;
    step->effects = NULL;
    step->effect_count = 0;

    plan->steps[plan->step_count++] = step;
    return 1;
}

int64_t plan_step_count(int64_t plan_ptr) {
    Plan* plan = (Plan*)plan_ptr;
    if (!plan) return 0;
    return plan->step_count;
}

int64_t plan_get_step(int64_t plan_ptr, int64_t index) {
    Plan* plan = (Plan*)plan_ptr;
    if (!plan || index < 0 || index >= plan->step_count) return 0;
    return (int64_t)plan->steps[index];
}

int64_t plan_set_precondition(int64_t plan_ptr, int64_t precond_ptr) {
    (void)precond_ptr;
    Plan* plan = (Plan*)plan_ptr;
    if (!plan) return 0;
    // Store precondition in a more complete implementation
    return 1;
}

int64_t plan_check_precondition(int64_t plan_ptr, int64_t state_ptr) {
    (void)state_ptr;
    Plan* plan = (Plan*)plan_ptr;
    if (!plan) return 0;
    // Check preconditions - always true for now
    return 1;
}

void plan_free(int64_t plan_ptr) {
    Plan* plan = (Plan*)plan_ptr;
    if (!plan) return;
    if (plan->name) free(plan->name);
    for (int i = 0; i < plan->step_count; i++) {
        PlanStep* step = plan->steps[i];
        if (step->action) free(step->action);
        for (int j = 0; j < step->precond_count; j++) free(step->preconditions[j]);
        if (step->preconditions) free(step->preconditions);
        for (int j = 0; j < step->effect_count; j++) free(step->effects[j]);
        if (step->effects) free(step->effects);
        free(step);
    }
    if (plan->steps) free(plan->steps);
    free(plan);
}

// --------------------------------------------------------------------------
// Standalone Intention Functions
// --------------------------------------------------------------------------

int64_t intention_new(int64_t goal_ptr, int64_t plan_ptr) {
    Goal* goal = (Goal*)goal_ptr;
    Plan* plan = (Plan*)plan_ptr;
    if (!goal || !plan) return 0;

    Intention* intent = (Intention*)malloc(sizeof(Intention));
    if (!intent) return 0;

    intent->id = g_intention_counter++;
    intent->goal = goal;
    intent->plan = plan;
    intent->created_at = (int64_t)time(NULL);
    intent->committed = 1;
    intent->suspended = 0;

    return (int64_t)intent;
}

int64_t intention_execute_step(int64_t intent_ptr) {
    Intention* intent = (Intention*)intent_ptr;
    if (!intent || !intent->plan || intent->suspended) return 0;

    Plan* plan = intent->plan;
    if (plan->current_step < plan->step_count) {
        plan->current_step++;
        return 1;
    }
    return 0;
}

int64_t intention_is_complete(int64_t intent_ptr) {
    Intention* intent = (Intention*)intent_ptr;
    if (!intent || !intent->plan) return 0;
    return intent->plan->current_step >= intent->plan->step_count ? 1 : 0;
}

int64_t intention_current_step(int64_t intent_ptr) {
    Intention* intent = (Intention*)intent_ptr;
    if (!intent || !intent->plan) return -1;
    return intent->plan->current_step;
}

int64_t intention_suspend(int64_t intent_ptr) {
    Intention* intent = (Intention*)intent_ptr;
    if (!intent) return 0;
    intent->suspended = 1;
    return 1;
}

int64_t intention_resume(int64_t intent_ptr) {
    Intention* intent = (Intention*)intent_ptr;
    if (!intent) return 0;
    intent->suspended = 0;
    return 1;
}

void intention_free(int64_t intent_ptr) {
    Intention* intent = (Intention*)intent_ptr;
    if (!intent) return;
    // Note: Don't free goal/plan - they may be shared
    free(intent);
}

typedef struct BDIAgent {
    Goal** goals;
    int goal_count;
    int goal_capacity;
    Plan** plan_library;
    int plan_count;
    int plan_capacity;
    Intention** intentions;
    int intention_count;
    int intention_capacity;
    BeliefStore* beliefs;
    int commitment_strategy;  // 0=bold, 1=cautious, 2=open
    pthread_mutex_t lock;
} BDIAgent;

// Create BDI agent
int64_t bdi_agent_new(void) {
    BDIAgent* agent = (BDIAgent*)malloc(sizeof(BDIAgent));
    if (!agent) return 0;
    
    agent->goal_capacity = 64;
    agent->goals = (Goal**)calloc(agent->goal_capacity, sizeof(Goal*));
    agent->goal_count = 0;
    
    agent->plan_capacity = 64;
    agent->plan_library = (Plan**)calloc(agent->plan_capacity, sizeof(Plan*));
    agent->plan_count = 0;
    
    agent->intention_capacity = 32;
    agent->intentions = (Intention**)calloc(agent->intention_capacity, sizeof(Intention*));
    agent->intention_count = 0;
    
    agent->beliefs = (BeliefStore*)belief_store_new();
    agent->commitment_strategy = 0;  // Bold by default
    pthread_mutex_init(&agent->lock, NULL);
    
    return (int64_t)agent;
}

// Add goal (internal, creates goal from description)
int64_t bdi_add_goal_desc(int64_t agent_ptr, int64_t desc_ptr, double priority) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    SxString* desc = (SxString*)desc_ptr;
    if (!agent || !desc) return -1;
    
    pthread_mutex_lock(&agent->lock);
    
    if (agent->goal_count >= agent->goal_capacity) {
        agent->goal_capacity *= 2;
        agent->goals = (Goal**)realloc(agent->goals, 
                                       agent->goal_capacity * sizeof(Goal*));
    }
    
    Goal* goal = (Goal*)malloc(sizeof(Goal));
    goal->id = agent->goal_count;
    goal->description = strdup(desc->data);
    goal->status = GOAL_PENDING;
    goal->priority = priority;
    goal->subgoals = NULL;
    goal->subgoal_count = 0;
    goal->parent_goal = -1;
    
    agent->goals[agent->goal_count] = goal;
    int64_t id = agent->goal_count;
    agent->goal_count++;
    
    pthread_mutex_unlock(&agent->lock);
    return id;
}

// Add subgoal
int64_t bdi_add_subgoal(int64_t agent_ptr, int64_t parent_id, int64_t desc_ptr, double priority) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    if (!agent || parent_id < 0 || parent_id >= agent->goal_count) return -1;

    int64_t subgoal_id = bdi_add_goal_desc(agent_ptr, desc_ptr, priority);
    if (subgoal_id < 0) return -1;
    
    pthread_mutex_lock(&agent->lock);
    
    Goal* parent = agent->goals[parent_id];
    Goal* child = agent->goals[subgoal_id];
    
    parent->subgoals = (int64_t*)realloc(parent->subgoals, 
                                         (parent->subgoal_count + 1) * sizeof(int64_t));
    parent->subgoals[parent->subgoal_count++] = subgoal_id;
    child->parent_goal = parent_id;
    
    pthread_mutex_unlock(&agent->lock);
    return subgoal_id;
}

// Add plan to library (internal, creates plan from name)
int64_t bdi_add_plan_internal(int64_t agent_ptr, int64_t name_ptr, int64_t goal_id) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    SxString* name = (SxString*)name_ptr;
    if (!agent || !name) return -1;
    
    pthread_mutex_lock(&agent->lock);
    
    if (agent->plan_count >= agent->plan_capacity) {
        agent->plan_capacity *= 2;
        agent->plan_library = (Plan**)realloc(agent->plan_library,
                                              agent->plan_capacity * sizeof(Plan*));
    }
    
    Plan* plan = (Plan*)malloc(sizeof(Plan));
    plan->id = agent->plan_count;
    plan->name = strdup(name->data);
    plan->goal_id = goal_id;
    plan->steps = NULL;
    plan->step_count = 0;
    plan->current_step = 0;
    plan->status = 0;
    
    agent->plan_library[agent->plan_count] = plan;
    int64_t id = agent->plan_count;
    agent->plan_count++;
    
    pthread_mutex_unlock(&agent->lock);
    return id;
}

// Add step to plan
int64_t bdi_add_plan_step(int64_t agent_ptr, int64_t plan_id, int64_t action_ptr) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    SxString* action = (SxString*)action_ptr;
    if (!agent || plan_id < 0 || plan_id >= agent->plan_count) return -1;
    
    pthread_mutex_lock(&agent->lock);
    
    Plan* plan = agent->plan_library[plan_id];
    plan->steps = (PlanStep**)realloc(plan->steps, 
                                      (plan->step_count + 1) * sizeof(PlanStep*));
    
    PlanStep* step = (PlanStep*)malloc(sizeof(PlanStep));
    step->action = strdup(action->data);
    step->preconditions = NULL;
    step->precond_count = 0;
    step->effects = NULL;
    step->effect_count = 0;
    
    plan->steps[plan->step_count] = step;
    int64_t step_id = plan->step_count;
    plan->step_count++;
    
    pthread_mutex_unlock(&agent->lock);
    return step_id;
}

// Select plan for goal (means-end reasoning, internal)
int64_t bdi_select_plan_for_goal(int64_t agent_ptr, int64_t goal_id) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    if (!agent || goal_id < 0) return -1;
    
    pthread_mutex_lock(&agent->lock);
    
    // Find applicable plans
    int64_t best_plan = -1;
    for (int i = 0; i < agent->plan_count; i++) {
        if (agent->plan_library[i]->goal_id == goal_id) {
            best_plan = i;
            break;
        }
    }
    
    pthread_mutex_unlock(&agent->lock);
    return best_plan;
}

// Create intention from plan
int64_t bdi_intend(int64_t agent_ptr, int64_t plan_id, int64_t goal_id) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    if (!agent || plan_id < 0 || plan_id >= agent->plan_count) return -1;
    
    pthread_mutex_lock(&agent->lock);
    
    if (agent->intention_count >= agent->intention_capacity) {
        agent->intention_capacity *= 2;
        agent->intentions = (Intention**)realloc(agent->intentions,
                                                 agent->intention_capacity * sizeof(Intention*));
    }
    
    Intention* intention = (Intention*)malloc(sizeof(Intention));
    intention->id = agent->intention_count;
    intention->plan = agent->plan_library[plan_id];
    intention->goal = goal_id >= 0 ? agent->goals[goal_id] : NULL;
    intention->created_at = (int64_t)time(NULL);
    intention->committed = 1;
    
    agent->intentions[agent->intention_count] = intention;
    int64_t id = agent->intention_count;
    agent->intention_count++;
    
    pthread_mutex_unlock(&agent->lock);
    return id;
}

// Execute next step of intention
int64_t bdi_execute_step(int64_t agent_ptr, int64_t intention_id) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    if (!agent || intention_id < 0 || intention_id >= agent->intention_count) return -1;
    
    pthread_mutex_lock(&agent->lock);
    
    Intention* intention = agent->intentions[intention_id];
    Plan* plan = intention->plan;
    
    if (plan->current_step >= plan->step_count) {
        plan->status = 2;  // Complete
        if (intention->goal) {
            intention->goal->status = GOAL_ACHIEVED;
        }
        pthread_mutex_unlock(&agent->lock);
        return 0;
    }
    
    plan->status = 1;  // Executing
    PlanStep* step = plan->steps[plan->current_step];
    
    // In real implementation, this would execute the action
    // For now, just advance to next step
    plan->current_step++;
    
    pthread_mutex_unlock(&agent->lock);
    return (int64_t)intrinsic_string_new(step->action);
}

// Set commitment strategy
int64_t bdi_set_commitment(int64_t agent_ptr, int64_t strategy) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    if (!agent) return 0;
    agent->commitment_strategy = (int)strategy;
    return 1;
}

// Get goal status
int64_t bdi_goal_status(int64_t agent_ptr, int64_t goal_id) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    if (!agent || goal_id < 0 || goal_id >= agent->goal_count) return -1;
    return (int64_t)agent->goals[goal_id]->status;
}

// Get goal count
int64_t bdi_goal_count(int64_t agent_ptr) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    return agent ? agent->goal_count : 0;
}

// Get intention count
int64_t bdi_intention_count(int64_t agent_ptr) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    return agent ? agent->intention_count : 0;
}

// Close BDI agent
void bdi_agent_close(int64_t agent_ptr) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    if (!agent) return;
    
    pthread_mutex_lock(&agent->lock);
    
    for (int i = 0; i < agent->goal_count; i++) {
        Goal* g = agent->goals[i];
        free(g->description);
        if (g->subgoals) free(g->subgoals);
        free(g);
    }
    free(agent->goals);
    
    for (int i = 0; i < agent->plan_count; i++) {
        Plan* p = agent->plan_library[i];
        free(p->name);
        for (int j = 0; j < p->step_count; j++) {
            PlanStep* s = p->steps[j];
            free(s->action);
            if (s->preconditions) {
                for (int k = 0; k < s->precond_count; k++) free(s->preconditions[k]);
                free(s->preconditions);
            }
            if (s->effects) {
                for (int k = 0; k < s->effect_count; k++) free(s->effects[k]);
                free(s->effects);
            }
            free(s);
        }
        if (p->steps) free(p->steps);
        free(p);
    }
    free(agent->plan_library);
    
    for (int i = 0; i < agent->intention_count; i++) {
        free(agent->intentions[i]);
    }
    free(agent->intentions);
    
    if (agent->beliefs) belief_store_close((int64_t)agent->beliefs);
    
    pthread_mutex_unlock(&agent->lock);
    pthread_mutex_destroy(&agent->lock);
    free(agent);
}

// Wrapper functions to match declarations
int64_t bdi_add_belief(int64_t agent_ptr, int64_t belief_str_ptr, double confidence) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    if (!agent || !agent->beliefs) return 0;
    return belief_add((int64_t)agent->beliefs, belief_str_ptr, confidence);
}

// Add pre-created goal to agent (matches declaration)
int64_t bdi_add_goal(int64_t agent_ptr, int64_t goal_ptr) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    Goal* goal = (Goal*)goal_ptr;
    if (!agent || !goal) return 0;

    pthread_mutex_lock(&agent->lock);

    if (agent->goal_count >= agent->goal_capacity) {
        agent->goal_capacity *= 2;
        agent->goals = (Goal**)realloc(agent->goals, agent->goal_capacity * sizeof(Goal*));
    }

    agent->goals[agent->goal_count] = goal;
    agent->goal_count++;

    pthread_mutex_unlock(&agent->lock);
    return 1;
}

// Add pre-created plan to agent (matches declaration)
int64_t bdi_add_plan(int64_t agent_ptr, int64_t goal_ptr, int64_t plan_ptr) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    Goal* goal = (Goal*)goal_ptr;
    Plan* plan = (Plan*)plan_ptr;
    if (!agent || !goal || !plan) return 0;

    pthread_mutex_lock(&agent->lock);

    if (agent->plan_count >= agent->plan_capacity) {
        agent->plan_capacity *= 2;
        agent->plan_library = (Plan**)realloc(agent->plan_library, agent->plan_capacity * sizeof(Plan*));
    }

    plan->goal_id = goal->id;
    agent->plan_library[agent->plan_count] = plan;
    agent->plan_count++;

    pthread_mutex_unlock(&agent->lock);
    return 1;
}

int64_t bdi_deliberate(int64_t agent_ptr) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    if (!agent) return 0;

    pthread_mutex_lock(&agent->lock);

    // Simple deliberation: for each pending goal, find a plan and create intention
    for (int i = 0; i < agent->goal_count; i++) {
        Goal* goal = agent->goals[i];
        if (goal->status == GOAL_PENDING) {
            // Find applicable plan
            for (int j = 0; j < agent->plan_count; j++) {
                Plan* plan = agent->plan_library[j];
                if (plan->goal_id == goal->id) {
                    // Create intention
                    if (agent->intention_count < agent->intention_capacity) {
                        Intention* intent = (Intention*)malloc(sizeof(Intention));
                        intent->id = agent->intention_count;
                        intent->goal = goal;
                        intent->plan = plan;
                        intent->created_at = (int64_t)time(NULL);
                        intent->committed = 1;
                        intent->suspended = 0;
                        agent->intentions[agent->intention_count++] = intent;
                        goal->status = GOAL_ACTIVE;
                    }
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&agent->lock);
    return agent->intention_count;
}

int64_t bdi_execute(int64_t agent_ptr) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    if (!agent) return 0;

    pthread_mutex_lock(&agent->lock);

    int executed = 0;
    for (int i = 0; i < agent->intention_count; i++) {
        Intention* intent = agent->intentions[i];
        if (!intent->suspended && intent->plan->current_step < intent->plan->step_count) {
            intent->plan->current_step++;
            executed++;
            if (intent->plan->current_step >= intent->plan->step_count) {
                intent->goal->status = GOAL_ACHIEVED;
            }
        }
    }

    pthread_mutex_unlock(&agent->lock);
    return executed;
}

int64_t bdi_find_plans_for_goal(int64_t agent_ptr, int64_t goal_ptr) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    Goal* goal = (Goal*)goal_ptr;
    if (!agent || !goal) return 0;

    pthread_mutex_lock(&agent->lock);

    SxVec* plans = intrinsic_vec_new();
    for (int i = 0; i < agent->plan_count; i++) {
        if (agent->plan_library[i]->goal_id == goal->id) {
            intrinsic_vec_push(plans, (void*)agent->plan_library[i]);
        }
    }

    pthread_mutex_unlock(&agent->lock);
    return (int64_t)plans;
}

// Select from a list of plans (matches declaration)
int64_t bdi_select_plan(int64_t agent_ptr, int64_t plans_ptr) {
    (void)agent_ptr;
    SxVec* plans = (SxVec*)plans_ptr;
    if (!plans || intrinsic_vec_len(plans) == 0) return 0;
    // Simple selection: return first plan
    return (int64_t)intrinsic_vec_get(plans, 0);
}

int64_t bdi_commit_to_intention(int64_t agent_ptr, int64_t plan_ptr) {
    BDIAgent* agent = (BDIAgent*)agent_ptr;
    Plan* plan = (Plan*)plan_ptr;
    if (!agent || !plan) return 0;

    pthread_mutex_lock(&agent->lock);

    if (agent->intention_count >= agent->intention_capacity) {
        pthread_mutex_unlock(&agent->lock);
        return 0;
    }

    // Find the goal for this plan
    Goal* goal = NULL;
    for (int i = 0; i < agent->goal_count; i++) {
        if (agent->goals[i]->id == plan->goal_id) {
            goal = agent->goals[i];
            break;
        }
    }

    Intention* intent = (Intention*)malloc(sizeof(Intention));
    intent->id = agent->intention_count;
    intent->goal = goal;
    intent->plan = plan;
    intent->created_at = (int64_t)time(NULL);
    intent->committed = 1;
    intent->suspended = 0;

    agent->intentions[agent->intention_count++] = intent;

    pthread_mutex_unlock(&agent->lock);
    return (int64_t)intent;
}

// ============================================================================
// Phase 29: Production AI Integration
// ============================================================================

// --------------------------------------------------------------------------
// 29.1 LLM Provider Abstraction
// --------------------------------------------------------------------------

typedef enum {
    PROVIDER_MOCK = 0,
    PROVIDER_ANTHROPIC = 1,
    PROVIDER_OPENAI = 2,
    PROVIDER_OLLAMA = 3
} LLMProvider;

typedef struct LLMClient {
    LLMProvider provider;
    char* api_key;
    char* base_url;
    char* model;
    double temperature;
    int max_tokens;
    int64_t total_tokens;
    double total_cost;
    pthread_mutex_t lock;
} LLMClient;

// Create LLM client
int64_t llm_client_new(int64_t provider) {
    LLMClient* client = (LLMClient*)malloc(sizeof(LLMClient));
    if (!client) return 0;
    
    client->provider = (LLMProvider)provider;
    client->api_key = NULL;
    client->base_url = NULL;
    client->model = strdup("mock-model");
    client->temperature = 0.7;
    client->max_tokens = 1024;
    client->total_tokens = 0;
    client->total_cost = 0.0;
    pthread_mutex_init(&client->lock, NULL);
    
    return (int64_t)client;
}

// Set API key
int64_t llm_set_api_key(int64_t client_ptr, int64_t key_ptr) {
    LLMClient* client = (LLMClient*)client_ptr;
    SxString* key = (SxString*)key_ptr;
    if (!client || !key) return 0;
    
    pthread_mutex_lock(&client->lock);
    if (client->api_key) free(client->api_key);
    client->api_key = strdup(key->data);
    pthread_mutex_unlock(&client->lock);
    
    return 1;
}

// Set model
int64_t llm_set_model(int64_t client_ptr, int64_t model_ptr) {
    LLMClient* client = (LLMClient*)client_ptr;
    SxString* model = (SxString*)model_ptr;
    if (!client || !model) return 0;
    
    pthread_mutex_lock(&client->lock);
    if (client->model) free(client->model);
    client->model = strdup(model->data);
    pthread_mutex_unlock(&client->lock);
    
    return 1;
}

// Set temperature
int64_t llm_set_temperature(int64_t client_ptr, double temp) {
    LLMClient* client = (LLMClient*)client_ptr;
    if (!client) return 0;
    client->temperature = temp;
    return 1;
}

// Set base URL (for custom endpoints, self-hosted models)
int64_t llm_set_base_url(int64_t client_ptr, int64_t url_ptr) {
    LLMClient* client = (LLMClient*)client_ptr;
    if (!client) return 0;

    SxString* url_str = (SxString*)url_ptr;
    if (!url_str || !url_str->data) return 0;

    pthread_mutex_lock(&client->lock);

    if (client->base_url) free(client->base_url);
    client->base_url = strndup(url_str->data, url_str->len);

    pthread_mutex_unlock(&client->lock);
    return 1;
}

// Helper: Extract JSON string value by key (simple parser)
static char* json_extract_string(const char* json, const char* key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char* pos = strstr(json, search);
    if (!pos) return NULL;

    // Find the colon after key
    pos = strchr(pos + strlen(search), ':');
    if (!pos) return NULL;

    // Skip whitespace
    pos++;
    while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r') pos++;

    if (*pos != '"') return NULL;
    pos++; // Skip opening quote

    // Find end of string (handle escapes)
    const char* start = pos;
    while (*pos && *pos != '"') {
        if (*pos == '\\' && *(pos+1)) pos += 2;
        else pos++;
    }

    size_t len = pos - start;
    char* result = (char*)malloc(len + 1);
    memcpy(result, start, len);
    result[len] = '\0';

    // Unescape basic sequences
    char* w = result;
    char* r = result;
    while (*r) {
        if (*r == '\\' && *(r+1)) {
            r++;
            switch (*r) {
                case 'n': *w++ = '\n'; break;
                case 't': *w++ = '\t'; break;
                case 'r': *w++ = '\r'; break;
                case '"': *w++ = '"'; break;
                case '\\': *w++ = '\\'; break;
                default: *w++ = *r; break;
            }
            r++;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';

    return result;
}

// Helper: Escape string for JSON
static char* json_escape_string(const char* str) {
    size_t len = strlen(str);
    char* escaped = (char*)malloc(len * 2 + 1);
    char* w = escaped;
    for (const char* r = str; *r; r++) {
        switch (*r) {
            case '"': *w++ = '\\'; *w++ = '"'; break;
            case '\\': *w++ = '\\'; *w++ = '\\'; break;
            case '\n': *w++ = '\\'; *w++ = 'n'; break;
            case '\r': *w++ = '\\'; *w++ = 'r'; break;
            case '\t': *w++ = '\\'; *w++ = 't'; break;
            default: *w++ = *r; break;
        }
    }
    *w = '\0';
    return escaped;
}

// Helper: Build Anthropic API request JSON
static char* build_anthropic_request(const char* model, const char* prompt, int max_tokens) {
    char* escaped = json_escape_string(prompt);
    char* json = (char*)malloc(strlen(escaped) + 512);
    sprintf(json,
        "{\"model\":\"%s\",\"max_tokens\":%d,\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}",
        model, max_tokens, escaped);
    free(escaped);
    return json;
}

// Helper: Build OpenAI API request JSON
static char* build_openai_request(const char* model, const char* prompt, int max_tokens) {
    char* escaped = json_escape_string(prompt);
    char* json = (char*)malloc(strlen(escaped) + 512);
    sprintf(json,
        "{\"model\":\"%s\",\"max_tokens\":%d,\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}",
        model, max_tokens, escaped);
    free(escaped);
    return json;
}

// Helper: Build Ollama API request JSON (local models)
static char* build_ollama_request(const char* model, const char* prompt) {
    char* escaped = json_escape_string(prompt);
    char* json = (char*)malloc(strlen(escaped) + 256);
    sprintf(json,
        "{\"model\":\"%s\",\"prompt\":\"%s\",\"stream\":false}",
        model, escaped);
    free(escaped);
    return json;
}

// Generate completion (real API implementation)
int64_t llm_complete(int64_t client_ptr, int64_t prompt_ptr) {
    LLMClient* client = (LLMClient*)client_ptr;
    SxString* prompt = (SxString*)prompt_ptr;
    if (!client || !prompt) return 0;

    pthread_mutex_lock(&client->lock);

    // Check for API key
    if (!client->api_key || strlen(client->api_key) == 0) {
        // Try environment variable
        const char* env_key = getenv("ANTHROPIC_API_KEY");
        if (!env_key) env_key = getenv("OPENAI_API_KEY");
        if (env_key) {
            client->api_key = strdup(env_key);
        } else {
            pthread_mutex_unlock(&client->lock);
            // Fall back to mock if no API key
            char response[512];
            snprintf(response, sizeof(response),
                     "[No API key - Mock response to: %.100s...]", prompt->data);
            return (int64_t)intrinsic_string_new(response);
        }
    }

    // Determine provider and endpoint
    const char* url = NULL;
    const char* auth_header = NULL;
    char auth_value[512];

    char* body = NULL;
    const char* default_model = NULL;

    switch (client->provider) {
        case PROVIDER_ANTHROPIC:
            url = client->base_url ? client->base_url : "https://api.anthropic.com/v1/messages";
            auth_header = "x-api-key";
            snprintf(auth_value, sizeof(auth_value), "%s", client->api_key);
            default_model = "claude-3-haiku-20240307";
            body = build_anthropic_request(
                client->model ? client->model : default_model,
                prompt->data,
                client->max_tokens > 0 ? client->max_tokens : 1024
            );
            break;

        case PROVIDER_OPENAI:
            url = client->base_url ? client->base_url : "https://api.openai.com/v1/chat/completions";
            auth_header = "Authorization";
            snprintf(auth_value, sizeof(auth_value), "Bearer %s", client->api_key);
            default_model = "gpt-4o-mini";
            body = build_openai_request(
                client->model ? client->model : default_model,
                prompt->data,
                client->max_tokens > 0 ? client->max_tokens : 1024
            );
            break;

        case PROVIDER_OLLAMA:
            url = client->base_url ? client->base_url : "http://localhost:11434/api/generate";
            auth_header = NULL;  // No auth for local Ollama
            default_model = "llama2";
            body = build_ollama_request(
                client->model ? client->model : default_model,
                prompt->data
            );
            break;

        case PROVIDER_MOCK:
        default:
            pthread_mutex_unlock(&client->lock);
            char response[512];
            snprintf(response, sizeof(response),
                     "[Mock response to: %.100s...]", prompt->data);
            return (int64_t)intrinsic_string_new(response);
    }

    // Create HTTP request
    SxString url_str = { .data = (char*)url, .len = strlen(url), .cap = 0 };
    SxString method_str = { .data = "POST", .len = 4, .cap = 0 };
    int64_t req = http_request_new((int64_t)&method_str, (int64_t)&url_str);

    // Set headers
    SxString ct_name = { .data = "Content-Type", .len = 12, .cap = 0 };
    SxString ct_value = { .data = "application/json", .len = 16, .cap = 0 };
    http_request_header(req, (int64_t)&ct_name, (int64_t)&ct_value);

    // Set auth header (if required - Ollama doesn't need it)
    if (auth_header) {
        SxString auth_name = { .data = (char*)auth_header, .len = strlen(auth_header), .cap = 0 };
        SxString auth_val = { .data = auth_value, .len = strlen(auth_value), .cap = 0 };
        http_request_header(req, (int64_t)&auth_name, (int64_t)&auth_val);
    }

    // Anthropic requires version header
    if (client->provider == PROVIDER_ANTHROPIC) {
        SxString ver_name = { .data = "anthropic-version", .len = 17, .cap = 0 };
        SxString ver_value = { .data = "2023-06-01", .len = 10, .cap = 0 };
        http_request_header(req, (int64_t)&ver_name, (int64_t)&ver_value);
    }

    // Set body
    SxString body_str = { .data = body, .len = strlen(body), .cap = 0 };
    http_request_body(req, (int64_t)&body_str);

    // Send request
    int64_t resp = http_request_send(req);
    http_request_free(req);
    free(body);

    if (!resp) {
        pthread_mutex_unlock(&client->lock);
        return (int64_t)intrinsic_string_new("[Error: HTTP request failed]");
    }

    // Check status
    int64_t status = http_response_status(resp);
    if (status != 200) {
        int64_t body_ptr = http_response_body(resp);
        SxString* body_str2 = body_ptr ? (SxString*)body_ptr : NULL;
        char error[1024];
        snprintf(error, sizeof(error), "[Error: HTTP %lld - %s]",
                 (long long)status,
                 body_str2 && body_str2->data ? body_str2->data : "Unknown error");
        http_response_free(resp);
        pthread_mutex_unlock(&client->lock);
        return (int64_t)intrinsic_string_new(error);
    }

    // Parse response body
    int64_t body_ptr = http_response_body(resp);
    SxString* response_body = (SxString*)body_ptr;
    if (!response_body || !response_body->data) {
        http_response_free(resp);
        pthread_mutex_unlock(&client->lock);
        return (int64_t)intrinsic_string_new("[Error: Empty response]");
    }

    // Extract text from response based on provider format
    char* text = NULL;
    switch (client->provider) {
        case PROVIDER_ANTHROPIC:
            // Format: {"content":[{"type":"text","text":"..."}],...}
            text = json_extract_string(response_body->data, "text");
            break;
        case PROVIDER_OPENAI:
            // Format: {"choices":[{"message":{"content":"..."}}]}
            text = json_extract_string(response_body->data, "content");
            break;
        case PROVIDER_OLLAMA:
            // Format: {"response":"..."}
            text = json_extract_string(response_body->data, "response");
            break;
        default:
            break;
    }
    // Fallback: try common field names
    if (!text) text = json_extract_string(response_body->data, "text");
    if (!text) text = json_extract_string(response_body->data, "content");
    if (!text) text = json_extract_string(response_body->data, "response");

    SxString* result;
    if (text) {
        result = intrinsic_string_new(text);
        free(text);
    } else {
        result = intrinsic_string_new("[Error: Could not parse response]");
    }

    // Track usage (estimate)
    client->total_tokens += strlen(prompt->data) / 4 + strlen(result->data) / 4;
    client->total_cost += 0.001 * (client->total_tokens / 1000.0);

    http_response_free(resp);
    pthread_mutex_unlock(&client->lock);

    return (int64_t)result;
}

// Chat completion (wrapper)
int64_t llm_chat(int64_t client_ptr, int64_t message_ptr) {
    // Chat uses same underlying mechanism as complete
    return llm_complete(client_ptr, message_ptr);
}

// Get embeddings (mock)
int64_t llm_embed(int64_t client_ptr, int64_t text_ptr) {
    LLMClient* client = (LLMClient*)client_ptr;
    SxString* text = (SxString*)text_ptr;
    if (!client || !text) return 0;

    // Mock embedding - just return a small embedding model result
    return embedding_embed((int64_t)embedding_model_new(64), text_ptr);
}

// Get token usage
int64_t llm_get_tokens(int64_t client_ptr) {
    LLMClient* client = (LLMClient*)client_ptr;
    return client ? client->total_tokens : 0;
}

// Get cost
double llm_get_cost(int64_t client_ptr) {
    LLMClient* client = (LLMClient*)client_ptr;
    return client ? client->total_cost : 0.0;
}

// Close client
void llm_client_close(int64_t client_ptr) {
    LLMClient* client = (LLMClient*)client_ptr;
    if (!client) return;
    
    pthread_mutex_lock(&client->lock);
    if (client->api_key) free(client->api_key);
    if (client->base_url) free(client->base_url);
    if (client->model) free(client->model);
    pthread_mutex_unlock(&client->lock);
    pthread_mutex_destroy(&client->lock);
    free(client);
}

// Provider constants
int64_t llm_provider_mock(void) { return PROVIDER_MOCK; }
int64_t llm_provider_anthropic(void) { return PROVIDER_ANTHROPIC; }
int64_t llm_provider_openai(void) { return PROVIDER_OPENAI; }
int64_t llm_provider_ollama(void) { return PROVIDER_OLLAMA; }

// --------------------------------------------------------------------------
// 29.2 Memory Bank Per Specialist
// --------------------------------------------------------------------------

typedef struct SpecialistMemory {
    char* specialist_id;
    MemoryDB* db;
    HNSW* index;
    EmbeddingModel* model;
    int context_window;
    pthread_mutex_t lock;
} SpecialistMemory;

// Create specialist memory
int64_t specialist_memory_new(int64_t specialist_id) {
    SpecialistMemory* mem = (SpecialistMemory*)malloc(sizeof(SpecialistMemory));
    if (!mem) return 0;

    // Store ID as string
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "specialist_%lld", (long long)specialist_id);
    mem->specialist_id = strdup(id_str);
    mem->db = (MemoryDB*)memdb_new(0);
    mem->index = (HNSW*)hnsw_new();
    mem->model = (EmbeddingModel*)embedding_model_new(64);
    mem->context_window = 4096;
    pthread_mutex_init(&mem->lock, NULL);

    return (int64_t)mem;
}

// Store memory (content_ptr: string content, category_ptr: category string)
int64_t specialist_memory_store(int64_t mem_ptr, int64_t content_ptr, int64_t category_ptr) {
    SpecialistMemory* mem = (SpecialistMemory*)mem_ptr;
    SxString* content = (SxString*)content_ptr;
    if (!mem || !content) return 0;

    pthread_mutex_lock(&mem->lock);

    // Create embedding
    Embedding* emb = (Embedding*)embedding_embed((int64_t)mem->model, content_ptr);

    // Store in DB with default importance
    double importance = 1.0;
    int64_t db_id = memdb_store((int64_t)mem->db, content_ptr, (int64_t)emb, importance);

    // Add to index
    hnsw_insert((int64_t)mem->index, (int64_t)emb, db_id);

    pthread_mutex_unlock(&mem->lock);
    return db_id;
}

// Retrieve relevant memories for context
int64_t specialist_memory_retrieve(int64_t mem_ptr, int64_t query_ptr, int64_t k) {
    SpecialistMemory* mem = (SpecialistMemory*)mem_ptr;
    SxString* query = (SxString*)query_ptr;
    if (!mem || !query) return 0;
    
    pthread_mutex_lock(&mem->lock);
    
    // Embed query
    Embedding* query_emb = (Embedding*)embedding_embed((int64_t)mem->model, query_ptr);
    
    // Search index
    SxVec* results = (SxVec*)hnsw_search((int64_t)mem->index, (int64_t)query_emb, k);
    
    // Fetch content
    SxVec* memories = intrinsic_vec_new();
    for (size_t i = 0; i < results->len; i++) {
        int64_t db_id = (int64_t)results->items[i];
        int64_t content = memdb_get((int64_t)mem->db, db_id);
        if (content) {
            intrinsic_vec_push(memories, (void*)content);
        }
    }
    
    embedding_free((int64_t)query_emb);
    
    pthread_mutex_unlock(&mem->lock);
    return (int64_t)memories;
}

// Close specialist memory
// Recall memories (wrapper for retrieve)
int64_t specialist_memory_recall(int64_t mem_ptr, int64_t category_ptr, int64_t k) {
    return specialist_memory_retrieve(mem_ptr, category_ptr, k);
}

// Get memory count
int64_t specialist_memory_count(int64_t mem_ptr) {
    SpecialistMemory* mem = (SpecialistMemory*)mem_ptr;
    if (!mem || !mem->db) return 0;
    return memdb_count((int64_t)mem->db);
}

// Forget/delete memory
int64_t specialist_memory_forget(int64_t mem_ptr, int64_t memory_id) {
    SpecialistMemory* mem = (SpecialistMemory*)mem_ptr;
    if (!mem || !mem->db) return 0;
    // Mark as deleted (soft delete)
    return memdb_set_importance((int64_t)mem->db, memory_id, -1.0);
}

void specialist_memory_close(int64_t mem_ptr) {
    SpecialistMemory* mem = (SpecialistMemory*)mem_ptr;
    if (!mem) return;

    pthread_mutex_lock(&mem->lock);
    free(mem->specialist_id);
    if (mem->db) memdb_close((int64_t)mem->db);
    if (mem->index) hnsw_close((int64_t)mem->index);
    if (mem->model) embedding_model_close((int64_t)mem->model);
    pthread_mutex_unlock(&mem->lock);
    pthread_mutex_destroy(&mem->lock);
    free(mem);
}

// --------------------------------------------------------------------------
// 29.5.5 Neural IR: Neural Gate Support
// --------------------------------------------------------------------------
// Neural gates are differentiable control flow constructs for training.
// In training mode, comparisons use sigmoid relaxation for gradient flow.
// In inference mode, comparisons are discrete (zero overhead).

// Global training state
static int64_t g_neural_training_mode = 0;  // 0 = inference, 1 = training
static double g_neural_temperature = 1.0;   // Temperature for Gumbel-Softmax

int64_t neural_get_training_mode(void) {
    return g_neural_training_mode;
}

void neural_set_training_mode(int64_t mode) {
    g_neural_training_mode = mode;
}

double neural_get_temperature(void) {
    return g_neural_temperature;
}

void neural_set_temperature(double temp) {
    g_neural_temperature = temp > 0.01 ? temp : 0.01;
}

double neural_sigmoid(double x) {
    return 1.0 / (1.0 + exp(-x * g_neural_temperature));
}

double neural_tanh(double x) {
    return tanh(x);
}

double neural_relu(double x) {
    return x > 0 ? x : 0;
}

// Neural gate structure
typedef struct NeuralGate {
    int64_t id;
    double threshold;
    double gradient;
    int64_t anima_binding;  // Pointer to bound anima, or 0
    char* name;
} NeuralGate;

int64_t neural_gate_new(int64_t id, double threshold, int64_t anima_binding, int64_t name_ptr) {
    NeuralGate* gate = (NeuralGate*)calloc(1, sizeof(NeuralGate));
    if (!gate) return 0;
    gate->id = id;
    gate->threshold = threshold;
    gate->gradient = 0.0;
    gate->anima_binding = anima_binding;
    SxString* name_str = (SxString*)name_ptr;
    gate->name = name_str ? strdup(name_str->data) : strdup("unnamed");
    return (int64_t)gate;
}

double neural_gate_threshold(int64_t gate_ptr) {
    NeuralGate* gate = (NeuralGate*)gate_ptr;
    return gate ? gate->threshold : 0.5;
}

void neural_gate_set_threshold(int64_t gate_ptr, double threshold) {
    NeuralGate* gate = (NeuralGate*)gate_ptr;
    if (gate) gate->threshold = threshold;
}

void neural_gate_add_gradient(int64_t gate_ptr, double grad) {
    NeuralGate* gate = (NeuralGate*)gate_ptr;
    if (gate) gate->gradient += grad;
}

double neural_gate_gradient(int64_t gate_ptr) {
    NeuralGate* gate = (NeuralGate*)gate_ptr;
    return gate ? gate->gradient : 0.0;
}

void neural_gate_zero_grad(int64_t gate_ptr) {
    NeuralGate* gate = (NeuralGate*)gate_ptr;
    if (gate) gate->gradient = 0.0;
}

void neural_gate_update(int64_t gate_ptr, double learning_rate) {
    NeuralGate* gate = (NeuralGate*)gate_ptr;
    if (gate) {
        gate->threshold -= learning_rate * gate->gradient;
        // Clamp threshold to [0, 1]
        if (gate->threshold < 0.0) gate->threshold = 0.0;
        if (gate->threshold > 1.0) gate->threshold = 1.0;
        gate->gradient = 0.0;
    }
}

// --------------------------------------------------------------------------
// 29.5.6 Neural IR: Autograd Support
// --------------------------------------------------------------------------
// Eager-mode automatic differentiation for neural gates.

typedef enum GradOp {
    GRAD_OP_INPUT = 0,
    GRAD_OP_ADD = 1,
    GRAD_OP_SUB = 2,
    GRAD_OP_MUL = 3,
    GRAD_OP_DIV = 4,
    GRAD_OP_NEG = 5,
    GRAD_OP_SIGMOID = 6,
    GRAD_OP_TANH = 7,
    GRAD_OP_RELU = 8,
    GRAD_OP_GT = 9,
    GRAD_OP_LT = 10,
    GRAD_OP_GE = 11,
    GRAD_OP_LE = 12,
} GradOp;

typedef struct GradValue {
    double value;
    double grad;
    GradOp op;
    struct GradValue* left;
    struct GradValue* right;
    int64_t tape;
} GradValue;

typedef struct GradTape {
    int64_t training_mode;
    double temperature;
    GradValue** nodes;
    int64_t node_count;
    int64_t node_capacity;
} GradTape;

int64_t grad_tape_new(void) {
    GradTape* tape = (GradTape*)calloc(1, sizeof(GradTape));
    if (!tape) return 0;
    tape->training_mode = 1;
    tape->temperature = 1.0;
    tape->node_capacity = 64;
    tape->nodes = (GradValue**)calloc(tape->node_capacity, sizeof(GradValue*));
    return (int64_t)tape;
}

void grad_tape_set_training(int64_t tape_ptr, int64_t mode) {
    GradTape* tape = (GradTape*)tape_ptr;
    if (tape) tape->training_mode = mode;
}

void grad_tape_set_temperature(int64_t tape_ptr, double temp) {
    GradTape* tape = (GradTape*)tape_ptr;
    if (tape) tape->temperature = temp > 0.01 ? temp : 0.01;
}

double grad_tape_temperature(int64_t tape_ptr) {
    GradTape* tape = (GradTape*)tape_ptr;
    return tape ? tape->temperature : 1.0;
}

void grad_tape_free(int64_t tape_ptr) {
    GradTape* tape = (GradTape*)tape_ptr;
    if (!tape) return;
    // Free all nodes in the tape
    for (int64_t i = 0; i < tape->node_count; i++) {
        if (tape->nodes[i]) {
            free(tape->nodes[i]);
        }
    }
    if (tape->nodes) free(tape->nodes);
    free(tape);
}

static void grad_tape_add_node(GradTape* tape, GradValue* node) {
    if (!tape || !node) return;
    if (tape->node_count >= tape->node_capacity) {
        tape->node_capacity *= 2;
        tape->nodes = (GradValue**)realloc(tape->nodes, tape->node_capacity * sizeof(GradValue*));
    }
    tape->nodes[tape->node_count++] = node;
    node->tape = (int64_t)tape;
}

int64_t grad_input(double value, int64_t tape_ptr) {
    GradTape* tape = (GradTape*)tape_ptr;
    GradValue* node = (GradValue*)calloc(1, sizeof(GradValue));
    if (!node) return 0;
    node->value = value;
    node->grad = 0.0;
    node->op = GRAD_OP_INPUT;
    node->left = NULL;
    node->right = NULL;
    if (tape) grad_tape_add_node(tape, node);
    return (int64_t)node;
}

double grad_value_get(int64_t node_ptr) {
    GradValue* node = (GradValue*)node_ptr;
    return node ? node->value : 0.0;
}

double grad_value_grad(int64_t node_ptr) {
    GradValue* node = (GradValue*)node_ptr;
    return node ? node->grad : 0.0;
}

int64_t grad_add(int64_t a_ptr, int64_t b_ptr) {
    GradValue* a = (GradValue*)a_ptr;
    GradValue* b = (GradValue*)b_ptr;
    if (!a || !b) return 0;
    GradValue* result = (GradValue*)calloc(1, sizeof(GradValue));
    if (!result) return 0;
    result->value = a->value + b->value;
    result->grad = 0.0;
    result->op = GRAD_OP_ADD;
    result->left = a;
    result->right = b;
    if (a->tape) grad_tape_add_node((GradTape*)a->tape, result);
    return (int64_t)result;
}

int64_t grad_sub(int64_t a_ptr, int64_t b_ptr) {
    GradValue* a = (GradValue*)a_ptr;
    GradValue* b = (GradValue*)b_ptr;
    if (!a || !b) return 0;
    GradValue* result = (GradValue*)calloc(1, sizeof(GradValue));
    if (!result) return 0;
    result->value = a->value - b->value;
    result->grad = 0.0;
    result->op = GRAD_OP_SUB;
    result->left = a;
    result->right = b;
    if (a->tape) grad_tape_add_node((GradTape*)a->tape, result);
    return (int64_t)result;
}

int64_t grad_mul(int64_t a_ptr, int64_t b_ptr) {
    GradValue* a = (GradValue*)a_ptr;
    GradValue* b = (GradValue*)b_ptr;
    if (!a || !b) return 0;
    GradValue* result = (GradValue*)calloc(1, sizeof(GradValue));
    if (!result) return 0;
    result->value = a->value * b->value;
    result->grad = 0.0;
    result->op = GRAD_OP_MUL;
    result->left = a;
    result->right = b;
    if (a->tape) grad_tape_add_node((GradTape*)a->tape, result);
    return (int64_t)result;
}

int64_t grad_div(int64_t a_ptr, int64_t b_ptr) {
    GradValue* a = (GradValue*)a_ptr;
    GradValue* b = (GradValue*)b_ptr;
    if (!a || !b || b->value == 0.0) return 0;
    GradValue* result = (GradValue*)calloc(1, sizeof(GradValue));
    if (!result) return 0;
    result->value = a->value / b->value;
    result->grad = 0.0;
    result->op = GRAD_OP_DIV;
    result->left = a;
    result->right = b;
    if (a->tape) grad_tape_add_node((GradTape*)a->tape, result);
    return (int64_t)result;
}

int64_t grad_neg(int64_t a_ptr) {
    GradValue* a = (GradValue*)a_ptr;
    if (!a) return 0;
    GradValue* result = (GradValue*)calloc(1, sizeof(GradValue));
    if (!result) return 0;
    result->value = -a->value;
    result->grad = 0.0;
    result->op = GRAD_OP_NEG;
    result->left = a;
    result->right = NULL;
    if (a->tape) grad_tape_add_node((GradTape*)a->tape, result);
    return (int64_t)result;
}

// Soft comparisons for differentiable control flow
int64_t grad_gt(int64_t a_ptr, int64_t b_ptr, int64_t tape_ptr) {
    GradValue* a = (GradValue*)a_ptr;
    GradValue* b = (GradValue*)b_ptr;
    GradTape* tape = (GradTape*)tape_ptr;
    if (!a || !b) return 0;
    GradValue* result = (GradValue*)calloc(1, sizeof(GradValue));
    if (!result) return 0;

    // In training mode: sigmoid((a - b) * temperature)
    // In inference mode: a > b ? 1.0 : 0.0
    if (tape && tape->training_mode) {
        double diff = a->value - b->value;
        result->value = 1.0 / (1.0 + exp(-diff * tape->temperature));
    } else {
        result->value = a->value > b->value ? 1.0 : 0.0;
    }
    result->grad = 0.0;
    result->op = GRAD_OP_GT;
    result->left = a;
    result->right = b;
    if (tape) grad_tape_add_node(tape, result);
    return (int64_t)result;
}

int64_t grad_lt(int64_t a_ptr, int64_t b_ptr, int64_t tape_ptr) {
    return grad_gt(b_ptr, a_ptr, tape_ptr);  // a < b ≡ b > a
}

int64_t grad_ge(int64_t a_ptr, int64_t b_ptr, int64_t tape_ptr) {
    // a >= b ≡ !(a < b) ≡ !(b > a)
    int64_t lt_result = grad_lt(a_ptr, b_ptr, tape_ptr);
    GradValue* lt_node = (GradValue*)lt_result;
    if (lt_node) lt_node->value = 1.0 - lt_node->value;
    return lt_result;
}

int64_t grad_le(int64_t a_ptr, int64_t b_ptr, int64_t tape_ptr) {
    // a <= b ≡ !(a > b)
    int64_t gt_result = grad_gt(a_ptr, b_ptr, tape_ptr);
    GradValue* gt_node = (GradValue*)gt_result;
    if (gt_node) gt_node->value = 1.0 - gt_node->value;
    return gt_result;
}

// Backward pass - compute gradients via backpropagation
static void backward_node(GradValue* node) {
    if (!node) return;
    switch (node->op) {
        case GRAD_OP_INPUT:
            break;
        case GRAD_OP_ADD:
            if (node->left) node->left->grad += node->grad;
            if (node->right) node->right->grad += node->grad;
            break;
        case GRAD_OP_SUB:
            if (node->left) node->left->grad += node->grad;
            if (node->right) node->right->grad -= node->grad;
            break;
        case GRAD_OP_MUL:
            if (node->left && node->right) {
                node->left->grad += node->grad * node->right->value;
                node->right->grad += node->grad * node->left->value;
            }
            break;
        case GRAD_OP_DIV:
            if (node->left && node->right && node->right->value != 0.0) {
                node->left->grad += node->grad / node->right->value;
                node->right->grad -= node->grad * node->left->value / (node->right->value * node->right->value);
            }
            break;
        case GRAD_OP_NEG:
            if (node->left) node->left->grad -= node->grad;
            break;
        case GRAD_OP_SIGMOID:
            // d/dx sigmoid(x) = sigmoid(x) * (1 - sigmoid(x))
            if (node->left) {
                double s = node->value;
                node->left->grad += node->grad * s * (1.0 - s);
            }
            break;
        case GRAD_OP_GT:
        case GRAD_OP_LT:
        case GRAD_OP_GE:
        case GRAD_OP_LE:
            // Gradient of sigmoid comparison
            if (node->left && node->right) {
                double s = node->value;
                double grad_s = s * (1.0 - s);
                node->left->grad += node->grad * grad_s;
                node->right->grad -= node->grad * grad_s;
            }
            break;
        default:
            break;
    }
}

void backward(int64_t node_ptr) {
    GradValue* node = (GradValue*)node_ptr;
    if (!node) return;

    GradTape* tape = (GradTape*)node->tape;
    if (!tape) return;

    // Set output gradient to 1
    node->grad = 1.0;

    // Backward through all nodes in reverse order
    for (int64_t i = tape->node_count - 1; i >= 0; i--) {
        backward_node(tape->nodes[i]);
    }
}

void anneal_exponential_tape(int64_t tape_ptr, double decay) {
    GradTape* tape = (GradTape*)tape_ptr;
    if (tape) {
        tape->temperature *= decay;
        if (tape->temperature < 0.01) tape->temperature = 0.01;
    }
}

// Standalone exponential annealing: returns new_temp = initial * decay^steps
double anneal_exponential(double initial_temp, double decay, int64_t steps) {
    double temp = initial_temp;
    for (int64_t i = 0; i < steps; i++) {
        temp *= decay;
    }
    if (temp < 0.01) temp = 0.01;
    return temp;
}

void anneal_linear(int64_t tape_ptr, double step) {
    GradTape* tape = (GradTape*)tape_ptr;
    if (tape) {
        tape->temperature -= step;
        if (tape->temperature < 0.01) tape->temperature = 0.01;
    }
}

// Sample from Gumbel(0, 1) distribution
// g = -log(-log(uniform(0,1)))
static double sample_gumbel_noise(void) {
    double u = (double)rand() / (double)RAND_MAX;
    // Clamp to avoid log(0)
    if (u < 1e-20) u = 1e-20;
    if (u > 1.0 - 1e-20) u = 1.0 - 1e-20;
    return -log(-log(u));
}

// Gumbel-Softmax for differentiable categorical sampling
// Implements: softmax((logits + gumbel_noise) / temperature)
// Reference: Jang et al., "Categorical Reparameterization with Gumbel-Softmax" (2017)
int64_t gumbel_softmax(int64_t logits_ptr, double temperature, int64_t tape_ptr) {
    SxVec* logits = (SxVec*)logits_ptr;
    if (!logits || logits->len == 0) return 0;

    SxVec* result = intrinsic_vec_new();
    double max_val = -INFINITY;
    double sum = 0.0;

    // Ensure minimum temperature for numerical stability
    if (temperature < 0.01) temperature = 0.01;

    // Allocate temp arrays for perturbed logits
    double* perturbed = (double*)malloc(logits->len * sizeof(double));
    if (!perturbed) return 0;

    // Step 1: Add Gumbel noise to logits and find max for stability
    for (size_t i = 0; i < logits->len; i++) {
        double val;
        memcpy(&val, &logits->items[i], sizeof(double));
        // Add Gumbel noise: g_i = -log(-log(u)) where u ~ Uniform(0,1)
        double gumbel_noise = sample_gumbel_noise();
        perturbed[i] = (val + gumbel_noise) / temperature;
        if (perturbed[i] > max_val) max_val = perturbed[i];
    }

    // Step 2: Compute softmax with numerical stability (subtract max)
    for (size_t i = 0; i < logits->len; i++) {
        double exp_val = exp(perturbed[i] - max_val);
        perturbed[i] = exp_val;
        sum += exp_val;
    }

    // Step 3: Normalize and store in result
    for (size_t i = 0; i < logits->len; i++) {
        perturbed[i] /= sum;
        // Store the double value as a pointer (bit cast)
        void* ptr;
        memcpy(&ptr, &perturbed[i], sizeof(double));
        intrinsic_vec_push(result, ptr);
    }

    free(perturbed);
    return (int64_t)result;
}

// Straight-Through Gumbel-Softmax
// Returns one-hot vector in forward pass, uses soft gradients in backward
// Reference: Bengio et al., "Estimating or Propagating Gradients Through Stochastic Neurons" (2013)
int64_t gumbel_softmax_hard(int64_t logits_ptr, double temperature, int64_t tape_ptr) {
    SxVec* logits = (SxVec*)logits_ptr;
    if (!logits || logits->len == 0) return 0;

    // First compute soft Gumbel-Softmax for gradients
    int64_t soft_result = gumbel_softmax(logits_ptr, temperature, tape_ptr);
    SxVec* soft = (SxVec*)soft_result;
    if (!soft) return 0;

    // Find argmax from soft result
    size_t argmax_idx = 0;
    double max_prob = -INFINITY;
    for (size_t i = 0; i < soft->len; i++) {
        double prob;
        memcpy(&prob, &soft->items[i], sizeof(double));
        if (prob > max_prob) {
            max_prob = prob;
            argmax_idx = i;
        }
    }

    // Create one-hot result (straight-through in forward)
    SxVec* result = intrinsic_vec_new();
    for (size_t i = 0; i < soft->len; i++) {
        double val = (i == argmax_idx) ? 1.0 : 0.0;
        void* ptr;
        memcpy(&ptr, &val, sizeof(double));
        intrinsic_vec_push(result, ptr);
    }

    // In a full autograd implementation, we would store soft_result
    // for backward pass gradient computation. For now, free it.
    // TODO: Track soft_result in tape for proper gradient computation
    intrinsic_vec_free(soft);

    return (int64_t)result;
}

// --------------------------------------------------------------------------
// 29.5.7 Neural IR: Contract Verification
// --------------------------------------------------------------------------
// Contracts provide safety guarantees for neural gates:
// - requires: preconditions that must hold before gate execution
// - ensures: postconditions that must hold after gate execution
// - invariant: properties that must hold throughout
// - fallback: alternative paths when contracts fail

typedef enum ContractViolationType {
    CONTRACT_OK = 0,
    CONTRACT_REQUIRES_FAILED = 1,
    CONTRACT_ENSURES_FAILED = 2,
    CONTRACT_INVARIANT_FAILED = 3,
    CONTRACT_CONFIDENCE_LOW = 4,
} ContractViolationType;

typedef struct ContractResult {
    int64_t satisfied;        // 1 if all contracts satisfied, 0 otherwise
    ContractViolationType violation_type;
    double actual_value;      // The actual value that caused violation
    double expected_min;      // Expected minimum bound
    double expected_max;      // Expected maximum bound
    char* message;            // Human-readable violation message
} ContractResult;

// Global contract settings
static double g_contract_min_confidence = 0.0;  // Default: no minimum
static int64_t g_contract_panic_on_fail = 0;    // Default: return fallback

void contract_set_min_confidence(double min_conf) {
    g_contract_min_confidence = min_conf;
}

double contract_get_min_confidence(void) {
    return g_contract_min_confidence;
}

void contract_set_panic_mode(int64_t panic) {
    g_contract_panic_on_fail = panic;
}

int64_t contract_get_panic_mode(void) {
    return g_contract_panic_on_fail;
}

// Create a new contract result (satisfied)
int64_t contract_result_ok(void) {
    ContractResult* result = (ContractResult*)calloc(1, sizeof(ContractResult));
    if (!result) return 0;
    result->satisfied = 1;
    result->violation_type = CONTRACT_OK;
    result->message = strdup("Contract satisfied");
    return (int64_t)result;
}

// Create a new contract result (failed)
int64_t contract_result_fail(int64_t violation_type, double actual, double expected_min, double expected_max, int64_t msg_ptr) {
    ContractResult* result = (ContractResult*)calloc(1, sizeof(ContractResult));
    if (!result) return 0;
    result->satisfied = 0;
    result->violation_type = (ContractViolationType)violation_type;
    result->actual_value = actual;
    result->expected_min = expected_min;
    result->expected_max = expected_max;
    SxString* msg = (SxString*)msg_ptr;
    result->message = msg ? strdup(msg->data) : strdup("Contract violated");
    return (int64_t)result;
}

// Check contract result
int64_t contract_result_satisfied(int64_t result_ptr) {
    ContractResult* result = (ContractResult*)result_ptr;
    return result ? result->satisfied : 0;
}

int64_t contract_result_violation_type(int64_t result_ptr) {
    ContractResult* result = (ContractResult*)result_ptr;
    return result ? (int64_t)result->violation_type : 0;
}

double contract_result_actual(int64_t result_ptr) {
    ContractResult* result = (ContractResult*)result_ptr;
    return result ? result->actual_value : 0.0;
}

int64_t contract_result_message(int64_t result_ptr) {
    ContractResult* result = (ContractResult*)result_ptr;
    if (!result || !result->message) return 0;
    return (int64_t)intrinsic_string_new(result->message);
}

void contract_result_free(int64_t result_ptr) {
    ContractResult* result = (ContractResult*)result_ptr;
    if (result) {
        if (result->message) free(result->message);
        free(result);
    }
}

// Check requires clause (precondition)
// Returns contract result
int64_t contract_check_requires(double condition, double min_confidence, int64_t msg_ptr) {
    // In training mode with soft comparisons, condition is [0,1]
    // In inference mode, condition is 0 or 1
    if (condition >= min_confidence && condition >= g_contract_min_confidence) {
        return contract_result_ok();
    }
    return contract_result_fail(CONTRACT_REQUIRES_FAILED, condition, min_confidence, 1.0, msg_ptr);
}

// Check ensures clause (postcondition)
int64_t contract_check_ensures(double condition, double min_confidence, int64_t msg_ptr) {
    if (condition >= min_confidence && condition >= g_contract_min_confidence) {
        return contract_result_ok();
    }
    return contract_result_fail(CONTRACT_ENSURES_FAILED, condition, min_confidence, 1.0, msg_ptr);
}

// Check invariant clause
int64_t contract_check_invariant(double condition, double min_confidence, int64_t msg_ptr) {
    if (condition >= min_confidence && condition >= g_contract_min_confidence) {
        return contract_result_ok();
    }
    return contract_result_fail(CONTRACT_INVARIANT_FAILED, condition, min_confidence, 1.0, msg_ptr);
}

// Check confidence bound for neural gate output
int64_t contract_check_confidence(double confidence, double min_bound, double max_bound, int64_t msg_ptr) {
    if (confidence >= min_bound && confidence <= max_bound) {
        return contract_result_ok();
    }
    return contract_result_fail(CONTRACT_CONFIDENCE_LOW, confidence, min_bound, max_bound, msg_ptr);
}

// Handle contract violation - either panic or log and continue
void contract_handle_violation(int64_t result_ptr) {
    ContractResult* result = (ContractResult*)result_ptr;
    if (!result || result->satisfied) return;

    // Log the violation
    fprintf(stderr, "CONTRACT VIOLATION [type=%d]: %s\n",
            result->violation_type,
            result->message ? result->message : "unknown");
    fprintf(stderr, "  actual=%.4f, expected=[%.4f, %.4f]\n",
            result->actual_value, result->expected_min, result->expected_max);

    if (g_contract_panic_on_fail) {
        fprintf(stderr, "PANIC: Contract violation in strict mode\n");
        exit(1);
    }
}

// Range contract for checking value bounds
int64_t contract_in_range(double value, double min_val, double max_val) {
    if (value >= min_val && value <= max_val) {
        return contract_result_ok();
    }
    return contract_result_fail(CONTRACT_INVARIANT_FAILED, value, min_val, max_val, 0);
}

// Neural gate with contract verification
// Returns: gate output if contracts pass, fallback_value if contracts fail
double neural_gate_with_contract(int64_t gate_ptr, double input, double fallback_value,
                                  double requires_conf, double ensures_conf) {
    NeuralGate* gate = (NeuralGate*)gate_ptr;
    if (!gate) return fallback_value;

    // Check precondition (input in valid range)
    int64_t pre_result = contract_check_requires(input >= 0.0 ? 1.0 : 0.0, requires_conf, 0);
    if (!contract_result_satisfied(pre_result)) {
        contract_handle_violation(pre_result);
        contract_result_free(pre_result);
        return fallback_value;
    }
    contract_result_free(pre_result);

    // Compute gate output
    double output;
    if (g_neural_training_mode) {
        // Training: soft sigmoid-based decision
        output = neural_sigmoid(input - gate->threshold);
    } else {
        // Inference: discrete decision
        output = input > gate->threshold ? 1.0 : 0.0;
    }

    // Check postcondition (output is valid probability)
    int64_t post_result = contract_check_ensures(output >= 0.0 && output <= 1.0 ? 1.0 : 0.0, ensures_conf, 0);
    if (!contract_result_satisfied(post_result)) {
        contract_handle_violation(post_result);
        contract_result_free(post_result);
        return fallback_value;
    }
    contract_result_free(post_result);

    return output;
}

// ==========================================================================
// Phase 3: Hardware-Aware Compilation
// ==========================================================================
// This phase implements graph partitioning, device targeting, and cross-device
// data marshalling for neural gates. Gates can be annotated with @cpu, @gpu,
// or @npu to control placement, or automatically placed based on operation type.

// --------------------------------------------------------------------------
// 3.1 Device Target Enumeration
// --------------------------------------------------------------------------

typedef enum DeviceTarget {
    DEVICE_CPU = 0,      // CPU execution (default for control flow)
    DEVICE_GPU = 1,      // GPU execution (batch tensor operations)
    DEVICE_NPU = 2,      // NPU execution (neural inference accelerator)
    DEVICE_AUTO = 3,     // Automatic placement by compiler
    DEVICE_ANY = 4       // Can run on any device
} DeviceTarget;

typedef enum OperationType {
    OP_TYPE_CONTROL_FLOW = 0,   // Branching, conditionals
    OP_TYPE_TENSOR_OPS = 1,     // Matrix multiply, convolution
    OP_TYPE_MEMORY_BOUND = 2,   // I/O, memory access
    OP_TYPE_NEURAL_INFER = 3,   // Neural network inference
    OP_TYPE_SCALAR_MATH = 4,    // Simple arithmetic
    OP_TYPE_REDUCTION = 5       // Aggregations, reductions
} OperationType;

typedef struct DeviceCapabilities {
    int64_t id;
    DeviceTarget type;
    char* name;
    int64_t compute_units;       // Number of compute units
    int64_t memory_bytes;        // Available memory
    double bandwidth_gbps;       // Memory bandwidth
    double flops_per_sec;        // Floating point operations/sec
    int64_t supports_fp16;       // Half precision support
    int64_t supports_int8;       // INT8 quantization support
    int64_t available;           // Device is available
} DeviceCapabilities;

// Global device registry
#define MAX_DEVICES 16
static DeviceCapabilities g_devices[MAX_DEVICES];
static int64_t g_device_count = 0;
static int64_t g_default_device = DEVICE_CPU;
static pthread_mutex_t g_device_lock = PTHREAD_MUTEX_INITIALIZER;

// Initialize device registry
void device_registry_init(void) {
    pthread_mutex_lock(&g_device_lock);
    if (g_device_count == 0) {
        // Always register CPU
        g_devices[0].id = 0;
        g_devices[0].type = DEVICE_CPU;
        g_devices[0].name = strdup("cpu0");
        g_devices[0].compute_units = 8;  // Assume 8 cores
        g_devices[0].memory_bytes = 16L * 1024 * 1024 * 1024;  // 16GB
        g_devices[0].bandwidth_gbps = 50.0;
        g_devices[0].flops_per_sec = 100e9;  // 100 GFLOPS
        g_devices[0].supports_fp16 = 1;
        g_devices[0].supports_int8 = 1;
        g_devices[0].available = 1;
        g_device_count = 1;
    }
    pthread_mutex_unlock(&g_device_lock);
}

// Register a new device
int64_t device_register(int64_t device_type, int64_t name_ptr, int64_t compute_units,
                        int64_t memory_bytes, double bandwidth, double flops) {
    pthread_mutex_lock(&g_device_lock);
    if (g_device_count >= MAX_DEVICES) {
        pthread_mutex_unlock(&g_device_lock);
        return -1;
    }

    int64_t id = g_device_count;
    g_devices[id].id = id;
    g_devices[id].type = (DeviceTarget)device_type;
    g_devices[id].name = name_ptr ? strdup((char*)name_ptr) : strdup("unknown");
    g_devices[id].compute_units = compute_units;
    g_devices[id].memory_bytes = memory_bytes;
    g_devices[id].bandwidth_gbps = bandwidth;
    g_devices[id].flops_per_sec = flops;
    g_devices[id].supports_fp16 = 1;
    g_devices[id].supports_int8 = 1;
    g_devices[id].available = 1;
    g_device_count++;

    pthread_mutex_unlock(&g_device_lock);
    return id;
}

// Get device by ID
int64_t device_get(int64_t device_id) {
    if (device_id < 0 || device_id >= g_device_count) return 0;
    return (int64_t)&g_devices[device_id];
}

// Get device type
int64_t device_type(int64_t device_ptr) {
    if (!device_ptr) return DEVICE_CPU;
    DeviceCapabilities* dev = (DeviceCapabilities*)device_ptr;
    return dev->type;
}

// Get device name
int64_t device_name(int64_t device_ptr) {
    if (!device_ptr) return 0;
    DeviceCapabilities* dev = (DeviceCapabilities*)device_ptr;
    return (int64_t)dev->name;
}

// Check device availability
int64_t device_available(int64_t device_ptr) {
    if (!device_ptr) return 0;
    DeviceCapabilities* dev = (DeviceCapabilities*)device_ptr;
    return dev->available;
}

// Set default device
void device_set_default(int64_t device_type) {
    g_default_device = device_type;
}

// Get default device
int64_t device_get_default(void) {
    return g_default_device;
}

// Count registered devices
int64_t device_count(void) {
    return g_device_count;
}

// --------------------------------------------------------------------------
// 3.2 Neural Gate Device Annotations
// --------------------------------------------------------------------------

typedef struct GateDeviceBinding {
    int64_t gate_id;
    DeviceTarget target;
    int64_t explicit_binding;    // 1 if user-specified, 0 if auto
    OperationType op_type;
    double estimated_flops;
    double memory_requirement;
} GateDeviceBinding;

#define MAX_GATE_BINDINGS 1024
static GateDeviceBinding g_gate_bindings[MAX_GATE_BINDINGS];
static int64_t g_gate_binding_count = 0;

// Bind a gate to a specific device
int64_t gate_bind_device(int64_t gate_id, int64_t device_type) {
    if (g_gate_binding_count >= MAX_GATE_BINDINGS) return 0;

    // Check if binding already exists
    for (int64_t i = 0; i < g_gate_binding_count; i++) {
        if (g_gate_bindings[i].gate_id == gate_id) {
            g_gate_bindings[i].target = (DeviceTarget)device_type;
            g_gate_bindings[i].explicit_binding = 1;
            return 1;
        }
    }

    // Create new binding
    g_gate_bindings[g_gate_binding_count].gate_id = gate_id;
    g_gate_bindings[g_gate_binding_count].target = (DeviceTarget)device_type;
    g_gate_bindings[g_gate_binding_count].explicit_binding = 1;
    g_gate_bindings[g_gate_binding_count].op_type = OP_TYPE_SCALAR_MATH;
    g_gate_bindings[g_gate_binding_count].estimated_flops = 0;
    g_gate_bindings[g_gate_binding_count].memory_requirement = 0;
    g_gate_binding_count++;

    return 1;
}

// Get gate device binding
int64_t gate_get_device(int64_t gate_id) {
    for (int64_t i = 0; i < g_gate_binding_count; i++) {
        if (g_gate_bindings[i].gate_id == gate_id) {
            return g_gate_bindings[i].target;
        }
    }
    return g_default_device;
}

// Check if gate has explicit binding
int64_t gate_has_explicit_binding(int64_t gate_id) {
    for (int64_t i = 0; i < g_gate_binding_count; i++) {
        if (g_gate_bindings[i].gate_id == gate_id) {
            return g_gate_bindings[i].explicit_binding;
        }
    }
    return 0;
}

// Set gate operation type (for auto-placement)
void gate_set_op_type(int64_t gate_id, int64_t op_type) {
    for (int64_t i = 0; i < g_gate_binding_count; i++) {
        if (g_gate_bindings[i].gate_id == gate_id) {
            g_gate_bindings[i].op_type = (OperationType)op_type;
            return;
        }
    }
    // Create binding if it doesn't exist
    if (g_gate_binding_count < MAX_GATE_BINDINGS) {
        g_gate_bindings[g_gate_binding_count].gate_id = gate_id;
        g_gate_bindings[g_gate_binding_count].target = DEVICE_AUTO;
        g_gate_bindings[g_gate_binding_count].explicit_binding = 0;
        g_gate_bindings[g_gate_binding_count].op_type = (OperationType)op_type;
        g_gate_binding_count++;
    }
}

// --------------------------------------------------------------------------
// 3.3 Graph Partitioning Algorithm
// --------------------------------------------------------------------------

typedef struct GraphNode {
    int64_t id;
    int64_t gate_id;
    DeviceTarget assigned_device;
    OperationType op_type;
    double cost;                 // Execution cost
    int64_t* dependencies;       // Array of dependency node IDs
    int64_t dep_count;
    int64_t* dependents;         // Nodes that depend on this
    int64_t dependent_count;
    int64_t scheduled;           // Already scheduled?
    int64_t partition_id;        // Which partition this belongs to
} GraphNode;

typedef struct GraphPartition {
    int64_t id;
    DeviceTarget device;
    int64_t* node_ids;
    int64_t node_count;
    int64_t capacity;
    double total_cost;
} GraphPartition;

typedef struct ComputeGraph {
    GraphNode* nodes;
    int64_t node_count;
    int64_t capacity;
    GraphPartition* partitions;
    int64_t partition_count;
    int64_t partition_capacity;
} ComputeGraph;

// Create a new compute graph
int64_t graph_new(void) {
    ComputeGraph* g = (ComputeGraph*)calloc(1, sizeof(ComputeGraph));
    if (!g) return 0;

    g->capacity = 64;
    g->nodes = (GraphNode*)calloc(g->capacity, sizeof(GraphNode));
    g->partition_capacity = 8;
    g->partitions = (GraphPartition*)calloc(g->partition_capacity, sizeof(GraphPartition));

    return (int64_t)g;
}

// Add a node to the graph
int64_t graph_add_node(int64_t graph_ptr, int64_t gate_id, int64_t op_type, double cost) {
    ComputeGraph* g = (ComputeGraph*)graph_ptr;
    if (!g) return -1;

    if (g->node_count >= g->capacity) {
        g->capacity *= 2;
        g->nodes = (GraphNode*)realloc(g->nodes, g->capacity * sizeof(GraphNode));
    }

    int64_t id = g->node_count;
    g->nodes[id].id = id;
    g->nodes[id].gate_id = gate_id;
    g->nodes[id].assigned_device = DEVICE_AUTO;
    g->nodes[id].op_type = (OperationType)op_type;
    g->nodes[id].cost = cost;
    g->nodes[id].dependencies = NULL;
    g->nodes[id].dep_count = 0;
    g->nodes[id].dependents = NULL;
    g->nodes[id].dependent_count = 0;
    g->nodes[id].scheduled = 0;
    g->nodes[id].partition_id = -1;
    g->node_count++;

    return id;
}

// Add a dependency edge
void graph_add_edge(int64_t graph_ptr, int64_t from_node, int64_t to_node) {
    ComputeGraph* g = (ComputeGraph*)graph_ptr;
    if (!g || from_node < 0 || from_node >= g->node_count ||
        to_node < 0 || to_node >= g->node_count) return;

    // Add to_node as dependency of from_node
    GraphNode* node = &g->nodes[from_node];
    node->dependencies = (int64_t*)realloc(node->dependencies,
                                           (node->dep_count + 1) * sizeof(int64_t));
    node->dependencies[node->dep_count++] = to_node;

    // Add from_node as dependent of to_node
    GraphNode* dep = &g->nodes[to_node];
    dep->dependents = (int64_t*)realloc(dep->dependents,
                                        (dep->dependent_count + 1) * sizeof(int64_t));
    dep->dependents[dep->dependent_count++] = from_node;
}

// Automatic device placement heuristics
DeviceTarget auto_place_node(GraphNode* node) {
    switch (node->op_type) {
        case OP_TYPE_CONTROL_FLOW:
            // Control flow runs on CPU (branching)
            return DEVICE_CPU;

        case OP_TYPE_TENSOR_OPS:
            // Tensor operations prefer GPU
            return DEVICE_GPU;

        case OP_TYPE_MEMORY_BOUND:
            // Memory-bound ops stay on CPU (I/O)
            return DEVICE_CPU;

        case OP_TYPE_NEURAL_INFER:
            // Neural inference prefers NPU if available, else GPU
            for (int64_t i = 0; i < g_device_count; i++) {
                if (g_devices[i].type == DEVICE_NPU && g_devices[i].available) {
                    return DEVICE_NPU;
                }
            }
            // Fall back to GPU
            for (int64_t i = 0; i < g_device_count; i++) {
                if (g_devices[i].type == DEVICE_GPU && g_devices[i].available) {
                    return DEVICE_GPU;
                }
            }
            return DEVICE_CPU;

        case OP_TYPE_SCALAR_MATH:
            // Simple math on CPU
            return DEVICE_CPU;

        case OP_TYPE_REDUCTION:
            // Reductions can go on GPU if large enough
            if (node->cost > 1000.0) {
                return DEVICE_GPU;
            }
            return DEVICE_CPU;

        default:
            return DEVICE_CPU;
    }
}

// Partition the graph by device
void graph_partition(int64_t graph_ptr) {
    ComputeGraph* g = (ComputeGraph*)graph_ptr;
    if (!g || g->node_count == 0) return;

    // First, assign devices to all nodes
    for (int64_t i = 0; i < g->node_count; i++) {
        GraphNode* node = &g->nodes[i];

        // Check for explicit binding
        if (gate_has_explicit_binding(node->gate_id)) {
            node->assigned_device = (DeviceTarget)gate_get_device(node->gate_id);
        } else {
            // Auto-place based on operation type
            node->assigned_device = auto_place_node(node);
        }
    }

    // Create partitions for each device type
    // Count nodes per device
    int64_t cpu_count = 0, gpu_count = 0, npu_count = 0;
    for (int64_t i = 0; i < g->node_count; i++) {
        switch (g->nodes[i].assigned_device) {
            case DEVICE_CPU: cpu_count++; break;
            case DEVICE_GPU: gpu_count++; break;
            case DEVICE_NPU: npu_count++; break;
            default: cpu_count++; break;
        }
    }

    // Create partitions
    g->partition_count = 0;

    if (cpu_count > 0) {
        GraphPartition* p = &g->partitions[g->partition_count];
        p->id = g->partition_count;
        p->device = DEVICE_CPU;
        p->node_ids = (int64_t*)malloc(cpu_count * sizeof(int64_t));
        p->node_count = 0;
        p->capacity = cpu_count;
        p->total_cost = 0;
        g->partition_count++;
    }

    if (gpu_count > 0) {
        GraphPartition* p = &g->partitions[g->partition_count];
        p->id = g->partition_count;
        p->device = DEVICE_GPU;
        p->node_ids = (int64_t*)malloc(gpu_count * sizeof(int64_t));
        p->node_count = 0;
        p->capacity = gpu_count;
        p->total_cost = 0;
        g->partition_count++;
    }

    if (npu_count > 0) {
        GraphPartition* p = &g->partitions[g->partition_count];
        p->id = g->partition_count;
        p->device = DEVICE_NPU;
        p->node_ids = (int64_t*)malloc(npu_count * sizeof(int64_t));
        p->node_count = 0;
        p->capacity = npu_count;
        p->total_cost = 0;
        g->partition_count++;
    }

    // Assign nodes to partitions
    for (int64_t i = 0; i < g->node_count; i++) {
        GraphNode* node = &g->nodes[i];

        // Find matching partition
        for (int64_t j = 0; j < g->partition_count; j++) {
            if (g->partitions[j].device == node->assigned_device ||
                (node->assigned_device != DEVICE_CPU &&
                 node->assigned_device != DEVICE_GPU &&
                 node->assigned_device != DEVICE_NPU &&
                 g->partitions[j].device == DEVICE_CPU)) {

                GraphPartition* p = &g->partitions[j];
                p->node_ids[p->node_count++] = i;
                p->total_cost += node->cost;
                node->partition_id = j;
                break;
            }
        }
    }
}

// Get number of partitions
int64_t graph_partition_count(int64_t graph_ptr) {
    ComputeGraph* g = (ComputeGraph*)graph_ptr;
    if (!g) return 0;
    return g->partition_count;
}

// Get partition device type
int64_t graph_partition_device(int64_t graph_ptr, int64_t partition_id) {
    ComputeGraph* g = (ComputeGraph*)graph_ptr;
    if (!g || partition_id < 0 || partition_id >= g->partition_count) return DEVICE_CPU;
    return g->partitions[partition_id].device;
}

// Get partition node count
int64_t graph_partition_node_count(int64_t graph_ptr, int64_t partition_id) {
    ComputeGraph* g = (ComputeGraph*)graph_ptr;
    if (!g || partition_id < 0 || partition_id >= g->partition_count) return 0;
    return g->partitions[partition_id].node_count;
}

// Get partition total cost
double graph_partition_cost(int64_t graph_ptr, int64_t partition_id) {
    ComputeGraph* g = (ComputeGraph*)graph_ptr;
    if (!g || partition_id < 0 || partition_id >= g->partition_count) return 0.0;
    return g->partitions[partition_id].total_cost;
}

// Free compute graph
void graph_free(int64_t graph_ptr) {
    ComputeGraph* g = (ComputeGraph*)graph_ptr;
    if (!g) return;

    for (int64_t i = 0; i < g->node_count; i++) {
        free(g->nodes[i].dependencies);
        free(g->nodes[i].dependents);
    }
    free(g->nodes);

    for (int64_t i = 0; i < g->partition_count; i++) {
        free(g->partitions[i].node_ids);
    }
    free(g->partitions);

    free(g);
}

// --------------------------------------------------------------------------
// 3.4 Cross-Device Data Marshalling
// --------------------------------------------------------------------------

typedef enum TransferDirection {
    TRANSFER_HOST_TO_DEVICE = 0,
    TRANSFER_DEVICE_TO_HOST = 1,
    TRANSFER_DEVICE_TO_DEVICE = 2
} TransferDirection;

typedef struct DataTransfer {
    int64_t id;
    void* src_ptr;
    void* dst_ptr;
    int64_t size_bytes;
    DeviceTarget src_device;
    DeviceTarget dst_device;
    int64_t async;               // Async transfer?
    int64_t completed;           // Transfer done?
    double transfer_time_ms;     // Actual transfer time
} DataTransfer;

typedef struct TransferQueue {
    DataTransfer* transfers;
    int64_t count;
    int64_t capacity;
    int64_t next_id;
    pthread_mutex_t lock;
} TransferQueue;

static TransferQueue g_transfer_queue = {0};

// Initialize transfer queue
void transfer_queue_init(void) {
    if (g_transfer_queue.transfers == NULL) {
        g_transfer_queue.capacity = 256;
        g_transfer_queue.transfers = (DataTransfer*)calloc(g_transfer_queue.capacity,
                                                           sizeof(DataTransfer));
        g_transfer_queue.count = 0;
        g_transfer_queue.next_id = 1;
        pthread_mutex_init(&g_transfer_queue.lock, NULL);
    }
}

// Queue a data transfer
int64_t transfer_queue_add(int64_t src_ptr, int64_t dst_ptr, int64_t size_bytes,
                           int64_t src_device, int64_t dst_device, int64_t async) {
    transfer_queue_init();

    pthread_mutex_lock(&g_transfer_queue.lock);

    if (g_transfer_queue.count >= g_transfer_queue.capacity) {
        g_transfer_queue.capacity *= 2;
        g_transfer_queue.transfers = (DataTransfer*)realloc(
            g_transfer_queue.transfers,
            g_transfer_queue.capacity * sizeof(DataTransfer));
    }

    int64_t id = g_transfer_queue.next_id++;
    DataTransfer* t = &g_transfer_queue.transfers[g_transfer_queue.count++];
    t->id = id;
    t->src_ptr = (void*)src_ptr;
    t->dst_ptr = (void*)dst_ptr;
    t->size_bytes = size_bytes;
    t->src_device = (DeviceTarget)src_device;
    t->dst_device = (DeviceTarget)dst_device;
    t->async = async;
    t->completed = 0;
    t->transfer_time_ms = 0;

    pthread_mutex_unlock(&g_transfer_queue.lock);

    return id;
}

// Execute a data transfer (simulated for CPU-only, real impl would use CUDA/etc)
void transfer_execute(int64_t transfer_id) {
    pthread_mutex_lock(&g_transfer_queue.lock);

    DataTransfer* t = NULL;
    for (int64_t i = 0; i < g_transfer_queue.count; i++) {
        if (g_transfer_queue.transfers[i].id == transfer_id) {
            t = &g_transfer_queue.transfers[i];
            break;
        }
    }

    if (t && !t->completed) {
        // Simulate transfer (in real impl, would use device-specific APIs)
        // For CPU-to-CPU, just memcpy
        if (t->src_device == DEVICE_CPU && t->dst_device == DEVICE_CPU) {
            memcpy(t->dst_ptr, t->src_ptr, t->size_bytes);
        } else {
            // For GPU/NPU, would call CUDA/OpenCL/etc APIs
            // For now, simulate with memcpy
            memcpy(t->dst_ptr, t->src_ptr, t->size_bytes);
        }

        // Estimate transfer time based on bandwidth
        double bandwidth_bps = 50e9;  // 50 GB/s default
        t->transfer_time_ms = (t->size_bytes / bandwidth_bps) * 1000.0;
        t->completed = 1;
    }

    pthread_mutex_unlock(&g_transfer_queue.lock);
}

// Check if transfer is complete
int64_t transfer_is_complete(int64_t transfer_id) {
    pthread_mutex_lock(&g_transfer_queue.lock);

    int64_t result = 0;
    for (int64_t i = 0; i < g_transfer_queue.count; i++) {
        if (g_transfer_queue.transfers[i].id == transfer_id) {
            result = g_transfer_queue.transfers[i].completed;
            break;
        }
    }

    pthread_mutex_unlock(&g_transfer_queue.lock);
    return result;
}

// Get transfer time
double transfer_time_ms(int64_t transfer_id) {
    pthread_mutex_lock(&g_transfer_queue.lock);

    double result = 0;
    for (int64_t i = 0; i < g_transfer_queue.count; i++) {
        if (g_transfer_queue.transfers[i].id == transfer_id) {
            result = g_transfer_queue.transfers[i].transfer_time_ms;
            break;
        }
    }

    pthread_mutex_unlock(&g_transfer_queue.lock);
    return result;
}

// Execute all pending transfers
void transfer_sync_all(void) {
    pthread_mutex_lock(&g_transfer_queue.lock);

    for (int64_t i = 0; i < g_transfer_queue.count; i++) {
        DataTransfer* t = &g_transfer_queue.transfers[i];
        if (!t->completed) {
            if (t->src_ptr && t->dst_ptr && t->size_bytes > 0) {
                memcpy(t->dst_ptr, t->src_ptr, t->size_bytes);
            }
            t->completed = 1;
        }
    }

    pthread_mutex_unlock(&g_transfer_queue.lock);
}

// --------------------------------------------------------------------------
// 3.5 Device-Aware Neural Gate Execution
// --------------------------------------------------------------------------

typedef struct DeviceKernel {
    int64_t id;
    char* name;
    DeviceTarget device;
    void* kernel_ptr;            // Function pointer or kernel handle
    int64_t input_count;
    int64_t output_count;
} DeviceKernel;

// Create a device kernel (stub for future GPU/NPU implementation)
int64_t kernel_create(int64_t name_ptr, int64_t device_type) {
    DeviceKernel* k = (DeviceKernel*)calloc(1, sizeof(DeviceKernel));
    if (!k) return 0;

    static int64_t next_id = 1;
    k->id = next_id++;
    k->name = name_ptr ? strdup((char*)name_ptr) : strdup("unnamed");
    k->device = (DeviceTarget)device_type;
    k->kernel_ptr = NULL;
    k->input_count = 0;
    k->output_count = 0;

    return (int64_t)k;
}

// Set kernel function pointer
void kernel_set_function(int64_t kernel_ptr, int64_t func_ptr) {
    DeviceKernel* k = (DeviceKernel*)kernel_ptr;
    if (k) {
        k->kernel_ptr = (void*)func_ptr;
    }
}

// Execute kernel on device
double kernel_execute(int64_t kernel_ptr, int64_t input_ptr, int64_t input_count,
                      int64_t output_ptr, int64_t output_count) {
    DeviceKernel* k = (DeviceKernel*)kernel_ptr;
    if (!k || !k->kernel_ptr) return 0.0;

    // For CPU, directly call the function
    if (k->device == DEVICE_CPU) {
        typedef double (*KernelFunc)(double*, int64_t, double*, int64_t);
        KernelFunc func = (KernelFunc)k->kernel_ptr;
        return func((double*)input_ptr, input_count, (double*)output_ptr, output_count);
    }

    // For GPU/NPU, would launch kernel (not implemented yet)
    // Return 0.0 as placeholder
    return 0.0;
}

// Free kernel
void kernel_free(int64_t kernel_ptr) {
    DeviceKernel* k = (DeviceKernel*)kernel_ptr;
    if (k) {
        free(k->name);
        free(k);
    }
}

// Execute neural gate on specific device
double neural_gate_execute_on_device(int64_t gate_ptr, double input, int64_t device_type) {
    NeuralGate* gate = (NeuralGate*)gate_ptr;
    if (!gate) return 0.0;

    // Check if we need to transfer data
    DeviceTarget current_device = DEVICE_CPU;  // Data always starts on CPU
    DeviceTarget target_device = (DeviceTarget)device_type;

    // For CPU execution, just run directly
    if (target_device == DEVICE_CPU || target_device == DEVICE_AUTO) {
        if (g_neural_training_mode) {
            return neural_sigmoid(input - gate->threshold);
        } else {
            return input > gate->threshold ? 1.0 : 0.0;
        }
    }

    // For GPU/NPU, would marshall data and execute on device
    // For now, fall back to CPU execution
    if (g_neural_training_mode) {
        return neural_sigmoid(input - gate->threshold);
    } else {
        return input > gate->threshold ? 1.0 : 0.0;
    }
}

// Batch execute gates (for GPU efficiency)
void neural_gate_batch_execute(int64_t* gate_ptrs, double* inputs, double* outputs,
                               int64_t count, int64_t device_type) {
    // For GPU, batch execution is much more efficient
    // For CPU, just loop
    for (int64_t i = 0; i < count; i++) {
        outputs[i] = neural_gate_execute_on_device(gate_ptrs[i], inputs[i], device_type);
    }
}

// --------------------------------------------------------------------------
// End of Phase 3: Hardware-Aware Compilation
// --------------------------------------------------------------------------

// ==========================================================================
// Phase 4: Structural Pruning
// ==========================================================================
// This phase implements pruning strategies to remove unused or low-importance
// neural gates after training, reducing binary size and improving inference speed.

// --------------------------------------------------------------------------
// 4.1 Activation Statistics Collection
// --------------------------------------------------------------------------

typedef struct GateActivationStats {
    int64_t gate_id;
    int64_t total_activations;      // Total times gate was evaluated
    int64_t positive_activations;   // Times gate fired (output > 0.5)
    double activation_rate;         // positive / total
    double mean_output;             // Average output value
    double variance_output;         // Variance of output
    double max_output;              // Maximum output seen
    double min_output;              // Minimum output seen
    double sum_output;              // For computing mean
    double sum_sq_output;           // For computing variance
    int64_t last_activation_epoch;  // Last epoch this gate was active
    int64_t consecutive_zero_epochs;// Epochs with zero activations
} GateActivationStats;

typedef struct ActivationTracker {
    GateActivationStats* stats;
    int64_t count;
    int64_t capacity;
    int64_t current_epoch;
    int64_t tracking_enabled;
    pthread_mutex_t lock;
} ActivationTracker;

static ActivationTracker g_activation_tracker = {0};

// Initialize activation tracker
void activation_tracker_init(void) {
    pthread_mutex_lock(&g_activation_tracker.lock);
    if (g_activation_tracker.stats == NULL) {
        g_activation_tracker.capacity = 1024;
        g_activation_tracker.stats = (GateActivationStats*)calloc(
            g_activation_tracker.capacity, sizeof(GateActivationStats));
        g_activation_tracker.count = 0;
        g_activation_tracker.current_epoch = 0;
        g_activation_tracker.tracking_enabled = 1;
    }
    pthread_mutex_unlock(&g_activation_tracker.lock);
}

// Enable/disable activation tracking
void activation_tracking_set_enabled(int64_t enabled) {
    g_activation_tracker.tracking_enabled = enabled;
}

// Get tracking enabled status
int64_t activation_tracking_enabled(void) {
    return g_activation_tracker.tracking_enabled;
}

// Find or create stats for a gate
static GateActivationStats* get_or_create_stats(int64_t gate_id) {
    for (int64_t i = 0; i < g_activation_tracker.count; i++) {
        if (g_activation_tracker.stats[i].gate_id == gate_id) {
            return &g_activation_tracker.stats[i];
        }
    }

    // Create new entry
    if (g_activation_tracker.count >= g_activation_tracker.capacity) {
        g_activation_tracker.capacity *= 2;
        g_activation_tracker.stats = (GateActivationStats*)realloc(
            g_activation_tracker.stats,
            g_activation_tracker.capacity * sizeof(GateActivationStats));
    }

    GateActivationStats* stats = &g_activation_tracker.stats[g_activation_tracker.count++];
    memset(stats, 0, sizeof(GateActivationStats));
    stats->gate_id = gate_id;
    stats->min_output = 1.0;  // Will be updated on first activation
    return stats;
}

// Record a gate activation
void activation_record(int64_t gate_id, double output) {
    if (!g_activation_tracker.tracking_enabled) return;

    pthread_mutex_lock(&g_activation_tracker.lock);

    GateActivationStats* stats = get_or_create_stats(gate_id);
    stats->total_activations++;

    if (output > 0.5) {
        stats->positive_activations++;
    }

    stats->sum_output += output;
    stats->sum_sq_output += output * output;

    if (output > stats->max_output) stats->max_output = output;
    if (output < stats->min_output) stats->min_output = output;

    stats->last_activation_epoch = g_activation_tracker.current_epoch;

    // Update activation rate
    if (stats->total_activations > 0) {
        stats->activation_rate = (double)stats->positive_activations / stats->total_activations;
        stats->mean_output = stats->sum_output / stats->total_activations;

        // Variance = E[X^2] - E[X]^2
        double mean_sq = stats->sum_sq_output / stats->total_activations;
        stats->variance_output = mean_sq - (stats->mean_output * stats->mean_output);
    }

    pthread_mutex_unlock(&g_activation_tracker.lock);
}

// Get activation stats for a gate
int64_t activation_stats_get(int64_t gate_id) {
    pthread_mutex_lock(&g_activation_tracker.lock);

    for (int64_t i = 0; i < g_activation_tracker.count; i++) {
        if (g_activation_tracker.stats[i].gate_id == gate_id) {
            pthread_mutex_unlock(&g_activation_tracker.lock);
            return (int64_t)&g_activation_tracker.stats[i];
        }
    }

    pthread_mutex_unlock(&g_activation_tracker.lock);
    return 0;
}

// Get activation rate for a gate
double activation_rate_get(int64_t gate_id) {
    int64_t stats_ptr = activation_stats_get(gate_id);
    if (!stats_ptr) return 0.0;
    GateActivationStats* stats = (GateActivationStats*)stats_ptr;
    return stats->activation_rate;
}

// Get total activations for a gate
int64_t activation_count_get(int64_t gate_id) {
    int64_t stats_ptr = activation_stats_get(gate_id);
    if (!stats_ptr) return 0;
    GateActivationStats* stats = (GateActivationStats*)stats_ptr;
    return stats->total_activations;
}

// Get mean output for a gate
double activation_mean_get(int64_t gate_id) {
    int64_t stats_ptr = activation_stats_get(gate_id);
    if (!stats_ptr) return 0.0;
    GateActivationStats* stats = (GateActivationStats*)stats_ptr;
    return stats->mean_output;
}

// Advance to next epoch
void activation_epoch_advance(void) {
    pthread_mutex_lock(&g_activation_tracker.lock);

    g_activation_tracker.current_epoch++;

    // Update consecutive zero epochs for gates not activated this epoch
    for (int64_t i = 0; i < g_activation_tracker.count; i++) {
        GateActivationStats* stats = &g_activation_tracker.stats[i];
        if (stats->last_activation_epoch < g_activation_tracker.current_epoch - 1) {
            stats->consecutive_zero_epochs++;
        } else {
            stats->consecutive_zero_epochs = 0;
        }
    }

    pthread_mutex_unlock(&g_activation_tracker.lock);
}

// Get current epoch
int64_t activation_epoch_current(void) {
    return g_activation_tracker.current_epoch;
}

// Get number of tracked gates
int64_t activation_gate_count(void) {
    return g_activation_tracker.count;
}

// Reset all activation statistics
void activation_stats_reset(void) {
    pthread_mutex_lock(&g_activation_tracker.lock);

    for (int64_t i = 0; i < g_activation_tracker.count; i++) {
        GateActivationStats* stats = &g_activation_tracker.stats[i];
        stats->total_activations = 0;
        stats->positive_activations = 0;
        stats->activation_rate = 0;
        stats->mean_output = 0;
        stats->variance_output = 0;
        stats->max_output = 0;
        stats->min_output = 1.0;
        stats->sum_output = 0;
        stats->sum_sq_output = 0;
    }

    pthread_mutex_unlock(&g_activation_tracker.lock);
}

// --------------------------------------------------------------------------
// 4.2 Weight Magnitude Pruning
// --------------------------------------------------------------------------

typedef enum PruningReason {
    PRUNE_NONE = 0,
    PRUNE_WEIGHT_MAGNITUDE = 1,    // |weight| < threshold
    PRUNE_LOW_ACTIVATION = 2,       // activation rate < threshold
    PRUNE_ZERO_GRADIENT = 3,        // gradient consistently zero
    PRUNE_DEAD_PATH = 4,            // unreachable after other pruning
    PRUNE_MANUAL = 5                // user explicitly pruned
} PruningReason;

typedef struct PruningCandidate {
    int64_t gate_id;
    PruningReason reason;
    double score;                   // Lower = more likely to prune
    int64_t should_prune;           // Final decision
    int64_t pruned;                 // Already pruned?
} PruningCandidate;

typedef struct PruningContext {
    PruningCandidate* candidates;
    int64_t count;
    int64_t capacity;
    double weight_threshold;        // |weight| < this = prune
    double activation_threshold;    // activation rate < this = prune
    int64_t zero_gradient_epochs;   // epochs with zero gradient = prune
    int64_t pruned_count;           // Number pruned so far
    int64_t total_gates;            // Total gates before pruning
} PruningContext;

// Create a new pruning context
int64_t pruning_context_new(double weight_threshold, double activation_threshold,
                            int64_t zero_gradient_epochs) {
    PruningContext* ctx = (PruningContext*)calloc(1, sizeof(PruningContext));
    if (!ctx) return 0;

    ctx->capacity = 256;
    ctx->candidates = (PruningCandidate*)calloc(ctx->capacity, sizeof(PruningCandidate));
    ctx->weight_threshold = weight_threshold > 0 ? weight_threshold : 0.01;
    ctx->activation_threshold = activation_threshold > 0 ? activation_threshold : 0.001;
    ctx->zero_gradient_epochs = zero_gradient_epochs > 0 ? zero_gradient_epochs : 5;

    return (int64_t)ctx;
}

// Add a gate to pruning analysis
void pruning_add_gate(int64_t ctx_ptr, int64_t gate_id, double weight) {
    PruningContext* ctx = (PruningContext*)ctx_ptr;
    if (!ctx) return;

    if (ctx->count >= ctx->capacity) {
        ctx->capacity *= 2;
        ctx->candidates = (PruningCandidate*)realloc(
            ctx->candidates, ctx->capacity * sizeof(PruningCandidate));
    }

    PruningCandidate* cand = &ctx->candidates[ctx->count++];
    cand->gate_id = gate_id;
    cand->reason = PRUNE_NONE;
    cand->score = 1.0;  // Default: don't prune
    cand->should_prune = 0;
    cand->pruned = 0;

    ctx->total_gates++;

    // Check weight magnitude
    double abs_weight = weight < 0 ? -weight : weight;
    if (abs_weight < ctx->weight_threshold) {
        cand->reason = PRUNE_WEIGHT_MAGNITUDE;
        cand->score = abs_weight / ctx->weight_threshold;  // 0-1, lower = prune
        cand->should_prune = 1;
    }
}

// Analyze activation statistics for pruning
void pruning_analyze_activations(int64_t ctx_ptr) {
    PruningContext* ctx = (PruningContext*)ctx_ptr;
    if (!ctx) return;

    for (int64_t i = 0; i < ctx->count; i++) {
        PruningCandidate* cand = &ctx->candidates[i];
        if (cand->should_prune) continue;  // Already marked

        double rate = activation_rate_get(cand->gate_id);
        if (rate < ctx->activation_threshold) {
            cand->reason = PRUNE_LOW_ACTIVATION;
            cand->score = rate / ctx->activation_threshold;
            cand->should_prune = 1;
        }
    }
}

// Analyze gradient statistics for pruning
void pruning_analyze_gradients(int64_t ctx_ptr, int64_t* gate_ids, double* gradients, int64_t count) {
    PruningContext* ctx = (PruningContext*)ctx_ptr;
    if (!ctx) return;

    for (int64_t i = 0; i < count; i++) {
        int64_t gate_id = gate_ids[i];
        double grad = gradients[i];
        double abs_grad = grad < 0 ? -grad : grad;

        // Find candidate
        for (int64_t j = 0; j < ctx->count; j++) {
            if (ctx->candidates[j].gate_id == gate_id) {
                PruningCandidate* cand = &ctx->candidates[j];
                if (!cand->should_prune && abs_grad < 1e-10) {
                    // Check if this gate has had zero gradient for many epochs
                    int64_t stats_ptr = activation_stats_get(gate_id);
                    if (stats_ptr) {
                        GateActivationStats* stats = (GateActivationStats*)stats_ptr;
                        if (stats->consecutive_zero_epochs >= ctx->zero_gradient_epochs) {
                            cand->reason = PRUNE_ZERO_GRADIENT;
                            cand->score = 0.0;
                            cand->should_prune = 1;
                        }
                    }
                }
                break;
            }
        }
    }
}

// Execute pruning - mark gates as pruned
int64_t pruning_execute(int64_t ctx_ptr) {
    PruningContext* ctx = (PruningContext*)ctx_ptr;
    if (!ctx) return 0;

    int64_t pruned = 0;
    for (int64_t i = 0; i < ctx->count; i++) {
        if (ctx->candidates[i].should_prune && !ctx->candidates[i].pruned) {
            ctx->candidates[i].pruned = 1;
            pruned++;
        }
    }

    ctx->pruned_count = pruned;
    return pruned;
}

// Get number of pruned gates
int64_t pruning_pruned_count(int64_t ctx_ptr) {
    PruningContext* ctx = (PruningContext*)ctx_ptr;
    if (!ctx) return 0;
    return ctx->pruned_count;
}

// Get total gates analyzed
int64_t pruning_total_count(int64_t ctx_ptr) {
    PruningContext* ctx = (PruningContext*)ctx_ptr;
    if (!ctx) return 0;
    return ctx->total_gates;
}

// Get pruning ratio (pruned / total)
double pruning_ratio(int64_t ctx_ptr) {
    PruningContext* ctx = (PruningContext*)ctx_ptr;
    if (!ctx || ctx->total_gates == 0) return 0.0;
    return (double)ctx->pruned_count / ctx->total_gates;
}

// Check if a gate is pruned
int64_t pruning_is_pruned(int64_t ctx_ptr, int64_t gate_id) {
    PruningContext* ctx = (PruningContext*)ctx_ptr;
    if (!ctx) return 0;

    for (int64_t i = 0; i < ctx->count; i++) {
        if (ctx->candidates[i].gate_id == gate_id) {
            return ctx->candidates[i].pruned;
        }
    }
    return 0;
}

// Get pruning reason for a gate
int64_t pruning_reason(int64_t ctx_ptr, int64_t gate_id) {
    PruningContext* ctx = (PruningContext*)ctx_ptr;
    if (!ctx) return PRUNE_NONE;

    for (int64_t i = 0; i < ctx->count; i++) {
        if (ctx->candidates[i].gate_id == gate_id) {
            return ctx->candidates[i].reason;
        }
    }
    return PRUNE_NONE;
}

// Free pruning context
void pruning_context_free(int64_t ctx_ptr) {
    PruningContext* ctx = (PruningContext*)ctx_ptr;
    if (!ctx) return;
    free(ctx->candidates);
    free(ctx);
}

// --------------------------------------------------------------------------
// 4.2.1 Simple Weight Magnitude Pruning API (Phase 4 completion)
// --------------------------------------------------------------------------
// A simple, user-friendly API for weight magnitude pruning that wraps
// the more complex PruningContext infrastructure.

// Global gate weight registry
typedef struct GateWeightEntry {
    int64_t gate_id;
    double weight;
    int64_t pruned;  // 0 = active, 1 = pruned
    char* name;
} GateWeightEntry;

static GateWeightEntry* g_gate_weights = NULL;
static int64_t g_gate_weight_count = 0;
static int64_t g_gate_weight_capacity = 0;

// Register a gate's weight for later pruning
void neural_register_gate_weight(int64_t gate_id, double weight, int64_t name_ptr) {
    // Ensure capacity
    if (g_gate_weight_count >= g_gate_weight_capacity) {
        int64_t new_cap = g_gate_weight_capacity == 0 ? 64 : g_gate_weight_capacity * 2;
        g_gate_weights = (GateWeightEntry*)realloc(g_gate_weights, new_cap * sizeof(GateWeightEntry));
        g_gate_weight_capacity = new_cap;
    }

    // Check if gate already registered, update if so
    for (int64_t i = 0; i < g_gate_weight_count; i++) {
        if (g_gate_weights[i].gate_id == gate_id) {
            g_gate_weights[i].weight = weight;
            return;
        }
    }

    // Add new entry
    g_gate_weights[g_gate_weight_count].gate_id = gate_id;
    g_gate_weights[g_gate_weight_count].weight = weight;
    g_gate_weights[g_gate_weight_count].pruned = 0;
    g_gate_weights[g_gate_weight_count].name = name_ptr ? (char*)name_ptr : NULL;
    g_gate_weight_count++;
}

// Update a gate's weight (e.g., after training)
void neural_update_gate_weight(int64_t gate_id, double new_weight) {
    for (int64_t i = 0; i < g_gate_weight_count; i++) {
        if (g_gate_weights[i].gate_id == gate_id) {
            g_gate_weights[i].weight = new_weight;
            return;
        }
    }
}

// Get a gate's current weight
double neural_get_gate_weight(int64_t gate_id) {
    for (int64_t i = 0; i < g_gate_weight_count; i++) {
        if (g_gate_weights[i].gate_id == gate_id) {
            return g_gate_weights[i].weight;
        }
    }
    return 1.0;  // Default weight
}

// Simple weight magnitude pruning: prune gates where |weight| < threshold
// Returns the number of gates pruned
int64_t neural_prune_by_weight_magnitude(double threshold) {
    int64_t pruned_count = 0;

    for (int64_t i = 0; i < g_gate_weight_count; i++) {
        if (g_gate_weights[i].pruned) continue;  // Already pruned

        double abs_weight = g_gate_weights[i].weight;
        if (abs_weight < 0) abs_weight = -abs_weight;

        if (abs_weight < threshold) {
            g_gate_weights[i].pruned = 1;
            pruned_count++;
        }
    }

    return pruned_count;
}

// Check if a gate has been pruned
int64_t neural_is_gate_pruned(int64_t gate_id) {
    for (int64_t i = 0; i < g_gate_weight_count; i++) {
        if (g_gate_weights[i].gate_id == gate_id) {
            return g_gate_weights[i].pruned;
        }
    }
    return 0;  // Not found = not pruned
}

// Get total number of registered gates
int64_t neural_get_gate_count(void) {
    return g_gate_weight_count;
}

// Get number of pruned gates
int64_t neural_get_pruned_gate_count(void) {
    int64_t count = 0;
    for (int64_t i = 0; i < g_gate_weight_count; i++) {
        if (g_gate_weights[i].pruned) count++;
    }
    return count;
}

// Get pruning ratio (pruned / total)
double neural_get_pruning_ratio(void) {
    if (g_gate_weight_count == 0) return 0.0;
    return (double)neural_get_pruned_gate_count() / (double)g_gate_weight_count;
}

// Reset all pruning flags (unpruned all gates)
void neural_reset_pruning(void) {
    for (int64_t i = 0; i < g_gate_weight_count; i++) {
        g_gate_weights[i].pruned = 0;
    }
}

// Clear all registered gates
void neural_clear_gate_registry(void) {
    free(g_gate_weights);
    g_gate_weights = NULL;
    g_gate_weight_count = 0;
    g_gate_weight_capacity = 0;
}

// --------------------------------------------------------------------------
// 4.3 Dead Path Elimination
// --------------------------------------------------------------------------

typedef struct DeadPathAnalyzer {
    int64_t* reachable_gates;       // Gates that are reachable
    int64_t reachable_count;
    int64_t* dead_gates;            // Gates that are dead
    int64_t dead_count;
    int64_t capacity;
    int64_t* adjacency;             // Adjacency list (flattened)
    int64_t* adj_offsets;           // Start offset for each gate
    int64_t adj_count;
    int64_t gate_count;
} DeadPathAnalyzer;

// Create dead path analyzer
int64_t dead_path_analyzer_new(int64_t gate_count) {
    DeadPathAnalyzer* dpa = (DeadPathAnalyzer*)calloc(1, sizeof(DeadPathAnalyzer));
    if (!dpa) return 0;

    dpa->capacity = gate_count > 0 ? gate_count : 256;
    dpa->reachable_gates = (int64_t*)calloc(dpa->capacity, sizeof(int64_t));
    dpa->dead_gates = (int64_t*)calloc(dpa->capacity, sizeof(int64_t));
    dpa->adj_offsets = (int64_t*)calloc(dpa->capacity + 1, sizeof(int64_t));
    dpa->adjacency = (int64_t*)calloc(dpa->capacity * 4, sizeof(int64_t));  // Assume avg 4 edges
    dpa->gate_count = gate_count;

    return (int64_t)dpa;
}

// Add edge to analyzer
void dead_path_add_edge(int64_t dpa_ptr, int64_t from_gate, int64_t to_gate) {
    DeadPathAnalyzer* dpa = (DeadPathAnalyzer*)dpa_ptr;
    if (!dpa) return;

    // Simple adjacency list append (would need proper building in practice)
    if (dpa->adj_count < dpa->capacity * 4) {
        dpa->adjacency[dpa->adj_count++] = (from_gate << 32) | to_gate;
    }
}

// Mark gate as entry point (always reachable)
void dead_path_mark_entry(int64_t dpa_ptr, int64_t gate_id) {
    DeadPathAnalyzer* dpa = (DeadPathAnalyzer*)dpa_ptr;
    if (!dpa || dpa->reachable_count >= dpa->capacity) return;

    // Check not already marked
    for (int64_t i = 0; i < dpa->reachable_count; i++) {
        if (dpa->reachable_gates[i] == gate_id) return;
    }

    dpa->reachable_gates[dpa->reachable_count++] = gate_id;
}

// Propagate reachability
void dead_path_propagate(int64_t dpa_ptr) {
    DeadPathAnalyzer* dpa = (DeadPathAnalyzer*)dpa_ptr;
    if (!dpa) return;

    // BFS from entry points
    int64_t queue_start = 0;

    while (queue_start < dpa->reachable_count) {
        int64_t current = dpa->reachable_gates[queue_start++];

        // Find all edges from current
        for (int64_t i = 0; i < dpa->adj_count; i++) {
            int64_t from = dpa->adjacency[i] >> 32;
            int64_t to = dpa->adjacency[i] & 0xFFFFFFFF;

            if (from == current) {
                // Check if 'to' is already reachable
                int64_t found = 0;
                for (int64_t j = 0; j < dpa->reachable_count; j++) {
                    if (dpa->reachable_gates[j] == to) {
                        found = 1;
                        break;
                    }
                }

                if (!found && dpa->reachable_count < dpa->capacity) {
                    dpa->reachable_gates[dpa->reachable_count++] = to;
                }
            }
        }
    }
}

// Find dead paths (gates not reachable)
void dead_path_find_dead(int64_t dpa_ptr, int64_t* all_gates, int64_t all_count) {
    DeadPathAnalyzer* dpa = (DeadPathAnalyzer*)dpa_ptr;
    if (!dpa) return;

    dpa->dead_count = 0;

    for (int64_t i = 0; i < all_count; i++) {
        int64_t gate = all_gates[i];
        int64_t is_reachable = 0;

        for (int64_t j = 0; j < dpa->reachable_count; j++) {
            if (dpa->reachable_gates[j] == gate) {
                is_reachable = 1;
                break;
            }
        }

        if (!is_reachable && dpa->dead_count < dpa->capacity) {
            dpa->dead_gates[dpa->dead_count++] = gate;
        }
    }
}

// Get number of dead gates
int64_t dead_path_dead_count(int64_t dpa_ptr) {
    DeadPathAnalyzer* dpa = (DeadPathAnalyzer*)dpa_ptr;
    if (!dpa) return 0;
    return dpa->dead_count;
}

// Get number of reachable gates
int64_t dead_path_reachable_count(int64_t dpa_ptr) {
    DeadPathAnalyzer* dpa = (DeadPathAnalyzer*)dpa_ptr;
    if (!dpa) return 0;
    return dpa->reachable_count;
}

// Check if gate is dead
int64_t dead_path_is_dead(int64_t dpa_ptr, int64_t gate_id) {
    DeadPathAnalyzer* dpa = (DeadPathAnalyzer*)dpa_ptr;
    if (!dpa) return 0;

    for (int64_t i = 0; i < dpa->dead_count; i++) {
        if (dpa->dead_gates[i] == gate_id) return 1;
    }
    return 0;
}

// Free analyzer
void dead_path_analyzer_free(int64_t dpa_ptr) {
    DeadPathAnalyzer* dpa = (DeadPathAnalyzer*)dpa_ptr;
    if (!dpa) return;

    free(dpa->reachable_gates);
    free(dpa->dead_gates);
    free(dpa->adjacency);
    free(dpa->adj_offsets);
    free(dpa);
}

// --------------------------------------------------------------------------
// 4.4 Structured Pruning (Subgraph Removal)
// --------------------------------------------------------------------------

typedef struct PrunedSubgraph {
    int64_t* gate_ids;
    int64_t count;
    int64_t capacity;
    double total_weight;            // Sum of weights in subgraph
    double importance_score;        // Higher = more important
} PrunedSubgraph;

typedef struct StructuredPruner {
    PrunedSubgraph* subgraphs;
    int64_t subgraph_count;
    int64_t capacity;
    double importance_threshold;    // Subgraphs below this get pruned
    int64_t* pruned_subgraph_ids;
    int64_t pruned_count;
} StructuredPruner;

// Create structured pruner
int64_t structured_pruner_new(double importance_threshold) {
    StructuredPruner* sp = (StructuredPruner*)calloc(1, sizeof(StructuredPruner));
    if (!sp) return 0;

    sp->capacity = 64;
    sp->subgraphs = (PrunedSubgraph*)calloc(sp->capacity, sizeof(PrunedSubgraph));
    sp->pruned_subgraph_ids = (int64_t*)calloc(sp->capacity, sizeof(int64_t));
    sp->importance_threshold = importance_threshold > 0 ? importance_threshold : 0.01;

    return (int64_t)sp;
}

// Add a subgraph to analyze
int64_t structured_pruner_add_subgraph(int64_t sp_ptr, int64_t* gate_ids, int64_t count,
                                       double total_weight) {
    StructuredPruner* sp = (StructuredPruner*)sp_ptr;
    if (!sp) return -1;

    if (sp->subgraph_count >= sp->capacity) {
        sp->capacity *= 2;
        sp->subgraphs = (PrunedSubgraph*)realloc(
            sp->subgraphs, sp->capacity * sizeof(PrunedSubgraph));
        sp->pruned_subgraph_ids = (int64_t*)realloc(
            sp->pruned_subgraph_ids, sp->capacity * sizeof(int64_t));
    }

    int64_t id = sp->subgraph_count;
    PrunedSubgraph* sg = &sp->subgraphs[sp->subgraph_count++];

    sg->capacity = count;
    sg->gate_ids = (int64_t*)malloc(count * sizeof(int64_t));
    memcpy(sg->gate_ids, gate_ids, count * sizeof(int64_t));
    sg->count = count;
    sg->total_weight = total_weight;

    // Calculate importance based on weight and activation
    double activation_sum = 0;
    for (int64_t i = 0; i < count; i++) {
        activation_sum += activation_rate_get(gate_ids[i]);
    }
    double avg_activation = count > 0 ? activation_sum / count : 0;

    // Importance = weight * activation
    sg->importance_score = (total_weight < 0 ? -total_weight : total_weight) * avg_activation;

    return id;
}

// Execute structured pruning
int64_t structured_pruner_execute(int64_t sp_ptr) {
    StructuredPruner* sp = (StructuredPruner*)sp_ptr;
    if (!sp) return 0;

    sp->pruned_count = 0;

    for (int64_t i = 0; i < sp->subgraph_count; i++) {
        if (sp->subgraphs[i].importance_score < sp->importance_threshold) {
            sp->pruned_subgraph_ids[sp->pruned_count++] = i;
        }
    }

    return sp->pruned_count;
}

// Get number of pruned subgraphs
int64_t structured_pruner_pruned_count(int64_t sp_ptr) {
    StructuredPruner* sp = (StructuredPruner*)sp_ptr;
    if (!sp) return 0;
    return sp->pruned_count;
}

// Get total subgraphs
int64_t structured_pruner_total_count(int64_t sp_ptr) {
    StructuredPruner* sp = (StructuredPruner*)sp_ptr;
    if (!sp) return 0;
    return sp->subgraph_count;
}

// Check if subgraph is pruned
int64_t structured_pruner_is_pruned(int64_t sp_ptr, int64_t subgraph_id) {
    StructuredPruner* sp = (StructuredPruner*)sp_ptr;
    if (!sp) return 0;

    for (int64_t i = 0; i < sp->pruned_count; i++) {
        if (sp->pruned_subgraph_ids[i] == subgraph_id) return 1;
    }
    return 0;
}

// Get importance score for subgraph
double structured_pruner_importance(int64_t sp_ptr, int64_t subgraph_id) {
    StructuredPruner* sp = (StructuredPruner*)sp_ptr;
    if (!sp || subgraph_id < 0 || subgraph_id >= sp->subgraph_count) return 0.0;
    return sp->subgraphs[subgraph_id].importance_score;
}

// Free structured pruner
void structured_pruner_free(int64_t sp_ptr) {
    StructuredPruner* sp = (StructuredPruner*)sp_ptr;
    if (!sp) return;

    for (int64_t i = 0; i < sp->subgraph_count; i++) {
        free(sp->subgraphs[i].gate_ids);
    }
    free(sp->subgraphs);
    free(sp->pruned_subgraph_ids);
    free(sp);
}

// --------------------------------------------------------------------------
// 4.5 Binary Size Optimization
// --------------------------------------------------------------------------

typedef struct OptimizationStats {
    int64_t original_gates;
    int64_t pruned_gates;
    int64_t original_edges;
    int64_t pruned_edges;
    int64_t original_subgraphs;
    int64_t pruned_subgraphs;
    double estimated_size_reduction;   // 0.0 to 1.0
    double estimated_speedup;          // 1.0 = no change, 2.0 = 2x faster
} OptimizationStats;

// Calculate optimization statistics
int64_t optimization_stats_calculate(int64_t pruning_ctx, int64_t dead_path_ctx,
                                     int64_t structured_ctx) {
    OptimizationStats* stats = (OptimizationStats*)calloc(1, sizeof(OptimizationStats));
    if (!stats) return 0;

    // Get pruning stats
    if (pruning_ctx) {
        PruningContext* pc = (PruningContext*)pruning_ctx;
        stats->original_gates = pc->total_gates;
        stats->pruned_gates = pc->pruned_count;
    }

    // Get dead path stats
    if (dead_path_ctx) {
        DeadPathAnalyzer* dpa = (DeadPathAnalyzer*)dead_path_ctx;
        stats->pruned_gates += dpa->dead_count;
    }

    // Get structured pruning stats
    if (structured_ctx) {
        StructuredPruner* sp = (StructuredPruner*)structured_ctx;
        stats->original_subgraphs = sp->subgraph_count;
        stats->pruned_subgraphs = sp->pruned_count;

        // Count gates in pruned subgraphs
        for (int64_t i = 0; i < sp->pruned_count; i++) {
            int64_t sg_id = sp->pruned_subgraph_ids[i];
            stats->pruned_gates += sp->subgraphs[sg_id].count;
        }
    }

    // Estimate size reduction
    if (stats->original_gates > 0) {
        stats->estimated_size_reduction =
            (double)stats->pruned_gates / stats->original_gates;
    }

    // Estimate speedup (fewer gates = faster, but diminishing returns)
    double remaining = 1.0 - stats->estimated_size_reduction;
    if (remaining > 0) {
        stats->estimated_speedup = 1.0 / remaining;
        // Cap at realistic speedup
        if (stats->estimated_speedup > 10.0) stats->estimated_speedup = 10.0;
    } else {
        stats->estimated_speedup = 1.0;
    }

    return (int64_t)stats;
}

// Get estimated size reduction
double optimization_size_reduction(int64_t stats_ptr) {
    OptimizationStats* stats = (OptimizationStats*)stats_ptr;
    if (!stats) return 0.0;
    return stats->estimated_size_reduction;
}

// Get estimated speedup
double optimization_speedup(int64_t stats_ptr) {
    OptimizationStats* stats = (OptimizationStats*)stats_ptr;
    if (!stats) return 1.0;
    return stats->estimated_speedup;
}

// Get pruned gate count
int64_t optimization_pruned_gates(int64_t stats_ptr) {
    OptimizationStats* stats = (OptimizationStats*)stats_ptr;
    if (!stats) return 0;
    return stats->pruned_gates;
}

// Get original gate count
int64_t optimization_original_gates(int64_t stats_ptr) {
    OptimizationStats* stats = (OptimizationStats*)stats_ptr;
    if (!stats) return 0;
    return stats->original_gates;
}

// Free optimization stats
void optimization_stats_free(int64_t stats_ptr) {
    OptimizationStats* stats = (OptimizationStats*)stats_ptr;
    free(stats);
}

// --------------------------------------------------------------------------
// End of Phase 4: Structural Pruning
// --------------------------------------------------------------------------

// ==========================================================================
// Phase 5: Superposition Memory Model
// ==========================================================================
// This phase implements weighted references and execution modes for handling
// probabilistic branching in neural programs without memory leaks.

// --------------------------------------------------------------------------
// 5.1 WeightedRef Type Implementation
// --------------------------------------------------------------------------

typedef enum ExecutionMode {
    EXEC_MODE_LAZY = 0,         // Allocate only dominant path
    EXEC_MODE_SPECULATIVE = 1,  // Allocate all, weight, GC unused
    EXEC_MODE_CHECKPOINT = 2,   // Snapshot/restore for exact gradients
    EXEC_MODE_POOLED = 3        // Pre-allocate max, reuse
} ExecutionMode;

typedef enum WeightedRefState {
    WREF_UNALLOCATED = 0,       // Not yet allocated
    WREF_ALLOCATED = 1,         // Currently allocated
    WREF_COLLAPSED = 2,         // Collapsed to definite value
    WREF_FREED = 3              // Already freed
} WeightedRefState;

typedef struct WeightedRef {
    int64_t id;
    void* ptr;                  // Actual pointer
    double weight;              // Probability weight (0.0 to 1.0)
    int64_t size_bytes;         // Size of allocation
    WeightedRefState state;
    int64_t ref_count;          // Reference count
    int64_t branch_id;          // Which branch this belongs to
    int64_t parent_id;          // Parent weighted ref (for hierarchy)
    struct WeightedRef* next;   // For linked list in registry
} WeightedRef;

typedef struct WeightedRefRegistry {
    WeightedRef* head;
    int64_t count;
    int64_t next_id;
    double gc_threshold;        // Weight below which to GC
    int64_t total_allocated;    // Total bytes allocated
    int64_t budget_bytes;       // Memory budget (0 = unlimited)
    ExecutionMode mode;
    pthread_mutex_t lock;
} WeightedRefRegistry;

static WeightedRefRegistry g_wref_registry = {0};

// Initialize weighted ref registry
void wref_registry_init(void) {
    pthread_mutex_lock(&g_wref_registry.lock);
    if (g_wref_registry.next_id == 0) {
        g_wref_registry.head = NULL;
        g_wref_registry.count = 0;
        g_wref_registry.next_id = 1;
        g_wref_registry.gc_threshold = 0.01;  // GC when weight < 1%
        g_wref_registry.total_allocated = 0;
        g_wref_registry.budget_bytes = 0;  // Unlimited
        g_wref_registry.mode = EXEC_MODE_LAZY;
    }
    pthread_mutex_unlock(&g_wref_registry.lock);
}

// Set execution mode
void wref_set_mode(int64_t mode) {
    g_wref_registry.mode = (ExecutionMode)mode;
}

// Get execution mode
int64_t wref_get_mode(void) {
    return g_wref_registry.mode;
}

// Set GC threshold
void wref_set_gc_threshold(double threshold) {
    g_wref_registry.gc_threshold = threshold;
}

// Get GC threshold
double wref_get_gc_threshold(void) {
    return g_wref_registry.gc_threshold;
}

// Set memory budget
void wref_set_budget(int64_t budget_bytes) {
    g_wref_registry.budget_bytes = budget_bytes;
}

// Get memory budget
int64_t wref_get_budget(void) {
    return g_wref_registry.budget_bytes;
}

// Get total allocated
int64_t wref_total_allocated(void) {
    return g_wref_registry.total_allocated;
}

// Alias: wref_bytes_allocated (same as wref_total_allocated)
int64_t wref_bytes_allocated(void) {
    return g_wref_registry.total_allocated;
}

// Alias: wref_set_weight_threshold (same as wref_set_gc_threshold)
void wref_set_weight_threshold(double threshold) {
    g_wref_registry.gc_threshold = threshold;
}

// Alias: wref_get_weight_threshold (same as wref_get_gc_threshold)
double wref_get_weight_threshold(void) {
    return g_wref_registry.gc_threshold;
}

// Create a new weighted reference
int64_t wref_new(int64_t size_bytes, double weight, int64_t branch_id) {
    wref_registry_init();

    pthread_mutex_lock(&g_wref_registry.lock);

    // Check budget in speculative mode
    if (g_wref_registry.mode == EXEC_MODE_SPECULATIVE &&
        g_wref_registry.budget_bytes > 0 &&
        g_wref_registry.total_allocated + size_bytes > g_wref_registry.budget_bytes) {
        pthread_mutex_unlock(&g_wref_registry.lock);
        return 0;  // Over budget
    }

    // In lazy mode, only allocate if weight > threshold
    if (g_wref_registry.mode == EXEC_MODE_LAZY && weight < g_wref_registry.gc_threshold) {
        pthread_mutex_unlock(&g_wref_registry.lock);
        return 0;  // Weight too low for lazy mode
    }

    WeightedRef* wref = (WeightedRef*)calloc(1, sizeof(WeightedRef));
    if (!wref) {
        pthread_mutex_unlock(&g_wref_registry.lock);
        return 0;
    }

    wref->id = g_wref_registry.next_id++;
    wref->size_bytes = size_bytes;
    wref->weight = weight;
    wref->branch_id = branch_id;
    wref->parent_id = 0;
    wref->ref_count = 1;

    // Allocate memory based on mode
    if (g_wref_registry.mode == EXEC_MODE_LAZY) {
        // Lazy: allocate immediately if weight is high enough
        wref->ptr = calloc(1, size_bytes);
        wref->state = wref->ptr ? WREF_ALLOCATED : WREF_UNALLOCATED;
    } else if (g_wref_registry.mode == EXEC_MODE_SPECULATIVE) {
        // Speculative: always allocate
        wref->ptr = calloc(1, size_bytes);
        wref->state = wref->ptr ? WREF_ALLOCATED : WREF_UNALLOCATED;
    } else if (g_wref_registry.mode == EXEC_MODE_CHECKPOINT) {
        // Checkpoint: allocate for gradient tracking
        wref->ptr = calloc(1, size_bytes);
        wref->state = wref->ptr ? WREF_ALLOCATED : WREF_UNALLOCATED;
    } else if (g_wref_registry.mode == EXEC_MODE_POOLED) {
        // Pooled: allocate from pool (simplified: just allocate)
        wref->ptr = calloc(1, size_bytes);
        wref->state = wref->ptr ? WREF_ALLOCATED : WREF_UNALLOCATED;
    }

    if (wref->state == WREF_ALLOCATED) {
        g_wref_registry.total_allocated += size_bytes;
    }

    // Add to registry
    wref->next = g_wref_registry.head;
    g_wref_registry.head = wref;
    g_wref_registry.count++;

    pthread_mutex_unlock(&g_wref_registry.lock);

    return (int64_t)wref;
}

// Get pointer from weighted ref
int64_t wref_ptr(int64_t wref_id) {
    WeightedRef* wref = (WeightedRef*)wref_id;
    if (!wref || wref->state != WREF_ALLOCATED) return 0;
    return (int64_t)wref->ptr;
}

// Get weight of weighted ref
double wref_weight(int64_t wref_id) {
    WeightedRef* wref = (WeightedRef*)wref_id;
    if (!wref) return 0.0;
    return wref->weight;
}

// Update weight of weighted ref
void wref_set_weight(int64_t wref_id, double weight) {
    WeightedRef* wref = (WeightedRef*)wref_id;
    if (!wref) return;

    pthread_mutex_lock(&g_wref_registry.lock);
    wref->weight = weight;

    // In lazy mode, free if weight drops below threshold
    if (g_wref_registry.mode == EXEC_MODE_LAZY &&
        weight < g_wref_registry.gc_threshold &&
        wref->state == WREF_ALLOCATED) {
        free(wref->ptr);
        wref->ptr = NULL;
        g_wref_registry.total_allocated -= wref->size_bytes;
        wref->state = WREF_FREED;
    }

    pthread_mutex_unlock(&g_wref_registry.lock);
}

// Check if weighted ref is allocated
int64_t wref_is_allocated(int64_t wref_id) {
    WeightedRef* wref = (WeightedRef*)wref_id;
    if (!wref) return 0;
    return wref->state == WREF_ALLOCATED ? 1 : 0;
}

// Get state of weighted ref
int64_t wref_state(int64_t wref_id) {
    WeightedRef* wref = (WeightedRef*)wref_id;
    if (!wref) return WREF_UNALLOCATED;
    return wref->state;
}

// Increment reference count
void wref_retain(int64_t wref_id) {
    WeightedRef* wref = (WeightedRef*)wref_id;
    if (!wref) return;
    __sync_fetch_and_add(&wref->ref_count, 1);
}

// Decrement reference count (may free)
void wref_release(int64_t wref_id) {
    WeightedRef* wref = (WeightedRef*)wref_id;
    if (!wref) return;

    int64_t new_count = __sync_sub_and_fetch(&wref->ref_count, 1);
    if (new_count <= 0 && wref->state == WREF_ALLOCATED) {
        pthread_mutex_lock(&g_wref_registry.lock);
        free(wref->ptr);
        wref->ptr = NULL;
        g_wref_registry.total_allocated -= wref->size_bytes;
        wref->state = WREF_FREED;
        pthread_mutex_unlock(&g_wref_registry.lock);
    }
}

// Get reference count
int64_t wref_ref_count(int64_t wref_id) {
    WeightedRef* wref = (WeightedRef*)wref_id;
    if (!wref) return 0;
    return wref->ref_count;
}

// Collapse superposition to single value (observation)
void wref_collapse(int64_t wref_id) {
    WeightedRef* wref = (WeightedRef*)wref_id;
    if (!wref) return;

    pthread_mutex_lock(&g_wref_registry.lock);
    if (wref->state == WREF_ALLOCATED) {
        wref->state = WREF_COLLAPSED;
        wref->weight = 1.0;  // Now definite
    }
    pthread_mutex_unlock(&g_wref_registry.lock);
}

// Get count of weighted refs
int64_t wref_count(void) {
    return g_wref_registry.count;
}

// --------------------------------------------------------------------------
// 5.2 Lazy Execution Mode
// --------------------------------------------------------------------------

typedef struct LazyBranch {
    int64_t id;
    double weight;
    int64_t executed;           // Has this branch been executed?
    int64_t wref_id;            // Associated weighted ref
    void* result;               // Cached result
    int64_t result_size;
} LazyBranch;

typedef struct LazyContext {
    LazyBranch* branches;
    int64_t count;
    int64_t capacity;
    double threshold;           // Only execute if weight > threshold
    int64_t dominant_branch;    // Branch with highest weight
} LazyContext;

// Create lazy execution context
int64_t lazy_context_new(double threshold) {
    LazyContext* ctx = (LazyContext*)calloc(1, sizeof(LazyContext));
    if (!ctx) return 0;

    ctx->capacity = 16;
    ctx->branches = (LazyBranch*)calloc(ctx->capacity, sizeof(LazyBranch));
    ctx->threshold = threshold > 0 ? threshold : 0.5;
    ctx->dominant_branch = -1;

    return (int64_t)ctx;
}

// Add a branch to lazy context
int64_t lazy_add_branch(int64_t ctx_ptr, double weight) {
    LazyContext* ctx = (LazyContext*)ctx_ptr;
    if (!ctx) return -1;

    if (ctx->count >= ctx->capacity) {
        ctx->capacity *= 2;
        ctx->branches = (LazyBranch*)realloc(ctx->branches,
                                              ctx->capacity * sizeof(LazyBranch));
    }

    int64_t id = ctx->count;
    LazyBranch* branch = &ctx->branches[ctx->count++];
    branch->id = id;
    branch->weight = weight;
    branch->executed = 0;
    branch->wref_id = 0;
    branch->result = NULL;
    branch->result_size = 0;

    // Update dominant branch
    if (ctx->dominant_branch < 0 || weight > ctx->branches[ctx->dominant_branch].weight) {
        ctx->dominant_branch = id;
    }

    return id;
}

// Check if branch should execute (lazy mode)
int64_t lazy_should_execute(int64_t ctx_ptr, int64_t branch_id) {
    LazyContext* ctx = (LazyContext*)ctx_ptr;
    if (!ctx || branch_id < 0 || branch_id >= ctx->count) return 0;

    // In lazy mode, only execute if weight > threshold
    return ctx->branches[branch_id].weight >= ctx->threshold ? 1 : 0;
}

// Mark branch as executed
void lazy_mark_executed(int64_t ctx_ptr, int64_t branch_id) {
    LazyContext* ctx = (LazyContext*)ctx_ptr;
    if (!ctx || branch_id < 0 || branch_id >= ctx->count) return;
    ctx->branches[branch_id].executed = 1;
}

// Get dominant branch
int64_t lazy_dominant_branch(int64_t ctx_ptr) {
    LazyContext* ctx = (LazyContext*)ctx_ptr;
    if (!ctx) return -1;
    return ctx->dominant_branch;
}

// Get branch weight
double lazy_branch_weight(int64_t ctx_ptr, int64_t branch_id) {
    LazyContext* ctx = (LazyContext*)ctx_ptr;
    if (!ctx || branch_id < 0 || branch_id >= ctx->count) return 0.0;
    return ctx->branches[branch_id].weight;
}

// Check if branch was executed
int64_t lazy_was_executed(int64_t ctx_ptr, int64_t branch_id) {
    LazyContext* ctx = (LazyContext*)ctx_ptr;
    if (!ctx || branch_id < 0 || branch_id >= ctx->count) return 0;
    return ctx->branches[branch_id].executed;
}

// Get branch count
int64_t lazy_branch_count(int64_t ctx_ptr) {
    LazyContext* ctx = (LazyContext*)ctx_ptr;
    if (!ctx) return 0;
    return ctx->count;
}

// Get executed count
int64_t lazy_executed_count(int64_t ctx_ptr) {
    LazyContext* ctx = (LazyContext*)ctx_ptr;
    if (!ctx) return 0;
    int64_t count = 0;
    for (int64_t i = 0; i < ctx->count; i++) {
        if (ctx->branches[i].executed) count++;
    }
    return count;
}

// Free lazy context
void lazy_context_free(int64_t ctx_ptr) {
    LazyContext* ctx = (LazyContext*)ctx_ptr;
    if (!ctx) return;

    for (int64_t i = 0; i < ctx->count; i++) {
        if (ctx->branches[i].result) {
            free(ctx->branches[i].result);
        }
    }
    free(ctx->branches);
    free(ctx);
}

// --------------------------------------------------------------------------
// 5.3 Speculative Execution Mode
// --------------------------------------------------------------------------

typedef struct SpeculativeBranch {
    int64_t id;
    double weight;
    int64_t wref_id;            // Weighted ref for this branch
    void* result;
    int64_t result_size;
    double weighted_contribution;  // weight * result (for aggregation)
} SpeculativeBranch;

typedef struct SpeculativeContext {
    SpeculativeBranch* branches;
    int64_t count;
    int64_t capacity;
    int64_t budget_bytes;       // Memory budget
    int64_t used_bytes;         // Currently used
    double min_weight;          // Minimum weight to keep
} SpeculativeContext;

// Create speculative execution context
int64_t speculative_context_new(int64_t budget_bytes, double min_weight) {
    SpeculativeContext* ctx = (SpeculativeContext*)calloc(1, sizeof(SpeculativeContext));
    if (!ctx) return 0;

    ctx->capacity = 16;
    ctx->branches = (SpeculativeBranch*)calloc(ctx->capacity, sizeof(SpeculativeBranch));
    ctx->budget_bytes = budget_bytes;
    ctx->min_weight = min_weight > 0 ? min_weight : 0.01;

    return (int64_t)ctx;
}

// Add a branch to speculative context
int64_t speculative_add_branch(int64_t ctx_ptr, double weight, int64_t result_size) {
    SpeculativeContext* ctx = (SpeculativeContext*)ctx_ptr;
    if (!ctx) return -1;

    // Check budget
    if (ctx->budget_bytes > 0 && ctx->used_bytes + result_size > ctx->budget_bytes) {
        return -1;  // Over budget
    }

    if (ctx->count >= ctx->capacity) {
        ctx->capacity *= 2;
        ctx->branches = (SpeculativeBranch*)realloc(ctx->branches,
                                                     ctx->capacity * sizeof(SpeculativeBranch));
    }

    int64_t id = ctx->count;
    SpeculativeBranch* branch = &ctx->branches[ctx->count++];
    branch->id = id;
    branch->weight = weight;
    branch->result = calloc(1, result_size);
    branch->result_size = result_size;
    branch->wref_id = 0;
    branch->weighted_contribution = 0;

    ctx->used_bytes += result_size;

    return id;
}

// Get result pointer for branch
int64_t speculative_result_ptr(int64_t ctx_ptr, int64_t branch_id) {
    SpeculativeContext* ctx = (SpeculativeContext*)ctx_ptr;
    if (!ctx || branch_id < 0 || branch_id >= ctx->count) return 0;
    return (int64_t)ctx->branches[branch_id].result;
}

// Set weighted contribution
void speculative_set_contribution(int64_t ctx_ptr, int64_t branch_id, double contribution) {
    SpeculativeContext* ctx = (SpeculativeContext*)ctx_ptr;
    if (!ctx || branch_id < 0 || branch_id >= ctx->count) return;
    ctx->branches[branch_id].weighted_contribution = contribution;
}

// Aggregate weighted results (returns weighted sum)
double speculative_aggregate(int64_t ctx_ptr) {
    SpeculativeContext* ctx = (SpeculativeContext*)ctx_ptr;
    if (!ctx) return 0.0;

    double total = 0.0;
    double weight_sum = 0.0;

    for (int64_t i = 0; i < ctx->count; i++) {
        total += ctx->branches[i].weighted_contribution * ctx->branches[i].weight;
        weight_sum += ctx->branches[i].weight;
    }

    return weight_sum > 0 ? total / weight_sum : 0.0;
}

// GC low-weight branches
int64_t speculative_gc(int64_t ctx_ptr) {
    SpeculativeContext* ctx = (SpeculativeContext*)ctx_ptr;
    if (!ctx) return 0;

    int64_t freed = 0;
    for (int64_t i = 0; i < ctx->count; i++) {
        if (ctx->branches[i].weight < ctx->min_weight && ctx->branches[i].result) {
            free(ctx->branches[i].result);
            ctx->used_bytes -= ctx->branches[i].result_size;
            ctx->branches[i].result = NULL;
            ctx->branches[i].result_size = 0;
            freed++;
        }
    }

    return freed;
}

// Get used bytes
int64_t speculative_used_bytes(int64_t ctx_ptr) {
    SpeculativeContext* ctx = (SpeculativeContext*)ctx_ptr;
    if (!ctx) return 0;
    return ctx->used_bytes;
}

// Get branch count
int64_t speculative_branch_count(int64_t ctx_ptr) {
    SpeculativeContext* ctx = (SpeculativeContext*)ctx_ptr;
    if (!ctx) return 0;
    return ctx->count;
}

// Free speculative context
void speculative_context_free(int64_t ctx_ptr) {
    SpeculativeContext* ctx = (SpeculativeContext*)ctx_ptr;
    if (!ctx) return;

    for (int64_t i = 0; i < ctx->count; i++) {
        if (ctx->branches[i].result) {
            free(ctx->branches[i].result);
        }
    }
    free(ctx->branches);
    free(ctx);
}

// Set result for branch (stores an i64 value)
void speculative_set_result(int64_t ctx_ptr, int64_t branch_id, int64_t result) {
    SpeculativeContext* ctx = (SpeculativeContext*)ctx_ptr;
    if (!ctx || branch_id < 0 || branch_id >= ctx->count) return;
    if (!ctx->branches[branch_id].result) return;
    *(int64_t*)ctx->branches[branch_id].result = result;
    ctx->branches[branch_id].weighted_contribution = (double)result;
}

// Get result for branch
int64_t speculative_get_result(int64_t ctx_ptr, int64_t branch_id) {
    SpeculativeContext* ctx = (SpeculativeContext*)ctx_ptr;
    if (!ctx || branch_id < 0 || branch_id >= ctx->count) return 0;
    if (!ctx->branches[branch_id].result) return 0;
    return *(int64_t*)ctx->branches[branch_id].result;
}

// Get weighted average result
double speculative_weighted_result(int64_t ctx_ptr) {
    SpeculativeContext* ctx = (SpeculativeContext*)ctx_ptr;
    if (!ctx) return 0.0;

    double total = 0.0;
    double weight_sum = 0.0;

    for (int64_t i = 0; i < ctx->count; i++) {
        if (ctx->branches[i].result) {
            int64_t val = *(int64_t*)ctx->branches[i].result;
            total += (double)val * ctx->branches[i].weight;
            weight_sum += ctx->branches[i].weight;
        }
    }

    return weight_sum > 0 ? total / weight_sum : 0.0;
}

// Get memory used (alias for speculative_used_bytes)
int64_t speculative_memory_used(int64_t ctx_ptr) {
    return speculative_used_bytes(ctx_ptr);
}

// --------------------------------------------------------------------------
// 5.4 Checkpoint Execution Mode
// --------------------------------------------------------------------------

typedef struct CheckpointState {
    int64_t id;
    void* data;                 // Snapshot of state
    int64_t size_bytes;
    int64_t branch_id;          // Which branch this is for
    double gradient;            // Accumulated gradient
    struct CheckpointState* next;
} CheckpointState;

typedef struct CheckpointContext {
    CheckpointState* head;
    int64_t count;
    int64_t next_id;
    int64_t current_branch;
    void* base_state;           // Original state before branching
    int64_t base_size;
} CheckpointContext;

// Create checkpoint context
int64_t checkpoint_context_new(void) {
    CheckpointContext* ctx = (CheckpointContext*)calloc(1, sizeof(CheckpointContext));
    if (!ctx) return 0;

    ctx->next_id = 1;
    ctx->current_branch = -1;

    return (int64_t)ctx;
}

// Save checkpoint (snapshot current state)
int64_t checkpoint_save(int64_t ctx_ptr, int64_t data_ptr, int64_t size_bytes, int64_t branch_id) {
    CheckpointContext* ctx = (CheckpointContext*)ctx_ptr;
    if (!ctx) return 0;

    CheckpointState* state = (CheckpointState*)calloc(1, sizeof(CheckpointState));
    if (!state) return 0;

    state->id = ctx->next_id++;
    state->size_bytes = size_bytes;
    state->branch_id = branch_id;
    state->gradient = 0.0;

    // Copy state data
    state->data = malloc(size_bytes);
    if (!state->data) {
        free(state);
        return 0;
    }
    memcpy(state->data, (void*)data_ptr, size_bytes);

    // Add to list
    state->next = ctx->head;
    ctx->head = state;
    ctx->count++;

    return state->id;
}

// Restore from checkpoint
int64_t checkpoint_restore(int64_t ctx_ptr, int64_t checkpoint_id, int64_t dest_ptr) {
    CheckpointContext* ctx = (CheckpointContext*)ctx_ptr;
    if (!ctx) return 0;

    // Find checkpoint
    for (CheckpointState* s = ctx->head; s; s = s->next) {
        if (s->id == checkpoint_id) {
            memcpy((void*)dest_ptr, s->data, s->size_bytes);
            ctx->current_branch = s->branch_id;
            return 1;
        }
    }

    return 0;  // Not found
}

// Set base state (before branching)
void checkpoint_set_base(int64_t ctx_ptr, int64_t data_ptr, int64_t size_bytes) {
    CheckpointContext* ctx = (CheckpointContext*)ctx_ptr;
    if (!ctx) return;

    if (ctx->base_state) {
        free(ctx->base_state);
    }

    ctx->base_state = malloc(size_bytes);
    if (ctx->base_state) {
        memcpy(ctx->base_state, (void*)data_ptr, size_bytes);
        ctx->base_size = size_bytes;
    }
}

// Restore to base state
int64_t checkpoint_restore_base(int64_t ctx_ptr, int64_t dest_ptr) {
    CheckpointContext* ctx = (CheckpointContext*)ctx_ptr;
    if (!ctx || !ctx->base_state) return 0;

    memcpy((void*)dest_ptr, ctx->base_state, ctx->base_size);
    ctx->current_branch = -1;
    return 1;
}

// Add gradient for checkpoint
void checkpoint_add_gradient(int64_t ctx_ptr, int64_t checkpoint_id, double gradient) {
    CheckpointContext* ctx = (CheckpointContext*)ctx_ptr;
    if (!ctx) return;

    for (CheckpointState* s = ctx->head; s; s = s->next) {
        if (s->id == checkpoint_id) {
            s->gradient += gradient;
            return;
        }
    }
}

// Get gradient for checkpoint
double checkpoint_get_gradient(int64_t ctx_ptr, int64_t checkpoint_id) {
    CheckpointContext* ctx = (CheckpointContext*)ctx_ptr;
    if (!ctx) return 0.0;

    for (CheckpointState* s = ctx->head; s; s = s->next) {
        if (s->id == checkpoint_id) {
            return s->gradient;
        }
    }
    return 0.0;
}

// Get current branch
int64_t checkpoint_current_branch(int64_t ctx_ptr) {
    CheckpointContext* ctx = (CheckpointContext*)ctx_ptr;
    if (!ctx) return -1;
    return ctx->current_branch;
}

// Get checkpoint count
int64_t checkpoint_count(int64_t ctx_ptr) {
    CheckpointContext* ctx = (CheckpointContext*)ctx_ptr;
    if (!ctx) return 0;
    return ctx->count;
}

// Free checkpoint context
void checkpoint_context_free(int64_t ctx_ptr) {
    CheckpointContext* ctx = (CheckpointContext*)ctx_ptr;
    if (!ctx) return;

    CheckpointState* s = ctx->head;
    while (s) {
        CheckpointState* next = s->next;
        free(s->data);
        free(s);
        s = next;
    }

    if (ctx->base_state) {
        free(ctx->base_state);
    }
    free(ctx);
}

// --------------------------------------------------------------------------
// 5.4b Simplified Checkpoint Wrappers for Phase 5 Tests
// --------------------------------------------------------------------------

// Simple checkpoint context (wraps the existing one)
int64_t ckpt_context_new(int64_t max_size) {
    (void)max_size;  // Size is per-checkpoint in full impl
    return checkpoint_context_new();
}

// Save checkpoint (simplified - uses a small dummy state)
static int64_t g_ckpt_dummy_state = 0;
int64_t ckpt_save(int64_t ctx_ptr) {
    g_ckpt_dummy_state++;
    return checkpoint_save(ctx_ptr, (int64_t)&g_ckpt_dummy_state, sizeof(int64_t), g_ckpt_dummy_state);
}

// Restore checkpoint
int64_t ckpt_restore(int64_t ctx_ptr, int64_t checkpoint_id) {
    int64_t dummy;
    return checkpoint_restore(ctx_ptr, checkpoint_id, (int64_t)&dummy);
}

// Start branch execution
int64_t ckpt_branch_start(int64_t ctx_ptr, int64_t checkpoint_id) {
    CheckpointContext* ctx = (CheckpointContext*)ctx_ptr;
    if (!ctx) return -1;
    ctx->current_branch = checkpoint_id;
    return checkpoint_id;
}

// End branch execution
void ckpt_branch_end(int64_t ctx_ptr) {
    CheckpointContext* ctx = (CheckpointContext*)ctx_ptr;
    if (ctx) ctx->current_branch = -1;
}

// Get current checkpoint
int64_t ckpt_current(int64_t ctx_ptr) {
    return checkpoint_current_branch(ctx_ptr);
}

// Get checkpoint count
int64_t ckpt_count(int64_t ctx_ptr) {
    return checkpoint_count(ctx_ptr);
}

// Get memory used by checkpoints
int64_t ckpt_memory_used(int64_t ctx_ptr) {
    CheckpointContext* ctx = (CheckpointContext*)ctx_ptr;
    if (!ctx) return 0;
    int64_t total = ctx->base_size;
    for (CheckpointState* s = ctx->head; s; s = s->next) {
        total += s->size_bytes;
    }
    return total;
}

// Free checkpoint context
void ckpt_context_free(int64_t ctx_ptr) {
    checkpoint_context_free(ctx_ptr);
}

// --------------------------------------------------------------------------
// 5.5 Weighted GC Algorithm
// --------------------------------------------------------------------------

typedef struct WeightedGCStats {
    int64_t total_refs;
    int64_t freed_refs;
    int64_t freed_bytes;
    double avg_weight_freed;
    int64_t retained_refs;
    int64_t retained_bytes;
} WeightedGCStats;

// Global GC tracking stats
static int64_t g_gc_last_collected = 0;
static int64_t g_gc_last_bytes = 0;
static int64_t g_gc_total_runs = 0;
static int64_t g_gc_total_collected = 0;
static int64_t g_gc_total_bytes = 0;

// Run weighted GC pass with threshold parameter
void wref_gc(double threshold) {
    wref_registry_init();

    pthread_mutex_lock(&g_wref_registry.lock);

    int64_t freed = 0;
    int64_t freed_bytes = 0;
    WeightedRef* prev = NULL;
    WeightedRef* curr = g_wref_registry.head;

    while (curr) {
        WeightedRef* next = curr->next;

        // Free if weight below threshold and not collapsed
        if (curr->weight < threshold &&
            curr->state == WREF_ALLOCATED &&
            curr->ref_count <= 0) {

            // Track freed bytes before freeing
            freed_bytes += curr->size_bytes;

            // Free the data
            free(curr->ptr);
            g_wref_registry.total_allocated -= curr->size_bytes;
            curr->state = WREF_FREED;
            freed++;

            // Remove from list
            if (prev) {
                prev->next = next;
            } else {
                g_wref_registry.head = next;
            }
            g_wref_registry.count--;
            free(curr);
        } else {
            prev = curr;
        }

        curr = next;
    }

    pthread_mutex_unlock(&g_wref_registry.lock);

    // Update stats
    g_gc_last_collected = freed;
    g_gc_last_bytes = freed_bytes;
    g_gc_total_runs++;
    g_gc_total_collected += freed;
    g_gc_total_bytes += freed_bytes;
}

// Get last GC collected count
int64_t wref_gc_last_collected(void) {
    return g_gc_last_collected;
}

// Get last GC bytes freed
int64_t wref_gc_last_bytes(void) {
    return g_gc_last_bytes;
}

// Get total GC runs
int64_t wref_gc_total_runs(void) {
    return g_gc_total_runs;
}

// Get total collected across all runs
int64_t wref_gc_total_collected(void) {
    return g_gc_total_collected;
}

// Get total bytes freed across all runs
int64_t wref_gc_total_bytes(void) {
    return g_gc_total_bytes;
}

// Get GC statistics
int64_t wref_gc_stats(void) {
    WeightedGCStats* stats = (WeightedGCStats*)calloc(1, sizeof(WeightedGCStats));
    if (!stats) return 0;

    pthread_mutex_lock(&g_wref_registry.lock);

    double weight_sum = 0;
    int64_t low_weight_count = 0;

    for (WeightedRef* w = g_wref_registry.head; w; w = w->next) {
        stats->total_refs++;

        if (w->weight < g_wref_registry.gc_threshold) {
            if (w->state == WREF_ALLOCATED) {
                stats->freed_bytes += w->size_bytes;
            }
            weight_sum += w->weight;
            low_weight_count++;
        } else {
            stats->retained_refs++;
            if (w->state == WREF_ALLOCATED) {
                stats->retained_bytes += w->size_bytes;
            }
        }
    }

    stats->freed_refs = low_weight_count;
    stats->avg_weight_freed = low_weight_count > 0 ? weight_sum / low_weight_count : 0;

    pthread_mutex_unlock(&g_wref_registry.lock);

    return (int64_t)stats;
}

// Get freed refs from stats
int64_t wref_gc_stats_freed_refs(int64_t stats_ptr) {
    WeightedGCStats* stats = (WeightedGCStats*)stats_ptr;
    if (!stats) return 0;
    return stats->freed_refs;
}

// Get freed bytes from stats
int64_t wref_gc_stats_freed_bytes(int64_t stats_ptr) {
    WeightedGCStats* stats = (WeightedGCStats*)stats_ptr;
    if (!stats) return 0;
    return stats->freed_bytes;
}

// Get retained refs from stats
int64_t wref_gc_stats_retained_refs(int64_t stats_ptr) {
    WeightedGCStats* stats = (WeightedGCStats*)stats_ptr;
    if (!stats) return 0;
    return stats->retained_refs;
}

// Get retained bytes from stats
int64_t wref_gc_stats_retained_bytes(int64_t stats_ptr) {
    WeightedGCStats* stats = (WeightedGCStats*)stats_ptr;
    if (!stats) return 0;
    return stats->retained_bytes;
}

// Free GC stats
void wref_gc_stats_free(int64_t stats_ptr) {
    free((void*)stats_ptr);
}

// Force full GC (ignore thresholds, free all non-collapsed)
int64_t wref_gc_full(void) {
    pthread_mutex_lock(&g_wref_registry.lock);

    int64_t freed = 0;
    WeightedRef* prev = NULL;
    WeightedRef* curr = g_wref_registry.head;

    while (curr) {
        WeightedRef* next = curr->next;

        // Free if not collapsed and refcount is zero
        if (curr->state == WREF_ALLOCATED &&
            curr->state != WREF_COLLAPSED &&
            curr->ref_count <= 0) {

            free(curr->ptr);
            g_wref_registry.total_allocated -= curr->size_bytes;
            curr->state = WREF_FREED;
            freed++;

            if (prev) {
                prev->next = next;
            } else {
                g_wref_registry.head = next;
            }
            g_wref_registry.count--;
            free(curr);
        } else {
            prev = curr;
        }

        curr = next;
    }

    pthread_mutex_unlock(&g_wref_registry.lock);

    return freed;
}

// --------------------------------------------------------------------------
// End of Phase 5: Superposition Memory Model
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// 29.6 Anima Cognitive Memory System
// --------------------------------------------------------------------------
// The Anima is the cognitive soul of a Simplex AI application.
// It contains: episodic memory (experiences), semantic memory (facts),
// procedural memory (skills), working memory (active context), and beliefs.

typedef struct Experience {
    int64_t id;
    char* content;
    double importance;
    int64_t timestamp;
    struct Experience* next;
} Experience;

typedef struct Fact {
    int64_t id;
    char* content;
    double confidence;
    char* source;
    struct Fact* next;
} Fact;

typedef struct AnimaProcedure {
    int64_t id;
    char* name;
    char* steps_json;
    double success_rate;
    struct AnimaProcedure* next;
} AnimaProcedure;

typedef struct AnimaBelief {
    int64_t id;
    char* content;
    double confidence;
    char* evidence_json;
    struct AnimaBelief* next;
} AnimaBelief;

typedef struct AnimaMemory {
    // Memory stores
    Experience* episodic_head;
    int64_t episodic_count;
    Fact* semantic_head;
    int64_t semantic_count;
    AnimaProcedure* procedural_head;
    int64_t procedural_count;
    AnimaBelief* beliefs_head;
    int64_t beliefs_count;
    // Working memory (ring buffer)
    void** working;
    int64_t working_capacity;
    int64_t working_head;
    int64_t working_count;
    // Counters
    int64_t next_id;
    pthread_mutex_t lock;
} AnimaMemory;

// Create new anima memory system
int64_t anima_memory_new(int64_t working_capacity) {
    AnimaMemory* mem = (AnimaMemory*)calloc(1, sizeof(AnimaMemory));
    if (!mem) return 0;

    mem->working_capacity = working_capacity > 0 ? working_capacity : 10;
    mem->working = (void**)calloc(mem->working_capacity, sizeof(void*));
    mem->next_id = 1;
    pthread_mutex_init(&mem->lock, NULL);

    return (int64_t)mem;
}

// Store experience in episodic memory
int64_t anima_remember(int64_t mem_ptr, int64_t content_ptr, double importance) {
    AnimaMemory* mem = (AnimaMemory*)mem_ptr;
    SxString* content = (SxString*)content_ptr;
    if (!mem || !content) return 0;

    pthread_mutex_lock(&mem->lock);

    Experience* exp = (Experience*)malloc(sizeof(Experience));
    exp->id = mem->next_id++;
    exp->content = strdup(content->data);
    exp->importance = importance;
    exp->timestamp = (int64_t)time(NULL);
    exp->next = mem->episodic_head;
    mem->episodic_head = exp;
    mem->episodic_count++;

    pthread_mutex_unlock(&mem->lock);
    return exp->id;
}

// Store fact in semantic memory
int64_t anima_learn(int64_t mem_ptr, int64_t content_ptr, double confidence, int64_t source_ptr) {
    AnimaMemory* mem = (AnimaMemory*)mem_ptr;
    SxString* content = (SxString*)content_ptr;
    SxString* source = (SxString*)source_ptr;
    if (!mem || !content) return 0;

    pthread_mutex_lock(&mem->lock);

    Fact* fact = (Fact*)malloc(sizeof(Fact));
    fact->id = mem->next_id++;
    fact->content = strdup(content->data);
    fact->confidence = confidence;
    fact->source = source ? strdup(source->data) : NULL;
    fact->next = mem->semantic_head;
    mem->semantic_head = fact;
    mem->semantic_count++;

    pthread_mutex_unlock(&mem->lock);
    return fact->id;
}

// Store procedure in procedural memory
int64_t anima_store_procedure(int64_t mem_ptr, int64_t name_ptr, int64_t steps_ptr) {
    AnimaMemory* mem = (AnimaMemory*)mem_ptr;
    SxString* name = (SxString*)name_ptr;
    SxString* steps = (SxString*)steps_ptr;
    if (!mem || !name) return 0;

    pthread_mutex_lock(&mem->lock);

    AnimaProcedure* proc = (AnimaProcedure*)malloc(sizeof(AnimaProcedure));
    proc->id = mem->next_id++;
    proc->name = strdup(name->data);
    proc->steps_json = steps ? strdup(steps->data) : NULL;
    proc->success_rate = 1.0;
    proc->next = mem->procedural_head;
    mem->procedural_head = proc;
    mem->procedural_count++;

    pthread_mutex_unlock(&mem->lock);
    return proc->id;
}

// Store belief with confidence
int64_t anima_believe(int64_t mem_ptr, int64_t content_ptr, double confidence, int64_t evidence_ptr) {
    AnimaMemory* mem = (AnimaMemory*)mem_ptr;
    SxString* content = (SxString*)content_ptr;
    SxString* evidence = (SxString*)evidence_ptr;
    if (!mem || !content) return 0;

    pthread_mutex_lock(&mem->lock);

    // Check for existing belief with same content
    AnimaBelief* existing = mem->beliefs_head;
    while (existing) {
        if (strcmp(existing->content, content->data) == 0) {
            // Update existing belief
            existing->confidence = confidence;
            if (evidence && existing->evidence_json) {
                free(existing->evidence_json);
                existing->evidence_json = strdup(evidence->data);
            }
            pthread_mutex_unlock(&mem->lock);
            return existing->id;
        }
        existing = existing->next;
    }

    // Create new belief
    AnimaBelief* belief = (AnimaBelief*)malloc(sizeof(AnimaBelief));
    belief->id = mem->next_id++;
    belief->content = strdup(content->data);
    belief->confidence = confidence;
    belief->evidence_json = evidence ? strdup(evidence->data) : NULL;
    belief->next = mem->beliefs_head;
    mem->beliefs_head = belief;
    mem->beliefs_count++;

    pthread_mutex_unlock(&mem->lock);
    return belief->id;
}

// Revise belief based on new evidence
int64_t anima_revise_belief(int64_t mem_ptr, int64_t belief_id, double new_confidence, int64_t new_evidence_ptr) {
    AnimaMemory* mem = (AnimaMemory*)mem_ptr;
    SxString* new_evidence = (SxString*)new_evidence_ptr;
    if (!mem) return 0;

    pthread_mutex_lock(&mem->lock);

    AnimaBelief* belief = mem->beliefs_head;
    while (belief) {
        if (belief->id == belief_id) {
            belief->confidence = new_confidence;
            if (new_evidence) {
                if (belief->evidence_json) free(belief->evidence_json);
                belief->evidence_json = strdup(new_evidence->data);
            }
            pthread_mutex_unlock(&mem->lock);
            return 1;
        }
        belief = belief->next;
    }

    pthread_mutex_unlock(&mem->lock);
    return 0;
}

// Push to working memory
int64_t anima_working_push(int64_t mem_ptr, int64_t item_ptr) {
    AnimaMemory* mem = (AnimaMemory*)mem_ptr;
    if (!mem) return 0;

    pthread_mutex_lock(&mem->lock);

    // If at capacity, pop oldest
    if (mem->working_count >= mem->working_capacity) {
        // Oldest is at (head - count) mod capacity
        // Just overwrite the oldest slot
    }

    int64_t slot = (mem->working_head + mem->working_count) % mem->working_capacity;
    mem->working[slot] = (void*)item_ptr;

    if (mem->working_count < mem->working_capacity) {
        mem->working_count++;
    } else {
        mem->working_head = (mem->working_head + 1) % mem->working_capacity;
    }

    pthread_mutex_unlock(&mem->lock);
    return 1;
}

// Pop from working memory
int64_t anima_working_pop(int64_t mem_ptr) {
    AnimaMemory* mem = (AnimaMemory*)mem_ptr;
    if (!mem || mem->working_count == 0) return 0;

    pthread_mutex_lock(&mem->lock);

    int64_t last_slot = (mem->working_head + mem->working_count - 1) % mem->working_capacity;
    void* item = mem->working[last_slot];
    mem->working[last_slot] = NULL;
    mem->working_count--;

    pthread_mutex_unlock(&mem->lock);
    return (int64_t)item;
}

// Get working memory as vector
int64_t anima_working_context(int64_t mem_ptr) {
    AnimaMemory* mem = (AnimaMemory*)mem_ptr;
    if (!mem) return 0;

    pthread_mutex_lock(&mem->lock);

    SxVec* vec = intrinsic_vec_new();
    for (int64_t i = 0; i < mem->working_count; i++) {
        int64_t slot = (mem->working_head + i) % mem->working_capacity;
        if (mem->working[slot]) {
            intrinsic_vec_push(vec, mem->working[slot]);
        }
    }

    pthread_mutex_unlock(&mem->lock);
    return (int64_t)vec;
}

// Goal-directed recall - finds relevant memories for a goal
int64_t anima_recall_for_goal(int64_t mem_ptr, int64_t goal_ptr, int64_t context_ptr, int64_t max_results) {
    AnimaMemory* mem = (AnimaMemory*)mem_ptr;
    SxString* goal = (SxString*)goal_ptr;
    if (!mem || !goal) return 0;

    pthread_mutex_lock(&mem->lock);

    SxVec* results = intrinsic_vec_new();

    // Simple keyword matching for now - will be enhanced with SLM
    // Search episodic memory
    Experience* exp = mem->episodic_head;
    int64_t count = 0;
    while (exp && count < max_results) {
        if (strstr(exp->content, goal->data) != NULL) {
            // Return as string for now
            SxString* s = intrinsic_string_new(exp->content);
            intrinsic_vec_push(results, s);
            count++;
        }
        exp = exp->next;
    }

    // Search semantic memory
    Fact* fact = mem->semantic_head;
    while (fact && count < max_results) {
        if (strstr(fact->content, goal->data) != NULL) {
            SxString* s = intrinsic_string_new(fact->content);
            intrinsic_vec_push(results, s);
            count++;
        }
        fact = fact->next;
    }

    pthread_mutex_unlock(&mem->lock);
    return (int64_t)results;
}

// Get counts
int64_t anima_episodic_count(int64_t mem_ptr) {
    AnimaMemory* mem = (AnimaMemory*)mem_ptr;
    return mem ? mem->episodic_count : 0;
}

int64_t anima_semantic_count(int64_t mem_ptr) {
    AnimaMemory* mem = (AnimaMemory*)mem_ptr;
    return mem ? mem->semantic_count : 0;
}

int64_t anima_beliefs_count(int64_t mem_ptr) {
    AnimaMemory* mem = (AnimaMemory*)mem_ptr;
    return mem ? mem->beliefs_count : 0;
}

int64_t anima_working_count(int64_t mem_ptr) {
    AnimaMemory* mem = (AnimaMemory*)mem_ptr;
    return mem ? mem->working_count : 0;
}

// Memory consolidation - summarize and prune low-importance memories
int64_t anima_consolidate(int64_t mem_ptr) {
    AnimaMemory* mem = (AnimaMemory*)mem_ptr;
    if (!mem) return 0;

    pthread_mutex_lock(&mem->lock);

    // Prune low importance experiences (importance < 0.3)
    Experience** exp_ptr = &mem->episodic_head;
    int64_t pruned = 0;
    while (*exp_ptr) {
        if ((*exp_ptr)->importance < 0.3) {
            Experience* to_free = *exp_ptr;
            *exp_ptr = to_free->next;
            free(to_free->content);
            free(to_free);
            mem->episodic_count--;
            pruned++;
        } else {
            exp_ptr = &(*exp_ptr)->next;
        }
    }

    // Prune low confidence facts (confidence < 0.5)
    Fact** fact_ptr = &mem->semantic_head;
    while (*fact_ptr) {
        if ((*fact_ptr)->confidence < 0.5) {
            Fact* to_free = *fact_ptr;
            *fact_ptr = to_free->next;
            free(to_free->content);
            if (to_free->source) free(to_free->source);
            free(to_free);
            mem->semantic_count--;
            pruned++;
        } else {
            fact_ptr = &(*fact_ptr)->next;
        }
    }

    pthread_mutex_unlock(&mem->lock);
    return pruned;
}

// Close and free anima memory
void anima_memory_close(int64_t mem_ptr) {
    AnimaMemory* mem = (AnimaMemory*)mem_ptr;
    if (!mem) return;

    pthread_mutex_lock(&mem->lock);

    // Free episodic memory
    Experience* exp = mem->episodic_head;
    while (exp) {
        Experience* next = exp->next;
        free(exp->content);
        free(exp);
        exp = next;
    }

    // Free semantic memory
    Fact* fact = mem->semantic_head;
    while (fact) {
        Fact* next = fact->next;
        free(fact->content);
        if (fact->source) free(fact->source);
        free(fact);
        fact = next;
    }

    // Free procedural memory
    AnimaProcedure* proc = mem->procedural_head;
    while (proc) {
        AnimaProcedure* next = proc->next;
        free(proc->name);
        if (proc->steps_json) free(proc->steps_json);
        free(proc);
        proc = next;
    }

    // Free beliefs
    AnimaBelief* belief = mem->beliefs_head;
    while (belief) {
        AnimaBelief* next = belief->next;
        free(belief->content);
        if (belief->evidence_json) free(belief->evidence_json);
        free(belief);
        belief = next;
    }

    // Free working memory
    free(mem->working);

    pthread_mutex_unlock(&mem->lock);
    pthread_mutex_destroy(&mem->lock);
    free(mem);
}

// --------------------------------------------------------------------------
// 29.6.5 Anima Desires and Intentions (BDI)
// --------------------------------------------------------------------------

typedef struct AnimaDesire {
    int64_t id;
    char* goal;
    double priority;
    int64_t status;  // 0=pending, 1=active, 2=achieved, 3=failed
    struct AnimaDesire* next;
} AnimaDesire;

typedef struct AnimaIntention {
    int64_t id;
    char* plan;
    char* steps_json;
    int64_t current_step;
    int64_t total_steps;
    int64_t status;  // 0=pending, 1=executing, 2=completed, 3=failed
    struct AnimaIntention* next;
} AnimaIntention;

// Extended AnimaMemory with BDI support (stored separately for modularity)
typedef struct AnimaBDI {
    AnimaDesire* desires_head;
    int64_t desires_count;
    AnimaIntention* intentions_head;
    int64_t intentions_count;
    int64_t next_id;
    pthread_mutex_t lock;
} AnimaBDI;

// Create new BDI system
int64_t anima_bdi_new() {
    AnimaBDI* bdi = (AnimaBDI*)calloc(1, sizeof(AnimaBDI));
    if (!bdi) return 0;
    bdi->next_id = 1;
    pthread_mutex_init(&bdi->lock, NULL);
    return (int64_t)bdi;
}

// Add a desire (goal with priority)
int64_t anima_add_desire(int64_t bdi_ptr, int64_t goal_ptr, double priority) {
    AnimaBDI* bdi = (AnimaBDI*)bdi_ptr;
    SxString* goal = (SxString*)goal_ptr;
    if (!bdi || !goal) return 0;

    pthread_mutex_lock(&bdi->lock);

    AnimaDesire* desire = (AnimaDesire*)malloc(sizeof(AnimaDesire));
    desire->id = bdi->next_id++;
    desire->goal = strdup(goal->data);
    desire->priority = priority;
    desire->status = 0;  // pending
    desire->next = bdi->desires_head;
    bdi->desires_head = desire;
    bdi->desires_count++;

    pthread_mutex_unlock(&bdi->lock);
    return desire->id;
}

// Get highest priority desire
int64_t anima_get_top_desire(int64_t bdi_ptr) {
    AnimaBDI* bdi = (AnimaBDI*)bdi_ptr;
    if (!bdi) return 0;

    pthread_mutex_lock(&bdi->lock);

    AnimaDesire* best = NULL;
    AnimaDesire* current = bdi->desires_head;
    while (current) {
        if (current->status == 0 || current->status == 1) {  // pending or active
            if (!best || current->priority > best->priority) {
                best = current;
            }
        }
        current = current->next;
    }

    pthread_mutex_unlock(&bdi->lock);

    if (best) {
        SxString* result = (SxString*)malloc(sizeof(SxString));
        result->data = strdup(best->goal);
        result->len = strlen(best->goal);
        result->cap = result->len + 1;
        return (int64_t)result;
    }
    return 0;
}

// Count desires
int64_t anima_desires_count(int64_t bdi_ptr) {
    AnimaBDI* bdi = (AnimaBDI*)bdi_ptr;
    if (!bdi) return 0;
    return bdi->desires_count;
}

// Set desire status
int64_t anima_set_desire_status(int64_t bdi_ptr, int64_t desire_id, int64_t status) {
    AnimaBDI* bdi = (AnimaBDI*)bdi_ptr;
    if (!bdi) return 0;

    pthread_mutex_lock(&bdi->lock);

    AnimaDesire* current = bdi->desires_head;
    while (current) {
        if (current->id == desire_id) {
            current->status = status;
            pthread_mutex_unlock(&bdi->lock);
            return 1;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&bdi->lock);
    return 0;
}

// Add an intention (active plan)
int64_t anima_add_intention(int64_t bdi_ptr, int64_t plan_ptr, int64_t steps_ptr, int64_t total_steps) {
    AnimaBDI* bdi = (AnimaBDI*)bdi_ptr;
    SxString* plan = (SxString*)plan_ptr;
    SxString* steps = (SxString*)steps_ptr;
    if (!bdi || !plan) return 0;

    pthread_mutex_lock(&bdi->lock);

    AnimaIntention* intention = (AnimaIntention*)malloc(sizeof(AnimaIntention));
    intention->id = bdi->next_id++;
    intention->plan = strdup(plan->data);
    intention->steps_json = steps ? strdup(steps->data) : NULL;
    intention->current_step = 0;
    intention->total_steps = total_steps;
    intention->status = 0;  // pending
    intention->next = bdi->intentions_head;
    bdi->intentions_head = intention;
    bdi->intentions_count++;

    pthread_mutex_unlock(&bdi->lock);
    return intention->id;
}

// Advance intention to next step
int64_t anima_advance_intention(int64_t bdi_ptr, int64_t intention_id) {
    AnimaBDI* bdi = (AnimaBDI*)bdi_ptr;
    if (!bdi) return 0;

    pthread_mutex_lock(&bdi->lock);

    AnimaIntention* current = bdi->intentions_head;
    while (current) {
        if (current->id == intention_id) {
            current->current_step++;
            if (current->current_step >= current->total_steps) {
                current->status = 2;  // completed
            } else {
                current->status = 1;  // executing
            }
            pthread_mutex_unlock(&bdi->lock);
            return current->current_step;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&bdi->lock);
    return -1;
}

// Get current step of intention
int64_t anima_intention_step(int64_t bdi_ptr, int64_t intention_id) {
    AnimaBDI* bdi = (AnimaBDI*)bdi_ptr;
    if (!bdi) return -1;

    pthread_mutex_lock(&bdi->lock);

    AnimaIntention* current = bdi->intentions_head;
    while (current) {
        if (current->id == intention_id) {
            int64_t step = current->current_step;
            pthread_mutex_unlock(&bdi->lock);
            return step;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&bdi->lock);
    return -1;
}

// Count intentions
int64_t anima_intentions_count(int64_t bdi_ptr) {
    AnimaBDI* bdi = (AnimaBDI*)bdi_ptr;
    if (!bdi) return 0;
    return bdi->intentions_count;
}

// Set intention status
int64_t anima_set_intention_status(int64_t bdi_ptr, int64_t intention_id, int64_t status) {
    AnimaBDI* bdi = (AnimaBDI*)bdi_ptr;
    if (!bdi) return 0;

    pthread_mutex_lock(&bdi->lock);

    AnimaIntention* current = bdi->intentions_head;
    while (current) {
        if (current->id == intention_id) {
            current->status = status;
            pthread_mutex_unlock(&bdi->lock);
            return 1;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&bdi->lock);
    return 0;
}

// Clean up BDI system
void anima_bdi_close(int64_t bdi_ptr) {
    AnimaBDI* bdi = (AnimaBDI*)bdi_ptr;
    if (!bdi) return;

    pthread_mutex_lock(&bdi->lock);

    // Free desires
    AnimaDesire* desire = bdi->desires_head;
    while (desire) {
        AnimaDesire* next = desire->next;
        free(desire->goal);
        free(desire);
        desire = next;
    }

    // Free intentions
    AnimaIntention* intention = bdi->intentions_head;
    while (intention) {
        AnimaIntention* next = intention->next;
        free(intention->plan);
        if (intention->steps_json) free(intention->steps_json);
        free(intention);
        intention = next;
    }

    pthread_mutex_unlock(&bdi->lock);
    pthread_mutex_destroy(&bdi->lock);
    free(bdi);
}

// --------------------------------------------------------------------------
// 29.6.6 Anima Persistence (Save/Load)
// --------------------------------------------------------------------------

// Save anima memory to JSON file
int64_t anima_save(int64_t mem_ptr, int64_t path_ptr) {
    AnimaMemory* mem = (AnimaMemory*)mem_ptr;
    SxString* path = (SxString*)path_ptr;
    if (!mem || !path) return 0;

    FILE* f = fopen(path->data, "w");
    if (!f) return 0;

    pthread_mutex_lock(&mem->lock);

    fprintf(f, "{\n");

    // Save episodic memories
    fprintf(f, "  \"episodic\": [\n");
    Experience* exp = mem->episodic_head;
    int first = 1;
    while (exp) {
        if (!first) fprintf(f, ",\n");
        fprintf(f, "    {\"id\": %lld, \"content\": \"%s\", \"importance\": %f, \"timestamp\": %lld}",
                (long long)exp->id, exp->content, exp->importance, (long long)exp->timestamp);
        first = 0;
        exp = exp->next;
    }
    fprintf(f, "\n  ],\n");

    // Save semantic memories
    fprintf(f, "  \"semantic\": [\n");
    Fact* fact = mem->semantic_head;
    first = 1;
    while (fact) {
        if (!first) fprintf(f, ",\n");
        fprintf(f, "    {\"id\": %lld, \"content\": \"%s\", \"confidence\": %f, \"source\": \"%s\"}",
                (long long)fact->id, fact->content, fact->confidence,
                fact->source ? fact->source : "");
        first = 0;
        fact = fact->next;
    }
    fprintf(f, "\n  ],\n");

    // Save procedural memories
    fprintf(f, "  \"procedural\": [\n");
    AnimaProcedure* proc = mem->procedural_head;
    first = 1;
    while (proc) {
        if (!first) fprintf(f, ",\n");
        fprintf(f, "    {\"id\": %lld, \"name\": \"%s\", \"steps\": %s, \"success_rate\": %f}",
                (long long)proc->id, proc->name,
                proc->steps_json ? proc->steps_json : "[]", proc->success_rate);
        first = 0;
        proc = proc->next;
    }
    fprintf(f, "\n  ],\n");

    // Save beliefs
    fprintf(f, "  \"beliefs\": [\n");
    AnimaBelief* belief = mem->beliefs_head;
    first = 1;
    while (belief) {
        if (!first) fprintf(f, ",\n");
        fprintf(f, "    {\"id\": %lld, \"content\": \"%s\", \"confidence\": %f, \"evidence\": %s}",
                (long long)belief->id, belief->content, belief->confidence,
                belief->evidence_json ? belief->evidence_json : "[]");
        first = 0;
        belief = belief->next;
    }
    fprintf(f, "\n  ],\n");

    // Save metadata
    fprintf(f, "  \"next_id\": %lld,\n", (long long)mem->next_id);
    fprintf(f, "  \"working_capacity\": %lld,\n", (long long)mem->working_capacity);
    fprintf(f, "  \"episodic_count\": %lld,\n", (long long)mem->episodic_count);
    fprintf(f, "  \"semantic_count\": %lld,\n", (long long)mem->semantic_count);
    fprintf(f, "  \"beliefs_count\": %lld\n", (long long)mem->beliefs_count);

    fprintf(f, "}\n");

    pthread_mutex_unlock(&mem->lock);
    fclose(f);
    return 1;
}

// Helper: Simple JSON string extraction (finds "key": "value" and returns value)
static char* json_get_string(const char* json, const char* key) {
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    char* pos = strstr(json, pattern);
    if (!pos) return NULL;
    pos += strlen(pattern);
    while (*pos == ' ' || *pos == '\t') pos++;
    if (*pos != '"') return NULL;
    pos++;  // skip opening quote
    char* end = strchr(pos, '"');
    if (!end) return NULL;
    size_t len = end - pos;
    char* result = (char*)malloc(len + 1);
    strncpy(result, pos, len);
    result[len] = '\0';
    return result;
}

// Helper: Simple JSON number extraction
static double json_get_number(const char* json, const char* key) {
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    char* pos = strstr(json, pattern);
    if (!pos) return 0.0;
    pos += strlen(pattern);
    while (*pos == ' ' || *pos == '\t') pos++;
    return atof(pos);
}

// Helper: Simple JSON integer extraction
static int64_t json_get_int(const char* json, const char* key) {
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    char* pos = strstr(json, pattern);
    if (!pos) return 0;
    pos += strlen(pattern);
    while (*pos == ' ' || *pos == '\t') pos++;
    return atoll(pos);
}

// Helper: Find matching closing bracket (handles nested brackets and strings)
static char* find_matching_bracket(char* start) {
    if (!start || *start != '[') return NULL;
    int depth = 1;
    char* pos = start + 1;
    int in_string = 0;
    while (*pos && depth > 0) {
        if (*pos == '"' && (pos == start + 1 || *(pos-1) != '\\')) {
            in_string = !in_string;
        } else if (!in_string) {
            if (*pos == '[') depth++;
            else if (*pos == ']') depth--;
        }
        if (depth > 0) pos++;
    }
    return (depth == 0) ? pos : NULL;
}

// Helper: Find matching closing brace (handles nested objects/arrays/strings)
static char* find_matching_brace(char* start) {
    if (!start || *start != '{') return NULL;
    int depth = 1;
    char* pos = start + 1;
    int in_string = 0;
    while (*pos && depth > 0) {
        if (*pos == '"' && (pos == start + 1 || *(pos-1) != '\\')) {
            in_string = !in_string;
        } else if (!in_string) {
            if (*pos == '{') depth++;
            else if (*pos == '}') depth--;
        }
        if (depth > 0) pos++;
    }
    return (depth == 0) ? pos : NULL;
}

// Load anima memory from JSON file
int64_t anima_load(int64_t path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path) return 0;

    FILE* f = fopen(path->data, "r");
    if (!f) return 0;

    // Read entire file
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* json = (char*)malloc(size + 1);
    fread(json, 1, size, f);
    json[size] = '\0';
    fclose(f);

    // Create new memory system
    int64_t capacity = json_get_int(json, "working_capacity");
    if (capacity <= 0) capacity = 10;
    AnimaMemory* mem = (AnimaMemory*)calloc(1, sizeof(AnimaMemory));
    mem->working_capacity = capacity;
    mem->working = (void**)calloc(mem->working_capacity, sizeof(void*));
    mem->next_id = json_get_int(json, "next_id");
    if (mem->next_id <= 0) mem->next_id = 1;
    pthread_mutex_init(&mem->lock, NULL);

    // Parse episodic memories
    char* episodic_start = strstr(json, "\"episodic\":");
    if (episodic_start) {
        char* array_start = strchr(episodic_start, '[');
        char* array_end = strchr(episodic_start, ']');
        if (array_start && array_end) {
            char* obj = strchr(array_start, '{');
            while (obj && *obj == '{' && obj < array_end) {
                char* obj_end = strchr(obj, '}');
                if (!obj_end || obj_end > array_end) break;
                size_t obj_len = obj_end - obj + 1;
                char* obj_str = (char*)malloc(obj_len + 1);
                strncpy(obj_str, obj, obj_len);
                obj_str[obj_len] = '\0';

                Experience* exp = (Experience*)malloc(sizeof(Experience));
                exp->id = json_get_int(obj_str, "id");
                exp->content = json_get_string(obj_str, "content");
                exp->importance = json_get_number(obj_str, "importance");
                exp->timestamp = json_get_int(obj_str, "timestamp");
                exp->next = mem->episodic_head;
                mem->episodic_head = exp;
                mem->episodic_count++;

                free(obj_str);
                obj = strchr(obj_end, '{');
            }
        }
    }

    // Parse semantic memories
    char* semantic_start = strstr(json, "\"semantic\":");
    if (semantic_start) {
        char* array_start = strchr(semantic_start, '[');
        char* array_end = strchr(semantic_start, ']');
        if (array_start && array_end) {
            char* obj = strchr(array_start, '{');
            while (obj && *obj == '{' && obj < array_end) {
                char* obj_end = strchr(obj, '}');
                if (!obj_end || obj_end > array_end) break;
                size_t obj_len = obj_end - obj + 1;
                char* obj_str = (char*)malloc(obj_len + 1);
                strncpy(obj_str, obj, obj_len);
                obj_str[obj_len] = '\0';

                Fact* fact = (Fact*)malloc(sizeof(Fact));
                fact->id = json_get_int(obj_str, "id");
                fact->content = json_get_string(obj_str, "content");
                fact->confidence = json_get_number(obj_str, "confidence");
                fact->source = json_get_string(obj_str, "source");
                fact->next = mem->semantic_head;
                mem->semantic_head = fact;
                mem->semantic_count++;

                free(obj_str);
                obj = strchr(obj_end, '{');
            }
        }
    }

    // Parse beliefs (using proper bracket matching for nested evidence arrays)
    char* beliefs_start = strstr(json, "\"beliefs\":");
    if (beliefs_start) {
        char* array_start = strchr(beliefs_start, '[');
        char* array_end = array_start ? find_matching_bracket(array_start) : NULL;
        if (array_start && array_end) {
            char* obj = strchr(array_start, '{');
            while (obj && *obj == '{' && obj < array_end) {
                char* obj_end = find_matching_brace(obj);
                if (!obj_end || obj_end > array_end) break;
                size_t obj_len = obj_end - obj + 1;
                char* obj_str = (char*)malloc(obj_len + 1);
                strncpy(obj_str, obj, obj_len);
                obj_str[obj_len] = '\0';

                AnimaBelief* belief = (AnimaBelief*)malloc(sizeof(AnimaBelief));
                belief->id = json_get_int(obj_str, "id");
                belief->content = json_get_string(obj_str, "content");
                belief->confidence = json_get_number(obj_str, "confidence");
                belief->evidence_json = NULL;  // Simplified for now
                belief->next = mem->beliefs_head;
                mem->beliefs_head = belief;
                mem->beliefs_count++;

                free(obj_str);
                obj = strchr(obj_end, '{');
            }
        }
    }

    free(json);
    return (int64_t)mem;
}

// Check if anima save file exists
int64_t anima_exists(int64_t path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path) return 0;
    FILE* f = fopen(path->data, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

// --------------------------------------------------------------------------
// 29.7 Tool Registry
// --------------------------------------------------------------------------

typedef struct Tool {
    char* name;
    char* description;
    char* schema_json;
    int64_t (*handler)(int64_t args);
} Tool;

typedef struct ToolRegistry {
    Tool** tools;
    int count;
    int capacity;
    pthread_mutex_t lock;
} ToolRegistry;

// Create tool registry
int64_t tool_registry_new(void) {
    ToolRegistry* reg = (ToolRegistry*)malloc(sizeof(ToolRegistry));
    if (!reg) return 0;
    
    reg->capacity = 32;
    reg->tools = (Tool**)calloc(reg->capacity, sizeof(Tool*));
    reg->count = 0;
    pthread_mutex_init(&reg->lock, NULL);
    
    return (int64_t)reg;
}

// Register tool
int64_t tool_register(int64_t reg_ptr, int64_t name_ptr, int64_t desc_ptr, int64_t schema_ptr) {
    ToolRegistry* reg = (ToolRegistry*)reg_ptr;
    SxString* name = (SxString*)name_ptr;
    SxString* desc = (SxString*)desc_ptr;
    SxString* schema = (SxString*)schema_ptr;
    if (!reg || !name || !desc) return -1;
    
    pthread_mutex_lock(&reg->lock);
    
    if (reg->count >= reg->capacity) {
        reg->capacity *= 2;
        reg->tools = (Tool**)realloc(reg->tools, reg->capacity * sizeof(Tool*));
    }
    
    Tool* tool = (Tool*)malloc(sizeof(Tool));
    tool->name = strdup(name->data);
    tool->description = strdup(desc->data);
    tool->schema_json = schema ? strdup(schema->data) : NULL;
    tool->handler = NULL;
    
    reg->tools[reg->count] = tool;
    int64_t id = reg->count;
    reg->count++;
    
    pthread_mutex_unlock(&reg->lock);
    return id;
}

// Get tool by name
int64_t tool_get(int64_t reg_ptr, int64_t name_ptr) {
    ToolRegistry* reg = (ToolRegistry*)reg_ptr;
    SxString* name = (SxString*)name_ptr;
    if (!reg || !name) return -1;
    
    pthread_mutex_lock(&reg->lock);
    
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->tools[i]->name, name->data) == 0) {
            pthread_mutex_unlock(&reg->lock);
            return i;
        }
    }
    
    pthread_mutex_unlock(&reg->lock);
    return -1;
}

// Get tool count
int64_t tool_count(int64_t reg_ptr) {
    ToolRegistry* reg = (ToolRegistry*)reg_ptr;
    return reg ? reg->count : 0;
}

// Close registry
// List all tools (returns vector of tool names)
int64_t tool_list(int64_t reg_ptr) {
    ToolRegistry* reg = (ToolRegistry*)reg_ptr;
    if (!reg) return 0;

    pthread_mutex_lock(&reg->lock);

    SxVec* names = intrinsic_vec_new();
    for (int i = 0; i < reg->count; i++) {
        Tool* t = reg->tools[i];
        intrinsic_vec_push(names, intrinsic_string_new(t->name));
    }

    pthread_mutex_unlock(&reg->lock);
    return (int64_t)names;
}

// Invoke tool by name
int64_t tool_invoke(int64_t reg_ptr, int64_t name_ptr, int64_t args_ptr) {
    ToolRegistry* reg = (ToolRegistry*)reg_ptr;
    SxString* name = (SxString*)name_ptr;
    SxString* args = (SxString*)args_ptr;
    if (!reg || !name) return 0;

    pthread_mutex_lock(&reg->lock);

    // Find tool
    Tool* tool = NULL;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->tools[i]->name, name->data) == 0) {
            tool = reg->tools[i];
            break;
        }
    }

    pthread_mutex_unlock(&reg->lock);

    if (!tool) return 0;

    // Mock invocation - return tool description + args
    char result[512];
    snprintf(result, sizeof(result), "Invoked %s with args: %s",
             tool->name, args ? args->data : "none");
    return (int64_t)intrinsic_string_new(result);
}

void tool_registry_close(int64_t reg_ptr) {
    ToolRegistry* reg = (ToolRegistry*)reg_ptr;
    if (!reg) return;

    pthread_mutex_lock(&reg->lock);

    for (int i = 0; i < reg->count; i++) {
        Tool* t = reg->tools[i];
        free(t->name);
        free(t->description);
        if (t->schema_json) free(t->schema_json);
        free(t);
    }
    free(reg->tools);

    pthread_mutex_unlock(&reg->lock);
    pthread_mutex_destroy(&reg->lock);
    free(reg);
}

// --------------------------------------------------------------------------
// 29.7.1 Tool Result Type
// --------------------------------------------------------------------------

typedef struct ToolResult {
    int success;      // 1 = success, 0 = error
    char* output;     // Result or error message
    int64_t data;     // Optional structured data (e.g., file content)
} ToolResult;

int64_t tool_result_new(int success, const char* output) {
    ToolResult* r = (ToolResult*)malloc(sizeof(ToolResult));
    r->success = success;
    r->output = output ? strdup(output) : NULL;
    r->data = 0;
    return (int64_t)r;
}

int64_t tool_result_success(ToolResult* r) {
    return r ? r->success : 0;
}

int64_t tool_result_output(int64_t res_ptr) {
    ToolResult* r = (ToolResult*)res_ptr;
    if (!r || !r->output) return 0;
    return (int64_t)intrinsic_string_new(r->output);
}

void tool_result_free(int64_t res_ptr) {
    ToolResult* r = (ToolResult*)res_ptr;
    if (!r) return;
    if (r->output) free(r->output);
    free(r);
}

// --------------------------------------------------------------------------
// 29.7.2 Built-in Tool Handlers
// --------------------------------------------------------------------------

// Parse simple JSON args - get string value for key
static char* tool_args_get_string(const char* args, const char* key) {
    if (!args || !key) return NULL;
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    char* pos = strstr(args, pattern);
    if (!pos) return NULL;
    pos += strlen(pattern);
    while (*pos == ' ' || *pos == '\t') pos++;
    if (*pos != '"') return NULL;
    pos++;  // Skip opening quote
    char* end = strchr(pos, '"');
    if (!end) return NULL;
    size_t len = end - pos;
    char* result = (char*)malloc(len + 1);
    strncpy(result, pos, len);
    result[len] = '\0';
    return result;
}

// Built-in: file_read
static int64_t builtin_file_read(const char* args) {
    char* path = tool_args_get_string(args, "path");
    if (!path) {
        return tool_result_new(0, "Error: 'path' argument required");
    }

    FILE* f = fopen(path, "r");
    if (!f) {
        char err[512];
        snprintf(err, sizeof(err), "Error: Cannot open file '%s'", path);
        free(path);
        return tool_result_new(0, err);
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Limit to 1MB for safety
    if (size > 1024 * 1024) {
        fclose(f);
        free(path);
        return tool_result_new(0, "Error: File too large (>1MB)");
    }

    char* content = (char*)malloc(size + 1);
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);
    free(path);

    int64_t result = tool_result_new(1, content);
    free(content);
    return result;
}

// Built-in: file_write
static int64_t builtin_file_write(const char* args) {
    char* path = tool_args_get_string(args, "path");
    char* content = tool_args_get_string(args, "content");

    if (!path) {
        if (content) free(content);
        return tool_result_new(0, "Error: 'path' argument required");
    }
    if (!content) {
        free(path);
        return tool_result_new(0, "Error: 'content' argument required");
    }

    FILE* f = fopen(path, "w");
    if (!f) {
        char err[512];
        snprintf(err, sizeof(err), "Error: Cannot write to file '%s'", path);
        free(path);
        free(content);
        return tool_result_new(0, err);
    }

    fputs(content, f);
    fclose(f);

    char msg[512];
    snprintf(msg, sizeof(msg), "Successfully wrote %zu bytes to '%s'", strlen(content), path);
    free(path);
    free(content);
    return tool_result_new(1, msg);
}

// Built-in: file_append
static int64_t builtin_file_append(const char* args) {
    char* path = tool_args_get_string(args, "path");
    char* content = tool_args_get_string(args, "content");

    if (!path) {
        if (content) free(content);
        return tool_result_new(0, "Error: 'path' argument required");
    }
    if (!content) {
        free(path);
        return tool_result_new(0, "Error: 'content' argument required");
    }

    FILE* f = fopen(path, "a");
    if (!f) {
        char err[512];
        snprintf(err, sizeof(err), "Error: Cannot append to file '%s'", path);
        free(path);
        free(content);
        return tool_result_new(0, err);
    }

    fputs(content, f);
    fclose(f);

    char msg[512];
    snprintf(msg, sizeof(msg), "Successfully appended %zu bytes to '%s'", strlen(content), path);
    free(path);
    free(content);
    return tool_result_new(1, msg);
}

// Built-in: list_dir
static int64_t builtin_list_dir(const char* args) {
    char* path = tool_args_get_string(args, "path");
    if (!path) {
        path = strdup(".");  // Default to current directory
    }

    DIR* d = opendir(path);
    if (!d) {
        char err[512];
        snprintf(err, sizeof(err), "Error: Cannot open directory '%s'", path);
        free(path);
        return tool_result_new(0, err);
    }

    // Build result as JSON array
    char* result = (char*)malloc(32768);
    strcpy(result, "[");
    int first = 1;

    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        if (!first) strcat(result, ", ");
        strcat(result, "\"");
        strcat(result, ent->d_name);
        strcat(result, "\"");
        first = 0;
    }
    strcat(result, "]");

    closedir(d);
    free(path);

    int64_t res = tool_result_new(1, result);
    free(result);
    return res;
}

// Built-in: shell_exec
static int64_t builtin_shell_exec(const char* args) {
    char* command = tool_args_get_string(args, "command");
    if (!command) {
        return tool_result_new(0, "Error: 'command' argument required");
    }

    // Open pipe to command
    FILE* pipe = popen(command, "r");
    if (!pipe) {
        char err[512];
        snprintf(err, sizeof(err), "Error: Cannot execute command '%s'", command);
        free(command);
        return tool_result_new(0, err);
    }

    // Read output
    char* output = (char*)malloc(65536);
    output[0] = '\0';
    size_t total = 0;
    char buf[4096];

    while (fgets(buf, sizeof(buf), pipe)) {
        size_t len = strlen(buf);
        if (total + len < 65536 - 1) {
            strcat(output, buf);
            total += len;
        }
    }

    int status = pclose(pipe);
    free(command);

    int success = (status == 0) ? 1 : 0;
    int64_t res = tool_result_new(success, output);
    free(output);
    return res;
}

// Built-in: file_exists
static int64_t builtin_file_exists(const char* args) {
    char* path = tool_args_get_string(args, "path");
    if (!path) {
        return tool_result_new(0, "Error: 'path' argument required");
    }

    FILE* f = fopen(path, "r");
    if (f) {
        fclose(f);
        free(path);
        return tool_result_new(1, "true");
    }
    free(path);
    return tool_result_new(1, "false");
}

// Built-in: file_delete
static int64_t builtin_file_delete(const char* args) {
    char* path = tool_args_get_string(args, "path");
    if (!path) {
        return tool_result_new(0, "Error: 'path' argument required");
    }

    if (remove(path) == 0) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Successfully deleted '%s'", path);
        free(path);
        return tool_result_new(1, msg);
    } else {
        char err[512];
        snprintf(err, sizeof(err), "Error: Cannot delete '%s'", path);
        free(path);
        return tool_result_new(0, err);
    }
}

// --------------------------------------------------------------------------
// 29.7.3 Tool Execution with Handlers
// --------------------------------------------------------------------------

// Built-in tool handler type
typedef int64_t (*BuiltinToolHandler)(const char* args);

// Struct mapping tool names to handlers
typedef struct BuiltinTool {
    const char* name;
    const char* description;
    const char* schema;
    BuiltinToolHandler handler;
} BuiltinTool;

// Table of built-in tools
static BuiltinTool builtin_tools[] = {
    {
        "file_read",
        "Read contents of a file",
        "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"File path to read\"}},\"required\":[\"path\"]}",
        builtin_file_read
    },
    {
        "file_write",
        "Write content to a file (overwrites)",
        "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"content\":{\"type\":\"string\"}},\"required\":[\"path\",\"content\"]}",
        builtin_file_write
    },
    {
        "file_append",
        "Append content to a file",
        "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"content\":{\"type\":\"string\"}},\"required\":[\"path\",\"content\"]}",
        builtin_file_append
    },
    {
        "file_exists",
        "Check if a file exists",
        "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}",
        builtin_file_exists
    },
    {
        "file_delete",
        "Delete a file",
        "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}",
        builtin_file_delete
    },
    {
        "list_dir",
        "List contents of a directory",
        "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"Directory path (default: current)\"}}}",
        builtin_list_dir
    },
    {
        "shell_exec",
        "Execute a shell command",
        "{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\"}},\"required\":[\"command\"]}",
        builtin_shell_exec
    },
    {NULL, NULL, NULL, NULL}  // Terminator
};

// Find built-in handler
static BuiltinToolHandler find_builtin_handler(const char* name) {
    for (int i = 0; builtin_tools[i].name != NULL; i++) {
        if (strcmp(builtin_tools[i].name, name) == 0) {
            return builtin_tools[i].handler;
        }
    }
    return NULL;
}

// Execute tool with real handlers
int64_t tool_execute(int64_t reg_ptr, int64_t name_ptr, int64_t args_ptr) {
    ToolRegistry* reg = (ToolRegistry*)reg_ptr;
    SxString* name = (SxString*)name_ptr;
    SxString* args = (SxString*)args_ptr;

    if (!name) {
        return tool_result_new(0, "Error: Tool name required");
    }

    // First check built-in handlers
    BuiltinToolHandler handler = find_builtin_handler(name->data);
    if (handler) {
        return handler(args ? args->data : "{}");
    }

    // Check registry for custom tools
    if (reg) {
        pthread_mutex_lock(&reg->lock);
        for (int i = 0; i < reg->count; i++) {
            if (strcmp(reg->tools[i]->name, name->data) == 0) {
                Tool* tool = reg->tools[i];
                pthread_mutex_unlock(&reg->lock);
                if (tool->handler) {
                    return tool->handler(args_ptr);
                }
                // Tool exists but has no handler
                char msg[256];
                snprintf(msg, sizeof(msg), "Tool '%s' registered but has no handler", name->data);
                return tool_result_new(0, msg);
            }
        }
        pthread_mutex_unlock(&reg->lock);
    }

    char err[256];
    snprintf(err, sizeof(err), "Error: Unknown tool '%s'", name->data);
    return tool_result_new(0, err);
}

// Register all built-in tools into a registry
int64_t tool_register_builtins(int64_t reg_ptr) {
    ToolRegistry* reg = (ToolRegistry*)reg_ptr;
    if (!reg) return 0;

    int count = 0;
    for (int i = 0; builtin_tools[i].name != NULL; i++) {
        SxString* name = intrinsic_string_new(builtin_tools[i].name);
        SxString* desc = intrinsic_string_new(builtin_tools[i].description);
        SxString* schema = intrinsic_string_new(builtin_tools[i].schema);
        tool_register(reg_ptr, (int64_t)name, (int64_t)desc, (int64_t)schema);
        count++;
    }
    return count;
}

// Get tool schema (for LLM consumption)
int64_t tool_get_schema(int64_t reg_ptr, int64_t name_ptr) {
    SxString* name = (SxString*)name_ptr;
    if (!name) return 0;

    // Check built-ins first
    for (int i = 0; builtin_tools[i].name != NULL; i++) {
        if (strcmp(builtin_tools[i].name, name->data) == 0) {
            return (int64_t)intrinsic_string_new(builtin_tools[i].schema);
        }
    }

    // Check registry
    ToolRegistry* reg = (ToolRegistry*)reg_ptr;
    if (reg) {
        pthread_mutex_lock(&reg->lock);
        for (int i = 0; i < reg->count; i++) {
            if (strcmp(reg->tools[i]->name, name->data) == 0) {
                char* schema = reg->tools[i]->schema_json;
                pthread_mutex_unlock(&reg->lock);
                return schema ? (int64_t)intrinsic_string_new(schema) : 0;
            }
        }
        pthread_mutex_unlock(&reg->lock);
    }

    return 0;
}

// Get all tool schemas as JSON array (for LLM consumption)
int64_t tool_get_all_schemas(int64_t reg_ptr) {
    char* result = (char*)malloc(65536);
    strcpy(result, "[");
    int first = 1;

    // Add built-in tools
    for (int i = 0; builtin_tools[i].name != NULL; i++) {
        if (!first) strcat(result, ",");
        strcat(result, "\n  {\"name\":\"");
        strcat(result, builtin_tools[i].name);
        strcat(result, "\",\"description\":\"");
        strcat(result, builtin_tools[i].description);
        strcat(result, "\",\"parameters\":");
        strcat(result, builtin_tools[i].schema);
        strcat(result, "}");
        first = 0;
    }

    // Add registry tools
    ToolRegistry* reg = (ToolRegistry*)reg_ptr;
    if (reg) {
        pthread_mutex_lock(&reg->lock);
        for (int i = 0; i < reg->count; i++) {
            Tool* t = reg->tools[i];
            // Skip if it's a built-in (already added)
            if (find_builtin_handler(t->name)) continue;

            if (!first) strcat(result, ",");
            strcat(result, "\n  {\"name\":\"");
            strcat(result, t->name);
            strcat(result, "\",\"description\":\"");
            strcat(result, t->description);
            strcat(result, "\",\"parameters\":");
            strcat(result, t->schema_json ? t->schema_json : "{}");
            strcat(result, "}");
            first = 0;
        }
        pthread_mutex_unlock(&reg->lock);
    }

    strcat(result, "\n]");
    int64_t res = (int64_t)intrinsic_string_new(result);
    free(result);
    return res;
}

// ============================================================================
// Phase 4: Multi-Actor Orchestration
// ============================================================================

// --------------------------------------------------------------------------
// 4.1 AI-Powered Actor Definition
// --------------------------------------------------------------------------

typedef struct AIActorConfig {
    char* name;
    char* role;             // Actor's role/purpose
    int64_t tools;          // ToolRegistry pointer
    int64_t memory;         // AnimaMemory pointer
    int64_t specialist;     // Specialist configuration
    int timeout_ms;         // Timeout for operations
    int max_retries;        // Retry count for failures
} AIActorConfig;

typedef struct AIActor {
    int64_t id;
    AIActorConfig config;
    int64_t mailbox;        // Message queue
    int64_t history;        // Conversation history (Vec of messages)
    int status;             // 0=idle, 1=busy, 2=error, 3=stopped
    pthread_mutex_t lock;
    pthread_t thread;
    int64_t parent;         // Parent actor for supervision
    int64_t last_heartbeat;
} AIActor;

typedef struct AIActorSystem {
    AIActor** actors;
    int count;
    int capacity;
    pthread_mutex_t lock;
    int next_id;
} AIActorSystem;

// Global actor system
static AIActorSystem* global_ai_system = NULL;

// Initialize actor system
int64_t ai_actor_system_new(void) {
    AIActorSystem* sys = (AIActorSystem*)calloc(1, sizeof(AIActorSystem));
    sys->capacity = 64;
    sys->actors = (AIActor**)calloc(sys->capacity, sizeof(AIActor*));
    sys->count = 0;
    sys->next_id = 1;
    pthread_mutex_init(&sys->lock, NULL);
    global_ai_system = sys;
    return (int64_t)sys;
}

// Create AI actor configuration
int64_t ai_actor_config_new(int64_t name_ptr, int64_t role_ptr) {
    AIActorConfig* config = (AIActorConfig*)calloc(1, sizeof(AIActorConfig));
    SxString* name = (SxString*)name_ptr;
    SxString* role = (SxString*)role_ptr;

    config->name = name ? strdup(name->data) : strdup("unnamed");
    config->role = role ? strdup(role->data) : strdup("general");
    config->timeout_ms = 30000;  // 30 second default
    config->max_retries = 3;

    return (int64_t)config;
}

// Set tools for actor config
void ai_actor_config_set_tools(int64_t config_ptr, int64_t tools_ptr) {
    AIActorConfig* config = (AIActorConfig*)config_ptr;
    if (config) config->tools = tools_ptr;
}

// Set memory for actor config
void ai_actor_config_set_memory(int64_t config_ptr, int64_t memory_ptr) {
    AIActorConfig* config = (AIActorConfig*)config_ptr;
    if (config) config->memory = memory_ptr;
}

// Set specialist for actor config
void ai_actor_config_set_specialist(int64_t config_ptr, int64_t specialist_ptr) {
    AIActorConfig* config = (AIActorConfig*)config_ptr;
    if (config) config->specialist = specialist_ptr;
}

// Set timeout
void ai_actor_config_set_timeout(int64_t config_ptr, int64_t timeout_ms) {
    AIActorConfig* config = (AIActorConfig*)config_ptr;
    if (config) config->timeout_ms = (int)timeout_ms;
}

// Spawn AI actor
int64_t ai_actor_spawn(int64_t sys_ptr, int64_t config_ptr) {
    AIActorSystem* sys = (AIActorSystem*)sys_ptr;
    AIActorConfig* config = (AIActorConfig*)config_ptr;
    if (!sys || !config) return 0;

    pthread_mutex_lock(&sys->lock);

    if (sys->count >= sys->capacity) {
        sys->capacity *= 2;
        sys->actors = (AIActor**)realloc(sys->actors, sys->capacity * sizeof(AIActor*));
    }

    AIActor* actor = (AIActor*)calloc(1, sizeof(AIActor));
    actor->id = sys->next_id++;
    actor->config = *config;
    actor->config.name = strdup(config->name);
    actor->config.role = strdup(config->role);
    actor->history = (int64_t)intrinsic_vec_new();
    actor->status = 0;  // idle
    actor->last_heartbeat = time(NULL);
    pthread_mutex_init(&actor->lock, NULL);

    sys->actors[sys->count++] = actor;
    int64_t id = actor->id;

    pthread_mutex_unlock(&sys->lock);
    return id;
}

// Get actor by ID
static AIActor* get_ai_actor(AIActorSystem* sys, int64_t id) {
    if (!sys) return NULL;
    for (int i = 0; i < sys->count; i++) {
        if (sys->actors[i] && sys->actors[i]->id == id) {
            return sys->actors[i];
        }
    }
    return NULL;
}

// Get actor status
int64_t ai_actor_status(int64_t sys_ptr, int64_t actor_id) {
    AIActorSystem* sys = (AIActorSystem*)sys_ptr;
    AIActor* actor = get_ai_actor(sys, actor_id);
    return actor ? actor->status : -1;
}

// Get actor name
int64_t ai_actor_name(int64_t sys_ptr, int64_t actor_id) {
    AIActorSystem* sys = (AIActorSystem*)sys_ptr;
    AIActor* actor = get_ai_actor(sys, actor_id);
    if (!actor) return 0;
    return (int64_t)intrinsic_string_new(actor->config.name);
}

// Stop actor
void ai_actor_stop(int64_t sys_ptr, int64_t actor_id) {
    AIActorSystem* sys = (AIActorSystem*)sys_ptr;
    AIActor* actor = get_ai_actor(sys, actor_id);
    if (actor) {
        pthread_mutex_lock(&actor->lock);
        actor->status = 3;  // stopped
        pthread_mutex_unlock(&actor->lock);
    }
}

// --------------------------------------------------------------------------
// 4.2 Conversation History
// --------------------------------------------------------------------------

typedef struct ConversationMessage {
    char* role;     // "user", "assistant", "system"
    char* content;
    int64_t timestamp;
} ConversationMessage;

// Add message to actor's history
int64_t ai_actor_add_message(int64_t sys_ptr, int64_t actor_id, int64_t role_ptr, int64_t content_ptr) {
    AIActorSystem* sys = (AIActorSystem*)sys_ptr;
    AIActor* actor = get_ai_actor(sys, actor_id);
    if (!actor) return 0;

    SxString* role = (SxString*)role_ptr;
    SxString* content = (SxString*)content_ptr;

    ConversationMessage* msg = (ConversationMessage*)malloc(sizeof(ConversationMessage));
    msg->role = role ? strdup(role->data) : strdup("user");
    msg->content = content ? strdup(content->data) : strdup("");
    msg->timestamp = time(NULL);

    pthread_mutex_lock(&actor->lock);
    intrinsic_vec_push((SxVec*)actor->history, (void*)msg);
    pthread_mutex_unlock(&actor->lock);

    return 1;
}

// Get conversation history length
int64_t ai_actor_history_len(int64_t sys_ptr, int64_t actor_id) {
    AIActorSystem* sys = (AIActorSystem*)sys_ptr;
    AIActor* actor = get_ai_actor(sys, actor_id);
    if (!actor) return 0;
    return intrinsic_vec_len((SxVec*)actor->history);
}

// Get message from history
int64_t ai_actor_get_message(int64_t sys_ptr, int64_t actor_id, int64_t index) {
    AIActorSystem* sys = (AIActorSystem*)sys_ptr;
    AIActor* actor = get_ai_actor(sys, actor_id);
    if (!actor) return 0;

    ConversationMessage* msg = (ConversationMessage*)intrinsic_vec_get((SxVec*)actor->history, index);
    if (!msg) return 0;

    // Return as JSON
    char* json = (char*)malloc(strlen(msg->content) + 256);
    sprintf(json, "{\"role\":\"%s\",\"content\":\"%s\",\"timestamp\":%ld}",
            msg->role, msg->content, (long)msg->timestamp);
    int64_t result = (int64_t)intrinsic_string_new(json);
    free(json);
    return result;
}

// Clear history
void ai_actor_clear_history(int64_t sys_ptr, int64_t actor_id) {
    AIActorSystem* sys = (AIActorSystem*)sys_ptr;
    AIActor* actor = get_ai_actor(sys, actor_id);
    if (!actor) return;

    pthread_mutex_lock(&actor->lock);
    SxVec* history = (SxVec*)actor->history;
    for (size_t i = 0; i < history->len; i++) {
        ConversationMessage* msg = (ConversationMessage*)history->items[i];
        if (msg) {
            free(msg->role);
            free(msg->content);
            free(msg);
        }
    }
    history->len = 0;
    pthread_mutex_unlock(&actor->lock);
}

// --------------------------------------------------------------------------
// 4.3 Communication Patterns: Pipeline
// --------------------------------------------------------------------------

typedef struct PipelineStage {
    int64_t actor_id;
    char* transform_fn;     // Function name to apply
    struct PipelineStage* next;
} PipelineStage;

typedef struct Pipeline {
    char* name;
    PipelineStage* head;
    PipelineStage* tail;
    int stage_count;
    pthread_mutex_t lock;
} Pipeline;

// Create pipeline
int64_t pipeline_new(int64_t name_ptr) {
    Pipeline* p = (Pipeline*)calloc(1, sizeof(Pipeline));
    SxString* name = (SxString*)name_ptr;
    p->name = name ? strdup(name->data) : strdup("pipeline");
    pthread_mutex_init(&p->lock, NULL);
    return (int64_t)p;
}

// Add stage to pipeline
int64_t pipeline_add_stage(int64_t pipeline_ptr, int64_t actor_id, int64_t transform_ptr) {
    Pipeline* p = (Pipeline*)pipeline_ptr;
    if (!p) return 0;

    SxString* transform = (SxString*)transform_ptr;

    PipelineStage* stage = (PipelineStage*)calloc(1, sizeof(PipelineStage));
    stage->actor_id = actor_id;
    stage->transform_fn = transform ? strdup(transform->data) : NULL;

    pthread_mutex_lock(&p->lock);
    if (!p->head) {
        p->head = p->tail = stage;
    } else {
        p->tail->next = stage;
        p->tail = stage;
    }
    p->stage_count++;
    pthread_mutex_unlock(&p->lock);

    return p->stage_count;
}

// Execute pipeline
int64_t pipeline_execute(int64_t sys_ptr, int64_t pipeline_ptr, int64_t input_ptr) {
    AIActorSystem* sys = (AIActorSystem*)sys_ptr;
    Pipeline* p = (Pipeline*)pipeline_ptr;
    SxString* input = (SxString*)input_ptr;

    if (!sys || !p || !input) return 0;

    char* current = strdup(input->data);

    pthread_mutex_lock(&p->lock);
    PipelineStage* stage = p->head;

    while (stage) {
        AIActor* actor = get_ai_actor(sys, stage->actor_id);
        if (!actor) {
            pthread_mutex_unlock(&p->lock);
            free(current);
            return (int64_t)intrinsic_string_new("Error: Actor not found in pipeline");
        }

        // Add input as message to actor history
        ai_actor_add_message((int64_t)sys, stage->actor_id,
                            (int64_t)intrinsic_string_new("user"),
                            (int64_t)intrinsic_string_new(current));

        // For now, just pass through (real impl would call specialist)
        // In full implementation: result = specialist_chat(actor->config.specialist, current)
        char* result = (char*)malloc(strlen(current) + 100);
        sprintf(result, "[%s processed: %s]", actor->config.name, current);

        free(current);
        current = result;

        stage = stage->next;
    }

    pthread_mutex_unlock(&p->lock);
    int64_t output = (int64_t)intrinsic_string_new(current);
    free(current);
    return output;
}

// Get pipeline stage count
int64_t pipeline_stage_count(int64_t pipeline_ptr) {
    Pipeline* p = (Pipeline*)pipeline_ptr;
    return p ? p->stage_count : 0;
}

// Close pipeline
void pipeline_close(int64_t pipeline_ptr) {
    Pipeline* p = (Pipeline*)pipeline_ptr;
    if (!p) return;

    PipelineStage* stage = p->head;
    while (stage) {
        PipelineStage* next = stage->next;
        if (stage->transform_fn) free(stage->transform_fn);
        free(stage);
        stage = next;
    }

    free(p->name);
    pthread_mutex_destroy(&p->lock);
    free(p);
}

// --------------------------------------------------------------------------
// 4.4 Communication Patterns: Parallel (Fan-out)
// --------------------------------------------------------------------------

typedef struct ParallelResult {
    int64_t actor_id;
    char* result;
    int success;
    double duration_ms;
} ParallelResult;

typedef struct ParallelGroup {
    char* name;
    int64_t* actor_ids;
    int actor_count;
    int capacity;
    pthread_mutex_t lock;
} ParallelGroup;

// Create parallel group
int64_t parallel_group_new(int64_t name_ptr) {
    ParallelGroup* g = (ParallelGroup*)calloc(1, sizeof(ParallelGroup));
    SxString* name = (SxString*)name_ptr;
    g->name = name ? strdup(name->data) : strdup("parallel");
    g->capacity = 16;
    g->actor_ids = (int64_t*)calloc(g->capacity, sizeof(int64_t));
    pthread_mutex_init(&g->lock, NULL);
    return (int64_t)g;
}

// Add actor to parallel group
int64_t parallel_group_add(int64_t group_ptr, int64_t actor_id) {
    ParallelGroup* g = (ParallelGroup*)group_ptr;
    if (!g) return 0;

    pthread_mutex_lock(&g->lock);
    if (g->actor_count >= g->capacity) {
        g->capacity *= 2;
        g->actor_ids = (int64_t*)realloc(g->actor_ids, g->capacity * sizeof(int64_t));
    }
    g->actor_ids[g->actor_count++] = actor_id;
    pthread_mutex_unlock(&g->lock);

    return g->actor_count;
}

// Execute parallel group (fan-out same input to all actors)
int64_t parallel_group_execute(int64_t sys_ptr, int64_t group_ptr, int64_t input_ptr) {
    AIActorSystem* sys = (AIActorSystem*)sys_ptr;
    ParallelGroup* g = (ParallelGroup*)group_ptr;
    SxString* input = (SxString*)input_ptr;

    if (!sys || !g || !input) return 0;

    // Collect results as JSON array
    char* results = (char*)malloc(65536);
    strcpy(results, "[");
    int first = 1;

    pthread_mutex_lock(&g->lock);
    for (int i = 0; i < g->actor_count; i++) {
        AIActor* actor = get_ai_actor(sys, g->actor_ids[i]);
        if (!actor) continue;

        // Add message to history
        ai_actor_add_message((int64_t)sys, g->actor_ids[i],
                            (int64_t)intrinsic_string_new("user"),
                            (int64_t)intrinsic_string_new(input->data));

        // Mock result (real impl would call specialist in parallel threads)
        if (!first) strcat(results, ",");
        char entry[1024];
        snprintf(entry, sizeof(entry),
                "\n  {\"actor\":\"%s\",\"actor_id\":%ld,\"result\":\"[processed by %s]\"}",
                actor->config.name, (long)g->actor_ids[i], actor->config.name);
        strcat(results, entry);
        first = 0;
    }
    pthread_mutex_unlock(&g->lock);

    strcat(results, "\n]");
    int64_t output = (int64_t)intrinsic_string_new(results);
    free(results);
    return output;
}

// Get group size
int64_t parallel_group_size(int64_t group_ptr) {
    ParallelGroup* g = (ParallelGroup*)group_ptr;
    return g ? g->actor_count : 0;
}

// Close parallel group
void parallel_group_close(int64_t group_ptr) {
    ParallelGroup* g = (ParallelGroup*)group_ptr;
    if (!g) return;
    free(g->name);
    free(g->actor_ids);
    pthread_mutex_destroy(&g->lock);
    free(g);
}

// --------------------------------------------------------------------------
// 4.5 Communication Patterns: Consensus (Voting)
// --------------------------------------------------------------------------

typedef struct ConsensusGroup {
    char* name;
    int64_t* actor_ids;
    int actor_count;
    int capacity;
    double threshold;       // Consensus threshold (0.5 = majority)
    pthread_mutex_t lock;
} ConsensusGroup;

// Create consensus group (threshold as percentage 0-100, e.g., 50 = 50%)
int64_t consensus_group_new(int64_t name_ptr, int64_t threshold_pct) {
    ConsensusGroup* g = (ConsensusGroup*)calloc(1, sizeof(ConsensusGroup));
    SxString* name = (SxString*)name_ptr;
    g->name = name ? strdup(name->data) : strdup("consensus");
    g->threshold = threshold_pct > 0 ? (double)threshold_pct / 100.0 : 0.5;
    g->capacity = 16;
    g->actor_ids = (int64_t*)calloc(g->capacity, sizeof(int64_t));
    pthread_mutex_init(&g->lock, NULL);
    return (int64_t)g;
}

// Add actor to consensus group
int64_t consensus_group_add(int64_t group_ptr, int64_t actor_id) {
    ConsensusGroup* g = (ConsensusGroup*)group_ptr;
    if (!g) return 0;

    pthread_mutex_lock(&g->lock);
    if (g->actor_count >= g->capacity) {
        g->capacity *= 2;
        g->actor_ids = (int64_t*)realloc(g->actor_ids, g->capacity * sizeof(int64_t));
    }
    g->actor_ids[g->actor_count++] = actor_id;
    pthread_mutex_unlock(&g->lock);

    return g->actor_count;
}

// Execute consensus vote
int64_t consensus_group_vote(int64_t sys_ptr, int64_t group_ptr, int64_t question_ptr) {
    AIActorSystem* sys = (AIActorSystem*)sys_ptr;
    ConsensusGroup* g = (ConsensusGroup*)group_ptr;
    SxString* question = (SxString*)question_ptr;

    if (!sys || !g || !question) return 0;

    // Collect votes (mock implementation - real would call specialists)
    int yes_votes = 0;
    int no_votes = 0;

    pthread_mutex_lock(&g->lock);
    for (int i = 0; i < g->actor_count; i++) {
        AIActor* actor = get_ai_actor(sys, g->actor_ids[i]);
        if (!actor) continue;

        // Mock: alternate yes/no based on actor id
        if (g->actor_ids[i] % 2 == 0) {
            yes_votes++;
        } else {
            no_votes++;
        }
    }
    pthread_mutex_unlock(&g->lock);

    int total = yes_votes + no_votes;
    double yes_ratio = total > 0 ? (double)yes_votes / total : 0;
    int consensus_reached = yes_ratio >= g->threshold;

    char* result = (char*)malloc(512);
    snprintf(result, 512,
            "{\"question\":\"%s\",\"yes_votes\":%d,\"no_votes\":%d,\"threshold\":%.2f,"
            "\"yes_ratio\":%.2f,\"consensus\":%s,\"decision\":\"%s\"}",
            question->data, yes_votes, no_votes, g->threshold,
            yes_ratio, consensus_reached ? "true" : "false",
            consensus_reached ? "approved" : "rejected");

    int64_t output = (int64_t)intrinsic_string_new(result);
    free(result);
    return output;
}

// Close consensus group
void consensus_group_close(int64_t group_ptr) {
    ConsensusGroup* g = (ConsensusGroup*)group_ptr;
    if (!g) return;
    free(g->name);
    free(g->actor_ids);
    pthread_mutex_destroy(&g->lock);
    free(g);
}

// --------------------------------------------------------------------------
// 4.6 AI Supervisor Pattern (for AI actor orchestration)
// --------------------------------------------------------------------------

typedef enum AISupervisorStrategy {
    AI_SUPERVISOR_ONE_FOR_ONE = 0,    // Restart only failed child
    AI_SUPERVISOR_ONE_FOR_ALL = 1,    // Restart all children on any failure
    AI_SUPERVISOR_REST_FOR_ONE = 2    // Restart failed and all started after it
} AISupervisorStrategy;

typedef struct AISupervisor {
    int64_t id;
    char* name;
    int64_t* child_ids;
    int child_count;
    int capacity;
    AISupervisorStrategy strategy;
    int max_restarts;
    int restart_window_ms;
    int restart_count;
    int64_t last_restart_time;
    pthread_mutex_t lock;
} AISupervisor;

// Create AI supervisor
int64_t ai_supervisor_new(int64_t name_ptr, int64_t strategy) {
    AISupervisor* s = (AISupervisor*)calloc(1, sizeof(AISupervisor));
    SxString* name = (SxString*)name_ptr;
    s->name = name ? strdup(name->data) : strdup("supervisor");
    s->strategy = (AISupervisorStrategy)strategy;
    s->max_restarts = 3;
    s->restart_window_ms = 5000;
    s->capacity = 16;
    s->child_ids = (int64_t*)calloc(s->capacity, sizeof(int64_t));
    pthread_mutex_init(&s->lock, NULL);
    return (int64_t)s;
}

// Add child to AI supervisor
int64_t ai_supervisor_add_child(int64_t sup_ptr, int64_t actor_id) {
    AISupervisor* s = (AISupervisor*)sup_ptr;
    if (!s) return 0;

    pthread_mutex_lock(&s->lock);
    if (s->child_count >= s->capacity) {
        s->capacity *= 2;
        s->child_ids = (int64_t*)realloc(s->child_ids, s->capacity * sizeof(int64_t));
    }
    s->child_ids[s->child_count++] = actor_id;
    pthread_mutex_unlock(&s->lock);

    return s->child_count;
}

// Check health of all children
int64_t ai_supervisor_check_health(int64_t sys_ptr, int64_t sup_ptr) {
    AIActorSystem* sys = (AIActorSystem*)sys_ptr;
    AISupervisor* s = (AISupervisor*)sup_ptr;
    if (!sys || !s) return 0;

    int healthy = 0;
    int unhealthy = 0;

    pthread_mutex_lock(&s->lock);
    for (int i = 0; i < s->child_count; i++) {
        AIActor* actor = get_ai_actor(sys, s->child_ids[i]);
        if (!actor || actor->status == 2 || actor->status == 3) {
            unhealthy++;
        } else {
            healthy++;
        }
    }
    pthread_mutex_unlock(&s->lock);

    char* result = (char*)malloc(256);
    snprintf(result, 256, "{\"healthy\":%d,\"unhealthy\":%d,\"total\":%d}",
            healthy, unhealthy, healthy + unhealthy);
    int64_t output = (int64_t)intrinsic_string_new(result);
    free(result);
    return output;
}

// Get child count
int64_t ai_supervisor_child_count(int64_t sup_ptr) {
    AISupervisor* s = (AISupervisor*)sup_ptr;
    return s ? s->child_count : 0;
}

// Close AI supervisor
void ai_supervisor_close(int64_t sup_ptr) {
    AISupervisor* s = (AISupervisor*)sup_ptr;
    if (!s) return;
    free(s->name);
    free(s->child_ids);
    pthread_mutex_destroy(&s->lock);
    free(s);
}

// --------------------------------------------------------------------------
// 4.7 Shared Memory Between Actors
// --------------------------------------------------------------------------

typedef struct SharedMemory {
    char* name;
    int64_t memory;         // AnimaMemory pointer
    int64_t* reader_ids;    // Actors with read access
    int64_t* writer_ids;    // Actors with write access
    int reader_count;
    int writer_count;
    int capacity;
    pthread_rwlock_t rwlock;
} SharedMemory;

// Create shared memory
int64_t shared_memory_new(int64_t name_ptr, int64_t capacity) {
    SharedMemory* sm = (SharedMemory*)calloc(1, sizeof(SharedMemory));
    SxString* name = (SxString*)name_ptr;
    sm->name = name ? strdup(name->data) : strdup("shared");
    sm->memory = anima_memory_new(capacity);
    sm->capacity = 16;
    sm->reader_ids = (int64_t*)calloc(sm->capacity, sizeof(int64_t));
    sm->writer_ids = (int64_t*)calloc(sm->capacity, sizeof(int64_t));
    pthread_rwlock_init(&sm->rwlock, NULL);
    return (int64_t)sm;
}

// Grant read access
int64_t shared_memory_grant_read(int64_t sm_ptr, int64_t actor_id) {
    SharedMemory* sm = (SharedMemory*)sm_ptr;
    if (!sm) return 0;

    pthread_rwlock_wrlock(&sm->rwlock);
    if (sm->reader_count >= sm->capacity) {
        sm->capacity *= 2;
        sm->reader_ids = (int64_t*)realloc(sm->reader_ids, sm->capacity * sizeof(int64_t));
        sm->writer_ids = (int64_t*)realloc(sm->writer_ids, sm->capacity * sizeof(int64_t));
    }
    sm->reader_ids[sm->reader_count++] = actor_id;
    pthread_rwlock_unlock(&sm->rwlock);

    return 1;
}

// Grant write access
int64_t shared_memory_grant_write(int64_t sm_ptr, int64_t actor_id) {
    SharedMemory* sm = (SharedMemory*)sm_ptr;
    if (!sm) return 0;

    pthread_rwlock_wrlock(&sm->rwlock);
    if (sm->writer_count >= sm->capacity) {
        sm->capacity *= 2;
        sm->reader_ids = (int64_t*)realloc(sm->reader_ids, sm->capacity * sizeof(int64_t));
        sm->writer_ids = (int64_t*)realloc(sm->writer_ids, sm->capacity * sizeof(int64_t));
    }
    sm->writer_ids[sm->writer_count++] = actor_id;
    pthread_rwlock_unlock(&sm->rwlock);

    return 1;
}

// Check if actor has read access
static int has_read_access(SharedMemory* sm, int64_t actor_id) {
    for (int i = 0; i < sm->reader_count; i++) {
        if (sm->reader_ids[i] == actor_id) return 1;
    }
    for (int i = 0; i < sm->writer_count; i++) {
        if (sm->writer_ids[i] == actor_id) return 1;  // Writers can read
    }
    return 0;
}

// Check if actor has write access
static int has_write_access(SharedMemory* sm, int64_t actor_id) {
    for (int i = 0; i < sm->writer_count; i++) {
        if (sm->writer_ids[i] == actor_id) return 1;
    }
    return 0;
}

// Read from shared memory (with access check)
int64_t shared_memory_recall(int64_t sm_ptr, int64_t actor_id, int64_t goal_ptr, int64_t context_ptr) {
    SharedMemory* sm = (SharedMemory*)sm_ptr;
    if (!sm) return 0;

    pthread_rwlock_rdlock(&sm->rwlock);
    if (!has_read_access(sm, actor_id)) {
        pthread_rwlock_unlock(&sm->rwlock);
        return (int64_t)intrinsic_string_new("Error: No read access");
    }

    int64_t result = anima_recall_for_goal(sm->memory, goal_ptr, context_ptr, 10);
    pthread_rwlock_unlock(&sm->rwlock);
    return result;
}

// Write to shared memory (with access check)
int64_t shared_memory_remember(int64_t sm_ptr, int64_t actor_id, int64_t content_ptr, double importance) {
    SharedMemory* sm = (SharedMemory*)sm_ptr;
    if (!sm) return 0;

    pthread_rwlock_wrlock(&sm->rwlock);
    if (!has_write_access(sm, actor_id)) {
        pthread_rwlock_unlock(&sm->rwlock);
        return 0;
    }

    int64_t result = anima_remember(sm->memory, content_ptr, importance);
    pthread_rwlock_unlock(&sm->rwlock);
    return result;
}

// Get underlying memory
int64_t shared_memory_get_memory(int64_t sm_ptr) {
    SharedMemory* sm = (SharedMemory*)sm_ptr;
    return sm ? sm->memory : 0;
}

// Close shared memory
void shared_memory_close(int64_t sm_ptr) {
    SharedMemory* sm = (SharedMemory*)sm_ptr;
    if (!sm) return;

    free(sm->name);
    free(sm->reader_ids);
    free(sm->writer_ids);
    anima_memory_close(sm->memory);
    pthread_rwlock_destroy(&sm->rwlock);
    free(sm);
}

// --------------------------------------------------------------------------
// 4.8 Actor System Utilities
// --------------------------------------------------------------------------

// Get actor count
int64_t ai_actor_system_count(int64_t sys_ptr) {
    AIActorSystem* sys = (AIActorSystem*)sys_ptr;
    return sys ? sys->count : 0;
}

// List all actors
int64_t ai_actor_system_list(int64_t sys_ptr) {
    AIActorSystem* sys = (AIActorSystem*)sys_ptr;
    if (!sys) return 0;

    char* result = (char*)malloc(8192);
    strcpy(result, "[");
    int first = 1;

    pthread_mutex_lock(&sys->lock);
    for (int i = 0; i < sys->count; i++) {
        AIActor* actor = sys->actors[i];
        if (!actor) continue;

        if (!first) strcat(result, ",");
        char entry[256];
        snprintf(entry, sizeof(entry),
                "\n  {\"id\":%ld,\"name\":\"%s\",\"role\":\"%s\",\"status\":%d}",
                (long)actor->id, actor->config.name, actor->config.role, actor->status);
        strcat(result, entry);
        first = 0;
    }
    pthread_mutex_unlock(&sys->lock);

    strcat(result, "\n]");
    int64_t output = (int64_t)intrinsic_string_new(result);
    free(result);
    return output;
}

// Close actor system
void ai_actor_system_close(int64_t sys_ptr) {
    AIActorSystem* sys = (AIActorSystem*)sys_ptr;
    if (!sys) return;

    pthread_mutex_lock(&sys->lock);
    for (int i = 0; i < sys->count; i++) {
        AIActor* actor = sys->actors[i];
        if (actor) {
            free(actor->config.name);
            free(actor->config.role);
            // Free history
            ai_actor_clear_history((int64_t)sys, actor->id);
            pthread_mutex_destroy(&actor->lock);
            free(actor);
        }
    }
    free(sys->actors);
    pthread_mutex_unlock(&sys->lock);
    pthread_mutex_destroy(&sys->lock);
    free(sys);

    if (global_ai_system == sys) {
        global_ai_system = NULL;
    }
}

// ============================================================================
// Phase 4.9: Specialist Enhancements - Multi-Provider Support & Reliability
// ============================================================================

// --------------------------------------------------------------------------
// 4.9.1 Provider Configuration
// --------------------------------------------------------------------------

// Re-use existing LLMProvider from Phase 29
// typedef enum { PROVIDER_MOCK=0, PROVIDER_ANTHROPIC=1, PROVIDER_OPENAI=2, PROVIDER_OLLAMA=3 } LLMProvider;

typedef enum ModelTier {
    TIER_FAST = 0,      // Fast, cheaper models (haiku, gpt-3.5)
    TIER_BALANCED = 1,  // Balanced (sonnet, gpt-4)
    TIER_PREMIUM = 2    // Best quality (opus, o1)
} ModelTier;

typedef struct ProviderConfig {
    LLMProvider type;
    char* name;
    char* api_key;
    char* base_url;
    char* model;
    ModelTier tier;
    int max_tokens;
    double temperature;
    int timeout_ms;
    int priority;           // For fallback ordering
    int enabled;
    // Cost tracking (per 1M tokens)
    double input_cost;
    double output_cost;
} ProviderConfig;

typedef struct ProviderRegistry {
    ProviderConfig** providers;
    int count;
    int capacity;
    int default_provider;
    pthread_mutex_t lock;
} ProviderRegistry;

// Provider usage stats
typedef struct ProviderStats {
    int64_t total_requests;
    int64_t successful_requests;
    int64_t failed_requests;
    int64_t total_input_tokens;
    int64_t total_output_tokens;
    double total_cost;
    double total_latency_ms;
    int64_t last_request_time;
    int consecutive_failures;
} ProviderStats;

// Global stats per provider
static ProviderStats* provider_stats = NULL;
static int stats_count = 0;

// Create provider registry
int64_t provider_registry_new(void) {
    ProviderRegistry* reg = (ProviderRegistry*)calloc(1, sizeof(ProviderRegistry));
    reg->capacity = 8;
    reg->providers = (ProviderConfig**)calloc(reg->capacity, sizeof(ProviderConfig*));
    pthread_mutex_init(&reg->lock, NULL);
    return (int64_t)reg;
}

// Create provider config
int64_t provider_config_new(int64_t type, int64_t name_ptr) {
    ProviderConfig* cfg = (ProviderConfig*)calloc(1, sizeof(ProviderConfig));
    SxString* name = (SxString*)name_ptr;

    cfg->type = (LLMProvider)type;
    cfg->name = name ? strdup(name->data) : strdup("default");
    cfg->max_tokens = 4096;
    cfg->temperature = 0.7;
    cfg->timeout_ms = 30000;
    cfg->priority = 0;
    cfg->enabled = 1;

    // Set defaults based on provider type
    switch (cfg->type) {
        case PROVIDER_ANTHROPIC:
            cfg->base_url = strdup("https://api.anthropic.com");
            cfg->model = strdup("claude-3-5-sonnet-20241022");
            cfg->tier = TIER_BALANCED;
            cfg->input_cost = 3.0;   // $3/1M input tokens
            cfg->output_cost = 15.0; // $15/1M output tokens
            break;
        case PROVIDER_OPENAI:
            cfg->base_url = strdup("https://api.openai.com");
            cfg->model = strdup("gpt-4o");
            cfg->tier = TIER_BALANCED;
            cfg->input_cost = 5.0;
            cfg->output_cost = 15.0;
            break;
        case PROVIDER_OLLAMA:
            cfg->base_url = strdup("http://localhost:11434");
            cfg->model = strdup("llama3.2");
            cfg->tier = TIER_FAST;
            cfg->input_cost = 0.0;
            cfg->output_cost = 0.0;
            break;
        default:
            cfg->base_url = strdup("");
            cfg->model = strdup("");
            cfg->tier = TIER_BALANCED;
            break;
    }

    return (int64_t)cfg;
}

// Set API key
void provider_config_set_key(int64_t cfg_ptr, int64_t key_ptr) {
    ProviderConfig* cfg = (ProviderConfig*)cfg_ptr;
    SxString* key = (SxString*)key_ptr;
    if (cfg && key) {
        free(cfg->api_key);
        cfg->api_key = strdup(key->data);
    }
}

// Set model
void provider_config_set_model(int64_t cfg_ptr, int64_t model_ptr) {
    ProviderConfig* cfg = (ProviderConfig*)cfg_ptr;
    SxString* model = (SxString*)model_ptr;
    if (cfg && model) {
        free(cfg->model);
        cfg->model = strdup(model->data);
    }
}

// Set base URL
void provider_config_set_url(int64_t cfg_ptr, int64_t url_ptr) {
    ProviderConfig* cfg = (ProviderConfig*)cfg_ptr;
    SxString* url = (SxString*)url_ptr;
    if (cfg && url) {
        free(cfg->base_url);
        cfg->base_url = strdup(url->data);
    }
}

// Set temperature
void provider_config_set_temp(int64_t cfg_ptr, double temp) {
    ProviderConfig* cfg = (ProviderConfig*)cfg_ptr;
    if (cfg) cfg->temperature = temp;
}

// Set max tokens
void provider_config_set_max_tokens(int64_t cfg_ptr, int64_t max_tokens) {
    ProviderConfig* cfg = (ProviderConfig*)cfg_ptr;
    if (cfg) cfg->max_tokens = (int)max_tokens;
}

// Set timeout
void provider_config_set_timeout(int64_t cfg_ptr, int64_t timeout_ms) {
    ProviderConfig* cfg = (ProviderConfig*)cfg_ptr;
    if (cfg) cfg->timeout_ms = (int)timeout_ms;
}

// Set priority
void provider_config_set_priority(int64_t cfg_ptr, int64_t priority) {
    ProviderConfig* cfg = (ProviderConfig*)cfg_ptr;
    if (cfg) cfg->priority = (int)priority;
}

// Set cost
void provider_config_set_cost(int64_t cfg_ptr, double input_cost, double output_cost) {
    ProviderConfig* cfg = (ProviderConfig*)cfg_ptr;
    if (cfg) {
        cfg->input_cost = input_cost;
        cfg->output_cost = output_cost;
    }
}

// Add provider to registry
int64_t provider_registry_add(int64_t reg_ptr, int64_t cfg_ptr) {
    ProviderRegistry* reg = (ProviderRegistry*)reg_ptr;
    ProviderConfig* cfg = (ProviderConfig*)cfg_ptr;
    if (!reg || !cfg) return -1;

    pthread_mutex_lock(&reg->lock);

    if (reg->count >= reg->capacity) {
        reg->capacity *= 2;
        reg->providers = (ProviderConfig**)realloc(reg->providers,
                                                    reg->capacity * sizeof(ProviderConfig*));
    }

    int id = reg->count++;
    reg->providers[id] = cfg;

    // Expand stats if needed
    if (id >= stats_count) {
        provider_stats = (ProviderStats*)realloc(provider_stats,
                                                  (id + 1) * sizeof(ProviderStats));
        memset(&provider_stats[id], 0, sizeof(ProviderStats));
        stats_count = id + 1;
    }

    pthread_mutex_unlock(&reg->lock);
    return id;
}

// Get provider by ID
int64_t provider_registry_get(int64_t reg_ptr, int64_t id) {
    ProviderRegistry* reg = (ProviderRegistry*)reg_ptr;
    if (!reg || id < 0 || id >= reg->count) return 0;
    return (int64_t)reg->providers[id];
}

// Get provider count
int64_t provider_registry_count(int64_t reg_ptr) {
    ProviderRegistry* reg = (ProviderRegistry*)reg_ptr;
    return reg ? reg->count : 0;
}

// Set default provider
void provider_registry_set_default(int64_t reg_ptr, int64_t id) {
    ProviderRegistry* reg = (ProviderRegistry*)reg_ptr;
    if (reg && id >= 0 && id < reg->count) {
        reg->default_provider = (int)id;
    }
}

// Get provider by tier
int64_t provider_get_by_tier(int64_t reg_ptr, int64_t tier) {
    ProviderRegistry* reg = (ProviderRegistry*)reg_ptr;
    if (!reg) return -1;

    pthread_mutex_lock(&reg->lock);
    for (int i = 0; i < reg->count; i++) {
        if (reg->providers[i]->tier == (ModelTier)tier && reg->providers[i]->enabled) {
            pthread_mutex_unlock(&reg->lock);
            return i;
        }
    }
    pthread_mutex_unlock(&reg->lock);
    return -1;
}

// List providers
int64_t provider_registry_list(int64_t reg_ptr) {
    ProviderRegistry* reg = (ProviderRegistry*)reg_ptr;
    if (!reg) return 0;

    char* result = (char*)malloc(4096);
    strcpy(result, "[");
    int first = 1;

    pthread_mutex_lock(&reg->lock);
    for (int i = 0; i < reg->count; i++) {
        ProviderConfig* cfg = reg->providers[i];
        if (!first) strcat(result, ",");
        char entry[512];
        snprintf(entry, sizeof(entry),
                "\n  {\"id\":%d,\"name\":\"%s\",\"type\":%d,\"model\":\"%s\",\"enabled\":%s,\"priority\":%d}",
                i, cfg->name, cfg->type, cfg->model,
                cfg->enabled ? "true" : "false", cfg->priority);
        strcat(result, entry);
        first = 0;
    }
    pthread_mutex_unlock(&reg->lock);

    strcat(result, "\n]");
    int64_t output = (int64_t)intrinsic_string_new(result);
    free(result);
    return output;
}

// --------------------------------------------------------------------------
// 4.9.2 Token Counting & Cost Tracking
// --------------------------------------------------------------------------

// Simple token counter (approximation: ~4 chars per token)
int64_t estimate_tokens(int64_t text_ptr) {
    SxString* text = (SxString*)text_ptr;
    if (!text) return 0;
    return (text->len + 3) / 4;  // Approximate
}

// More accurate tokenizer (uses word boundaries)
int64_t count_tokens_accurate(int64_t text_ptr) {
    SxString* text = (SxString*)text_ptr;
    if (!text || !text->data) return 0;

    int64_t tokens = 0;
    int in_word = 0;

    for (size_t i = 0; i < text->len; i++) {
        char c = text->data[i];
        if (c == ' ' || c == '\n' || c == '\t' || c == '.' || c == ',' ||
            c == '!' || c == '?' || c == ';' || c == ':') {
            if (in_word) {
                tokens++;
                in_word = 0;
            }
            // Punctuation counts as token
            if (c != ' ' && c != '\n' && c != '\t') tokens++;
        } else {
            in_word = 1;
        }
    }
    if (in_word) tokens++;

    return tokens;
}

// Calculate cost for request
double calculate_cost(int64_t cfg_ptr, int64_t input_tokens, int64_t output_tokens) {
    ProviderConfig* cfg = (ProviderConfig*)cfg_ptr;
    if (!cfg) return 0.0;

    double input_cost = (cfg->input_cost / 1000000.0) * input_tokens;
    double output_cost = (cfg->output_cost / 1000000.0) * output_tokens;
    return input_cost + output_cost;
}

// Get provider stats
int64_t provider_get_stats(int64_t provider_id) {
    if (provider_id < 0 || provider_id >= stats_count) return 0;

    ProviderStats* stats = &provider_stats[provider_id];
    char* result = (char*)malloc(512);
    snprintf(result, 512,
            "{\"total_requests\":%ld,\"successful\":%ld,\"failed\":%ld,"
            "\"input_tokens\":%ld,\"output_tokens\":%ld,"
            "\"total_cost\":%.6f,\"avg_latency\":%.2f}",
            (long)stats->total_requests,
            (long)stats->successful_requests,
            (long)stats->failed_requests,
            (long)stats->total_input_tokens,
            (long)stats->total_output_tokens,
            stats->total_cost,
            stats->total_requests > 0 ? stats->total_latency_ms / stats->total_requests : 0.0);

    int64_t output = (int64_t)intrinsic_string_new(result);
    free(result);
    return output;
}

// Record request stats
void provider_record_request(int64_t provider_id, int success, int64_t input_tokens,
                            int64_t output_tokens, double cost, double latency_ms) {
    if (provider_id < 0 || provider_id >= stats_count) return;

    ProviderStats* stats = &provider_stats[provider_id];
    stats->total_requests++;
    if (success) {
        stats->successful_requests++;
        stats->consecutive_failures = 0;
    } else {
        stats->failed_requests++;
        stats->consecutive_failures++;
    }
    stats->total_input_tokens += input_tokens;
    stats->total_output_tokens += output_tokens;
    stats->total_cost += cost;
    stats->total_latency_ms += latency_ms;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    stats->last_request_time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// Get total cost across all providers
double provider_total_cost(int64_t reg_ptr) {
    (void)reg_ptr;
    double total = 0.0;
    for (int i = 0; i < stats_count; i++) {
        total += provider_stats[i].total_cost;
    }
    return total;
}

// --------------------------------------------------------------------------
// 4.9.3 Retry with Exponential Backoff
// --------------------------------------------------------------------------

typedef struct RetryConfig {
    int max_retries;
    int64_t initial_delay_ms;
    int64_t max_delay_ms;
    double backoff_multiplier;
    int retry_on_timeout;
    int retry_on_rate_limit;
    int retry_on_server_error;
} RetryConfig;

// Create retry config
int64_t retry_config_new(void) {
    RetryConfig* cfg = (RetryConfig*)calloc(1, sizeof(RetryConfig));
    cfg->max_retries = 3;
    cfg->initial_delay_ms = 1000;
    cfg->max_delay_ms = 30000;
    cfg->backoff_multiplier = 2.0;
    cfg->retry_on_timeout = 1;
    cfg->retry_on_rate_limit = 1;
    cfg->retry_on_server_error = 1;
    return (int64_t)cfg;
}

// Set max retries
void retry_config_set_max(int64_t cfg_ptr, int64_t max_retries) {
    RetryConfig* cfg = (RetryConfig*)cfg_ptr;
    if (cfg) cfg->max_retries = (int)max_retries;
}

// Set initial delay
void retry_config_set_delay(int64_t cfg_ptr, int64_t delay_ms) {
    RetryConfig* cfg = (RetryConfig*)cfg_ptr;
    if (cfg) cfg->initial_delay_ms = delay_ms;
}

// Set backoff multiplier
void retry_config_set_backoff(int64_t cfg_ptr, double multiplier) {
    RetryConfig* cfg = (RetryConfig*)cfg_ptr;
    if (cfg) cfg->backoff_multiplier = multiplier;
}

// Calculate delay for retry attempt
int64_t retry_calculate_delay(int64_t cfg_ptr, int64_t attempt) {
    RetryConfig* cfg = (RetryConfig*)cfg_ptr;
    if (!cfg) return 1000;

    double delay = cfg->initial_delay_ms * pow(cfg->backoff_multiplier, attempt);
    if (delay > cfg->max_delay_ms) delay = cfg->max_delay_ms;

    // Add jitter (10% random variation)
    double jitter = ((double)rand() / RAND_MAX - 0.5) * 0.2 * delay;
    return (int64_t)(delay + jitter);
}

// Should retry based on error type
int64_t retry_should_retry(int64_t cfg_ptr, int64_t error_type, int64_t attempt) {
    RetryConfig* cfg = (RetryConfig*)cfg_ptr;
    if (!cfg) return 0;
    if (attempt >= cfg->max_retries) return 0;

    // Error types: 0=unknown, 1=timeout, 2=rate_limit, 3=server_error, 4=auth_error
    switch (error_type) {
        case 1: return cfg->retry_on_timeout;
        case 2: return cfg->retry_on_rate_limit;
        case 3: return cfg->retry_on_server_error;
        case 4: return 0;  // Never retry auth errors
        default: return 0;
    }
}

// Free retry config
void retry_config_close(int64_t cfg_ptr) {
    free((RetryConfig*)cfg_ptr);
}

// --------------------------------------------------------------------------
// 4.9.4 Fallback Provider Chain
// --------------------------------------------------------------------------

typedef struct FallbackChain {
    int64_t* provider_ids;
    int count;
    int capacity;
    pthread_mutex_t lock;
} FallbackChain;

// Create fallback chain
int64_t fallback_chain_new(void) {
    FallbackChain* chain = (FallbackChain*)calloc(1, sizeof(FallbackChain));
    chain->capacity = 8;
    chain->provider_ids = (int64_t*)calloc(chain->capacity, sizeof(int64_t));
    pthread_mutex_init(&chain->lock, NULL);
    return (int64_t)chain;
}

// Add provider to chain
int64_t fallback_chain_add(int64_t chain_ptr, int64_t provider_id) {
    FallbackChain* chain = (FallbackChain*)chain_ptr;
    if (!chain) return -1;

    pthread_mutex_lock(&chain->lock);
    if (chain->count >= chain->capacity) {
        chain->capacity *= 2;
        chain->provider_ids = (int64_t*)realloc(chain->provider_ids,
                                                 chain->capacity * sizeof(int64_t));
    }
    chain->provider_ids[chain->count++] = provider_id;
    pthread_mutex_unlock(&chain->lock);

    return chain->count - 1;
}

// Get next provider in chain
int64_t fallback_chain_next(int64_t chain_ptr, int64_t current_index) {
    FallbackChain* chain = (FallbackChain*)chain_ptr;
    if (!chain) return -1;

    int64_t next = current_index + 1;
    if (next >= chain->count) return -1;
    return chain->provider_ids[next];
}

// Get provider by index
int64_t fallback_chain_get(int64_t chain_ptr, int64_t index) {
    FallbackChain* chain = (FallbackChain*)chain_ptr;
    if (!chain || index < 0 || index >= chain->count) return -1;
    return chain->provider_ids[index];
}

// Get chain size
int64_t fallback_chain_size(int64_t chain_ptr) {
    FallbackChain* chain = (FallbackChain*)chain_ptr;
    return chain ? chain->count : 0;
}

// Close fallback chain
void fallback_chain_close(int64_t chain_ptr) {
    FallbackChain* chain = (FallbackChain*)chain_ptr;
    if (!chain) return;
    free(chain->provider_ids);
    pthread_mutex_destroy(&chain->lock);
    free(chain);
}

// --------------------------------------------------------------------------
// 4.9.5 Streaming Support
// --------------------------------------------------------------------------

typedef void (*StreamCallback)(int64_t chunk_ptr, int64_t user_data);

typedef struct StreamContext {
    StreamCallback callback;
    int64_t user_data;
    int64_t total_tokens;
    int64_t bytes_received;
    int is_complete;
    int had_error;
    char* error_message;
    char* accumulated;
    size_t accumulated_len;
    size_t accumulated_cap;
    pthread_mutex_t lock;
} StreamContext;

// Create stream context
int64_t stream_context_new(int64_t callback_ptr, int64_t user_data) {
    StreamContext* ctx = (StreamContext*)calloc(1, sizeof(StreamContext));
    ctx->callback = (StreamCallback)callback_ptr;
    ctx->user_data = user_data;
    ctx->accumulated_cap = 4096;
    ctx->accumulated = (char*)malloc(ctx->accumulated_cap);
    ctx->accumulated[0] = '\0';
    pthread_mutex_init(&ctx->lock, NULL);
    return (int64_t)ctx;
}

// Process stream chunk
void stream_process_chunk(int64_t ctx_ptr, int64_t chunk_ptr) {
    StreamContext* ctx = (StreamContext*)ctx_ptr;
    SxString* chunk = (SxString*)chunk_ptr;
    if (!ctx || !chunk) return;

    pthread_mutex_lock(&ctx->lock);

    // Accumulate chunk
    size_t new_len = ctx->accumulated_len + chunk->len;
    if (new_len >= ctx->accumulated_cap) {
        ctx->accumulated_cap = new_len * 2;
        ctx->accumulated = (char*)realloc(ctx->accumulated, ctx->accumulated_cap);
    }
    memcpy(ctx->accumulated + ctx->accumulated_len, chunk->data, chunk->len);
    ctx->accumulated_len = new_len;
    ctx->accumulated[new_len] = '\0';

    ctx->bytes_received += chunk->len;
    ctx->total_tokens += (chunk->len + 3) / 4;  // Estimate tokens

    pthread_mutex_unlock(&ctx->lock);

    // Call callback if set
    if (ctx->callback) {
        ctx->callback(chunk_ptr, ctx->user_data);
    }
}

// Mark stream complete
void stream_complete(int64_t ctx_ptr) {
    StreamContext* ctx = (StreamContext*)ctx_ptr;
    if (ctx) ctx->is_complete = 1;
}

// Mark stream error
void stream_error(int64_t ctx_ptr, int64_t error_ptr) {
    StreamContext* ctx = (StreamContext*)ctx_ptr;
    SxString* error = (SxString*)error_ptr;
    if (!ctx) return;

    ctx->had_error = 1;
    if (error) {
        free(ctx->error_message);
        ctx->error_message = strdup(error->data);
    }
}

// Get accumulated content
int64_t stream_get_content(int64_t ctx_ptr) {
    StreamContext* ctx = (StreamContext*)ctx_ptr;
    if (!ctx) return 0;
    return (int64_t)intrinsic_string_new(ctx->accumulated);
}

// Check if complete
int64_t stream_is_complete(int64_t ctx_ptr) {
    StreamContext* ctx = (StreamContext*)ctx_ptr;
    return ctx ? ctx->is_complete : 0;
}

// Check for error
int64_t stream_has_error(int64_t ctx_ptr) {
    StreamContext* ctx = (StreamContext*)ctx_ptr;
    return ctx ? ctx->had_error : 0;
}

// Get error message
int64_t stream_get_error(int64_t ctx_ptr) {
    StreamContext* ctx = (StreamContext*)ctx_ptr;
    if (!ctx || !ctx->error_message) return 0;
    return (int64_t)intrinsic_string_new(ctx->error_message);
}

// Get token count
int64_t stream_token_count(int64_t ctx_ptr) {
    StreamContext* ctx = (StreamContext*)ctx_ptr;
    return ctx ? ctx->total_tokens : 0;
}

// Close stream context
void stream_context_close(int64_t ctx_ptr) {
    StreamContext* ctx = (StreamContext*)ctx_ptr;
    if (!ctx) return;
    free(ctx->accumulated);
    free(ctx->error_message);
    pthread_mutex_destroy(&ctx->lock);
    free(ctx);
}

// --------------------------------------------------------------------------
// 4.9.6 Structured Output (JSON Schema Validation)
// --------------------------------------------------------------------------

typedef struct OutputSchema {
    char* name;
    char* json_schema;
    int strict;
} OutputSchema;

// Create output schema
int64_t output_schema_new(int64_t name_ptr, int64_t schema_ptr) {
    OutputSchema* schema = (OutputSchema*)calloc(1, sizeof(OutputSchema));
    SxString* name = (SxString*)name_ptr;
    SxString* json = (SxString*)schema_ptr;

    schema->name = name ? strdup(name->data) : strdup("output");
    schema->json_schema = json ? strdup(json->data) : strdup("{}");
    schema->strict = 1;

    return (int64_t)schema;
}

// Set strict mode
void output_schema_set_strict(int64_t schema_ptr, int64_t strict) {
    OutputSchema* schema = (OutputSchema*)schema_ptr;
    if (schema) schema->strict = (int)strict;
}

// Get schema JSON
int64_t output_schema_get_json(int64_t schema_ptr) {
    OutputSchema* schema = (OutputSchema*)schema_ptr;
    if (!schema) return 0;
    return (int64_t)intrinsic_string_new(schema->json_schema);
}

// Simple JSON validation (checks basic structure)
int64_t validate_json_output(int64_t output_ptr, int64_t schema_ptr) {
    SxString* output = (SxString*)output_ptr;
    OutputSchema* schema = (OutputSchema*)schema_ptr;
    (void)schema;  // Full validation would check against schema

    if (!output || !output->data) return 0;

    // Basic JSON structure check
    char* data = output->data;
    int len = (int)output->len;

    // Skip whitespace
    int i = 0;
    while (i < len && (data[i] == ' ' || data[i] == '\n' || data[i] == '\t')) i++;

    if (i >= len) return 0;

    // Must start with { or [
    if (data[i] != '{' && data[i] != '[') return 0;

    // Find matching end
    char start = data[i];
    char end = (start == '{') ? '}' : ']';
    int depth = 1;
    int in_string = 0;
    i++;

    while (i < len && depth > 0) {
        if (data[i] == '"' && (i == 0 || data[i-1] != '\\')) {
            in_string = !in_string;
        } else if (!in_string) {
            if (data[i] == start || data[i] == '{' || data[i] == '[') depth++;
            else if (data[i] == end || data[i] == '}' || data[i] == ']') depth--;
        }
        i++;
    }

    return depth == 0 ? 1 : 0;
}

// Close output schema
void output_schema_close(int64_t schema_ptr) {
    OutputSchema* schema = (OutputSchema*)schema_ptr;
    if (!schema) return;
    free(schema->name);
    free(schema->json_schema);
    free(schema);
}

// --------------------------------------------------------------------------
// 4.9.7 Request Builder
// --------------------------------------------------------------------------

typedef struct LLMRequest {
    int64_t provider_id;
    char* system_prompt;
    char* user_prompt;
    char* model_override;
    int max_tokens;
    double temperature;
    int64_t output_schema;    // OutputSchema pointer
    int stream;
    int64_t stream_context;   // StreamContext pointer
    int64_t tools;            // Tool registry pointer
    int64_t retry_config;     // RetryConfig pointer
} LLMRequest;

// Create request
int64_t llm_request_new(int64_t provider_id) {
    LLMRequest* req = (LLMRequest*)calloc(1, sizeof(LLMRequest));
    req->provider_id = provider_id;
    req->max_tokens = 4096;
    req->temperature = 0.7;
    return (int64_t)req;
}

// Set system prompt
void llm_request_set_system(int64_t req_ptr, int64_t prompt_ptr) {
    LLMRequest* req = (LLMRequest*)req_ptr;
    SxString* prompt = (SxString*)prompt_ptr;
    if (req && prompt) {
        free(req->system_prompt);
        req->system_prompt = strdup(prompt->data);
    }
}

// Set user prompt
void llm_request_set_prompt(int64_t req_ptr, int64_t prompt_ptr) {
    LLMRequest* req = (LLMRequest*)req_ptr;
    SxString* prompt = (SxString*)prompt_ptr;
    if (req && prompt) {
        free(req->user_prompt);
        req->user_prompt = strdup(prompt->data);
    }
}

// Set model override
void llm_request_set_model(int64_t req_ptr, int64_t model_ptr) {
    LLMRequest* req = (LLMRequest*)req_ptr;
    SxString* model = (SxString*)model_ptr;
    if (req && model) {
        free(req->model_override);
        req->model_override = strdup(model->data);
    }
}

// Set max tokens
void llm_request_set_max_tokens(int64_t req_ptr, int64_t max_tokens) {
    LLMRequest* req = (LLMRequest*)req_ptr;
    if (req) req->max_tokens = (int)max_tokens;
}

// Set temperature
void llm_request_set_temperature(int64_t req_ptr, double temperature) {
    LLMRequest* req = (LLMRequest*)req_ptr;
    if (req) req->temperature = temperature;
}

// Set output schema
void llm_request_set_schema(int64_t req_ptr, int64_t schema_ptr) {
    LLMRequest* req = (LLMRequest*)req_ptr;
    if (req) req->output_schema = schema_ptr;
}

// Enable streaming
void llm_request_enable_stream(int64_t req_ptr, int64_t stream_ctx_ptr) {
    LLMRequest* req = (LLMRequest*)req_ptr;
    if (req) {
        req->stream = 1;
        req->stream_context = stream_ctx_ptr;
    }
}

// Set tools
void llm_request_set_tools(int64_t req_ptr, int64_t tools_ptr) {
    LLMRequest* req = (LLMRequest*)req_ptr;
    if (req) req->tools = tools_ptr;
}

// Set retry config
void llm_request_set_retry(int64_t req_ptr, int64_t retry_ptr) {
    LLMRequest* req = (LLMRequest*)req_ptr;
    if (req) req->retry_config = retry_ptr;
}

// Build request JSON
int64_t llm_request_to_json(int64_t req_ptr, int64_t reg_ptr) {
    LLMRequest* req = (LLMRequest*)req_ptr;
    ProviderRegistry* reg = (ProviderRegistry*)reg_ptr;
    if (!req || !reg) return 0;

    ProviderConfig* cfg = (ProviderConfig*)provider_registry_get(reg_ptr, req->provider_id);
    if (!cfg) return 0;

    char* json = (char*)malloc(16384);
    char* model = req->model_override ? req->model_override : cfg->model;

    // Build based on provider type
    if (cfg->type == PROVIDER_ANTHROPIC) {
        snprintf(json, 16384,
                "{\"model\":\"%s\",\"max_tokens\":%d,\"temperature\":%.2f,"
                "\"system\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}",
                model, req->max_tokens, req->temperature,
                req->system_prompt ? req->system_prompt : "",
                req->user_prompt ? req->user_prompt : "");
    } else if (cfg->type == PROVIDER_OPENAI) {
        snprintf(json, 16384,
                "{\"model\":\"%s\",\"max_tokens\":%d,\"temperature\":%.2f,"
                "\"messages\":[{\"role\":\"system\",\"content\":\"%s\"},"
                "{\"role\":\"user\",\"content\":\"%s\"}]}",
                model, req->max_tokens, req->temperature,
                req->system_prompt ? req->system_prompt : "",
                req->user_prompt ? req->user_prompt : "");
    } else {
        // Generic format
        snprintf(json, 16384,
                "{\"model\":\"%s\",\"prompt\":\"%s\",\"max_tokens\":%d}",
                model, req->user_prompt ? req->user_prompt : "", req->max_tokens);
    }

    int64_t output = (int64_t)intrinsic_string_new(json);
    free(json);
    return output;
}

// Free request
void llm_request_close(int64_t req_ptr) {
    LLMRequest* req = (LLMRequest*)req_ptr;
    if (!req) return;
    free(req->system_prompt);
    free(req->user_prompt);
    free(req->model_override);
    free(req);
}

// --------------------------------------------------------------------------
// 4.9.8 Response Handler
// --------------------------------------------------------------------------

typedef struct LLMResponse {
    int success;
    char* content;
    char* error;
    int64_t input_tokens;
    int64_t output_tokens;
    double cost;
    double latency_ms;
    int64_t provider_id;
    char* model_used;
    char* finish_reason;
    int64_t tool_calls;  // Vector of tool call structs
} LLMResponse;

// Create response
int64_t llm_response_new(void) {
    LLMResponse* resp = (LLMResponse*)calloc(1, sizeof(LLMResponse));
    return (int64_t)resp;
}

// Set success
void llm_response_set_success(int64_t resp_ptr, int64_t success) {
    LLMResponse* resp = (LLMResponse*)resp_ptr;
    if (resp) resp->success = (int)success;
}

// Set content
void llm_response_set_content(int64_t resp_ptr, int64_t content_ptr) {
    LLMResponse* resp = (LLMResponse*)resp_ptr;
    SxString* content = (SxString*)content_ptr;
    if (resp && content) {
        free(resp->content);
        resp->content = strdup(content->data);
    }
}

// Set error
void llm_response_set_error(int64_t resp_ptr, int64_t error_ptr) {
    LLMResponse* resp = (LLMResponse*)resp_ptr;
    SxString* error = (SxString*)error_ptr;
    if (resp && error) {
        free(resp->error);
        resp->error = strdup(error->data);
    }
}

// Set token counts
void llm_response_set_tokens(int64_t resp_ptr, int64_t input, int64_t output) {
    LLMResponse* resp = (LLMResponse*)resp_ptr;
    if (resp) {
        resp->input_tokens = input;
        resp->output_tokens = output;
    }
}

// Set cost
void llm_response_set_cost(int64_t resp_ptr, double cost) {
    LLMResponse* resp = (LLMResponse*)resp_ptr;
    if (resp) resp->cost = cost;
}

// Set latency
void llm_response_set_latency(int64_t resp_ptr, double latency_ms) {
    LLMResponse* resp = (LLMResponse*)resp_ptr;
    if (resp) resp->latency_ms = latency_ms;
}

// Get success
int64_t llm_response_is_success(int64_t resp_ptr) {
    LLMResponse* resp = (LLMResponse*)resp_ptr;
    return resp ? resp->success : 0;
}

// Get content
int64_t llm_response_get_content(int64_t resp_ptr) {
    LLMResponse* resp = (LLMResponse*)resp_ptr;
    if (!resp || !resp->content) return 0;
    return (int64_t)intrinsic_string_new(resp->content);
}

// Get error
int64_t llm_response_get_error(int64_t resp_ptr) {
    LLMResponse* resp = (LLMResponse*)resp_ptr;
    if (!resp || !resp->error) return 0;
    return (int64_t)intrinsic_string_new(resp->error);
}

// Get input tokens
int64_t llm_response_input_tokens(int64_t resp_ptr) {
    LLMResponse* resp = (LLMResponse*)resp_ptr;
    return resp ? resp->input_tokens : 0;
}

// Get output tokens
int64_t llm_response_output_tokens(int64_t resp_ptr) {
    LLMResponse* resp = (LLMResponse*)resp_ptr;
    return resp ? resp->output_tokens : 0;
}

// Get cost
double llm_response_get_cost(int64_t resp_ptr) {
    LLMResponse* resp = (LLMResponse*)resp_ptr;
    return resp ? resp->cost : 0.0;
}

// Get latency
double llm_response_get_latency(int64_t resp_ptr) {
    LLMResponse* resp = (LLMResponse*)resp_ptr;
    return resp ? resp->latency_ms : 0.0;
}

// To JSON
int64_t llm_response_to_json(int64_t resp_ptr) {
    LLMResponse* resp = (LLMResponse*)resp_ptr;
    if (!resp) return 0;

    char* json = (char*)malloc(8192);
    snprintf(json, 8192,
            "{\"success\":%s,\"content\":\"%s\",\"error\":\"%s\","
            "\"input_tokens\":%ld,\"output_tokens\":%ld,"
            "\"cost\":%.6f,\"latency_ms\":%.2f}",
            resp->success ? "true" : "false",
            resp->content ? resp->content : "",
            resp->error ? resp->error : "",
            (long)resp->input_tokens, (long)resp->output_tokens,
            resp->cost, resp->latency_ms);

    int64_t output = (int64_t)intrinsic_string_new(json);
    free(json);
    return output;
}

// Free response
void llm_response_close(int64_t resp_ptr) {
    LLMResponse* resp = (LLMResponse*)resp_ptr;
    if (!resp) return;
    free(resp->content);
    free(resp->error);
    free(resp->model_used);
    free(resp->finish_reason);
    free(resp);
}

// Close provider registry
void provider_registry_close(int64_t reg_ptr) {
    ProviderRegistry* reg = (ProviderRegistry*)reg_ptr;
    if (!reg) return;

    pthread_mutex_lock(&reg->lock);
    for (int i = 0; i < reg->count; i++) {
        ProviderConfig* cfg = reg->providers[i];
        if (cfg) {
            free(cfg->name);
            free(cfg->api_key);
            free(cfg->base_url);
            free(cfg->model);
            free(cfg);
        }
    }
    free(reg->providers);
    pthread_mutex_unlock(&reg->lock);
    pthread_mutex_destroy(&reg->lock);
    free(reg);
}

// ============================================================================
// Phase 4.10: Actor-Anima Integration
// ============================================================================

// --------------------------------------------------------------------------
// 4.10.1 Cognitive Actor (Actor with Anima Memory)
// --------------------------------------------------------------------------

typedef struct CognitiveActor {
    int64_t actor_id;           // AI Actor ID from the actor system
    int64_t anima;              // AnimaMemory pointer
    char* name;
    char* personality;          // System prompt / personality description
    int64_t tools;              // Tool registry pointer
    int64_t provider;           // Provider ID
    int auto_learn;             // Automatically learn from interactions
    int auto_remember;          // Automatically remember conversations
    double importance_threshold; // Minimum importance for auto-remember
    pthread_mutex_t lock;
} CognitiveActor;

typedef struct CognitiveActorRegistry {
    CognitiveActor** actors;
    int count;
    int capacity;
    pthread_mutex_t lock;
} CognitiveActorRegistry;

static CognitiveActorRegistry* global_cognitive_registry = NULL;

// Initialize global registry
static void ensure_cognitive_registry(void) {
    if (!global_cognitive_registry) {
        global_cognitive_registry = (CognitiveActorRegistry*)calloc(1, sizeof(CognitiveActorRegistry));
        global_cognitive_registry->capacity = 16;
        global_cognitive_registry->actors = (CognitiveActor**)calloc(16, sizeof(CognitiveActor*));
        pthread_mutex_init(&global_cognitive_registry->lock, NULL);
    }
}

// Create cognitive actor
int64_t cognitive_actor_new(int64_t actor_id, int64_t name_ptr, int64_t personality_ptr) {
    ensure_cognitive_registry();

    CognitiveActor* ca = (CognitiveActor*)calloc(1, sizeof(CognitiveActor));
    SxString* name = (SxString*)name_ptr;
    SxString* personality = (SxString*)personality_ptr;

    ca->actor_id = actor_id;
    ca->anima = anima_memory_new(100);  // Default capacity
    ca->name = name ? strdup(name->data) : strdup("cognitive");
    ca->personality = personality ? strdup(personality->data) : strdup("You are a helpful AI assistant.");
    ca->auto_learn = 1;
    ca->auto_remember = 1;
    ca->importance_threshold = 0.5;
    pthread_mutex_init(&ca->lock, NULL);

    // Add to registry
    pthread_mutex_lock(&global_cognitive_registry->lock);
    if (global_cognitive_registry->count >= global_cognitive_registry->capacity) {
        global_cognitive_registry->capacity *= 2;
        global_cognitive_registry->actors = (CognitiveActor**)realloc(
            global_cognitive_registry->actors,
            global_cognitive_registry->capacity * sizeof(CognitiveActor*));
    }
    int id = global_cognitive_registry->count++;
    global_cognitive_registry->actors[id] = ca;
    pthread_mutex_unlock(&global_cognitive_registry->lock);

    return (int64_t)ca;
}

// Get anima memory from cognitive actor
int64_t cognitive_actor_get_anima(int64_t ca_ptr) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    return ca ? ca->anima : 0;
}

// Set tools for cognitive actor
void cognitive_actor_set_tools(int64_t ca_ptr, int64_t tools_ptr) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (ca) ca->tools = tools_ptr;
}

// Set provider for cognitive actor
void cognitive_actor_set_provider(int64_t ca_ptr, int64_t provider_id) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (ca) ca->provider = provider_id;
}

// Enable/disable auto learning
void cognitive_actor_set_auto_learn(int64_t ca_ptr, int64_t enabled) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (ca) ca->auto_learn = (int)enabled;
}

// Enable/disable auto remember
void cognitive_actor_set_auto_remember(int64_t ca_ptr, int64_t enabled) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (ca) ca->auto_remember = (int)enabled;
}

// Set importance threshold
void cognitive_actor_set_threshold(int64_t ca_ptr, double threshold) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (ca) ca->importance_threshold = threshold;
}

// Get personality/system prompt
int64_t cognitive_actor_get_personality(int64_t ca_ptr) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (!ca || !ca->personality) return 0;
    return (int64_t)intrinsic_string_new(ca->personality);
}

// Set personality/system prompt
void cognitive_actor_set_personality(int64_t ca_ptr, int64_t personality_ptr) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    SxString* personality = (SxString*)personality_ptr;
    if (ca && personality) {
        free(ca->personality);
        ca->personality = strdup(personality->data);
    }
}

// --------------------------------------------------------------------------
// 4.10.2 Cognitive Actor Operations
// --------------------------------------------------------------------------

// Remember an interaction with importance calculation
int64_t cognitive_actor_remember(int64_t ca_ptr, int64_t content_ptr, double importance) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (!ca) return 0;

    pthread_mutex_lock(&ca->lock);
    int64_t result = anima_remember(ca->anima, content_ptr, importance);
    pthread_mutex_unlock(&ca->lock);

    return result;
}

// Learn a fact
int64_t cognitive_actor_learn(int64_t ca_ptr, int64_t content_ptr, double confidence, int64_t source_ptr) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (!ca) return 0;

    pthread_mutex_lock(&ca->lock);
    int64_t result = anima_learn(ca->anima, content_ptr, confidence, source_ptr);
    pthread_mutex_unlock(&ca->lock);

    return result;
}

// Form a belief
int64_t cognitive_actor_believe(int64_t ca_ptr, int64_t content_ptr, double confidence, int64_t evidence_ptr) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (!ca) return 0;

    pthread_mutex_lock(&ca->lock);
    int64_t result = anima_believe(ca->anima, content_ptr, confidence, evidence_ptr);
    pthread_mutex_unlock(&ca->lock);

    return result;
}

// Recall relevant memories for a goal (returns JSON string)
int64_t cognitive_actor_recall(int64_t ca_ptr, int64_t goal_ptr, int64_t context_ptr) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (!ca) return 0;

    pthread_mutex_lock(&ca->lock);
    int64_t vec_ptr = anima_recall_for_goal(ca->anima, goal_ptr, context_ptr, 10);
    pthread_mutex_unlock(&ca->lock);

    if (!vec_ptr) return (int64_t)intrinsic_string_new("[]");

    // Convert vector to JSON array
    SxVec* vec = (SxVec*)vec_ptr;
    char* result = (char*)malloc(8192);
    strcpy(result, "[");
    int first = 1;

    for (size_t i = 0; i < vec->len; i++) {
        SxString* s = (SxString*)vec->items[i];
        if (s && s->data) {
            if (!first) strcat(result, ",");
            strcat(result, "\n  \"");
            // Escape special chars in JSON
            char* p = s->data;
            char* end = result + strlen(result);
            while (*p && end - result < 8000) {
                if (*p == '"' || *p == '\\') {
                    *end++ = '\\';
                }
                *end++ = *p++;
            }
            *end = '\0';
            strcat(result, "\"");
            first = 0;
        }
    }

    if (!first) strcat(result, "\n");
    strcat(result, "]");

    int64_t output = (int64_t)intrinsic_string_new(result);
    free(result);
    return output;
}

// Process an interaction and optionally auto-learn/remember
int64_t cognitive_actor_process_interaction(int64_t ca_ptr, int64_t user_msg_ptr, int64_t assistant_msg_ptr, double importance) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (!ca) return 0;

    pthread_mutex_lock(&ca->lock);

    // Auto-remember if enabled and importance meets threshold
    if (ca->auto_remember && importance >= ca->importance_threshold) {
        // Remember user message
        anima_remember(ca->anima, user_msg_ptr, importance);
        // Remember assistant response
        anima_remember(ca->anima, assistant_msg_ptr, importance * 0.8);  // Slightly lower for own responses
    }

    pthread_mutex_unlock(&ca->lock);

    return 1;
}

// Get cognitive context (relevant memories, beliefs, facts for a query)
int64_t cognitive_actor_get_context(int64_t ca_ptr, int64_t query_ptr) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (!ca) return 0;

    char* result = (char*)malloc(8192);
    strcpy(result, "{");

    pthread_mutex_lock(&ca->lock);

    // Get relevant episodic memories
    int64_t episodic = anima_recall_for_goal(ca->anima, query_ptr, 0, 5);
    if (episodic) {
        SxString* ep_str = (SxString*)episodic;
        strcat(result, "\"episodic_memories\":");
        if (ep_str->data) strcat(result, ep_str->data);
        else strcat(result, "[]");
        strcat(result, ",");
    } else {
        strcat(result, "\"episodic_memories\":[],");
    }

    // Get semantic knowledge count
    char buf[64];
    snprintf(buf, sizeof(buf), "\"semantic_count\":%ld,", (long)anima_semantic_count(ca->anima));
    strcat(result, buf);

    // Get beliefs count
    snprintf(buf, sizeof(buf), "\"beliefs_count\":%ld", (long)anima_beliefs_count(ca->anima));
    strcat(result, buf);

    pthread_mutex_unlock(&ca->lock);

    strcat(result, "}");

    int64_t output = (int64_t)intrinsic_string_new(result);
    free(result);
    return output;
}

// Build enhanced prompt with cognitive context
int64_t cognitive_actor_build_prompt(int64_t ca_ptr, int64_t user_query_ptr) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (!ca) return 0;

    SxString* query = (SxString*)user_query_ptr;
    if (!query) return 0;

    pthread_mutex_lock(&ca->lock);

    // Get relevant memories (returns vector)
    int64_t vec_ptr = anima_recall_for_goal(ca->anima, user_query_ptr, 0, 3);

    // Format memories as text
    char memories_text[4096];
    memories_text[0] = '\0';

    if (vec_ptr) {
        SxVec* vec = (SxVec*)vec_ptr;
        for (size_t i = 0; i < vec->len && strlen(memories_text) < 3800; i++) {
            SxString* s = (SxString*)vec->items[i];
            if (s && s->data) {
                strcat(memories_text, "- ");
                strncat(memories_text, s->data, 500);
                strcat(memories_text, "\n");
            }
        }
    }

    if (strlen(memories_text) == 0) {
        strcpy(memories_text, "No relevant memories found.");
    }

    // Build prompt
    char* prompt = (char*)malloc(16384);
    snprintf(prompt, 16384,
            "%s\n\n"
            "## Relevant Context\n"
            "Based on your memory, you recall:\n%s\n"
            "## User Query\n%s",
            ca->personality,
            memories_text,
            query->data);

    pthread_mutex_unlock(&ca->lock);

    int64_t output = (int64_t)intrinsic_string_new(prompt);
    free(prompt);
    return output;
}

// --------------------------------------------------------------------------
// 4.10.3 Cognitive Actor Team
// --------------------------------------------------------------------------

typedef struct CognitiveTeam {
    char* name;
    int64_t* actor_ptrs;        // CognitiveActor pointers
    int count;
    int capacity;
    int64_t shared_memory;      // SharedMemory for team knowledge
    pthread_mutex_t lock;
} CognitiveTeam;

// Create cognitive team
int64_t cognitive_team_new(int64_t name_ptr) {
    CognitiveTeam* team = (CognitiveTeam*)calloc(1, sizeof(CognitiveTeam));
    SxString* name = (SxString*)name_ptr;

    team->name = name ? strdup(name->data) : strdup("team");
    team->capacity = 8;
    team->actor_ptrs = (int64_t*)calloc(team->capacity, sizeof(int64_t));
    team->shared_memory = shared_memory_new((int64_t)intrinsic_string_new("team_memory"), 100);
    pthread_mutex_init(&team->lock, NULL);

    return (int64_t)team;
}

// Add actor to team
int64_t cognitive_team_add(int64_t team_ptr, int64_t ca_ptr) {
    CognitiveTeam* team = (CognitiveTeam*)team_ptr;
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (!team || !ca) return -1;

    pthread_mutex_lock(&team->lock);

    if (team->count >= team->capacity) {
        team->capacity *= 2;
        team->actor_ptrs = (int64_t*)realloc(team->actor_ptrs, team->capacity * sizeof(int64_t));
    }

    int idx = team->count++;
    team->actor_ptrs[idx] = ca_ptr;

    // Grant read/write access to shared memory
    shared_memory_grant_read(team->shared_memory, ca->actor_id);
    shared_memory_grant_write(team->shared_memory, ca->actor_id);

    pthread_mutex_unlock(&team->lock);

    return idx;
}

// Share knowledge with team
int64_t cognitive_team_share(int64_t team_ptr, int64_t ca_ptr, int64_t knowledge_ptr, double importance) {
    CognitiveTeam* team = (CognitiveTeam*)team_ptr;
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (!team || !ca) return 0;

    // Store in shared memory
    return shared_memory_remember(team->shared_memory, ca->actor_id, knowledge_ptr, importance);
}

// Get team size
int64_t cognitive_team_size(int64_t team_ptr) {
    CognitiveTeam* team = (CognitiveTeam*)team_ptr;
    return team ? team->count : 0;
}

// Get shared memory
int64_t cognitive_team_get_shared(int64_t team_ptr) {
    CognitiveTeam* team = (CognitiveTeam*)team_ptr;
    return team ? team->shared_memory : 0;
}

// Recall from team shared memory
int64_t cognitive_team_recall(int64_t team_ptr, int64_t ca_ptr, int64_t goal_ptr) {
    CognitiveTeam* team = (CognitiveTeam*)team_ptr;
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (!team || !ca) return 0;

    return shared_memory_recall(team->shared_memory, ca->actor_id, goal_ptr, 0);
}

// Close cognitive team
void cognitive_team_close(int64_t team_ptr) {
    CognitiveTeam* team = (CognitiveTeam*)team_ptr;
    if (!team) return;

    free(team->name);
    free(team->actor_ptrs);
    shared_memory_close(team->shared_memory);
    pthread_mutex_destroy(&team->lock);
    free(team);
}

// --------------------------------------------------------------------------
// 4.10.4 Cognitive Actor Persistence
// --------------------------------------------------------------------------

// Save cognitive actor state (includes anima memory)
int64_t cognitive_actor_save(int64_t ca_ptr, int64_t path_ptr) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    SxString* path = (SxString*)path_ptr;
    if (!ca || !path) return 0;

    // Build path for anima memory
    char anima_path[512];
    snprintf(anima_path, sizeof(anima_path), "%s.anima", path->data);

    // Save anima memory
    int64_t anima_path_str = (int64_t)intrinsic_string_new(anima_path);
    if (!anima_save(ca->anima, anima_path_str)) {
        return 0;
    }

    // Save actor metadata
    FILE* f = fopen(path->data, "w");
    if (!f) return 0;

    fprintf(f, "{\n");
    fprintf(f, "  \"actor_id\": %ld,\n", (long)ca->actor_id);
    fprintf(f, "  \"name\": \"%s\",\n", ca->name);
    fprintf(f, "  \"personality\": \"%s\",\n", ca->personality);
    fprintf(f, "  \"auto_learn\": %d,\n", ca->auto_learn);
    fprintf(f, "  \"auto_remember\": %d,\n", ca->auto_remember);
    fprintf(f, "  \"importance_threshold\": %.2f\n", ca->importance_threshold);
    fprintf(f, "}\n");
    fclose(f);

    return 1;
}

// Load cognitive actor state
int64_t cognitive_actor_load(int64_t path_ptr) {
    SxString* path = (SxString*)path_ptr;
    if (!path) return 0;

    FILE* f = fopen(path->data, "r");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = (char*)malloc(len + 1);
    if (fread(content, 1, len, f) != (size_t)len) {
        free(content);
        fclose(f);
        return 0;
    }
    content[len] = '\0';
    fclose(f);

    // Parse metadata (simple parsing)
    ensure_cognitive_registry();

    CognitiveActor* ca = (CognitiveActor*)calloc(1, sizeof(CognitiveActor));
    pthread_mutex_init(&ca->lock, NULL);

    // Parse actor_id
    char* actor_id_pos = strstr(content, "\"actor_id\":");
    if (actor_id_pos) {
        ca->actor_id = atol(actor_id_pos + 12);
    }

    // Parse name
    char* name_pos = strstr(content, "\"name\": \"");
    if (name_pos) {
        name_pos += 9;
        char* end = strchr(name_pos, '"');
        if (end) {
            int name_len = (int)(end - name_pos);
            ca->name = (char*)malloc(name_len + 1);
            memcpy(ca->name, name_pos, name_len);
            ca->name[name_len] = '\0';
        }
    }
    if (!ca->name) ca->name = strdup("cognitive");

    // Parse personality
    char* pers_pos = strstr(content, "\"personality\": \"");
    if (pers_pos) {
        pers_pos += 16;
        char* end = strchr(pers_pos, '"');
        if (end) {
            int pers_len = (int)(end - pers_pos);
            ca->personality = (char*)malloc(pers_len + 1);
            memcpy(ca->personality, pers_pos, pers_len);
            ca->personality[pers_len] = '\0';
        }
    }
    if (!ca->personality) ca->personality = strdup("You are a helpful AI assistant.");

    // Parse auto_learn
    char* auto_learn_pos = strstr(content, "\"auto_learn\":");
    if (auto_learn_pos) {
        ca->auto_learn = atoi(auto_learn_pos + 13);
    } else {
        ca->auto_learn = 1;
    }

    // Parse auto_remember
    char* auto_rem_pos = strstr(content, "\"auto_remember\":");
    if (auto_rem_pos) {
        ca->auto_remember = atoi(auto_rem_pos + 16);
    } else {
        ca->auto_remember = 1;
    }

    // Parse importance_threshold
    char* thresh_pos = strstr(content, "\"importance_threshold\":");
    if (thresh_pos) {
        ca->importance_threshold = atof(thresh_pos + 23);
    } else {
        ca->importance_threshold = 0.5;
    }

    free(content);

    // Load anima memory
    char anima_path[512];
    snprintf(anima_path, sizeof(anima_path), "%s.anima", path->data);
    int64_t anima_path_str = (int64_t)intrinsic_string_new(anima_path);
    ca->anima = anima_load(anima_path_str);

    if (!ca->anima) {
        ca->anima = anima_memory_new(100);  // Create new if load failed
    }

    // Add to registry
    pthread_mutex_lock(&global_cognitive_registry->lock);
    if (global_cognitive_registry->count >= global_cognitive_registry->capacity) {
        global_cognitive_registry->capacity *= 2;
        global_cognitive_registry->actors = (CognitiveActor**)realloc(
            global_cognitive_registry->actors,
            global_cognitive_registry->capacity * sizeof(CognitiveActor*));
    }
    global_cognitive_registry->actors[global_cognitive_registry->count++] = ca;
    pthread_mutex_unlock(&global_cognitive_registry->lock);

    return (int64_t)ca;
}

// Close cognitive actor
void cognitive_actor_close(int64_t ca_ptr) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (!ca) return;

    free(ca->name);
    free(ca->personality);
    anima_memory_close(ca->anima);
    pthread_mutex_destroy(&ca->lock);
    free(ca);
}

// Get cognitive actor info as JSON
int64_t cognitive_actor_info(int64_t ca_ptr) {
    CognitiveActor* ca = (CognitiveActor*)ca_ptr;
    if (!ca) return 0;

    char* result = (char*)malloc(1024);
    pthread_mutex_lock(&ca->lock);
    snprintf(result, 1024,
            "{\"name\":\"%s\",\"actor_id\":%ld,"
            "\"episodic_count\":%ld,\"semantic_count\":%ld,\"beliefs_count\":%ld,"
            "\"auto_learn\":%s,\"auto_remember\":%s,\"importance_threshold\":%.2f}",
            ca->name, (long)ca->actor_id,
            (long)anima_episodic_count(ca->anima),
            (long)anima_semantic_count(ca->anima),
            (long)anima_beliefs_count(ca->anima),
            ca->auto_learn ? "true" : "false",
            ca->auto_remember ? "true" : "false",
            ca->importance_threshold);
    pthread_mutex_unlock(&ca->lock);

    int64_t output = (int64_t)intrinsic_string_new(result);
    free(result);
    return output;
}

// ============================================================================
// Phase 30: Advanced Evolution
// ============================================================================

// --------------------------------------------------------------------------
// 30.1 Genetic Operators
// --------------------------------------------------------------------------

typedef struct Individual {
    double* genes;
    int gene_count;
    double fitness;
    int rank;  // For NSGA-II
    double crowding_distance;
} Individual;

typedef struct Population {
    Individual** individuals;
    int size;
    int capacity;
    int gene_count;
    pthread_mutex_t lock;
} Population;

// Create individual
int64_t individual_new(int64_t gene_count) {
    Individual* ind = (Individual*)malloc(sizeof(Individual));
    if (!ind) return 0;
    
    ind->gene_count = (int)gene_count;
    ind->genes = (double*)malloc(gene_count * sizeof(double));
    ind->fitness = 0.0;
    ind->rank = 0;
    ind->crowding_distance = 0.0;
    
    // Random initialization
    for (int i = 0; i < gene_count; i++) {
        ind->genes[i] = (double)rand() / RAND_MAX;
    }
    
    return (int64_t)ind;
}

// Get gene
double individual_get_gene(int64_t ind_ptr, int64_t idx) {
    Individual* ind = (Individual*)ind_ptr;
    if (!ind || idx < 0 || idx >= ind->gene_count) return 0.0;
    return ind->genes[idx];
}

// Set gene
int64_t individual_set_gene(int64_t ind_ptr, int64_t idx, double value) {
    Individual* ind = (Individual*)ind_ptr;
    if (!ind || idx < 0 || idx >= ind->gene_count) return 0;
    ind->genes[idx] = value;
    return 1;
}

// Set fitness
int64_t individual_set_fitness(int64_t ind_ptr, double fitness) {
    Individual* ind = (Individual*)ind_ptr;
    if (!ind) return 0;
    ind->fitness = fitness;
    return 1;
}

// Get fitness
double individual_get_fitness(int64_t ind_ptr) {
    Individual* ind = (Individual*)ind_ptr;
    return ind ? ind->fitness : 0.0;
}

// Free individual
void individual_free(int64_t ind_ptr) {
    Individual* ind = (Individual*)ind_ptr;
    if (!ind) return;
    if (ind->genes) free(ind->genes);
    free(ind);
}

// Create population
int64_t population_new(int64_t size, int64_t gene_count) {
    Population* pop = (Population*)malloc(sizeof(Population));
    if (!pop) return 0;
    
    pop->capacity = (int)size;
    pop->size = (int)size;
    pop->gene_count = (int)gene_count;
    pop->individuals = (Individual**)malloc(size * sizeof(Individual*));
    pthread_mutex_init(&pop->lock, NULL);
    
    for (int i = 0; i < size; i++) {
        pop->individuals[i] = (Individual*)individual_new(gene_count);
    }
    
    return (int64_t)pop;
}

// Get individual
int64_t population_get(int64_t pop_ptr, int64_t idx) {
    Population* pop = (Population*)pop_ptr;
    if (!pop || idx < 0 || idx >= pop->size) return 0;
    return (int64_t)pop->individuals[idx];
}

// Tournament selection
int64_t selection_tournament(int64_t pop_ptr, int64_t tournament_size) {
    Population* pop = (Population*)pop_ptr;
    if (!pop || pop->size == 0) return 0;
    
    int best_idx = rand() % pop->size;
    double best_fitness = pop->individuals[best_idx]->fitness;
    
    for (int i = 1; i < tournament_size && i < pop->size; i++) {
        int idx = rand() % pop->size;
        if (pop->individuals[idx]->fitness > best_fitness) {
            best_idx = idx;
            best_fitness = pop->individuals[idx]->fitness;
        }
    }
    
    return (int64_t)pop->individuals[best_idx];
}

// Single-point crossover
int64_t crossover_single_point(int64_t parent1_ptr, int64_t parent2_ptr) {
    Individual* p1 = (Individual*)parent1_ptr;
    Individual* p2 = (Individual*)parent2_ptr;
    if (!p1 || !p2 || p1->gene_count != p2->gene_count) return 0;
    
    Individual* child = (Individual*)individual_new(p1->gene_count);
    int crossover_point = rand() % p1->gene_count;
    
    for (int i = 0; i < p1->gene_count; i++) {
        child->genes[i] = (i < crossover_point) ? p1->genes[i] : p2->genes[i];
    }
    
    return (int64_t)child;
}

// Uniform crossover (with mix rate)
int64_t crossover_uniform(int64_t parent1_ptr, int64_t parent2_ptr, double mix_rate) {
    Individual* p1 = (Individual*)parent1_ptr;
    Individual* p2 = (Individual*)parent2_ptr;
    if (!p1 || !p2 || p1->gene_count != p2->gene_count) return 0;

    Individual* child = (Individual*)individual_new(p1->gene_count);

    for (int i = 0; i < p1->gene_count; i++) {
        if ((double)rand() / RAND_MAX < mix_rate) {
            child->genes[i] = p2->genes[i];
        } else {
            child->genes[i] = p1->genes[i];
        }
    }

    return (int64_t)child;
}

// Gaussian mutation
int64_t mutation_gaussian(int64_t ind_ptr, double rate, double sigma) {
    Individual* ind = (Individual*)ind_ptr;
    if (!ind) return 0;
    
    for (int i = 0; i < ind->gene_count; i++) {
        if ((double)rand() / RAND_MAX < rate) {
            // Box-Muller transform for Gaussian random
            double u1 = (double)rand() / RAND_MAX;
            double u2 = (double)rand() / RAND_MAX;
            double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
            
            ind->genes[i] += z * sigma;
            // Clamp to [0, 1]
            if (ind->genes[i] < 0) ind->genes[i] = 0;
            if (ind->genes[i] > 1) ind->genes[i] = 1;
        }
    }
    
    return 1;
}

// Get population size
int64_t population_size(int64_t pop_ptr) {
    Population* pop = (Population*)pop_ptr;
    return pop ? pop->size : 0;
}

// Close population
void population_close(int64_t pop_ptr) {
    Population* pop = (Population*)pop_ptr;
    if (!pop) return;
    
    pthread_mutex_lock(&pop->lock);
    for (int i = 0; i < pop->size; i++) {
        individual_free((int64_t)pop->individuals[i]);
    }
    free(pop->individuals);
    pthread_mutex_unlock(&pop->lock);
    pthread_mutex_destroy(&pop->lock);
    free(pop);
}

// --------------------------------------------------------------------------
// 30.2 Multi-Objective (NSGA-II)
// --------------------------------------------------------------------------

// Check if ind1 dominates ind2 (for multi-objective)
int64_t nsga_dominates(int64_t ind1_ptr, int64_t ind2_ptr, int64_t objectives_ptr) {
    Individual* ind1 = (Individual*)ind1_ptr;
    Individual* ind2 = (Individual*)ind2_ptr;
    SxVec* objectives = (SxVec*)objectives_ptr;
    if (!ind1 || !ind2) return 0;
    
    int better_in_any = 0;
    int worse_in_any = 0;
    
    // Compare on each objective (stored in genes)
    int num_obj = objectives ? (int)objectives->len : 1;
    for (int i = 0; i < num_obj && i < ind1->gene_count; i++) {
        if (ind1->genes[i] > ind2->genes[i]) better_in_any = 1;
        if (ind1->genes[i] < ind2->genes[i]) worse_in_any = 1;
    }
    
    return (better_in_any && !worse_in_any) ? 1 : 0;
}

// Calculate crowding distance
int64_t nsga_crowding_distance(int64_t pop_ptr) {
    Population* pop = (Population*)pop_ptr;
    if (!pop) return 0;
    
    // Simple implementation: distance to neighbors in objective space
    for (int i = 0; i < pop->size; i++) {
        pop->individuals[i]->crowding_distance = 0;
    }
    
    // For each objective
    for (int obj = 0; obj < pop->gene_count; obj++) {
        // Sort by this objective
        for (int i = 0; i < pop->size - 1; i++) {
            for (int j = 0; j < pop->size - i - 1; j++) {
                if (pop->individuals[j]->genes[obj] > pop->individuals[j+1]->genes[obj]) {
                    Individual* temp = pop->individuals[j];
                    pop->individuals[j] = pop->individuals[j+1];
                    pop->individuals[j+1] = temp;
                }
            }
        }
        
        // Boundary individuals get infinite distance
        pop->individuals[0]->crowding_distance = 1e9;
        pop->individuals[pop->size-1]->crowding_distance = 1e9;
        
        // Middle individuals
        double range = pop->individuals[pop->size-1]->genes[obj] - 
                       pop->individuals[0]->genes[obj];
        if (range > 0) {
            for (int i = 1; i < pop->size - 1; i++) {
                double dist = (pop->individuals[i+1]->genes[obj] - 
                              pop->individuals[i-1]->genes[obj]) / range;
                pop->individuals[i]->crowding_distance += dist;
            }
        }
    }
    
    return 1;
}

// --------------------------------------------------------------------------
// 30.5 Fitness Landscape Analysis
// --------------------------------------------------------------------------

// Calculate autocorrelation (simplified)
double landscape_autocorrelation(int64_t pop_ptr, int64_t steps) {
    Population* pop = (Population*)pop_ptr;
    if (!pop || pop->size < 2) return 0.0;
    
    double mean = 0;
    for (int i = 0; i < pop->size; i++) {
        mean += pop->individuals[i]->fitness;
    }
    mean /= pop->size;
    
    double var = 0;
    for (int i = 0; i < pop->size; i++) {
        double d = pop->individuals[i]->fitness - mean;
        var += d * d;
    }
    var /= pop->size;
    
    if (var < 1e-10) return 1.0;
    
    // Simple lag-1 autocorrelation
    double cov = 0;
    for (int i = 0; i < pop->size - 1; i++) {
        cov += (pop->individuals[i]->fitness - mean) * 
               (pop->individuals[i+1]->fitness - mean);
    }
    cov /= (pop->size - 1);
    
    return cov / var;
}

// Additional Phase 30 wrapper functions

// Clone an individual
int64_t individual_clone(int64_t ind_ptr) {
    Individual* ind = (Individual*)ind_ptr;
    if (!ind) return 0;

    Individual* clone = (Individual*)individual_new(ind->gene_count);
    if (!clone) return 0;

    clone->fitness = ind->fitness;
    clone->crowding_distance = ind->crowding_distance;
    clone->rank = ind->rank;
    for (int i = 0; i < ind->gene_count; i++) {
        clone->genes[i] = ind->genes[i];
    }

    return (int64_t)clone;
}

// Get gene count
int64_t individual_gene_count(int64_t ind_ptr) {
    Individual* ind = (Individual*)ind_ptr;
    return ind ? ind->gene_count : 0;
}

// Add individual to population
int64_t population_add(int64_t pop_ptr, int64_t ind_ptr) {
    Population* pop = (Population*)pop_ptr;
    Individual* ind = (Individual*)ind_ptr;
    if (!pop || !ind) return 0;

    pthread_mutex_lock(&pop->lock);
    if (pop->size >= pop->capacity) {
        pop->capacity *= 2;
        pop->individuals = (Individual**)realloc(pop->individuals, pop->capacity * sizeof(Individual*));
    }
    pop->individuals[pop->size] = ind;
    pop->size++;
    pthread_mutex_unlock(&pop->lock);
    return 1;
}

// Get best individual (highest fitness)
int64_t population_best(int64_t pop_ptr) {
    Population* pop = (Population*)pop_ptr;
    if (!pop || pop->size == 0) return 0;

    Individual* best = pop->individuals[0];
    for (int i = 1; i < pop->size; i++) {
        if (pop->individuals[i]->fitness > best->fitness) {
            best = pop->individuals[i];
        }
    }
    return (int64_t)best;
}

// Roulette wheel selection
int64_t selection_roulette(int64_t pop_ptr) {
    Population* pop = (Population*)pop_ptr;
    if (!pop || pop->size == 0) return 0;

    // Calculate total fitness
    double total = 0;
    for (int i = 0; i < pop->size; i++) {
        total += pop->individuals[i]->fitness;
    }

    if (total <= 0) {
        // Random selection if no positive fitness
        int idx = rand() % pop->size;
        return (int64_t)pop->individuals[idx];
    }

    // Spin the wheel
    double spin = (double)rand() / RAND_MAX * total;
    double cumulative = 0;
    for (int i = 0; i < pop->size; i++) {
        cumulative += pop->individuals[i]->fitness;
        if (cumulative >= spin) {
            return (int64_t)pop->individuals[i];
        }
    }
    return (int64_t)pop->individuals[pop->size - 1];
}

// Rank-based selection
int64_t selection_rank(int64_t pop_ptr) {
    Population* pop = (Population*)pop_ptr;
    if (!pop || pop->size == 0) return 0;

    // Sort by fitness (bubble sort for simplicity)
    for (int i = 0; i < pop->size - 1; i++) {
        for (int j = 0; j < pop->size - i - 1; j++) {
            if (pop->individuals[j]->fitness > pop->individuals[j+1]->fitness) {
                Individual* temp = pop->individuals[j];
                pop->individuals[j] = pop->individuals[j+1];
                pop->individuals[j+1] = temp;
            }
        }
    }

    // Select based on rank (higher rank = better)
    double total = pop->size * (pop->size + 1) / 2.0;
    double spin = (double)rand() / RAND_MAX * total;
    double cumulative = 0;
    for (int i = 0; i < pop->size; i++) {
        cumulative += (i + 1);
        if (cumulative >= spin) {
            return (int64_t)pop->individuals[i];
        }
    }
    return (int64_t)pop->individuals[pop->size - 1];
}

// Two-point crossover
int64_t crossover_two_point(int64_t parent1_ptr, int64_t parent2_ptr) {
    Individual* p1 = (Individual*)parent1_ptr;
    Individual* p2 = (Individual*)parent2_ptr;
    if (!p1 || !p2 || p1->gene_count != p2->gene_count) return 0;

    Individual* child = (Individual*)individual_new(p1->gene_count);
    if (!child) return 0;

    int point1 = rand() % p1->gene_count;
    int point2 = rand() % p1->gene_count;
    if (point1 > point2) { int t = point1; point1 = point2; point2 = t; }

    for (int i = 0; i < p1->gene_count; i++) {
        if (i >= point1 && i < point2) {
            child->genes[i] = p2->genes[i];
        } else {
            child->genes[i] = p1->genes[i];
        }
    }
    return (int64_t)child;
}

// Uniform mutation
int64_t mutation_uniform(int64_t ind_ptr, double min_val, double max_val) {
    Individual* ind = (Individual*)ind_ptr;
    if (!ind) return 0;

    double mutation_rate = 0.1;  // Default 10%
    for (int i = 0; i < ind->gene_count; i++) {
        if ((double)rand() / RAND_MAX < mutation_rate) {
            ind->genes[i] = min_val + (double)rand() / RAND_MAX * (max_val - min_val);
        }
    }
    return 1;
}

// Bit-flip mutation (for binary-coded genes)
int64_t mutation_bit_flip(int64_t ind_ptr, double rate) {
    Individual* ind = (Individual*)ind_ptr;
    if (!ind) return 0;

    for (int i = 0; i < ind->gene_count; i++) {
        if ((double)rand() / RAND_MAX < rate) {
            // Flip gene from 0<->1 or invert in [0,1] range
            ind->genes[i] = 1.0 - ind->genes[i];
        }
    }
    return 1;
}

// NSGA-II algorithm state
typedef struct NSGA2 {
    int pop_size;
    int num_objectives;
    Population* population;
    int* objective_types;  // 0 = minimize, 1 = maximize
    pthread_mutex_t lock;
} NSGA2;

// Create NSGA-II optimizer
int64_t nsga2_new(int64_t pop_size, int64_t num_objectives) {
    NSGA2* nsga = (NSGA2*)malloc(sizeof(NSGA2));
    if (!nsga) return 0;

    nsga->pop_size = (int)pop_size;
    nsga->num_objectives = (int)num_objectives;
    nsga->objective_types = (int*)calloc(num_objectives, sizeof(int));
    nsga->population = (Population*)population_new(pop_size, (int)num_objectives);
    pthread_mutex_init(&nsga->lock, NULL);

    return (int64_t)nsga;
}

// Set objective type (0=minimize, 1=maximize)
int64_t nsga2_set_objective(int64_t nsga_ptr, int64_t obj_idx, int64_t obj_type) {
    NSGA2* nsga = (NSGA2*)nsga_ptr;
    if (!nsga || obj_idx < 0 || obj_idx >= nsga->num_objectives) return 0;
    nsga->objective_types[obj_idx] = (int)obj_type;
    return 1;
}

// Run evolution for n generations
int64_t nsga2_evolve(int64_t nsga_ptr, int64_t generations) {
    NSGA2* nsga = (NSGA2*)nsga_ptr;
    if (!nsga) return -1;

    pthread_mutex_lock(&nsga->lock);
    // Simple mock evolution loop
    for (int gen = 0; gen < generations; gen++) {
        // In real implementation: selection, crossover, mutation, non-dominated sorting
        nsga_crowding_distance((int64_t)nsga->population);
    }
    pthread_mutex_unlock(&nsga->lock);

    return generations;
}

// Get Pareto front (non-dominated solutions)
int64_t nsga2_pareto_front(int64_t nsga_ptr) {
    NSGA2* nsga = (NSGA2*)nsga_ptr;
    if (!nsga) return 0;

    // Return vector of non-dominated individuals
    SxVec* front = intrinsic_vec_new();
    for (int i = 0; i < nsga->population->size; i++) {
        if (nsga->population->individuals[i]->rank == 0) {
            intrinsic_vec_push(front, (void*)(int64_t)nsga->population->individuals[i]);
        }
    }
    return (int64_t)front;
}

// Close NSGA-II
void nsga2_close(int64_t nsga_ptr) {
    NSGA2* nsga = (NSGA2*)nsga_ptr;
    if (!nsga) return;

    pthread_mutex_lock(&nsga->lock);
    if (nsga->population) population_close((int64_t)nsga->population);
    free(nsga->objective_types);
    pthread_mutex_unlock(&nsga->lock);
    pthread_mutex_destroy(&nsga->lock);
    free(nsga);
}

// ============================================================================
// Phase 31: Distributed Intelligence
// ============================================================================

// --------------------------------------------------------------------------
// 31.1-31.3 Consensus Protocols
// --------------------------------------------------------------------------

typedef enum {
    CONSENSUS_NONE = 0,
    CONSENSUS_PAXOS = 1,
    CONSENSUS_RAFT = 2,
    CONSENSUS_PBFT = 3
} ConsensusType;

typedef enum {
    RAFT_FOLLOWER = 0,
    RAFT_CANDIDATE = 1,
    RAFT_LEADER = 2
} RaftState;

typedef struct ConsensusNode {
    char* node_id;
    ConsensusType protocol;
    RaftState state;  // For Raft
    int64_t current_term;
    char* voted_for;
    int64_t commit_index;
    int64_t last_applied;
    int64_t* log_terms;
    char** log_entries;
    int log_count;
    int log_capacity;
    double* weights;  // For weighted voting
    int node_count;
    pthread_mutex_t lock;
} ConsensusNode;

// Create consensus node (internal version with string ID)
int64_t consensus_new_with_id(int64_t id_ptr, int64_t protocol) {
    SxString* id = (SxString*)id_ptr;
    if (!id) return 0;

    ConsensusNode* node = (ConsensusNode*)malloc(sizeof(ConsensusNode));
    if (!node) return 0;

    node->node_id = strdup(id->data);
    node->protocol = (ConsensusType)protocol;
    node->state = RAFT_FOLLOWER;
    node->current_term = 0;
    node->voted_for = NULL;
    node->commit_index = 0;
    node->last_applied = 0;
    node->log_capacity = 256;
    node->log_terms = (int64_t*)calloc(node->log_capacity, sizeof(int64_t));
    node->log_entries = (char**)calloc(node->log_capacity, sizeof(char*));
    node->log_count = 0;
    node->weights = NULL;
    node->node_count = 0;
    pthread_mutex_init(&node->lock, NULL);

    return (int64_t)node;
}

// Append to log
int64_t consensus_append(int64_t node_ptr, int64_t entry_ptr) {
    ConsensusNode* node = (ConsensusNode*)node_ptr;
    SxString* entry = (SxString*)entry_ptr;
    if (!node || !entry) return -1;
    
    pthread_mutex_lock(&node->lock);
    
    if (node->log_count >= node->log_capacity) {
        node->log_capacity *= 2;
        node->log_terms = (int64_t*)realloc(node->log_terms, 
                                            node->log_capacity * sizeof(int64_t));
        node->log_entries = (char**)realloc(node->log_entries,
                                            node->log_capacity * sizeof(char*));
    }
    
    node->log_terms[node->log_count] = node->current_term;
    node->log_entries[node->log_count] = strdup(entry->data);
    int64_t idx = node->log_count;
    node->log_count++;
    
    pthread_mutex_unlock(&node->lock);
    return idx;
}

// Get current term
int64_t consensus_term(int64_t node_ptr) {
    ConsensusNode* node = (ConsensusNode*)node_ptr;
    return node ? node->current_term : 0;
}

// Increment term
int64_t consensus_increment_term(int64_t node_ptr) {
    ConsensusNode* node = (ConsensusNode*)node_ptr;
    if (!node) return 0;
    
    pthread_mutex_lock(&node->lock);
    node->current_term++;
    if (node->voted_for) {
        free(node->voted_for);
        node->voted_for = NULL;
    }
    pthread_mutex_unlock(&node->lock);
    
    return node->current_term;
}

// Set state (Raft)
int64_t consensus_set_state(int64_t node_ptr, int64_t state) {
    ConsensusNode* node = (ConsensusNode*)node_ptr;
    if (!node) return 0;
    node->state = (RaftState)state;
    return 1;
}

// Get state
int64_t consensus_get_state(int64_t node_ptr) {
    ConsensusNode* node = (ConsensusNode*)node_ptr;
    return node ? (int64_t)node->state : -1;
}

// Commit all pending entries (1-arg version)
int64_t consensus_commit(int64_t node_ptr) {
    ConsensusNode* node = (ConsensusNode*)node_ptr;
    if (!node) return -1;

    pthread_mutex_lock(&node->lock);
    // Commit all pending entries
    node->commit_index = node->log_count > 0 ? node->log_count - 1 : 0;
    pthread_mutex_unlock(&node->lock);

    return node->commit_index;
}

// Commit up to specific index
int64_t consensus_commit_to(int64_t node_ptr, int64_t index) {
    ConsensusNode* node = (ConsensusNode*)node_ptr;
    if (!node || index < 0 || index >= node->log_count) return 0;

    pthread_mutex_lock(&node->lock);
    if (index > node->commit_index) {
        node->commit_index = index;
    }
    pthread_mutex_unlock(&node->lock);

    return 1;
}

// Get commit index
int64_t consensus_commit_index(int64_t node_ptr) {
    ConsensusNode* node = (ConsensusNode*)node_ptr;
    return node ? node->commit_index : -1;
}

// Get log count
int64_t consensus_log_count(int64_t node_ptr) {
    ConsensusNode* node = (ConsensusNode*)node_ptr;
    return node ? node->log_count : 0;
}

// Protocol constants
int64_t consensus_paxos(void) { return CONSENSUS_PAXOS; }
int64_t consensus_raft(void) { return CONSENSUS_RAFT; }
int64_t consensus_pbft(void) { return CONSENSUS_PBFT; }

// State constants
int64_t raft_follower(void) { return RAFT_FOLLOWER; }
int64_t raft_candidate(void) { return RAFT_CANDIDATE; }
int64_t raft_leader(void) { return RAFT_LEADER; }

// Close consensus node
void consensus_close(int64_t node_ptr) {
    ConsensusNode* node = (ConsensusNode*)node_ptr;
    if (!node) return;
    
    pthread_mutex_lock(&node->lock);
    free(node->node_id);
    if (node->voted_for) free(node->voted_for);
    free(node->log_terms);
    for (int i = 0; i < node->log_count; i++) {
        free(node->log_entries[i]);
    }
    free(node->log_entries);
    if (node->weights) free(node->weights);
    pthread_mutex_unlock(&node->lock);
    pthread_mutex_destroy(&node->lock);
    free(node);
}

// --------------------------------------------------------------------------
// 31.4 Stigmergy
// --------------------------------------------------------------------------

typedef struct Pheromone {
    double* grid;
    int width;
    int height;
    double evaporation_rate;
    pthread_mutex_t lock;
} Pheromone;

// Create pheromone grid (with default evap_rate)
int64_t pheromone_new(int64_t width, int64_t height) {
    Pheromone* p = (Pheromone*)malloc(sizeof(Pheromone));
    if (!p) return 0;

    p->width = (int)width;
    p->height = (int)height;
    p->grid = (double*)calloc(width * height, sizeof(double));
    p->evaporation_rate = 0.1;  // default
    pthread_mutex_init(&p->lock, NULL);

    return (int64_t)p;
}

// Deposit pheromone
int64_t pheromone_deposit(int64_t p_ptr, int64_t x, int64_t y, double amount) {
    Pheromone* p = (Pheromone*)p_ptr;
    if (!p || x < 0 || x >= p->width || y < 0 || y >= p->height) return 0;
    
    pthread_mutex_lock(&p->lock);
    p->grid[y * p->width + x] += amount;
    pthread_mutex_unlock(&p->lock);
    
    return 1;
}

// Read pheromone level
double pheromone_read(int64_t p_ptr, int64_t x, int64_t y) {
    Pheromone* p = (Pheromone*)p_ptr;
    if (!p || x < 0 || x >= p->width || y < 0 || y >= p->height) return 0.0;
    return p->grid[y * p->width + x];
}

// Evaporate pheromones
int64_t pheromone_evaporate(int64_t p_ptr) {
    Pheromone* p = (Pheromone*)p_ptr;
    if (!p) return 0;
    
    pthread_mutex_lock(&p->lock);
    for (int i = 0; i < p->width * p->height; i++) {
        p->grid[i] *= (1.0 - p->evaporation_rate);
    }
    pthread_mutex_unlock(&p->lock);
    
    return 1;
}

// Close pheromone grid
void pheromone_close(int64_t p_ptr) {
    Pheromone* p = (Pheromone*)p_ptr;
    if (!p) return;
    
    pthread_mutex_lock(&p->lock);
    free(p->grid);
    pthread_mutex_unlock(&p->lock);
    pthread_mutex_destroy(&p->lock);
    free(p);
}

// --------------------------------------------------------------------------
// 31.5 Swarm Optimization (PSO)
// --------------------------------------------------------------------------

typedef struct Particle {
    double* position;
    double* velocity;
    double* best_position;
    double best_fitness;
    int dim;
} Particle;

typedef struct Swarm {
    Particle** particles;
    int size;
    double* global_best;
    double global_best_fitness;
    double inertia;
    double cognitive;
    double social;
    int dim;
    pthread_mutex_t lock;
} Swarm;

// Create swarm
int64_t swarm_new(int64_t size, int64_t dim) {
    Swarm* s = (Swarm*)malloc(sizeof(Swarm));
    if (!s) return 0;
    
    s->size = (int)size;
    s->dim = (int)dim;
    s->particles = (Particle**)malloc(size * sizeof(Particle*));
    s->global_best = (double*)calloc(dim, sizeof(double));
    s->global_best_fitness = -1e9;
    s->inertia = 0.7;
    s->cognitive = 1.5;
    s->social = 1.5;
    pthread_mutex_init(&s->lock, NULL);
    
    for (int i = 0; i < size; i++) {
        Particle* p = (Particle*)malloc(sizeof(Particle));
        p->position = (double*)malloc(dim * sizeof(double));
        p->velocity = (double*)calloc(dim, sizeof(double));
        p->best_position = (double*)malloc(dim * sizeof(double));
        p->best_fitness = -1e9;
        p->dim = (int)dim;
        
        // Random initialization
        for (int j = 0; j < dim; j++) {
            p->position[j] = (double)rand() / RAND_MAX;
            p->best_position[j] = p->position[j];
        }
        
        s->particles[i] = p;
    }
    
    return (int64_t)s;
}

// Get particle position
double swarm_get_position(int64_t swarm_ptr, int64_t particle_idx, int64_t dim_idx) {
    Swarm* s = (Swarm*)swarm_ptr;
    if (!s || particle_idx < 0 || particle_idx >= s->size || 
        dim_idx < 0 || dim_idx >= s->dim) return 0.0;
    return s->particles[particle_idx]->position[dim_idx];
}

// Update particle fitness
int64_t swarm_update_fitness(int64_t swarm_ptr, int64_t particle_idx, double fitness) {
    Swarm* s = (Swarm*)swarm_ptr;
    if (!s || particle_idx < 0 || particle_idx >= s->size) return 0;
    
    pthread_mutex_lock(&s->lock);
    
    Particle* p = s->particles[particle_idx];
    
    // Update personal best
    if (fitness > p->best_fitness) {
        p->best_fitness = fitness;
        memcpy(p->best_position, p->position, p->dim * sizeof(double));
    }
    
    // Update global best
    if (fitness > s->global_best_fitness) {
        s->global_best_fitness = fitness;
        memcpy(s->global_best, p->position, s->dim * sizeof(double));
    }
    
    pthread_mutex_unlock(&s->lock);
    return 1;
}

// Update particle velocity and position
int64_t swarm_update_particle(int64_t swarm_ptr, int64_t particle_idx) {
    Swarm* s = (Swarm*)swarm_ptr;
    if (!s || particle_idx < 0 || particle_idx >= s->size) return 0;
    
    pthread_mutex_lock(&s->lock);
    
    Particle* p = s->particles[particle_idx];
    
    for (int d = 0; d < p->dim; d++) {
        double r1 = (double)rand() / RAND_MAX;
        double r2 = (double)rand() / RAND_MAX;
        
        p->velocity[d] = s->inertia * p->velocity[d] +
                        s->cognitive * r1 * (p->best_position[d] - p->position[d]) +
                        s->social * r2 * (s->global_best[d] - p->position[d]);
        
        p->position[d] += p->velocity[d];
        
        // Clamp to [0, 1]
        if (p->position[d] < 0) p->position[d] = 0;
        if (p->position[d] > 1) p->position[d] = 1;
    }
    
    pthread_mutex_unlock(&s->lock);
    return 1;
}

// Get global best fitness
double swarm_best_fitness(int64_t swarm_ptr) {
    Swarm* s = (Swarm*)swarm_ptr;
    return s ? s->global_best_fitness : -1e9;
}

// Get swarm size
int64_t swarm_size(int64_t swarm_ptr) {
    Swarm* s = (Swarm*)swarm_ptr;
    return s ? s->size : 0;
}

// Close swarm
void swarm_close(int64_t swarm_ptr) {
    Swarm* s = (Swarm*)swarm_ptr;
    if (!s) return;
    
    pthread_mutex_lock(&s->lock);
    
    for (int i = 0; i < s->size; i++) {
        Particle* p = s->particles[i];
        free(p->position);
        free(p->velocity);
        free(p->best_position);
        free(p);
    }
    free(s->particles);
    free(s->global_best);
    
    pthread_mutex_unlock(&s->lock);
    pthread_mutex_destroy(&s->lock);
    free(s);
}

// --------------------------------------------------------------------------
// 31.6 Collective Decision Making
// --------------------------------------------------------------------------

typedef struct P31Vote {
    char* voter_id;
    int64_t* preferences;  // Ranked preferences
    int pref_count;
    double weight;
} P31Vote;

typedef struct VotingSystem {
    P31Vote** votes;
    int vote_count;
    int vote_capacity;
    char** options;
    int option_count;
    pthread_mutex_t lock;
} VotingSystem;

// Create voting system (type: 0=plurality, 1=ranked, etc.)
int64_t voting_new(int64_t type) {
    (void)type;  // Currently ignored, always plurality
    VotingSystem* vs = (VotingSystem*)malloc(sizeof(VotingSystem));
    if (!vs) return 0;
    
    vs->vote_capacity = 64;
    vs->votes = (P31Vote**)calloc(vs->vote_capacity, sizeof(P31Vote*));
    vs->vote_count = 0;
    vs->options = NULL;
    vs->option_count = 0;
    pthread_mutex_init(&vs->lock, NULL);
    
    return (int64_t)vs;
}

// Add option
int64_t voting_add_option(int64_t vs_ptr, int64_t option_ptr) {
    VotingSystem* vs = (VotingSystem*)vs_ptr;
    SxString* option = (SxString*)option_ptr;
    if (!vs || !option) return -1;
    
    pthread_mutex_lock(&vs->lock);
    
    vs->options = (char**)realloc(vs->options, (vs->option_count + 1) * sizeof(char*));
    vs->options[vs->option_count] = strdup(option->data);
    int64_t id = vs->option_count;
    vs->option_count++;
    
    pthread_mutex_unlock(&vs->lock);
    return id;
}

// Cast vote
int64_t voting_cast(int64_t vs_ptr, int64_t voter_id, int64_t choice) {
    VotingSystem* vs = (VotingSystem*)vs_ptr;
    if (!vs || choice < 0 || choice >= vs->option_count) return 0;

    pthread_mutex_lock(&vs->lock);

    if (vs->vote_count >= vs->vote_capacity) {
        vs->vote_capacity *= 2;
        vs->votes = (P31Vote**)realloc(vs->votes, vs->vote_capacity * sizeof(P31Vote*));
    }

    P31Vote* vote = (P31Vote*)malloc(sizeof(P31Vote));
    vote->voter_id = NULL;  // Just use numeric voter_id
    vote->preferences = (int64_t*)malloc(sizeof(int64_t));
    vote->preferences[0] = choice;
    vote->pref_count = 1;
    vote->weight = 1.0;
    
    vs->votes[vs->vote_count] = vote;
    vs->vote_count++;
    
    pthread_mutex_unlock(&vs->lock);
    return 1;
}

// Tally votes (plurality)
int64_t voting_tally(int64_t vs_ptr) {
    VotingSystem* vs = (VotingSystem*)vs_ptr;
    if (!vs || vs->option_count == 0) return -1;
    
    pthread_mutex_lock(&vs->lock);
    
    double* scores = (double*)calloc(vs->option_count, sizeof(double));
    
    for (int i = 0; i < vs->vote_count; i++) {
        P31Vote* vote = vs->votes[i];
        if (vote->pref_count > 0) {
            int64_t choice = vote->preferences[0];
            if (choice >= 0 && choice < vs->option_count) {
                scores[choice] += vote->weight;
            }
        }
    }
    
    int64_t winner = 0;
    double max_score = scores[0];
    for (int i = 1; i < vs->option_count; i++) {
        if (scores[i] > max_score) {
            max_score = scores[i];
            winner = i;
        }
    }
    
    free(scores);
    pthread_mutex_unlock(&vs->lock);
    
    return winner;
}

// Get vote count
int64_t voting_count(int64_t vs_ptr) {
    VotingSystem* vs = (VotingSystem*)vs_ptr;
    return vs ? vs->vote_count : 0;
}

// Get winner (option_id with most votes)
int64_t voting_winner(int64_t vs_ptr) {
    VotingSystem* vs = (VotingSystem*)vs_ptr;
    if (!vs || vs->option_count == 0) return -1;

    pthread_mutex_lock(&vs->lock);

    int64_t* counts = (int64_t*)calloc(vs->option_count, sizeof(int64_t));

    for (int i = 0; i < vs->vote_count; i++) {
        P31Vote* vote = vs->votes[i];
        if (vote->pref_count > 0) {
            int64_t choice = vote->preferences[0];
            if (choice >= 0 && choice < vs->option_count) {
                counts[choice]++;
            }
        }
    }

    int64_t winner = 0;
    int64_t max_count = counts[0];
    for (int i = 1; i < vs->option_count; i++) {
        if (counts[i] > max_count) {
            max_count = counts[i];
            winner = i;
        }
    }

    free(counts);
    pthread_mutex_unlock(&vs->lock);

    return winner;
}

// Get result (vote count) for specific option
int64_t voting_result(int64_t vs_ptr, int64_t option_id) {
    VotingSystem* vs = (VotingSystem*)vs_ptr;
    if (!vs || option_id < 0 || option_id >= vs->option_count) return -1;

    pthread_mutex_lock(&vs->lock);

    int64_t count = 0;
    for (int i = 0; i < vs->vote_count; i++) {
        P31Vote* vote = vs->votes[i];
        if (vote->pref_count > 0 && vote->preferences[0] == option_id) {
            count++;
        }
    }

    pthread_mutex_unlock(&vs->lock);
    return count;
}

// Close voting system
void voting_close(int64_t vs_ptr) {
    VotingSystem* vs = (VotingSystem*)vs_ptr;
    if (!vs) return;

    pthread_mutex_lock(&vs->lock);

    for (int i = 0; i < vs->vote_count; i++) {
        P31Vote* v = vs->votes[i];
        if (v->voter_id) free(v->voter_id);
        free(v->preferences);
        free(v);
    }
    free(vs->votes);

    for (int i = 0; i < vs->option_count; i++) {
        free(vs->options[i]);
    }
    if (vs->options) free(vs->options);

    pthread_mutex_unlock(&vs->lock);
    pthread_mutex_destroy(&vs->lock);
    free(vs);
}

// ==========================================================================
// Phase 31 Wrapper Functions for Simplified Test API
// ==========================================================================

// Wrapper: consensus_new with just protocol ID (auto-generate node ID)
int64_t consensus_new(int64_t protocol) {
    // Simple version: just create with generated ID
    char id[32];
    snprintf(id, sizeof(id), "node_%lld", (long long)protocol);
    ConsensusNode* node = (ConsensusNode*)malloc(sizeof(ConsensusNode));
    if (!node) return 0;
    node->node_id = strdup(id);
    node->protocol = (ConsensusType)protocol;
    node->state = RAFT_FOLLOWER;
    node->current_term = 0;
    node->voted_for = NULL;
    node->commit_index = 0;
    node->last_applied = 0;
    node->log_capacity = 256;
    node->log_terms = (int64_t*)calloc(node->log_capacity, sizeof(int64_t));
    node->log_entries = (char**)calloc(node->log_capacity, sizeof(char*));
    node->log_count = 0;
    node->weights = NULL;
    node->node_count = 0;
    pthread_mutex_init(&node->lock, NULL);
    return (int64_t)node;
}

// Propose a value
int64_t consensus_propose(int64_t node_ptr, int64_t value_ptr) {
    return consensus_append(node_ptr, value_ptr);
}

// Accept a proposal
int64_t consensus_accept(int64_t node_ptr, int64_t proposal_id) {
    ConsensusNode* node = (ConsensusNode*)node_ptr;
    if (!node || proposal_id < 0 || proposal_id >= node->log_count) return -1;
    // Mark as accepted (no-op in simple impl)
    return proposal_id;
}

// Commit (wrapper for consensus_commit with auto index)
int64_t consensus_commit_all(int64_t node_ptr);

// Get status
int64_t consensus_status(int64_t node_ptr) {
    ConsensusNode* node = (ConsensusNode*)node_ptr;
    if (!node) return -1;
    // Return state as status
    return (int64_t)node->state;
}



// Swarm wrapper functions
int64_t swarm_set_position(int64_t swarm_ptr, int64_t particle_idx, int64_t dim_idx, double value) {
    Swarm* swarm = (Swarm*)swarm_ptr;
    if (!swarm || particle_idx < 0 || particle_idx >= swarm->size) return 0;
    if (dim_idx < 0 || dim_idx >= swarm->dim) return 0;

    pthread_mutex_lock(&swarm->lock);
    swarm->particles[particle_idx]->position[dim_idx] = value;
    pthread_mutex_unlock(&swarm->lock);
    return 1;
}

int64_t swarm_set_velocity(int64_t swarm_ptr, int64_t particle_idx, int64_t dim_idx, double value) {
    Swarm* swarm = (Swarm*)swarm_ptr;
    if (!swarm || particle_idx < 0 || particle_idx >= swarm->size) return 0;
    if (dim_idx < 0 || dim_idx >= swarm->dim) return 0;

    pthread_mutex_lock(&swarm->lock);
    swarm->particles[particle_idx]->velocity[dim_idx] = value;
    pthread_mutex_unlock(&swarm->lock);
    return 1;
}

int64_t swarm_update(int64_t swarm_ptr) {
    Swarm* swarm = (Swarm*)swarm_ptr;
    if (!swarm) return 0;

    pthread_mutex_lock(&swarm->lock);
    // Simple PSO update
    double w = 0.7;   // inertia
    double c1 = 1.5;  // cognitive
    double c2 = 1.5;  // social

    for (int i = 0; i < swarm->size; i++) {
        Particle* p = swarm->particles[i];
        for (int d = 0; d < swarm->dim; d++) {
            double r1 = (double)rand() / RAND_MAX;
            double r2 = (double)rand() / RAND_MAX;

            p->velocity[d] = w * p->velocity[d]
                + c1 * r1 * (p->best_position[d] - p->position[d])
                + c2 * r2 * (swarm->global_best[d] - p->position[d]);

            p->position[d] += p->velocity[d];
            // Clamp to [0, 1]
            if (p->position[d] < 0) p->position[d] = 0;
            if (p->position[d] > 1) p->position[d] = 1;
        }
    }
    pthread_mutex_unlock(&swarm->lock);
    return 1;
}

int64_t swarm_best_particle(int64_t swarm_ptr) {
    Swarm* swarm = (Swarm*)swarm_ptr;
    if (!swarm || swarm->size == 0) return -1;

    int best = 0;
    double best_fitness = swarm->particles[0]->best_fitness;
    for (int i = 1; i < swarm->size; i++) {
        if (swarm->particles[i]->best_fitness > best_fitness) {
            best_fitness = swarm->particles[i]->best_fitness;
            best = i;
        }
    }
    return best;
}

// ============================================
// Generator/Iterator Support
// ============================================

typedef struct Generator {
    int64_t* values;
    int capacity;
    int count;
    int current;
    pthread_mutex_t lock;
} Generator;

// Create a new generator (returns handle)
int64_t generator_new(int64_t initial_capacity) {
    Generator* gen = (Generator*)malloc(sizeof(Generator));
    if (!gen) return 0;

    gen->capacity = initial_capacity > 0 ? (int)initial_capacity : 16;
    gen->values = (int64_t*)malloc(sizeof(int64_t) * gen->capacity);
    if (!gen->values) {
        free(gen);
        return 0;
    }

    gen->count = 0;
    gen->current = 0;
    pthread_mutex_init(&gen->lock, NULL);
    return (int64_t)gen;
}

// Yield a value (for simple iterator use - stores value)
int64_t generator_yield(int64_t value) {
    // For simple use, just return the value
    // In a full implementation, this would store state in a coroutine context
    return value;
}

// Get next value from generator
int64_t generator_next(int64_t gen_ptr) {
    Generator* gen = (Generator*)gen_ptr;
    if (!gen) return 0;

    pthread_mutex_lock(&gen->lock);
    int64_t result = 0;
    if (gen->current < gen->count) {
        result = gen->values[gen->current++];
    }
    pthread_mutex_unlock(&gen->lock);
    return result;
}

// Check if generator has more values
int64_t generator_has_next(int64_t gen_ptr) {
    Generator* gen = (Generator*)gen_ptr;
    if (!gen) return 0;

    pthread_mutex_lock(&gen->lock);
    int64_t result = gen->current < gen->count ? 1 : 0;
    pthread_mutex_unlock(&gen->lock);
    return result;
}

// Push a value to generator (for building iterators)
int64_t generator_push(int64_t gen_ptr, int64_t value) {
    Generator* gen = (Generator*)gen_ptr;
    if (!gen) return 0;

    pthread_mutex_lock(&gen->lock);

    // Grow if needed
    if (gen->count >= gen->capacity) {
        int new_cap = gen->capacity * 2;
        int64_t* new_vals = (int64_t*)realloc(gen->values, sizeof(int64_t) * new_cap);
        if (!new_vals) {
            pthread_mutex_unlock(&gen->lock);
            return 0;
        }
        gen->values = new_vals;
        gen->capacity = new_cap;
    }

    gen->values[gen->count++] = value;
    pthread_mutex_unlock(&gen->lock);
    return 1;
}

// Reset generator to beginning
int64_t generator_reset(int64_t gen_ptr) {
    Generator* gen = (Generator*)gen_ptr;
    if (!gen) return 0;

    pthread_mutex_lock(&gen->lock);
    gen->current = 0;
    pthread_mutex_unlock(&gen->lock);
    return 1;
}

// Get generator count
int64_t generator_count(int64_t gen_ptr) {
    Generator* gen = (Generator*)gen_ptr;
    if (!gen) return 0;
    return gen->count;
}

// Close/free generator
void generator_close(int64_t gen_ptr) {
    Generator* gen = (Generator*)gen_ptr;
    if (!gen) return;

    pthread_mutex_lock(&gen->lock);
    if (gen->values) free(gen->values);
    pthread_mutex_unlock(&gen->lock);
    pthread_mutex_destroy(&gen->lock);
    free(gen);
}

// ============================================================================
// Phase 11: Test Framework
// ============================================================================

typedef struct TestCase {
    char* name;
    int64_t (*fn)(void);
    int64_t passed;
    int64_t failed;
    char* error_msg;
} TestCase;

typedef struct TestRunner {
    TestCase** tests;
    int64_t count;
    int64_t capacity;
    int64_t passed;
    int64_t failed;
    int64_t verbose;
} TestRunner;

// Create new test runner
int64_t test_runner_new(void) {
    TestRunner* runner = (TestRunner*)malloc(sizeof(TestRunner));
    if (!runner) return 0;

    runner->capacity = 64;
    runner->tests = (TestCase**)malloc(sizeof(TestCase*) * runner->capacity);
    if (!runner->tests) {
        free(runner);
        return 0;
    }

    runner->count = 0;
    runner->passed = 0;
    runner->failed = 0;
    runner->verbose = 1;

    return (int64_t)runner;
}

// Add test to runner
int64_t test_runner_add(int64_t runner_ptr, int64_t name_ptr, int64_t fn_ptr) {
    TestRunner* runner = (TestRunner*)runner_ptr;
    SxString* name = (SxString*)name_ptr;
    if (!runner || !name) return 0;

    // Grow if needed
    if (runner->count >= runner->capacity) {
        int64_t new_cap = runner->capacity * 2;
        TestCase** new_tests = (TestCase**)realloc(runner->tests, sizeof(TestCase*) * new_cap);
        if (!new_tests) return 0;
        runner->tests = new_tests;
        runner->capacity = new_cap;
    }

    TestCase* test = (TestCase*)malloc(sizeof(TestCase));
    if (!test) return 0;

    test->name = strdup(name->data);
    test->fn = (int64_t (*)(void))fn_ptr;
    test->passed = 0;
    test->failed = 0;
    test->error_msg = NULL;

    runner->tests[runner->count++] = test;
    return 1;
}

// Run all tests
int64_t test_runner_run(int64_t runner_ptr) {
    TestRunner* runner = (TestRunner*)runner_ptr;
    if (!runner) return 0;

    runner->passed = 0;
    runner->failed = 0;

    printf("\nRunning %lld tests...\n\n", runner->count);

    for (int64_t i = 0; i < runner->count; i++) {
        TestCase* test = runner->tests[i];
        if (runner->verbose) {
            printf("  test %s ... ", test->name);
            fflush(stdout);
        }

        // Run test function (expects 0 for pass, non-zero for fail)
        int64_t result = test->fn();

        if (result == 0) {
            test->passed = 1;
            runner->passed++;
            if (runner->verbose) {
                printf("\033[32mok\033[0m\n");
            }
        } else {
            test->failed = 1;
            runner->failed++;
            if (runner->verbose) {
                printf("\033[31mFAILED\033[0m\n");
            }
        }
    }

    printf("\ntest result: ");
    if (runner->failed == 0) {
        printf("\033[32mok\033[0m");
    } else {
        printf("\033[31mFAILED\033[0m");
    }
    printf(". %lld passed; %lld failed\n\n", runner->passed, runner->failed);

    return runner->failed == 0 ? 0 : 1;
}

// Get pass count
int64_t test_runner_passed(int64_t runner_ptr) {
    TestRunner* runner = (TestRunner*)runner_ptr;
    return runner ? runner->passed : 0;
}

// Get fail count
int64_t test_runner_failed(int64_t runner_ptr) {
    TestRunner* runner = (TestRunner*)runner_ptr;
    return runner ? runner->failed : 0;
}

// Get test count
int64_t test_runner_count(int64_t runner_ptr) {
    TestRunner* runner = (TestRunner*)runner_ptr;
    return runner ? runner->count : 0;
}

// Set verbose mode
int64_t test_runner_set_verbose(int64_t runner_ptr, int64_t verbose) {
    TestRunner* runner = (TestRunner*)runner_ptr;
    if (!runner) return 0;
    runner->verbose = verbose;
    return 1;
}

// Close test runner
void test_runner_close(int64_t runner_ptr) {
    TestRunner* runner = (TestRunner*)runner_ptr;
    if (!runner) return;

    for (int64_t i = 0; i < runner->count; i++) {
        TestCase* test = runner->tests[i];
        if (test->name) free(test->name);
        if (test->error_msg) free(test->error_msg);
        free(test);
    }
    free(runner->tests);
    free(runner);
}

// Assertion helpers that return error codes instead of exiting
int64_t test_assert_true(int64_t value) {
    return value ? 0 : 1;
}

int64_t test_assert_false(int64_t value) {
    return value ? 1 : 0;
}

int64_t test_assert_eq_i64(int64_t left, int64_t right) {
    return left == right ? 0 : 1;
}

int64_t test_assert_ne_i64(int64_t left, int64_t right) {
    return left != right ? 0 : 1;
}

int64_t test_assert_lt_i64(int64_t left, int64_t right) {
    return left < right ? 0 : 1;
}

int64_t test_assert_le_i64(int64_t left, int64_t right) {
    return left <= right ? 0 : 1;
}

int64_t test_assert_gt_i64(int64_t left, int64_t right) {
    return left > right ? 0 : 1;
}

int64_t test_assert_ge_i64(int64_t left, int64_t right) {
    return left >= right ? 0 : 1;
}

int64_t test_assert_eq_str(int64_t left_ptr, int64_t right_ptr) {
    SxString* left = (SxString*)left_ptr;
    SxString* right = (SxString*)right_ptr;

    const char* l = left ? left->data : "";
    const char* r = right ? right->data : "";

    return strcmp(l, r) == 0 ? 0 : 1;
}

int64_t test_assert_contains(int64_t haystack_ptr, int64_t needle_ptr) {
    SxString* haystack = (SxString*)haystack_ptr;
    SxString* needle = (SxString*)needle_ptr;

    if (!haystack || !needle) return 1;

    return strstr(haystack->data, needle->data) != NULL ? 0 : 1;
}

int64_t test_assert_null(int64_t ptr) {
    return ptr == 0 ? 0 : 1;
}

int64_t test_assert_not_null(int64_t ptr) {
    return ptr != 0 ? 0 : 1;
}

// ============================================================================
// Phase 11: Debug Adapter Protocol (DAP)
// ============================================================================

// Breakpoint entry
typedef struct Breakpoint {
    int64_t id;
    char* file;
    int64_t line;
    int64_t enabled;
    char* condition;
    int64_t hit_count;
} Breakpoint;

// Variable entry for inspection
typedef struct DebugVariable {
    char* name;
    char* type;
    char* value;
    int64_t scope;  // 0=local, 1=global, 2=argument
} DebugVariable;

// Stack frame
typedef struct StackFrame {
    int64_t id;
    char* name;
    char* file;
    int64_t line;
    int64_t column;
    DebugVariable** variables;
    int64_t var_count;
} StackFrame;

// Debugger state
typedef struct Debugger {
    Breakpoint** breakpoints;
    int64_t bp_count;
    int64_t bp_capacity;
    int64_t bp_next_id;

    StackFrame** frames;
    int64_t frame_count;
    int64_t frame_capacity;

    int64_t paused;
    int64_t stepping;  // 0=run, 1=step_over, 2=step_into, 3=step_out
    int64_t current_frame;

    char* current_file;
    int64_t current_line;
} Debugger;

static Debugger* g_debugger = NULL;

// Create debugger
int64_t debugger_new(void) {
    Debugger* dbg = (Debugger*)malloc(sizeof(Debugger));
    if (!dbg) return 0;

    dbg->bp_capacity = 64;
    dbg->breakpoints = (Breakpoint**)malloc(sizeof(Breakpoint*) * dbg->bp_capacity);
    dbg->bp_count = 0;
    dbg->bp_next_id = 1;

    dbg->frame_capacity = 64;
    dbg->frames = (StackFrame**)malloc(sizeof(StackFrame*) * dbg->frame_capacity);
    dbg->frame_count = 0;

    dbg->paused = 0;
    dbg->stepping = 0;
    dbg->current_frame = 0;
    dbg->current_file = NULL;
    dbg->current_line = 0;

    g_debugger = dbg;
    return (int64_t)dbg;
}

// Set breakpoint
int64_t debugger_set_breakpoint(int64_t dbg_ptr, int64_t file_ptr, int64_t line) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    SxString* file = (SxString*)file_ptr;
    if (!dbg || !file) return 0;

    // Grow if needed
    if (dbg->bp_count >= dbg->bp_capacity) {
        int64_t new_cap = dbg->bp_capacity * 2;
        Breakpoint** new_bps = (Breakpoint**)realloc(dbg->breakpoints, sizeof(Breakpoint*) * new_cap);
        if (!new_bps) return 0;
        dbg->breakpoints = new_bps;
        dbg->bp_capacity = new_cap;
    }

    Breakpoint* bp = (Breakpoint*)malloc(sizeof(Breakpoint));
    if (!bp) return 0;

    bp->id = dbg->bp_next_id++;
    bp->file = strdup(file->data);
    bp->line = line;
    bp->enabled = 1;
    bp->condition = NULL;
    bp->hit_count = 0;

    dbg->breakpoints[dbg->bp_count++] = bp;
    return bp->id;
}

// Remove breakpoint
int64_t debugger_remove_breakpoint(int64_t dbg_ptr, int64_t bp_id) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    if (!dbg) return 0;

    for (int64_t i = 0; i < dbg->bp_count; i++) {
        if (dbg->breakpoints[i]->id == bp_id) {
            if (dbg->breakpoints[i]->file) free(dbg->breakpoints[i]->file);
            if (dbg->breakpoints[i]->condition) free(dbg->breakpoints[i]->condition);
            free(dbg->breakpoints[i]);

            // Shift remaining
            for (int64_t j = i; j < dbg->bp_count - 1; j++) {
                dbg->breakpoints[j] = dbg->breakpoints[j + 1];
            }
            dbg->bp_count--;
            return 1;
        }
    }
    return 0;
}

// Enable/disable breakpoint
int64_t debugger_enable_breakpoint(int64_t dbg_ptr, int64_t bp_id, int64_t enabled) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    if (!dbg) return 0;

    for (int64_t i = 0; i < dbg->bp_count; i++) {
        if (dbg->breakpoints[i]->id == bp_id) {
            dbg->breakpoints[i]->enabled = enabled;
            return 1;
        }
    }
    return 0;
}

// Set conditional breakpoint
int64_t debugger_set_condition(int64_t dbg_ptr, int64_t bp_id, int64_t cond_ptr) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    SxString* cond = (SxString*)cond_ptr;
    if (!dbg) return 0;

    for (int64_t i = 0; i < dbg->bp_count; i++) {
        if (dbg->breakpoints[i]->id == bp_id) {
            if (dbg->breakpoints[i]->condition) free(dbg->breakpoints[i]->condition);
            dbg->breakpoints[i]->condition = cond ? strdup(cond->data) : NULL;
            return 1;
        }
    }
    return 0;
}

// Get breakpoint count
int64_t debugger_breakpoint_count(int64_t dbg_ptr) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    return dbg ? dbg->bp_count : 0;
}

// Check if at breakpoint
int64_t debugger_at_breakpoint(int64_t dbg_ptr, int64_t file_ptr, int64_t line) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    SxString* file = (SxString*)file_ptr;
    if (!dbg || !file) return 0;

    for (int64_t i = 0; i < dbg->bp_count; i++) {
        Breakpoint* bp = dbg->breakpoints[i];
        if (bp->enabled && bp->line == line && strcmp(bp->file, file->data) == 0) {
            bp->hit_count++;
            return bp->id;
        }
    }
    return 0;
}

// Push stack frame
int64_t debugger_push_frame(int64_t dbg_ptr, int64_t name_ptr, int64_t file_ptr, int64_t line) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    SxString* name = (SxString*)name_ptr;
    SxString* file = (SxString*)file_ptr;
    if (!dbg || !name) return 0;

    // Grow if needed
    if (dbg->frame_count >= dbg->frame_capacity) {
        int64_t new_cap = dbg->frame_capacity * 2;
        StackFrame** new_frames = (StackFrame**)realloc(dbg->frames, sizeof(StackFrame*) * new_cap);
        if (!new_frames) return 0;
        dbg->frames = new_frames;
        dbg->frame_capacity = new_cap;
    }

    StackFrame* frame = (StackFrame*)malloc(sizeof(StackFrame));
    if (!frame) return 0;

    frame->id = dbg->frame_count;
    frame->name = strdup(name->data);
    frame->file = file ? strdup(file->data) : NULL;
    frame->line = line;
    frame->column = 0;
    frame->variables = (DebugVariable**)malloc(sizeof(DebugVariable*) * 64);
    frame->var_count = 0;

    dbg->frames[dbg->frame_count++] = frame;
    return frame->id;
}

// Pop stack frame
int64_t debugger_pop_frame(int64_t dbg_ptr) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    if (!dbg || dbg->frame_count == 0) return 0;

    dbg->frame_count--;
    StackFrame* frame = dbg->frames[dbg->frame_count];

    // Free variables
    for (int64_t i = 0; i < frame->var_count; i++) {
        if (frame->variables[i]->name) free(frame->variables[i]->name);
        if (frame->variables[i]->type) free(frame->variables[i]->type);
        if (frame->variables[i]->value) free(frame->variables[i]->value);
        free(frame->variables[i]);
    }
    free(frame->variables);

    if (frame->name) free(frame->name);
    if (frame->file) free(frame->file);
    free(frame);

    return 1;
}

// Get frame count
int64_t debugger_frame_count(int64_t dbg_ptr) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    return dbg ? dbg->frame_count : 0;
}

// Get frame name
int64_t debugger_frame_name(int64_t dbg_ptr, int64_t frame_id) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    if (!dbg || frame_id < 0 || frame_id >= dbg->frame_count) return 0;

    StackFrame* frame = dbg->frames[frame_id];
    return (int64_t)intrinsic_string_new(frame->name);
}

// Add variable to current frame
int64_t debugger_add_variable(int64_t dbg_ptr, int64_t name_ptr, int64_t type_ptr, int64_t value_ptr) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    SxString* name = (SxString*)name_ptr;
    SxString* type = (SxString*)type_ptr;
    SxString* value = (SxString*)value_ptr;
    if (!dbg || !name || dbg->frame_count == 0) return 0;

    StackFrame* frame = dbg->frames[dbg->frame_count - 1];

    DebugVariable* var = (DebugVariable*)malloc(sizeof(DebugVariable));
    if (!var) return 0;

    var->name = strdup(name->data);
    var->type = type ? strdup(type->data) : strdup("unknown");
    var->value = value ? strdup(value->data) : strdup("null");
    var->scope = 0;  // local

    frame->variables[frame->var_count++] = var;
    return 1;
}

// Get variable count in frame
int64_t debugger_variable_count(int64_t dbg_ptr, int64_t frame_id) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    if (!dbg || frame_id < 0 || frame_id >= dbg->frame_count) return 0;
    return dbg->frames[frame_id]->var_count;
}

// Get variable name
int64_t debugger_variable_name(int64_t dbg_ptr, int64_t frame_id, int64_t var_idx) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    if (!dbg || frame_id < 0 || frame_id >= dbg->frame_count) return 0;

    StackFrame* frame = dbg->frames[frame_id];
    if (var_idx < 0 || var_idx >= frame->var_count) return 0;

    return (int64_t)intrinsic_string_new(frame->variables[var_idx]->name);
}

// Get variable value
int64_t debugger_variable_value(int64_t dbg_ptr, int64_t frame_id, int64_t var_idx) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    if (!dbg || frame_id < 0 || frame_id >= dbg->frame_count) return 0;

    StackFrame* frame = dbg->frames[frame_id];
    if (var_idx < 0 || var_idx >= frame->var_count) return 0;

    return (int64_t)intrinsic_string_new(frame->variables[var_idx]->value);
}

// Step controls
int64_t debugger_pause(int64_t dbg_ptr) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    if (!dbg) return 0;
    dbg->paused = 1;
    return 1;
}

int64_t debugger_continue(int64_t dbg_ptr) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    if (!dbg) return 0;
    dbg->paused = 0;
    dbg->stepping = 0;
    return 1;
}

int64_t debugger_step_over(int64_t dbg_ptr) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    if (!dbg) return 0;
    dbg->stepping = 1;
    return 1;
}

int64_t debugger_step_into(int64_t dbg_ptr) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    if (!dbg) return 0;
    dbg->stepping = 2;
    return 1;
}

int64_t debugger_step_out(int64_t dbg_ptr) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    if (!dbg) return 0;
    dbg->stepping = 3;
    return 1;
}

int64_t debugger_is_paused(int64_t dbg_ptr) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    return dbg ? dbg->paused : 0;
}

// Update current location
int64_t debugger_set_location(int64_t dbg_ptr, int64_t file_ptr, int64_t line) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    SxString* file = (SxString*)file_ptr;
    if (!dbg) return 0;

    if (dbg->current_file) free(dbg->current_file);
    dbg->current_file = file ? strdup(file->data) : NULL;
    dbg->current_line = line;

    return 1;
}

// Get current line
int64_t debugger_current_line(int64_t dbg_ptr) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    return dbg ? dbg->current_line : 0;
}

// Close debugger
void debugger_close(int64_t dbg_ptr) {
    Debugger* dbg = (Debugger*)dbg_ptr;
    if (!dbg) return;

    // Free breakpoints
    for (int64_t i = 0; i < dbg->bp_count; i++) {
        if (dbg->breakpoints[i]->file) free(dbg->breakpoints[i]->file);
        if (dbg->breakpoints[i]->condition) free(dbg->breakpoints[i]->condition);
        free(dbg->breakpoints[i]);
    }
    free(dbg->breakpoints);

    // Free frames
    for (int64_t i = 0; i < dbg->frame_count; i++) {
        StackFrame* frame = dbg->frames[i];
        for (int64_t j = 0; j < frame->var_count; j++) {
            if (frame->variables[j]->name) free(frame->variables[j]->name);
            if (frame->variables[j]->type) free(frame->variables[j]->type);
            if (frame->variables[j]->value) free(frame->variables[j]->value);
            free(frame->variables[j]);
        }
        free(frame->variables);
        if (frame->name) free(frame->name);
        if (frame->file) free(frame->file);
        free(frame);
    }
    free(dbg->frames);

    if (dbg->current_file) free(dbg->current_file);
    free(dbg);

    if (g_debugger == dbg) g_debugger = NULL;
}

// ============================================================================
// Phase 11: Cursus Bytecode VM
// ============================================================================

// Bytecode opcodes
#define OP_NOP      0x00
#define OP_PUSH     0x01  // Push i64 constant
#define OP_POP      0x02  // Pop top of stack
#define OP_DUP      0x03  // Duplicate top of stack
#define OP_SWAP     0x04  // Swap top two values

#define OP_ADD      0x10  // Add top two values
#define OP_SUB      0x11  // Subtract
#define OP_MUL      0x12  // Multiply
#define OP_DIV      0x13  // Divide
#define OP_MOD      0x14  // Modulo
#define OP_NEG      0x15  // Negate

#define OP_AND      0x20  // Bitwise AND
#define OP_OR       0x21  // Bitwise OR
#define OP_XOR      0x22  // Bitwise XOR
#define OP_NOT      0x23  // Bitwise NOT
#define OP_SHL      0x24  // Shift left
#define OP_SHR      0x25  // Shift right

#define OP_EQ       0x30  // Equal
#define OP_NE       0x31  // Not equal
#define OP_LT       0x32  // Less than
#define OP_LE       0x33  // Less or equal
#define OP_GT       0x34  // Greater than
#define OP_GE       0x35  // Greater or equal

#define OP_JMP      0x40  // Unconditional jump
#define OP_JZ       0x41  // Jump if zero
#define OP_JNZ      0x42  // Jump if not zero

#define OP_LOAD     0x50  // Load local variable
#define OP_STORE    0x51  // Store local variable
#define OP_GLOAD    0x52  // Load global
#define OP_GSTORE   0x53  // Store global

#define OP_CALL     0x60  // Call function
#define OP_RET      0x61  // Return from function
#define OP_NATIVE   0x62  // Call native function

#define OP_PRINT    0x70  // Print top of stack
#define OP_HALT     0xFF  // Stop execution

// VM state
typedef struct VmFrame {
    int64_t return_addr;
    int64_t base_ptr;
} VmFrame;

typedef struct CursusVm {
    uint8_t* code;
    int64_t code_len;
    int64_t ip;  // instruction pointer

    int64_t* stack;
    int64_t stack_size;
    int64_t sp;  // stack pointer

    int64_t* locals;
    int64_t locals_size;

    int64_t* globals;
    int64_t globals_size;

    VmFrame* frames;
    int64_t frame_count;
    int64_t frame_capacity;

    int64_t running;
    int64_t error;
} CursusVm;

// Create VM
int64_t vm_new(int64_t stack_size, int64_t locals_size) {
    CursusVm* vm = (CursusVm*)malloc(sizeof(CursusVm));
    if (!vm) return 0;

    vm->code = NULL;
    vm->code_len = 0;
    vm->ip = 0;

    vm->stack_size = stack_size > 0 ? stack_size : 1024;
    vm->stack = (int64_t*)calloc(vm->stack_size, sizeof(int64_t));
    vm->sp = 0;

    vm->locals_size = locals_size > 0 ? locals_size : 256;
    vm->locals = (int64_t*)calloc(vm->locals_size, sizeof(int64_t));

    vm->globals_size = 256;
    vm->globals = (int64_t*)calloc(vm->globals_size, sizeof(int64_t));

    vm->frame_capacity = 64;
    vm->frames = (VmFrame*)calloc(vm->frame_capacity, sizeof(VmFrame));
    vm->frame_count = 0;

    vm->running = 0;
    vm->error = 0;

    return (int64_t)vm;
}

// Load bytecode
int64_t vm_load(int64_t vm_ptr, int64_t code_ptr, int64_t code_len) {
    CursusVm* vm = (CursusVm*)vm_ptr;
    if (!vm || !code_ptr || code_len <= 0) return 0;

    if (vm->code) free(vm->code);
    vm->code = (uint8_t*)malloc(code_len);
    if (!vm->code) return 0;

    memcpy(vm->code, (void*)code_ptr, code_len);
    vm->code_len = code_len;
    vm->ip = 0;

    return 1;
}

// Push value onto stack
static int64_t vm_push(CursusVm* vm, int64_t value) {
    if (vm->sp >= vm->stack_size) {
        vm->error = 1;  // Stack overflow
        return 0;
    }
    vm->stack[vm->sp++] = value;
    return 1;
}

// Pop value from stack
static int64_t vm_pop(CursusVm* vm) {
    if (vm->sp <= 0) {
        vm->error = 2;  // Stack underflow
        return 0;
    }
    return vm->stack[--vm->sp];
}

// Peek at top of stack
static int64_t vm_peek(CursusVm* vm) {
    if (vm->sp <= 0) {
        vm->error = 2;
        return 0;
    }
    return vm->stack[vm->sp - 1];
}

// Read immediate i64 from bytecode
static int64_t vm_read_i64(CursusVm* vm) {
    if (vm->ip + 8 > vm->code_len) {
        vm->error = 3;  // Code overflow
        return 0;
    }
    int64_t value;
    memcpy(&value, &vm->code[vm->ip], 8);
    vm->ip += 8;
    return value;
}

// Execute one instruction
int64_t vm_step(int64_t vm_ptr) {
    CursusVm* vm = (CursusVm*)vm_ptr;
    if (!vm || !vm->code || vm->ip >= vm->code_len || vm->error) return 0;

    uint8_t op = vm->code[vm->ip++];
    int64_t a, b, addr;

    switch (op) {
        case OP_NOP:
            break;

        case OP_PUSH:
            a = vm_read_i64(vm);
            vm_push(vm, a);
            break;

        case OP_POP:
            vm_pop(vm);
            break;

        case OP_DUP:
            a = vm_peek(vm);
            vm_push(vm, a);
            break;

        case OP_SWAP:
            a = vm_pop(vm);
            b = vm_pop(vm);
            vm_push(vm, a);
            vm_push(vm, b);
            break;

        case OP_ADD:
            b = vm_pop(vm);
            a = vm_pop(vm);
            vm_push(vm, a + b);
            break;

        case OP_SUB:
            b = vm_pop(vm);
            a = vm_pop(vm);
            vm_push(vm, a - b);
            break;

        case OP_MUL:
            b = vm_pop(vm);
            a = vm_pop(vm);
            vm_push(vm, a * b);
            break;

        case OP_DIV:
            b = vm_pop(vm);
            a = vm_pop(vm);
            if (b == 0) { vm->error = 4; break; }  // Division by zero
            vm_push(vm, a / b);
            break;

        case OP_MOD:
            b = vm_pop(vm);
            a = vm_pop(vm);
            if (b == 0) { vm->error = 4; break; }
            vm_push(vm, a % b);
            break;

        case OP_NEG:
            a = vm_pop(vm);
            vm_push(vm, -a);
            break;

        case OP_AND:
            b = vm_pop(vm);
            a = vm_pop(vm);
            vm_push(vm, a & b);
            break;

        case OP_OR:
            b = vm_pop(vm);
            a = vm_pop(vm);
            vm_push(vm, a | b);
            break;

        case OP_XOR:
            b = vm_pop(vm);
            a = vm_pop(vm);
            vm_push(vm, a ^ b);
            break;

        case OP_NOT:
            a = vm_pop(vm);
            vm_push(vm, ~a);
            break;

        case OP_SHL:
            b = vm_pop(vm);
            a = vm_pop(vm);
            vm_push(vm, a << b);
            break;

        case OP_SHR:
            b = vm_pop(vm);
            a = vm_pop(vm);
            vm_push(vm, a >> b);
            break;

        case OP_EQ:
            b = vm_pop(vm);
            a = vm_pop(vm);
            vm_push(vm, a == b ? 1 : 0);
            break;

        case OP_NE:
            b = vm_pop(vm);
            a = vm_pop(vm);
            vm_push(vm, a != b ? 1 : 0);
            break;

        case OP_LT:
            b = vm_pop(vm);
            a = vm_pop(vm);
            vm_push(vm, a < b ? 1 : 0);
            break;

        case OP_LE:
            b = vm_pop(vm);
            a = vm_pop(vm);
            vm_push(vm, a <= b ? 1 : 0);
            break;

        case OP_GT:
            b = vm_pop(vm);
            a = vm_pop(vm);
            vm_push(vm, a > b ? 1 : 0);
            break;

        case OP_GE:
            b = vm_pop(vm);
            a = vm_pop(vm);
            vm_push(vm, a >= b ? 1 : 0);
            break;

        case OP_JMP:
            addr = vm_read_i64(vm);
            vm->ip = addr;
            break;

        case OP_JZ:
            addr = vm_read_i64(vm);
            a = vm_pop(vm);
            if (a == 0) vm->ip = addr;
            break;

        case OP_JNZ:
            addr = vm_read_i64(vm);
            a = vm_pop(vm);
            if (a != 0) vm->ip = addr;
            break;

        case OP_LOAD:
            addr = vm_read_i64(vm);
            if (addr >= 0 && addr < vm->locals_size) {
                vm_push(vm, vm->locals[addr]);
            } else {
                vm->error = 5;  // Invalid local
            }
            break;

        case OP_STORE:
            addr = vm_read_i64(vm);
            a = vm_pop(vm);
            if (addr >= 0 && addr < vm->locals_size) {
                vm->locals[addr] = a;
            } else {
                vm->error = 5;
            }
            break;

        case OP_GLOAD:
            addr = vm_read_i64(vm);
            if (addr >= 0 && addr < vm->globals_size) {
                vm_push(vm, vm->globals[addr]);
            } else {
                vm->error = 6;  // Invalid global
            }
            break;

        case OP_GSTORE:
            addr = vm_read_i64(vm);
            a = vm_pop(vm);
            if (addr >= 0 && addr < vm->globals_size) {
                vm->globals[addr] = a;
            } else {
                vm->error = 6;
            }
            break;

        case OP_CALL:
            addr = vm_read_i64(vm);
            if (vm->frame_count >= vm->frame_capacity) {
                vm->error = 7;  // Call stack overflow
                break;
            }
            vm->frames[vm->frame_count].return_addr = vm->ip;
            vm->frames[vm->frame_count].base_ptr = vm->sp;
            vm->frame_count++;
            vm->ip = addr;
            break;

        case OP_RET:
            if (vm->frame_count <= 0) {
                vm->running = 0;
                break;
            }
            vm->frame_count--;
            vm->ip = vm->frames[vm->frame_count].return_addr;
            break;

        case OP_PRINT:
            a = vm_pop(vm);
            printf("%lld\n", a);
            break;

        case OP_HALT:
            vm->running = 0;
            break;

        default:
            vm->error = 8;  // Unknown opcode
            break;
    }

    return vm->error == 0 ? 1 : 0;
}

// Run until halt or error
int64_t vm_run(int64_t vm_ptr) {
    CursusVm* vm = (CursusVm*)vm_ptr;
    if (!vm || !vm->code) return 0;

    vm->running = 1;
    vm->error = 0;
    vm->ip = 0;

    while (vm->running && vm->ip < vm->code_len && !vm->error) {
        vm_step(vm_ptr);
    }

    return vm->error == 0 ? 1 : 0;
}

// Get stack pointer
int64_t vm_sp(int64_t vm_ptr) {
    CursusVm* vm = (CursusVm*)vm_ptr;
    return vm ? vm->sp : 0;
}

// Get top of stack
int64_t vm_top(int64_t vm_ptr) {
    CursusVm* vm = (CursusVm*)vm_ptr;
    if (!vm || vm->sp <= 0) return 0;
    return vm->stack[vm->sp - 1];
}

// Get error code
int64_t vm_error(int64_t vm_ptr) {
    CursusVm* vm = (CursusVm*)vm_ptr;
    return vm ? vm->error : -1;
}

// Get IP
int64_t vm_ip(int64_t vm_ptr) {
    CursusVm* vm = (CursusVm*)vm_ptr;
    return vm ? vm->ip : 0;
}

// Set local variable
int64_t vm_set_local(int64_t vm_ptr, int64_t index, int64_t value) {
    CursusVm* vm = (CursusVm*)vm_ptr;
    if (!vm || index < 0 || index >= vm->locals_size) return 0;
    vm->locals[index] = value;
    return 1;
}

// Get local variable
int64_t vm_get_local(int64_t vm_ptr, int64_t index) {
    CursusVm* vm = (CursusVm*)vm_ptr;
    if (!vm || index < 0 || index >= vm->locals_size) return 0;
    return vm->locals[index];
}

// Reset VM state
int64_t vm_reset(int64_t vm_ptr) {
    CursusVm* vm = (CursusVm*)vm_ptr;
    if (!vm) return 0;

    vm->ip = 0;
    vm->sp = 0;
    vm->frame_count = 0;
    vm->running = 0;
    vm->error = 0;
    memset(vm->locals, 0, vm->locals_size * sizeof(int64_t));

    return 1;
}

// Close VM
void vm_close(int64_t vm_ptr) {
    CursusVm* vm = (CursusVm*)vm_ptr;
    if (!vm) return;

    if (vm->code) free(vm->code);
    if (vm->stack) free(vm->stack);
    if (vm->locals) free(vm->locals);
    if (vm->globals) free(vm->globals);
    if (vm->frames) free(vm->frames);
    free(vm);
}

// Opcode constants for bytecode generation
int64_t vm_op_nop(void) { return OP_NOP; }
int64_t vm_op_push(void) { return OP_PUSH; }
int64_t vm_op_pop(void) { return OP_POP; }
int64_t vm_op_dup(void) { return OP_DUP; }
int64_t vm_op_add(void) { return OP_ADD; }
int64_t vm_op_sub(void) { return OP_SUB; }
int64_t vm_op_mul(void) { return OP_MUL; }
int64_t vm_op_div(void) { return OP_DIV; }
int64_t vm_op_mod(void) { return OP_MOD; }
int64_t vm_op_neg(void) { return OP_NEG; }
int64_t vm_op_eq(void) { return OP_EQ; }
int64_t vm_op_ne(void) { return OP_NE; }
int64_t vm_op_lt(void) { return OP_LT; }
int64_t vm_op_le(void) { return OP_LE; }
int64_t vm_op_gt(void) { return OP_GT; }
int64_t vm_op_ge(void) { return OP_GE; }
int64_t vm_op_jmp(void) { return OP_JMP; }
int64_t vm_op_jz(void) { return OP_JZ; }
int64_t vm_op_jnz(void) { return OP_JNZ; }
int64_t vm_op_load(void) { return OP_LOAD; }
int64_t vm_op_store(void) { return OP_STORE; }
int64_t vm_op_call(void) { return OP_CALL; }
int64_t vm_op_ret(void) { return OP_RET; }
int64_t vm_op_print(void) { return OP_PRINT; }
int64_t vm_op_halt(void) { return OP_HALT; }

// ============================================================================
// Phase 11: Cross-Compilation Support
// ============================================================================

// Target architectures
#define ARCH_X86_64     1
#define ARCH_AARCH64    2
#define ARCH_RISCV64    3
#define ARCH_WASM32     4

// Target operating systems
#define OS_LINUX        1
#define OS_MACOS        2
#define OS_WINDOWS      3
#define OS_FREEBSD      4
#define OS_WASI         5

// Target environments
#define ENV_GNU         1
#define ENV_MUSL        2
#define ENV_MSVC        3
#define ENV_NONE        4

// Target triple structure
typedef struct Target {
    int64_t arch;
    int64_t os;
    int64_t env;
    char* triple;  // Full triple string
    int64_t pointer_size;
    int64_t endian;  // 0=little, 1=big
} Target;

// Parse target triple
int64_t target_parse(int64_t triple_ptr) {
    SxString* triple = (SxString*)triple_ptr;
    if (!triple) return 0;

    Target* target = (Target*)malloc(sizeof(Target));
    if (!target) return 0;

    target->triple = strdup(triple->data);
    target->pointer_size = 8;  // Default to 64-bit
    target->endian = 0;  // Default to little-endian

    // Parse architecture
    if (strstr(triple->data, "x86_64") || strstr(triple->data, "amd64")) {
        target->arch = ARCH_X86_64;
    } else if (strstr(triple->data, "aarch64") || strstr(triple->data, "arm64")) {
        target->arch = ARCH_AARCH64;
    } else if (strstr(triple->data, "riscv64")) {
        target->arch = ARCH_RISCV64;
    } else if (strstr(triple->data, "wasm32")) {
        target->arch = ARCH_WASM32;
        target->pointer_size = 4;
    } else {
        target->arch = ARCH_X86_64;  // Default
    }

    // Parse OS
    if (strstr(triple->data, "linux")) {
        target->os = OS_LINUX;
    } else if (strstr(triple->data, "darwin") || strstr(triple->data, "macos")) {
        target->os = OS_MACOS;
    } else if (strstr(triple->data, "windows") || strstr(triple->data, "win32")) {
        target->os = OS_WINDOWS;
    } else if (strstr(triple->data, "freebsd")) {
        target->os = OS_FREEBSD;
    } else if (strstr(triple->data, "wasi")) {
        target->os = OS_WASI;
    } else {
        target->os = OS_LINUX;  // Default
    }

    // Parse environment
    if (strstr(triple->data, "gnu")) {
        target->env = ENV_GNU;
    } else if (strstr(triple->data, "musl")) {
        target->env = ENV_MUSL;
    } else if (strstr(triple->data, "msvc")) {
        target->env = ENV_MSVC;
    } else {
        target->env = ENV_NONE;
    }

    return (int64_t)target;
}

// Get target architecture
int64_t target_arch(int64_t target_ptr) {
    Target* target = (Target*)target_ptr;
    return target ? target->arch : 0;
}

// Get target OS
int64_t target_os(int64_t target_ptr) {
    Target* target = (Target*)target_ptr;
    return target ? target->os : 0;
}

// Get target environment
int64_t target_env(int64_t target_ptr) {
    Target* target = (Target*)target_ptr;
    return target ? target->env : 0;
}

// Get pointer size in bytes
int64_t target_pointer_size(int64_t target_ptr) {
    Target* target = (Target*)target_ptr;
    return target ? target->pointer_size : 8;
}

// Is little-endian?
int64_t target_is_little_endian(int64_t target_ptr) {
    Target* target = (Target*)target_ptr;
    return target ? (target->endian == 0 ? 1 : 0) : 1;
}

// Get triple string
int64_t target_triple_string(int64_t target_ptr) {
    Target* target = (Target*)target_ptr;
    if (!target || !target->triple) return 0;
    return (int64_t)intrinsic_string_new(target->triple);
}

// Get LLVM target triple
int64_t target_llvm_triple(int64_t target_ptr) {
    Target* target = (Target*)target_ptr;
    if (!target) return 0;

    char buf[256];

    const char* arch_str;
    switch (target->arch) {
        case ARCH_X86_64: arch_str = "x86_64"; break;
        case ARCH_AARCH64: arch_str = "aarch64"; break;
        case ARCH_RISCV64: arch_str = "riscv64"; break;
        case ARCH_WASM32: arch_str = "wasm32"; break;
        default: arch_str = "x86_64"; break;
    }

    const char* os_str;
    switch (target->os) {
        case OS_LINUX: os_str = "linux"; break;
        case OS_MACOS: os_str = "apple-darwin"; break;
        case OS_WINDOWS: os_str = "windows"; break;
        case OS_FREEBSD: os_str = "freebsd"; break;
        case OS_WASI: os_str = "wasi"; break;
        default: os_str = "linux"; break;
    }

    const char* env_str;
    switch (target->env) {
        case ENV_GNU: env_str = "gnu"; break;
        case ENV_MUSL: env_str = "musl"; break;
        case ENV_MSVC: env_str = "msvc"; break;
        default: env_str = ""; break;
    }

    if (strlen(env_str) > 0) {
        snprintf(buf, sizeof(buf), "%s-%s-%s", arch_str, os_str, env_str);
    } else {
        snprintf(buf, sizeof(buf), "%s-%s", arch_str, os_str);
    }

    return (int64_t)intrinsic_string_new(buf);
}

// Get data layout string for LLVM
int64_t target_data_layout(int64_t target_ptr) {
    Target* target = (Target*)target_ptr;
    if (!target) return 0;

    const char* layout;
    switch (target->arch) {
        case ARCH_X86_64:
            layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128";
            break;
        case ARCH_AARCH64:
            layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128";
            break;
        case ARCH_WASM32:
            layout = "e-m:e-p:32:32-i64:64-n32:64-S128";
            break;
        default:
            layout = "e-m:e-i64:64-n8:16:32:64-S128";
            break;
    }

    return (int64_t)intrinsic_string_new(layout);
}

// Close target
void target_close(int64_t target_ptr) {
    Target* target = (Target*)target_ptr;
    if (!target) return;
    if (target->triple) free(target->triple);
    free(target);
}

// Get host target
int64_t target_host(void) {
#if defined(__x86_64__) || defined(_M_X64)
    #if defined(__APPLE__)
        return target_parse((int64_t)intrinsic_string_new("x86_64-apple-darwin"));
    #elif defined(_WIN32)
        return target_parse((int64_t)intrinsic_string_new("x86_64-windows-msvc"));
    #else
        return target_parse((int64_t)intrinsic_string_new("x86_64-linux-gnu"));
    #endif
#elif defined(__aarch64__) || defined(_M_ARM64)
    #if defined(__APPLE__)
        return target_parse((int64_t)intrinsic_string_new("aarch64-apple-darwin"));
    #else
        return target_parse((int64_t)intrinsic_string_new("aarch64-linux-gnu"));
    #endif
#else
    return target_parse((int64_t)intrinsic_string_new("x86_64-linux-gnu"));
#endif
}

// Architecture constants
int64_t arch_x86_64(void) { return ARCH_X86_64; }
int64_t arch_aarch64(void) { return ARCH_AARCH64; }
int64_t arch_riscv64(void) { return ARCH_RISCV64; }
int64_t arch_wasm32(void) { return ARCH_WASM32; }

// OS constants
int64_t os_linux(void) { return OS_LINUX; }
int64_t os_macos(void) { return OS_MACOS; }
int64_t os_windows(void) { return OS_WINDOWS; }
int64_t os_freebsd(void) { return OS_FREEBSD; }
int64_t os_wasi(void) { return OS_WASI; }

// Environment constants
int64_t env_gnu(void) { return ENV_GNU; }
int64_t env_musl(void) { return ENV_MUSL; }
int64_t env_msvc(void) { return ENV_MSVC; }
int64_t env_none(void) { return ENV_NONE; }

// ============================================================================
// Phase 35.3.14: Distributed Actor Runtime
// ============================================================================

// Distributed node structure
typedef struct DistributedNode {
    char* address;
    int port;
    int socket_fd;
    int64_t node_id;
    int connected;
    pthread_t listener_thread;
    pthread_mutex_t lock;
    void** remote_actors;  // Remote actor references
    size_t actor_count;
    size_t actor_cap;
} DistributedNode;

// Remote actor reference
typedef struct RemoteActor {
    int64_t node_id;
    int64_t actor_id;
    char* name;
} RemoteActor;

// Create a distributed node
int64_t distributed_node_new(int64_t addr_ptr, int64_t port) {
    SxString* addr = (SxString*)addr_ptr;

    DistributedNode* node = (DistributedNode*)malloc(sizeof(DistributedNode));
    if (!node) return 0;

    node->address = addr ? strdup(addr->data) : strdup("127.0.0.1");
    node->port = (int)port;
    node->socket_fd = -1;
    node->node_id = (int64_t)node;  // Use pointer as unique ID
    node->connected = 0;
    pthread_mutex_init(&node->lock, NULL);
    node->remote_actors = NULL;
    node->actor_count = 0;
    node->actor_cap = 0;

    return (int64_t)node;
}

// Start the distributed node (begin listening)
int64_t distributed_node_start(int64_t node_ptr) {
    DistributedNode* node = (DistributedNode*)node_ptr;
    if (!node) return 0;

    // Create socket
    node->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (node->socket_fd < 0) return 0;

    // Allow address reuse
    int opt = 1;
    setsockopt(node->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(node->address);
    addr.sin_port = htons(node->port);

    if (bind(node->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(node->socket_fd);
        node->socket_fd = -1;
        return 0;
    }

    // Listen
    if (listen(node->socket_fd, 10) < 0) {
        close(node->socket_fd);
        node->socket_fd = -1;
        return 0;
    }

    node->connected = 1;
    return 1;
}

// Connect to another node
int64_t distributed_node_connect(int64_t node_ptr, int64_t remote_addr_ptr, int64_t remote_port) {
    DistributedNode* node = (DistributedNode*)node_ptr;
    SxString* remote_addr = (SxString*)remote_addr_ptr;
    if (!node || !remote_addr) return 0;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(remote_addr->data);
    addr.sin_port = htons((int)remote_port);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return 0;
    }

    // Store connection (simplified - real impl would track multiple connections)
    return (int64_t)sock;
}

// Send message to remote actor
int64_t distributed_send(int64_t conn, int64_t actor_id, int64_t msg_type, int64_t payload_ptr) {
    int sock = (int)conn;
    if (sock < 0) return 0;

    // Message format: [actor_id:8][msg_type:8][payload_len:8][payload]
    int64_t header[3];
    header[0] = actor_id;
    header[1] = msg_type;

    SxString* payload = (SxString*)payload_ptr;
    header[2] = payload ? payload->len : 0;

    if (send(sock, header, sizeof(header), 0) < 0) return 0;
    if (payload && payload->len > 0) {
        if (send(sock, payload->data, payload->len, 0) < 0) return 0;
    }

    return 1;
}

// Receive message from connection
int64_t distributed_recv(int64_t conn) {
    int sock = (int)conn;
    if (sock < 0) return 0;

    int64_t header[3];
    ssize_t n = recv(sock, header, sizeof(header), MSG_WAITALL);
    if (n != sizeof(header)) return 0;

    // Allocate message struct: [actor_id:8][msg_type:8][payload_ptr:8]
    int64_t* msg = (int64_t*)malloc(3 * sizeof(int64_t));
    msg[0] = header[0];  // actor_id
    msg[1] = header[1];  // msg_type

    if (header[2] > 0) {
        char* payload = (char*)malloc(header[2] + 1);
        recv(sock, payload, header[2], MSG_WAITALL);
        payload[header[2]] = '\0';
        msg[2] = (int64_t)intrinsic_string_new(payload);
        free(payload);
    } else {
        msg[2] = 0;
    }

    return (int64_t)msg;
}

// Get node ID
int64_t distributed_node_id(int64_t node_ptr) {
    DistributedNode* node = (DistributedNode*)node_ptr;
    return node ? node->node_id : 0;
}

// Stop distributed node
void distributed_node_stop(int64_t node_ptr) {
    DistributedNode* node = (DistributedNode*)node_ptr;
    if (!node) return;

    if (node->socket_fd >= 0) {
        close(node->socket_fd);
        node->socket_fd = -1;
    }
    node->connected = 0;
}

// Free distributed node
void distributed_node_free(int64_t node_ptr) {
    DistributedNode* node = (DistributedNode*)node_ptr;
    if (!node) return;

    distributed_node_stop(node_ptr);
    if (node->address) free(node->address);
    pthread_mutex_destroy(&node->lock);
    if (node->remote_actors) free(node->remote_actors);
    free(node);
}

// ============================================================================
// Phase 35.3.15: SWIM Protocol Runtime (Gossip-based Failure Detection)
// ============================================================================

// SWIM member states - using local values for this module
// (Note: SWIM_ macros defined differently earlier, these are internal)
#undef SWIM_ALIVE
#undef SWIM_SUSPECT
#undef SWIM_DEAD
#define SWIM_ALIVE      0
#define SWIM_SUSPECT    1
#define SWIM_DEAD       2

// SWIM member structure
typedef struct SwimMember {
    int64_t node_id;
    char* address;
    int port;
    int state;
    int64_t incarnation;
    int64_t last_ping;
} SwimMember;

// SWIM cluster structure
typedef struct SwimCluster {
    SwimMember** members;
    size_t member_count;
    size_t member_cap;
    int64_t local_id;
    int64_t incarnation;
    int ping_interval_ms;
    int ping_timeout_ms;
    int suspect_timeout_ms;
    pthread_mutex_t lock;
    int running;
    pthread_t protocol_thread;
} SwimCluster;

// Create SWIM cluster
int64_t swim_cluster_new(int64_t local_id) {
    SwimCluster* cluster = (SwimCluster*)malloc(sizeof(SwimCluster));
    if (!cluster) return 0;

    cluster->members = NULL;
    cluster->member_count = 0;
    cluster->member_cap = 0;
    cluster->local_id = local_id;
    cluster->incarnation = 0;
    cluster->ping_interval_ms = 1000;
    cluster->ping_timeout_ms = 500;
    cluster->suspect_timeout_ms = 2000;
    pthread_mutex_init(&cluster->lock, NULL);
    cluster->running = 0;

    return (int64_t)cluster;
}

// Add member to cluster
int64_t swim_add_member(int64_t cluster_ptr, int64_t node_id, int64_t addr_ptr, int64_t port) {
    SwimCluster* cluster = (SwimCluster*)cluster_ptr;
    SxString* addr = (SxString*)addr_ptr;
    if (!cluster) return 0;

    pthread_mutex_lock(&cluster->lock);

    if (cluster->member_count >= cluster->member_cap) {
        size_t new_cap = cluster->member_cap == 0 ? 8 : cluster->member_cap * 2;
        cluster->members = (SwimMember**)realloc(cluster->members, new_cap * sizeof(SwimMember*));
        cluster->member_cap = new_cap;
    }

    SwimMember* member = (SwimMember*)malloc(sizeof(SwimMember));
    member->node_id = node_id;
    member->address = addr ? strdup(addr->data) : strdup("127.0.0.1");
    member->port = (int)port;
    member->state = SWIM_ALIVE;
    member->incarnation = 0;
    member->last_ping = time(NULL) * 1000;

    cluster->members[cluster->member_count++] = member;

    pthread_mutex_unlock(&cluster->lock);
    return 1;
}

// Get member state
int64_t swim_member_state(int64_t cluster_ptr, int64_t node_id) {
    SwimCluster* cluster = (SwimCluster*)cluster_ptr;
    if (!cluster) return SWIM_DEAD;

    pthread_mutex_lock(&cluster->lock);

    for (size_t i = 0; i < cluster->member_count; i++) {
        if (cluster->members[i]->node_id == node_id) {
            int state = cluster->members[i]->state;
            pthread_mutex_unlock(&cluster->lock);
            return state;
        }
    }

    pthread_mutex_unlock(&cluster->lock);
    return SWIM_DEAD;
}

// Mark member as suspect
void swim_suspect_member(int64_t cluster_ptr, int64_t node_id) {
    SwimCluster* cluster = (SwimCluster*)cluster_ptr;
    if (!cluster) return;

    pthread_mutex_lock(&cluster->lock);

    for (size_t i = 0; i < cluster->member_count; i++) {
        if (cluster->members[i]->node_id == node_id) {
            cluster->members[i]->state = SWIM_SUSPECT;
            break;
        }
    }

    pthread_mutex_unlock(&cluster->lock);
}

// Mark member as dead
void swim_dead_member(int64_t cluster_ptr, int64_t node_id) {
    SwimCluster* cluster = (SwimCluster*)cluster_ptr;
    if (!cluster) return;

    pthread_mutex_lock(&cluster->lock);

    for (size_t i = 0; i < cluster->member_count; i++) {
        if (cluster->members[i]->node_id == node_id) {
            cluster->members[i]->state = SWIM_DEAD;
            break;
        }
    }

    pthread_mutex_unlock(&cluster->lock);
}

// Get member count
int64_t swim_member_count(int64_t cluster_ptr) {
    SwimCluster* cluster = (SwimCluster*)cluster_ptr;
    if (!cluster) return 0;
    return cluster->member_count;
}

// Get alive member count
int64_t swim_alive_count(int64_t cluster_ptr) {
    SwimCluster* cluster = (SwimCluster*)cluster_ptr;
    if (!cluster) return 0;

    pthread_mutex_lock(&cluster->lock);

    size_t count = 0;
    for (size_t i = 0; i < cluster->member_count; i++) {
        if (cluster->members[i]->state == SWIM_ALIVE) {
            count++;
        }
    }

    pthread_mutex_unlock(&cluster->lock);
    return count;
}

// Start SWIM protocol
int64_t swim_start(int64_t cluster_ptr) {
    SwimCluster* cluster = (SwimCluster*)cluster_ptr;
    if (!cluster) return 0;

    cluster->running = 1;
    // In a full implementation, spawn protocol thread here
    return 1;
}

// Stop SWIM protocol
void swim_stop(int64_t cluster_ptr) {
    SwimCluster* cluster = (SwimCluster*)cluster_ptr;
    if (!cluster) return;

    cluster->running = 0;
}

// Free SWIM cluster
void swim_cluster_free(int64_t cluster_ptr) {
    SwimCluster* cluster = (SwimCluster*)cluster_ptr;
    if (!cluster) return;

    swim_stop(cluster_ptr);

    for (size_t i = 0; i < cluster->member_count; i++) {
        if (cluster->members[i]->address) free(cluster->members[i]->address);
        free(cluster->members[i]);
    }
    if (cluster->members) free(cluster->members);

    pthread_mutex_destroy(&cluster->lock);
    free(cluster);
}

// SWIM state constants
int64_t swim_state_alive(void) { return SWIM_ALIVE; }
int64_t swim_state_suspect(void) { return SWIM_SUSPECT; }
int64_t swim_state_dead(void) { return SWIM_DEAD; }

// ============================================================================
// Phase 35.3.20: Evolution Runtime (Genetic Algorithms)
// ============================================================================

// Gene structure for evolution (renamed to avoid conflict)
typedef struct EvolutionGene {
    double* weights;
    size_t weight_count;
    double fitness;
} EvolutionGene;

// Population for evolution (renamed to avoid conflict with existing Population)
typedef struct EvolutionPopulation {
    EvolutionGene** genes;
    size_t gene_count;
    size_t gene_cap;
    size_t generation;
    double mutation_rate;
    double crossover_rate;
} EvolutionPopulation;

// Create a new evolution gene
int64_t evolution_gene_new(int64_t weight_count) {
    EvolutionGene* gene = (EvolutionGene*)malloc(sizeof(EvolutionGene));
    if (!gene) return 0;

    gene->weight_count = (size_t)weight_count;
    gene->weights = (double*)malloc(weight_count * sizeof(double));
    gene->fitness = 0.0;

    // Initialize with random weights
    for (size_t i = 0; i < gene->weight_count; i++) {
        gene->weights[i] = ((double)rand() / RAND_MAX) * 2.0 - 1.0;  // -1 to 1
    }

    return (int64_t)gene;
}

// Get evolution gene weight
double evolution_gene_weight(int64_t gene_ptr, int64_t index) {
    EvolutionGene* gene = (EvolutionGene*)gene_ptr;
    if (!gene || index < 0 || (size_t)index >= gene->weight_count) return 0.0;
    return gene->weights[index];
}

// Set evolution gene weight
void evolution_gene_set_weight(int64_t gene_ptr, int64_t index, double value) {
    EvolutionGene* gene = (EvolutionGene*)gene_ptr;
    if (!gene || index < 0 || (size_t)index >= gene->weight_count) return;
    gene->weights[index] = value;
}

// Get evolution gene fitness
double evolution_gene_fitness(int64_t gene_ptr) {
    EvolutionGene* gene = (EvolutionGene*)gene_ptr;
    return gene ? gene->fitness : 0.0;
}

// Set evolution gene fitness
void evolution_gene_set_fitness(int64_t gene_ptr, double fitness) {
    EvolutionGene* gene = (EvolutionGene*)gene_ptr;
    if (gene) gene->fitness = fitness;
}

// Create an evolution population
int64_t evolution_population_new(int64_t size, int64_t gene_size) {
    EvolutionPopulation* pop = (EvolutionPopulation*)malloc(sizeof(EvolutionPopulation));
    if (!pop) return 0;

    pop->gene_count = (size_t)size;
    pop->gene_cap = (size_t)size;
    pop->genes = (EvolutionGene**)malloc(size * sizeof(EvolutionGene*));
    pop->generation = 0;
    pop->mutation_rate = 0.01;
    pop->crossover_rate = 0.7;

    for (size_t i = 0; i < pop->gene_count; i++) {
        pop->genes[i] = (EvolutionGene*)evolution_gene_new(gene_size);
    }

    return (int64_t)pop;
}

// Get evolution population gene
int64_t evolution_population_get(int64_t pop_ptr, int64_t index) {
    EvolutionPopulation* pop = (EvolutionPopulation*)pop_ptr;
    if (!pop || index < 0 || (size_t)index >= pop->gene_count) return 0;
    return (int64_t)pop->genes[index];
}

// Get evolution population size
int64_t evolution_population_size(int64_t pop_ptr) {
    EvolutionPopulation* pop = (EvolutionPopulation*)pop_ptr;
    return pop ? pop->gene_count : 0;
}

// Get current generation
int64_t evolution_population_generation(int64_t pop_ptr) {
    EvolutionPopulation* pop = (EvolutionPopulation*)pop_ptr;
    return pop ? pop->generation : 0;
}

// Select parent using tournament selection
static EvolutionGene* evolution_tournament_select(EvolutionPopulation* pop, int tournament_size) {
    EvolutionGene* best = NULL;
    for (int i = 0; i < tournament_size; i++) {
        EvolutionGene* candidate = pop->genes[rand() % pop->gene_count];
        if (!best || candidate->fitness > best->fitness) {
            best = candidate;
        }
    }
    return best;
}

// Crossover two evolution genes
static EvolutionGene* evolution_crossover(EvolutionGene* parent1, EvolutionGene* parent2) {
    EvolutionGene* child = (EvolutionGene*)malloc(sizeof(EvolutionGene));
    child->weight_count = parent1->weight_count;
    child->weights = (double*)malloc(child->weight_count * sizeof(double));
    child->fitness = 0.0;

    // Single-point crossover
    size_t crossover_point = rand() % child->weight_count;
    for (size_t i = 0; i < child->weight_count; i++) {
        child->weights[i] = (i < crossover_point) ? parent1->weights[i] : parent2->weights[i];
    }

    return child;
}

// Mutate an evolution gene
static void evolution_mutate(EvolutionGene* gene, double rate) {
    for (size_t i = 0; i < gene->weight_count; i++) {
        if ((double)rand() / RAND_MAX < rate) {
            gene->weights[i] += ((double)rand() / RAND_MAX - 0.5) * 0.2;
            // Clamp to -1 to 1
            if (gene->weights[i] > 1.0) gene->weights[i] = 1.0;
            if (gene->weights[i] < -1.0) gene->weights[i] = -1.0;
        }
    }
}

// Evolve population to next generation
int64_t evolution_population_evolve(int64_t pop_ptr) {
    EvolutionPopulation* pop = (EvolutionPopulation*)pop_ptr;
    if (!pop || pop->gene_count < 2) return 0;

    // Create new generation
    EvolutionGene** new_genes = (EvolutionGene**)malloc(pop->gene_count * sizeof(EvolutionGene*));

    // Elitism: keep best gene
    EvolutionGene* best = pop->genes[0];
    for (size_t i = 1; i < pop->gene_count; i++) {
        if (pop->genes[i]->fitness > best->fitness) {
            best = pop->genes[i];
        }
    }

    // Copy best to new generation
    new_genes[0] = (EvolutionGene*)malloc(sizeof(EvolutionGene));
    new_genes[0]->weight_count = best->weight_count;
    new_genes[0]->weights = (double*)malloc(best->weight_count * sizeof(double));
    memcpy(new_genes[0]->weights, best->weights, best->weight_count * sizeof(double));
    new_genes[0]->fitness = 0.0;

    // Create rest through crossover and mutation
    for (size_t i = 1; i < pop->gene_count; i++) {
        EvolutionGene* parent1 = evolution_tournament_select(pop, 3);
        EvolutionGene* parent2 = evolution_tournament_select(pop, 3);

        EvolutionGene* child;
        if ((double)rand() / RAND_MAX < pop->crossover_rate) {
            child = evolution_crossover(parent1, parent2);
        } else {
            // Copy parent
            child = (EvolutionGene*)malloc(sizeof(EvolutionGene));
            child->weight_count = parent1->weight_count;
            child->weights = (double*)malloc(child->weight_count * sizeof(double));
            memcpy(child->weights, parent1->weights, child->weight_count * sizeof(double));
            child->fitness = 0.0;
        }

        evolution_mutate(child, pop->mutation_rate);
        new_genes[i] = child;
    }

    // Free old genes and replace
    for (size_t i = 0; i < pop->gene_count; i++) {
        free(pop->genes[i]->weights);
        free(pop->genes[i]);
    }
    free(pop->genes);

    pop->genes = new_genes;
    pop->generation++;

    return 1;
}

// Get best evolution gene
int64_t evolution_population_best(int64_t pop_ptr) {
    EvolutionPopulation* pop = (EvolutionPopulation*)pop_ptr;
    if (!pop || pop->gene_count == 0) return 0;

    EvolutionGene* best = pop->genes[0];
    for (size_t i = 1; i < pop->gene_count; i++) {
        if (pop->genes[i]->fitness > best->fitness) {
            best = pop->genes[i];
        }
    }

    return (int64_t)best;
}

// Free evolution gene
void evolution_gene_free(int64_t gene_ptr) {
    EvolutionGene* gene = (EvolutionGene*)gene_ptr;
    if (!gene) return;
    if (gene->weights) free(gene->weights);
    free(gene);
}

// Free evolution population
void evolution_population_free(int64_t pop_ptr) {
    EvolutionPopulation* pop = (EvolutionPopulation*)pop_ptr;
    if (!pop) return;

    for (size_t i = 0; i < pop->gene_count; i++) {
        if (pop->genes[i]) {
            if (pop->genes[i]->weights) free(pop->genes[i]->weights);
            free(pop->genes[i]);
        }
    }
    if (pop->genes) free(pop->genes);
    free(pop);
}

// ============================================================================
// HASHMAP IMPLEMENTATION - Phase 1 Core
// ============================================================================
//
// A complete HashMap implementation with string keys and i64 values.
// Uses separate chaining for collision resolution.
// Automatically resizes when load factor exceeds 0.75.
//
// Keys: SxString* (ownership transferred to map on insert)
// Values: int64_t (can store integers or pointers)
// ============================================================================

#define HASHMAP_INITIAL_CAPACITY 16
#define HASHMAP_LOAD_FACTOR 0.75

// Entry in a hash bucket (linked list node)
typedef struct HashMapEntry {
    SxString* key;
    int64_t value;
    struct HashMapEntry* next;
} HashMapEntry;

// HashMap structure
typedef struct {
    HashMapEntry** buckets;  // Array of bucket heads
    size_t capacity;         // Number of buckets
    size_t size;             // Number of entries
} HashMap;

// Iterator for HashMap
typedef struct {
    HashMap* map;
    size_t bucket_index;
    HashMapEntry* current_entry;
    int iter_type;  // 0=keys, 1=values, 2=entries
} HashMapIterator;

// Key-value pair for entry iteration
typedef struct {
    SxString* key;
    int64_t value;
} HashMapKV;

// Forward declarations
static void hashmap_resize(HashMap* map, size_t new_capacity);
static size_t hashmap_bucket_index(HashMap* map, SxString* key);

// Hash function for strings (FNV-1a)
static uint64_t hash_string(SxString* str) {
    if (!str || !str->data) return 0;
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < str->len; i++) {
        hash ^= (unsigned char)str->data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

// Compare two strings for equality
static int string_equals(SxString* a, SxString* b) {
    if (!a || !b) return a == b;
    if (a->len != b->len) return 0;
    return memcmp(a->data, b->data, a->len) == 0;
}

// Duplicate a string (for storing keys)
static SxString* string_dup(SxString* str) {
    if (!str) return NULL;
    SxString* copy = malloc(sizeof(SxString));
    copy->len = str->len;
    copy->cap = str->len + 1;
    copy->data = malloc(copy->cap);
    memcpy(copy->data, str->data, str->len);
    copy->data[str->len] = '\0';
    return copy;
}

// Get bucket index for a key
static size_t hashmap_bucket_index(HashMap* map, SxString* key) {
    return hash_string(key) % map->capacity;
}

// Create a new empty HashMap
int64_t hashmap_new(void) {
    HashMap* map = malloc(sizeof(HashMap));
    if (!map) return 0;

    map->capacity = HASHMAP_INITIAL_CAPACITY;
    map->size = 0;
    map->buckets = calloc(map->capacity, sizeof(HashMapEntry*));
    if (!map->buckets) {
        free(map);
        return 0;
    }

    return (int64_t)map;
}

// Create a HashMap with specified initial capacity
int64_t hashmap_with_capacity(int64_t capacity) {
    if (capacity <= 0) capacity = HASHMAP_INITIAL_CAPACITY;

    HashMap* map = malloc(sizeof(HashMap));
    if (!map) return 0;

    map->capacity = (size_t)capacity;
    map->size = 0;
    map->buckets = calloc(map->capacity, sizeof(HashMapEntry*));
    if (!map->buckets) {
        free(map);
        return 0;
    }

    return (int64_t)map;
}

// Resize the HashMap when load factor is exceeded
static void hashmap_resize(HashMap* map, size_t new_capacity) {
    HashMapEntry** old_buckets = map->buckets;
    size_t old_capacity = map->capacity;

    // Allocate new bucket array
    map->buckets = calloc(new_capacity, sizeof(HashMapEntry*));
    if (!map->buckets) {
        map->buckets = old_buckets;
        return;  // Failed to resize, keep old buckets
    }
    map->capacity = new_capacity;

    // Rehash all entries
    for (size_t i = 0; i < old_capacity; i++) {
        HashMapEntry* entry = old_buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;

            // Compute new bucket index
            size_t new_index = hashmap_bucket_index(map, entry->key);

            // Insert at head of new bucket
            entry->next = map->buckets[new_index];
            map->buckets[new_index] = entry;

            entry = next;
        }
    }

    free(old_buckets);
}

// Insert a key-value pair (returns previous value if key existed, or 0)
int64_t hashmap_insert(int64_t map_ptr, int64_t key_ptr, int64_t value) {
    HashMap* map = (HashMap*)map_ptr;
    SxString* key = (SxString*)key_ptr;
    if (!map || !key) return 0;

    // Check if resize is needed
    if ((double)(map->size + 1) / map->capacity > HASHMAP_LOAD_FACTOR) {
        hashmap_resize(map, map->capacity * 2);
    }

    size_t index = hashmap_bucket_index(map, key);

    // Check if key already exists
    HashMapEntry* entry = map->buckets[index];
    while (entry) {
        if (string_equals(entry->key, key)) {
            // Key exists, update value and return old value
            int64_t old_value = entry->value;
            entry->value = value;
            return old_value;
        }
        entry = entry->next;
    }

    // Key doesn't exist, create new entry
    HashMapEntry* new_entry = malloc(sizeof(HashMapEntry));
    if (!new_entry) return 0;

    new_entry->key = string_dup(key);  // Store a copy of the key
    new_entry->value = value;
    new_entry->next = map->buckets[index];
    map->buckets[index] = new_entry;
    map->size++;

    return 0;  // No previous value
}

// Get value by key (returns Option: 0 = None, otherwise value | with high bit check)
// Actually, we return a struct-like encoding: low bit = found, rest = value
// For simplicity: returns value if found, 0 if not found (caller should use contains first)
// Better approach: return pointer to a result struct
int64_t hashmap_get(int64_t map_ptr, int64_t key_ptr) {
    HashMap* map = (HashMap*)map_ptr;
    SxString* key = (SxString*)key_ptr;
    if (!map || !key) return 0;

    size_t index = hashmap_bucket_index(map, key);
    HashMapEntry* entry = map->buckets[index];

    while (entry) {
        if (string_equals(entry->key, key)) {
            return entry->value;
        }
        entry = entry->next;
    }

    return 0;  // Not found
}

// Get value with Option return (returns pointer to value, or 0 if not found)
// This is safer - returns the address of the value or NULL
int64_t hashmap_get_ptr(int64_t map_ptr, int64_t key_ptr) {
    HashMap* map = (HashMap*)map_ptr;
    SxString* key = (SxString*)key_ptr;
    if (!map || !key) return 0;

    size_t index = hashmap_bucket_index(map, key);
    HashMapEntry* entry = map->buckets[index];

    while (entry) {
        if (string_equals(entry->key, key)) {
            return (int64_t)&entry->value;
        }
        entry = entry->next;
    }

    return 0;  // Not found
}

// Remove entry by key (returns removed value, or 0 if not found)
int64_t hashmap_remove(int64_t map_ptr, int64_t key_ptr) {
    HashMap* map = (HashMap*)map_ptr;
    SxString* key = (SxString*)key_ptr;
    if (!map || !key) return 0;

    size_t index = hashmap_bucket_index(map, key);
    HashMapEntry* entry = map->buckets[index];
    HashMapEntry* prev = NULL;

    while (entry) {
        if (string_equals(entry->key, key)) {
            // Found - remove from chain
            if (prev) {
                prev->next = entry->next;
            } else {
                map->buckets[index] = entry->next;
            }

            int64_t value = entry->value;

            // Free the entry
            if (entry->key) {
                if (entry->key->data) free(entry->key->data);
                free(entry->key);
            }
            free(entry);

            map->size--;
            return value;
        }
        prev = entry;
        entry = entry->next;
    }

    return 0;  // Not found
}

// Check if key exists in map
int8_t hashmap_contains(int64_t map_ptr, int64_t key_ptr) {
    HashMap* map = (HashMap*)map_ptr;
    SxString* key = (SxString*)key_ptr;
    if (!map || !key) return 0;

    size_t index = hashmap_bucket_index(map, key);
    HashMapEntry* entry = map->buckets[index];

    while (entry) {
        if (string_equals(entry->key, key)) {
            return 1;  // Found
        }
        entry = entry->next;
    }

    return 0;  // Not found
}

// Get number of entries in map
int64_t hashmap_len(int64_t map_ptr) {
    HashMap* map = (HashMap*)map_ptr;
    return map ? (int64_t)map->size : 0;
}

// Check if map is empty
int8_t hashmap_is_empty(int64_t map_ptr) {
    HashMap* map = (HashMap*)map_ptr;
    return !map || map->size == 0;
}

// Get capacity of map
int64_t hashmap_capacity(int64_t map_ptr) {
    HashMap* map = (HashMap*)map_ptr;
    return map ? (int64_t)map->capacity : 0;
}

// Clear all entries from map (keep capacity)
void hashmap_clear(int64_t map_ptr) {
    HashMap* map = (HashMap*)map_ptr;
    if (!map) return;

    for (size_t i = 0; i < map->capacity; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;

            // Free key
            if (entry->key) {
                if (entry->key->data) free(entry->key->data);
                free(entry->key);
            }
            free(entry);

            entry = next;
        }
        map->buckets[i] = NULL;
    }

    map->size = 0;
}

// Free the entire map
void hashmap_free(int64_t map_ptr) {
    HashMap* map = (HashMap*)map_ptr;
    if (!map) return;

    // Clear all entries
    hashmap_clear(map_ptr);

    // Free buckets array and map
    free(map->buckets);
    free(map);
}

// ============================================================================
// HASHMAP ITERATORS
// ============================================================================

// Helper to advance iterator to next valid entry
static void hashmap_iter_advance(HashMapIterator* iter) {
    if (!iter || !iter->map) return;

    // If we have a current entry, try its next
    if (iter->current_entry) {
        iter->current_entry = iter->current_entry->next;
        if (iter->current_entry) return;  // Found next in same bucket
        iter->bucket_index++;  // Move to next bucket
    }

    // Find next non-empty bucket
    while (iter->bucket_index < iter->map->capacity) {
        if (iter->map->buckets[iter->bucket_index]) {
            iter->current_entry = iter->map->buckets[iter->bucket_index];
            return;
        }
        iter->bucket_index++;
    }

    // No more entries
    iter->current_entry = NULL;
}

// Create iterator over keys
int64_t hashmap_keys(int64_t map_ptr) {
    HashMap* map = (HashMap*)map_ptr;
    if (!map) return 0;

    HashMapIterator* iter = malloc(sizeof(HashMapIterator));
    if (!iter) return 0;

    iter->map = map;
    iter->bucket_index = 0;
    iter->current_entry = NULL;
    iter->iter_type = 0;  // keys

    // Find first entry
    hashmap_iter_advance(iter);

    return (int64_t)iter;
}

// Create iterator over values
int64_t hashmap_values(int64_t map_ptr) {
    HashMap* map = (HashMap*)map_ptr;
    if (!map) return 0;

    HashMapIterator* iter = malloc(sizeof(HashMapIterator));
    if (!iter) return 0;

    iter->map = map;
    iter->bucket_index = 0;
    iter->current_entry = NULL;
    iter->iter_type = 1;  // values

    // Find first entry
    hashmap_iter_advance(iter);

    return (int64_t)iter;
}

// Create iterator over entries (key-value pairs)
int64_t hashmap_iter(int64_t map_ptr) {
    HashMap* map = (HashMap*)map_ptr;
    if (!map) return 0;

    HashMapIterator* iter = malloc(sizeof(HashMapIterator));
    if (!iter) return 0;

    iter->map = map;
    iter->bucket_index = 0;
    iter->current_entry = NULL;
    iter->iter_type = 2;  // entries

    // Find first entry
    hashmap_iter_advance(iter);

    return (int64_t)iter;
}

// Get next key from keys iterator (returns 0 when done)
int64_t hashmap_keys_next(int64_t iter_ptr) {
    HashMapIterator* iter = (HashMapIterator*)iter_ptr;
    if (!iter || !iter->current_entry) return 0;

    SxString* key = iter->current_entry->key;
    hashmap_iter_advance(iter);

    return (int64_t)key;
}

// Get next value from values iterator (returns 0 when done - ambiguous with actual 0 value)
int64_t hashmap_values_next(int64_t iter_ptr) {
    HashMapIterator* iter = (HashMapIterator*)iter_ptr;
    if (!iter || !iter->current_entry) return 0;

    int64_t value = iter->current_entry->value;
    hashmap_iter_advance(iter);

    return value;
}

// Get next entry from entries iterator (returns pointer to KV pair, or 0 when done)
int64_t hashmap_iter_next(int64_t iter_ptr) {
    HashMapIterator* iter = (HashMapIterator*)iter_ptr;
    if (!iter || !iter->current_entry) return 0;

    // Allocate a KV pair to return
    HashMapKV* kv = malloc(sizeof(HashMapKV));
    if (!kv) return 0;

    kv->key = iter->current_entry->key;
    kv->value = iter->current_entry->value;

    hashmap_iter_advance(iter);

    return (int64_t)kv;
}

// Check if iterator has more elements
int8_t hashmap_iter_has_next(int64_t iter_ptr) {
    HashMapIterator* iter = (HashMapIterator*)iter_ptr;
    return iter && iter->current_entry != NULL;
}

// Free iterator
void hashmap_iter_free(int64_t iter_ptr) {
    HashMapIterator* iter = (HashMapIterator*)iter_ptr;
    if (iter) free(iter);
}

// Free KV pair from entry iteration
void hashmap_kv_free(int64_t kv_ptr) {
    HashMapKV* kv = (HashMapKV*)kv_ptr;
    if (kv) free(kv);
}

// Get key from KV pair
int64_t hashmap_kv_key(int64_t kv_ptr) {
    HashMapKV* kv = (HashMapKV*)kv_ptr;
    return kv ? (int64_t)kv->key : 0;
}

// Get value from KV pair
int64_t hashmap_kv_value(int64_t kv_ptr) {
    HashMapKV* kv = (HashMapKV*)kv_ptr;
    return kv ? kv->value : 0;
}

// ============================================================================
// HASHMAP CONVENIENCE FUNCTIONS
// ============================================================================

// Insert with C string key (convenience)
int64_t hashmap_insert_cstr(int64_t map_ptr, const char* key, int64_t value) {
    SxString* key_str = intrinsic_string_new(key);
    int64_t result = hashmap_insert(map_ptr, (int64_t)key_str, value);
    // Note: key is copied in hashmap_insert, so we can free this
    if (key_str->data) free(key_str->data);
    free(key_str);
    return result;
}

// Get with C string key (convenience)
int64_t hashmap_get_cstr(int64_t map_ptr, const char* key) {
    SxString* key_str = intrinsic_string_new(key);
    int64_t result = hashmap_get(map_ptr, (int64_t)key_str);
    if (key_str->data) free(key_str->data);
    free(key_str);
    return result;
}

// Contains with C string key (convenience)
int8_t hashmap_contains_cstr(int64_t map_ptr, const char* key) {
    SxString* key_str = intrinsic_string_new(key);
    int8_t result = hashmap_contains(map_ptr, (int64_t)key_str);
    if (key_str->data) free(key_str->data);
    free(key_str);
    return result;
}

// Remove with C string key (convenience)
int64_t hashmap_remove_cstr(int64_t map_ptr, const char* key) {
    SxString* key_str = intrinsic_string_new(key);
    int64_t result = hashmap_remove(map_ptr, (int64_t)key_str);
    if (key_str->data) free(key_str->data);
    free(key_str);
    return result;
}

// ============================================================================
// INTEGER-KEYED HASHMAP (for when keys are i64)
// ============================================================================

typedef struct IntHashMapEntry {
    int64_t key;
    int64_t value;
    struct IntHashMapEntry* next;
    int8_t occupied;  // For distinguishing 0 keys
} IntHashMapEntry;

typedef struct {
    IntHashMapEntry** buckets;
    size_t capacity;
    size_t size;
} IntHashMap;

typedef struct {
    IntHashMap* map;
    size_t bucket_index;
    IntHashMapEntry* current_entry;
} IntHashMapIterator;

// Hash function for integers
static uint64_t hash_int(int64_t key) {
    // Mixing function for better distribution
    uint64_t x = (uint64_t)key;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return x;
}

// Create new integer-keyed HashMap
int64_t int_hashmap_new(void) {
    IntHashMap* map = malloc(sizeof(IntHashMap));
    if (!map) return 0;

    map->capacity = HASHMAP_INITIAL_CAPACITY;
    map->size = 0;
    map->buckets = calloc(map->capacity, sizeof(IntHashMapEntry*));
    if (!map->buckets) {
        free(map);
        return 0;
    }

    return (int64_t)map;
}

// Resize integer HashMap
static void int_hashmap_resize(IntHashMap* map, size_t new_capacity) {
    IntHashMapEntry** old_buckets = map->buckets;
    size_t old_capacity = map->capacity;

    map->buckets = calloc(new_capacity, sizeof(IntHashMapEntry*));
    if (!map->buckets) {
        map->buckets = old_buckets;
        return;
    }
    map->capacity = new_capacity;

    for (size_t i = 0; i < old_capacity; i++) {
        IntHashMapEntry* entry = old_buckets[i];
        while (entry) {
            IntHashMapEntry* next = entry->next;
            size_t new_index = hash_int(entry->key) % new_capacity;
            entry->next = map->buckets[new_index];
            map->buckets[new_index] = entry;
            entry = next;
        }
    }

    free(old_buckets);
}

// Insert into integer HashMap
int64_t int_hashmap_insert(int64_t map_ptr, int64_t key, int64_t value) {
    IntHashMap* map = (IntHashMap*)map_ptr;
    if (!map) return 0;

    if ((double)(map->size + 1) / map->capacity > HASHMAP_LOAD_FACTOR) {
        int_hashmap_resize(map, map->capacity * 2);
    }

    size_t index = hash_int(key) % map->capacity;

    // Check if key exists
    IntHashMapEntry* entry = map->buckets[index];
    while (entry) {
        if (entry->key == key) {
            int64_t old_value = entry->value;
            entry->value = value;
            return old_value;
        }
        entry = entry->next;
    }

    // Create new entry
    IntHashMapEntry* new_entry = malloc(sizeof(IntHashMapEntry));
    if (!new_entry) return 0;

    new_entry->key = key;
    new_entry->value = value;
    new_entry->occupied = 1;
    new_entry->next = map->buckets[index];
    map->buckets[index] = new_entry;
    map->size++;

    return 0;
}

// Get from integer HashMap
int64_t int_hashmap_get(int64_t map_ptr, int64_t key) {
    IntHashMap* map = (IntHashMap*)map_ptr;
    if (!map) return 0;

    size_t index = hash_int(key) % map->capacity;
    IntHashMapEntry* entry = map->buckets[index];

    while (entry) {
        if (entry->key == key) {
            return entry->value;
        }
        entry = entry->next;
    }

    return 0;
}

// Contains for integer HashMap
int8_t int_hashmap_contains(int64_t map_ptr, int64_t key) {
    IntHashMap* map = (IntHashMap*)map_ptr;
    if (!map) return 0;

    size_t index = hash_int(key) % map->capacity;
    IntHashMapEntry* entry = map->buckets[index];

    while (entry) {
        if (entry->key == key) {
            return 1;
        }
        entry = entry->next;
    }

    return 0;
}

// Remove from integer HashMap
int64_t int_hashmap_remove(int64_t map_ptr, int64_t key) {
    IntHashMap* map = (IntHashMap*)map_ptr;
    if (!map) return 0;

    size_t index = hash_int(key) % map->capacity;
    IntHashMapEntry* entry = map->buckets[index];
    IntHashMapEntry* prev = NULL;

    while (entry) {
        if (entry->key == key) {
            if (prev) {
                prev->next = entry->next;
            } else {
                map->buckets[index] = entry->next;
            }

            int64_t value = entry->value;
            free(entry);
            map->size--;
            return value;
        }
        prev = entry;
        entry = entry->next;
    }

    return 0;
}

// Get length of integer HashMap
int64_t int_hashmap_len(int64_t map_ptr) {
    IntHashMap* map = (IntHashMap*)map_ptr;
    return map ? (int64_t)map->size : 0;
}

// Clear integer HashMap
void int_hashmap_clear(int64_t map_ptr) {
    IntHashMap* map = (IntHashMap*)map_ptr;
    if (!map) return;

    for (size_t i = 0; i < map->capacity; i++) {
        IntHashMapEntry* entry = map->buckets[i];
        while (entry) {
            IntHashMapEntry* next = entry->next;
            free(entry);
            entry = next;
        }
        map->buckets[i] = NULL;
    }

    map->size = 0;
}

// Free integer HashMap
void int_hashmap_free(int64_t map_ptr) {
    IntHashMap* map = (IntHashMap*)map_ptr;
    if (!map) return;

    int_hashmap_clear(map_ptr);
    free(map->buckets);
    free(map);
}

// Iterator for integer HashMap
int64_t int_hashmap_iter(int64_t map_ptr) {
    IntHashMap* map = (IntHashMap*)map_ptr;
    if (!map) return 0;

    IntHashMapIterator* iter = malloc(sizeof(IntHashMapIterator));
    if (!iter) return 0;

    iter->map = map;
    iter->bucket_index = 0;
    iter->current_entry = NULL;

    // Find first entry
    while (iter->bucket_index < map->capacity) {
        if (map->buckets[iter->bucket_index]) {
            iter->current_entry = map->buckets[iter->bucket_index];
            break;
        }
        iter->bucket_index++;
    }

    return (int64_t)iter;
}

// Advance integer HashMap iterator
static void int_hashmap_iter_advance(IntHashMapIterator* iter) {
    if (!iter || !iter->map) return;

    if (iter->current_entry) {
        iter->current_entry = iter->current_entry->next;
        if (iter->current_entry) return;
        iter->bucket_index++;
    }

    while (iter->bucket_index < iter->map->capacity) {
        if (iter->map->buckets[iter->bucket_index]) {
            iter->current_entry = iter->map->buckets[iter->bucket_index];
            return;
        }
        iter->bucket_index++;
    }

    iter->current_entry = NULL;
}

// Get next key from integer HashMap iterator
int64_t int_hashmap_iter_next_key(int64_t iter_ptr) {
    IntHashMapIterator* iter = (IntHashMapIterator*)iter_ptr;
    if (!iter || !iter->current_entry) return 0;

    int64_t key = iter->current_entry->key;
    int_hashmap_iter_advance(iter);
    return key;
}

// Check if integer HashMap iterator has next
int8_t int_hashmap_iter_has_next(int64_t iter_ptr) {
    IntHashMapIterator* iter = (IntHashMapIterator*)iter_ptr;
    return iter && iter->current_entry != NULL;
}

// Free integer HashMap iterator
void int_hashmap_iter_free(int64_t iter_ptr) {
    IntHashMapIterator* iter = (IntHashMapIterator*)iter_ptr;
    if (iter) free(iter);
}

// ============================================================================
// HASHSET IMPLEMENTATION - Phase 1 Core
// ============================================================================
//
// A complete HashSet implementation with string elements.
// Implemented as a wrapper around HashMap (value is always 1).
// ============================================================================

// HashSet is just a HashMap where we only care about keys
// We use the value field to indicate presence (1 = present)

// Create a new empty HashSet
int64_t hashset_new(void) {
    return hashmap_new();
}

// Create a HashSet with specified initial capacity
int64_t hashset_with_capacity(int64_t capacity) {
    return hashmap_with_capacity(capacity);
}

// Insert a value into the set (returns 1 if newly inserted, 0 if already existed)
int8_t hashset_insert(int64_t set_ptr, int64_t value_ptr) {
    if (!hashmap_contains(set_ptr, value_ptr)) {
        hashmap_insert(set_ptr, value_ptr, 1);
        return 1;  // Newly inserted
    }
    return 0;  // Already existed
}

// Remove a value from the set (returns 1 if removed, 0 if not found)
int8_t hashset_remove(int64_t set_ptr, int64_t value_ptr) {
    if (hashmap_contains(set_ptr, value_ptr)) {
        hashmap_remove(set_ptr, value_ptr);
        return 1;  // Removed
    }
    return 0;  // Not found
}

// Check if value exists in set
int8_t hashset_contains(int64_t set_ptr, int64_t value_ptr) {
    return hashmap_contains(set_ptr, value_ptr);
}

// Get number of elements in set
int64_t hashset_len(int64_t set_ptr) {
    return hashmap_len(set_ptr);
}

// Check if set is empty
int8_t hashset_is_empty(int64_t set_ptr) {
    return hashmap_is_empty(set_ptr);
}

// Clear all elements from set
void hashset_clear(int64_t set_ptr) {
    hashmap_clear(set_ptr);
}

// Free the set
void hashset_free(int64_t set_ptr) {
    hashmap_free(set_ptr);
}

// Create iterator over set elements
int64_t hashset_iter(int64_t set_ptr) {
    return hashmap_keys(set_ptr);
}

// Get next element from iterator (returns 0 when done)
int64_t hashset_iter_next(int64_t iter_ptr) {
    return hashmap_keys_next(iter_ptr);
}

// Check if iterator has more elements
int8_t hashset_iter_has_next(int64_t iter_ptr) {
    return hashmap_iter_has_next(iter_ptr);
}

// Free iterator
void hashset_iter_free(int64_t iter_ptr) {
    hashmap_iter_free(iter_ptr);
}

// ============================================================================
// HASHSET OPERATIONS (Set Algebra)
// ============================================================================

// Union of two sets (returns new set containing elements in either a or b)
int64_t hashset_union(int64_t set_a_ptr, int64_t set_b_ptr) {
    int64_t result = hashset_new();

    // Add all from set a
    int64_t iter_a = hashset_iter(set_a_ptr);
    while (hashset_iter_has_next(iter_a)) {
        int64_t elem = hashset_iter_next(iter_a);
        // Need to duplicate the string key
        SxString* key = (SxString*)elem;
        SxString* copy = malloc(sizeof(SxString));
        copy->len = key->len;
        copy->cap = key->len + 1;
        copy->data = malloc(copy->cap);
        memcpy(copy->data, key->data, key->len);
        copy->data[key->len] = '\0';
        hashset_insert(result, (int64_t)copy);
    }
    hashset_iter_free(iter_a);

    // Add all from set b (duplicates ignored due to set semantics)
    int64_t iter_b = hashset_iter(set_b_ptr);
    while (hashset_iter_has_next(iter_b)) {
        int64_t elem = hashset_iter_next(iter_b);
        SxString* key = (SxString*)elem;
        SxString* copy = malloc(sizeof(SxString));
        copy->len = key->len;
        copy->cap = key->len + 1;
        copy->data = malloc(copy->cap);
        memcpy(copy->data, key->data, key->len);
        copy->data[key->len] = '\0';
        hashset_insert(result, (int64_t)copy);
    }
    hashset_iter_free(iter_b);

    return result;
}

// Intersection of two sets (returns new set containing elements in both a and b)
int64_t hashset_intersection(int64_t set_a_ptr, int64_t set_b_ptr) {
    int64_t result = hashset_new();

    // Iterate over smaller set for efficiency
    int64_t smaller = hashset_len(set_a_ptr) <= hashset_len(set_b_ptr) ? set_a_ptr : set_b_ptr;
    int64_t larger = smaller == set_a_ptr ? set_b_ptr : set_a_ptr;

    int64_t iter = hashset_iter(smaller);
    while (hashset_iter_has_next(iter)) {
        int64_t elem = hashset_iter_next(iter);
        if (hashset_contains(larger, elem)) {
            SxString* key = (SxString*)elem;
            SxString* copy = malloc(sizeof(SxString));
            copy->len = key->len;
            copy->cap = key->len + 1;
            copy->data = malloc(copy->cap);
            memcpy(copy->data, key->data, key->len);
            copy->data[key->len] = '\0';
            hashset_insert(result, (int64_t)copy);
        }
    }
    hashset_iter_free(iter);

    return result;
}

// Difference of two sets (returns new set containing elements in a but not in b)
int64_t hashset_difference(int64_t set_a_ptr, int64_t set_b_ptr) {
    int64_t result = hashset_new();

    int64_t iter = hashset_iter(set_a_ptr);
    while (hashset_iter_has_next(iter)) {
        int64_t elem = hashset_iter_next(iter);
        if (!hashset_contains(set_b_ptr, elem)) {
            SxString* key = (SxString*)elem;
            SxString* copy = malloc(sizeof(SxString));
            copy->len = key->len;
            copy->cap = key->len + 1;
            copy->data = malloc(copy->cap);
            memcpy(copy->data, key->data, key->len);
            copy->data[key->len] = '\0';
            hashset_insert(result, (int64_t)copy);
        }
    }
    hashset_iter_free(iter);

    return result;
}

// Symmetric difference (returns new set containing elements in a or b but not both)
int64_t hashset_symmetric_difference(int64_t set_a_ptr, int64_t set_b_ptr) {
    int64_t result = hashset_new();

    // Elements in a but not in b
    int64_t iter_a = hashset_iter(set_a_ptr);
    while (hashset_iter_has_next(iter_a)) {
        int64_t elem = hashset_iter_next(iter_a);
        if (!hashset_contains(set_b_ptr, elem)) {
            SxString* key = (SxString*)elem;
            SxString* copy = malloc(sizeof(SxString));
            copy->len = key->len;
            copy->cap = key->len + 1;
            copy->data = malloc(copy->cap);
            memcpy(copy->data, key->data, key->len);
            copy->data[key->len] = '\0';
            hashset_insert(result, (int64_t)copy);
        }
    }
    hashset_iter_free(iter_a);

    // Elements in b but not in a
    int64_t iter_b = hashset_iter(set_b_ptr);
    while (hashset_iter_has_next(iter_b)) {
        int64_t elem = hashset_iter_next(iter_b);
        if (!hashset_contains(set_a_ptr, elem)) {
            SxString* key = (SxString*)elem;
            SxString* copy = malloc(sizeof(SxString));
            copy->len = key->len;
            copy->cap = key->len + 1;
            copy->data = malloc(copy->cap);
            memcpy(copy->data, key->data, key->len);
            copy->data[key->len] = '\0';
            hashset_insert(result, (int64_t)copy);
        }
    }
    hashset_iter_free(iter_b);

    return result;
}

// Check if set a is subset of set b
int8_t hashset_is_subset(int64_t set_a_ptr, int64_t set_b_ptr) {
    int64_t iter = hashset_iter(set_a_ptr);
    while (hashset_iter_has_next(iter)) {
        int64_t elem = hashset_iter_next(iter);
        if (!hashset_contains(set_b_ptr, elem)) {
            hashset_iter_free(iter);
            return 0;  // Not a subset
        }
    }
    hashset_iter_free(iter);
    return 1;  // Is a subset
}

// Check if set a is superset of set b
int8_t hashset_is_superset(int64_t set_a_ptr, int64_t set_b_ptr) {
    return hashset_is_subset(set_b_ptr, set_a_ptr);
}

// Check if two sets are disjoint (no common elements)
int8_t hashset_is_disjoint(int64_t set_a_ptr, int64_t set_b_ptr) {
    int64_t smaller = hashset_len(set_a_ptr) <= hashset_len(set_b_ptr) ? set_a_ptr : set_b_ptr;
    int64_t larger = smaller == set_a_ptr ? set_b_ptr : set_a_ptr;

    int64_t iter = hashset_iter(smaller);
    while (hashset_iter_has_next(iter)) {
        int64_t elem = hashset_iter_next(iter);
        if (hashset_contains(larger, elem)) {
            hashset_iter_free(iter);
            return 0;  // Not disjoint
        }
    }
    hashset_iter_free(iter);
    return 1;  // Is disjoint
}

// ============================================================================
// HASHSET CONVENIENCE FUNCTIONS (C string)
// ============================================================================

// Insert with C string
int8_t hashset_insert_cstr(int64_t set_ptr, const char* value) {
    SxString* str = intrinsic_string_new(value);
    int8_t result = hashset_insert(set_ptr, (int64_t)str);
    if (!result) {
        // Already existed, free the temp string
        if (str->data) free(str->data);
        free(str);
    }
    return result;
}

// Check if C string exists in set
int8_t hashset_contains_cstr(int64_t set_ptr, const char* value) {
    SxString* str = intrinsic_string_new(value);
    int8_t result = hashset_contains(set_ptr, (int64_t)str);
    if (str->data) free(str->data);
    free(str);
    return result;
}

// Remove C string from set
int8_t hashset_remove_cstr(int64_t set_ptr, const char* value) {
    SxString* str = intrinsic_string_new(value);
    int8_t result = hashset_remove(set_ptr, (int64_t)str);
    if (str->data) free(str->data);
    free(str);
    return result;
}

// ============================================================================
// INTEGER HASHSET (for when elements are i64)
// ============================================================================

// Create new integer HashSet
int64_t int_hashset_new(void) {
    return int_hashmap_new();
}

// Insert integer into set
int8_t int_hashset_insert(int64_t set_ptr, int64_t value) {
    if (!int_hashmap_contains(set_ptr, value)) {
        int_hashmap_insert(set_ptr, value, 1);
        return 1;
    }
    return 0;
}

// Remove integer from set
int8_t int_hashset_remove(int64_t set_ptr, int64_t value) {
    if (int_hashmap_contains(set_ptr, value)) {
        int_hashmap_remove(set_ptr, value);
        return 1;
    }
    return 0;
}

// Check if integer exists in set
int8_t int_hashset_contains(int64_t set_ptr, int64_t value) {
    return int_hashmap_contains(set_ptr, value);
}

// Get number of elements in integer set
int64_t int_hashset_len(int64_t set_ptr) {
    return int_hashmap_len(set_ptr);
}

// Clear all elements from integer set
void int_hashset_clear(int64_t set_ptr) {
    int_hashmap_clear(set_ptr);
}

// Free integer set
void int_hashset_free(int64_t set_ptr) {
    int_hashmap_free(set_ptr);
}

// Create iterator over integer set
int64_t int_hashset_iter(int64_t set_ptr) {
    return int_hashmap_iter(set_ptr);
}

// Get next element from integer set iterator
int64_t int_hashset_iter_next(int64_t iter_ptr) {
    return int_hashmap_iter_next_key(iter_ptr);
}

// Check if integer set iterator has more elements
int8_t int_hashset_iter_has_next(int64_t iter_ptr) {
    return int_hashmap_iter_has_next(iter_ptr);
}

// Free integer set iterator
void int_hashset_iter_free(int64_t iter_ptr) {
    int_hashmap_iter_free(iter_ptr);
}

// Union of two integer sets
int64_t int_hashset_union(int64_t set_a_ptr, int64_t set_b_ptr) {
    int64_t result = int_hashset_new();

    int64_t iter_a = int_hashset_iter(set_a_ptr);
    while (int_hashset_iter_has_next(iter_a)) {
        int_hashset_insert(result, int_hashset_iter_next(iter_a));
    }
    int_hashset_iter_free(iter_a);

    int64_t iter_b = int_hashset_iter(set_b_ptr);
    while (int_hashset_iter_has_next(iter_b)) {
        int_hashset_insert(result, int_hashset_iter_next(iter_b));
    }
    int_hashset_iter_free(iter_b);

    return result;
}

// Intersection of two integer sets
int64_t int_hashset_intersection(int64_t set_a_ptr, int64_t set_b_ptr) {
    int64_t result = int_hashset_new();

    int64_t smaller = int_hashset_len(set_a_ptr) <= int_hashset_len(set_b_ptr) ? set_a_ptr : set_b_ptr;
    int64_t larger = smaller == set_a_ptr ? set_b_ptr : set_a_ptr;

    int64_t iter = int_hashset_iter(smaller);
    while (int_hashset_iter_has_next(iter)) {
        int64_t elem = int_hashset_iter_next(iter);
        if (int_hashset_contains(larger, elem)) {
            int_hashset_insert(result, elem);
        }
    }
    int_hashset_iter_free(iter);

    return result;
}

// Difference of two integer sets (a - b)
int64_t int_hashset_difference(int64_t set_a_ptr, int64_t set_b_ptr) {
    int64_t result = int_hashset_new();

    int64_t iter = int_hashset_iter(set_a_ptr);
    while (int_hashset_iter_has_next(iter)) {
        int64_t elem = int_hashset_iter_next(iter);
        if (!int_hashset_contains(set_b_ptr, elem)) {
            int_hashset_insert(result, elem);
        }
    }
    int_hashset_iter_free(iter);

    return result;
}

// Check if integer set a is subset of set b
int8_t int_hashset_is_subset(int64_t set_a_ptr, int64_t set_b_ptr) {
    int64_t iter = int_hashset_iter(set_a_ptr);
    while (int_hashset_iter_has_next(iter)) {
        if (!int_hashset_contains(set_b_ptr, int_hashset_iter_next(iter))) {
            int_hashset_iter_free(iter);
            return 0;
        }
    }
    int_hashset_iter_free(iter);
    return 1;
}

// =============================================================================
// Phase 4.11: Observability - Metrics, Tracing, and Logging
// =============================================================================

// Log levels
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
    LOG_FATAL = 4
} LogLevel;

// Metric types
typedef enum {
    METRIC_COUNTER = 0,
    METRIC_GAUGE = 1,
    METRIC_HISTOGRAM = 2
} MetricType;

// Span status
typedef enum {
    SPAN_STATUS_UNSET = 0,
    SPAN_STATUS_OK = 1,
    SPAN_STATUS_ERROR = 2
} SpanStatus;

// Label pair for metrics
typedef struct MetricLabel {
    char* key;
    char* value;
    struct MetricLabel* next;
} MetricLabel;

// Counter metric
typedef struct {
    char* name;
    char* description;
    double value;
    MetricLabel* labels;
    pthread_mutex_t lock;
} Counter;

// Gauge metric
typedef struct {
    char* name;
    char* description;
    double value;
    MetricLabel* labels;
    pthread_mutex_t lock;
} Gauge;

// Histogram bucket
typedef struct {
    double upper_bound;
    int64_t count;
} HistogramBucket;

// Histogram metric
typedef struct {
    char* name;
    char* description;
    double sum;
    int64_t count;
    double min;
    double max;
    HistogramBucket* buckets;
    size_t bucket_count;
    MetricLabel* labels;
    pthread_mutex_t lock;
} Histogram;

// Metrics registry
typedef struct MetricEntry {
    char* name;
    MetricType type;
    void* metric;
    struct MetricEntry* next;
} MetricEntry;

typedef struct {
    MetricEntry* metrics;
    size_t count;
    pthread_mutex_t lock;
} MetricsRegistry;

// Global metrics registry
static MetricsRegistry* global_metrics_registry = NULL;

// Span event
typedef struct SpanEvent {
    char* name;
    int64_t timestamp;
    MetricLabel* attributes;
    struct SpanEvent* next;
} SpanEvent;

// Span
typedef struct {
    char* trace_id;
    char* span_id;
    char* parent_span_id;
    char* name;
    int64_t start_time;
    int64_t end_time;
    SpanStatus status;
    char* status_message;
    MetricLabel* attributes;
    SpanEvent* events;
    int ended;
    pthread_mutex_t lock;
} Span;

// Trace context
typedef struct {
    char* trace_id;
    char* span_id;
} TraceContext;

// Tracer
typedef struct {
    char* service_name;
    Span** active_spans;
    size_t span_count;
    size_t span_cap;
    pthread_mutex_t lock;
} Tracer;

// Log context field
typedef struct LogField {
    char* key;
    char* value;
    struct LogField* next;
} LogField;

// Logger
typedef struct {
    char* name;
    LogLevel min_level;
    int console_output;
    int json_output;
    FILE* file_output;
    LogField* context;
    pthread_mutex_t lock;
} Logger;

// Global logger
static Logger* global_logger = NULL;

// ==================== Metrics Functions ====================

// Initialize metrics registry
int64_t metrics_registry_new(void) {
    MetricsRegistry* reg = malloc(sizeof(MetricsRegistry));
    reg->metrics = NULL;
    reg->count = 0;
    pthread_mutex_init(&reg->lock, NULL);
    global_metrics_registry = reg;
    return (int64_t)reg;
}

// Get global metrics registry
int64_t metrics_registry_global(void) {
    if (!global_metrics_registry) {
        return metrics_registry_new();
    }
    return (int64_t)global_metrics_registry;
}

// Create a counter
int64_t counter_new(int64_t name_ptr, int64_t desc_ptr) {
    Counter* c = malloc(sizeof(Counter));
    SxString* name = (SxString*)name_ptr;
    SxString* desc = (SxString*)desc_ptr;

    c->name = strdup(name ? name->data : "unnamed");
    c->description = strdup(desc ? desc->data : "");
    c->value = 0;
    c->labels = NULL;
    pthread_mutex_init(&c->lock, NULL);

    // Register with global registry
    if (global_metrics_registry) {
        pthread_mutex_lock(&global_metrics_registry->lock);
        MetricEntry* entry = malloc(sizeof(MetricEntry));
        entry->name = strdup(c->name);
        entry->type = METRIC_COUNTER;
        entry->metric = c;
        entry->next = global_metrics_registry->metrics;
        global_metrics_registry->metrics = entry;
        global_metrics_registry->count++;
        pthread_mutex_unlock(&global_metrics_registry->lock);
    }

    return (int64_t)c;
}

// Increment counter
void counter_inc(int64_t counter_ptr) {
    Counter* c = (Counter*)counter_ptr;
    if (!c) return;
    pthread_mutex_lock(&c->lock);
    c->value += 1;
    pthread_mutex_unlock(&c->lock);
}

// Add to counter
void counter_add(int64_t counter_ptr, double value) {
    Counter* c = (Counter*)counter_ptr;
    if (!c || value < 0) return;  // Counters only increase
    pthread_mutex_lock(&c->lock);
    c->value += value;
    pthread_mutex_unlock(&c->lock);
}

// Get counter value
double counter_value(int64_t counter_ptr) {
    Counter* c = (Counter*)counter_ptr;
    if (!c) return 0;
    pthread_mutex_lock(&c->lock);
    double v = c->value;
    pthread_mutex_unlock(&c->lock);
    return v;
}

// Add label to counter
void counter_add_label(int64_t counter_ptr, int64_t key_ptr, int64_t value_ptr) {
    Counter* c = (Counter*)counter_ptr;
    if (!c) return;
    SxString* key = (SxString*)key_ptr;
    SxString* value = (SxString*)value_ptr;

    MetricLabel* label = malloc(sizeof(MetricLabel));
    label->key = strdup(key ? key->data : "");
    label->value = strdup(value ? value->data : "");

    pthread_mutex_lock(&c->lock);
    label->next = c->labels;
    c->labels = label;
    pthread_mutex_unlock(&c->lock);
}

// Create a gauge
int64_t gauge_new(int64_t name_ptr, int64_t desc_ptr) {
    Gauge* g = malloc(sizeof(Gauge));
    SxString* name = (SxString*)name_ptr;
    SxString* desc = (SxString*)desc_ptr;

    g->name = strdup(name ? name->data : "unnamed");
    g->description = strdup(desc ? desc->data : "");
    g->value = 0;
    g->labels = NULL;
    pthread_mutex_init(&g->lock, NULL);

    // Register with global registry
    if (global_metrics_registry) {
        pthread_mutex_lock(&global_metrics_registry->lock);
        MetricEntry* entry = malloc(sizeof(MetricEntry));
        entry->name = strdup(g->name);
        entry->type = METRIC_GAUGE;
        entry->metric = g;
        entry->next = global_metrics_registry->metrics;
        global_metrics_registry->metrics = entry;
        global_metrics_registry->count++;
        pthread_mutex_unlock(&global_metrics_registry->lock);
    }

    return (int64_t)g;
}

// Set gauge value
void gauge_set(int64_t gauge_ptr, double value) {
    Gauge* g = (Gauge*)gauge_ptr;
    if (!g) return;
    pthread_mutex_lock(&g->lock);
    g->value = value;
    pthread_mutex_unlock(&g->lock);
}

// Increment gauge
void gauge_inc(int64_t gauge_ptr) {
    Gauge* g = (Gauge*)gauge_ptr;
    if (!g) return;
    pthread_mutex_lock(&g->lock);
    g->value += 1;
    pthread_mutex_unlock(&g->lock);
}

// Decrement gauge
void gauge_dec(int64_t gauge_ptr) {
    Gauge* g = (Gauge*)gauge_ptr;
    if (!g) return;
    pthread_mutex_lock(&g->lock);
    g->value -= 1;
    pthread_mutex_unlock(&g->lock);
}

// Add to gauge
void gauge_add(int64_t gauge_ptr, double value) {
    Gauge* g = (Gauge*)gauge_ptr;
    if (!g) return;
    pthread_mutex_lock(&g->lock);
    g->value += value;
    pthread_mutex_unlock(&g->lock);
}

// Get gauge value
double gauge_value(int64_t gauge_ptr) {
    Gauge* g = (Gauge*)gauge_ptr;
    if (!g) return 0;
    pthread_mutex_lock(&g->lock);
    double v = g->value;
    pthread_mutex_unlock(&g->lock);
    return v;
}

// Create a histogram with default buckets
int64_t histogram_new(int64_t name_ptr, int64_t desc_ptr) {
    Histogram* h = malloc(sizeof(Histogram));
    SxString* name = (SxString*)name_ptr;
    SxString* desc = (SxString*)desc_ptr;

    h->name = strdup(name ? name->data : "unnamed");
    h->description = strdup(desc ? desc->data : "");
    h->sum = 0;
    h->count = 0;
    h->min = 0;
    h->max = 0;
    h->labels = NULL;
    pthread_mutex_init(&h->lock, NULL);

    // Default buckets: 5, 10, 25, 50, 100, 250, 500, 1000, 2500, 5000, 10000 ms
    double default_bounds[] = {5, 10, 25, 50, 100, 250, 500, 1000, 2500, 5000, 10000};
    h->bucket_count = 11;
    h->buckets = malloc(sizeof(HistogramBucket) * h->bucket_count);
    for (size_t i = 0; i < h->bucket_count; i++) {
        h->buckets[i].upper_bound = default_bounds[i];
        h->buckets[i].count = 0;
    }

    // Register with global registry
    if (global_metrics_registry) {
        pthread_mutex_lock(&global_metrics_registry->lock);
        MetricEntry* entry = malloc(sizeof(MetricEntry));
        entry->name = strdup(h->name);
        entry->type = METRIC_HISTOGRAM;
        entry->metric = h;
        entry->next = global_metrics_registry->metrics;
        global_metrics_registry->metrics = entry;
        global_metrics_registry->count++;
        pthread_mutex_unlock(&global_metrics_registry->lock);
    }

    return (int64_t)h;
}

// Create histogram with custom buckets
int64_t histogram_new_with_buckets(int64_t name_ptr, int64_t desc_ptr, int64_t buckets_ptr) {
    Histogram* h = malloc(sizeof(Histogram));
    SxString* name = (SxString*)name_ptr;
    SxString* desc = (SxString*)desc_ptr;
    SxVec* buckets = (SxVec*)buckets_ptr;

    h->name = strdup(name ? name->data : "unnamed");
    h->description = strdup(desc ? desc->data : "");
    h->sum = 0;
    h->count = 0;
    h->min = 0;
    h->max = 0;
    h->labels = NULL;
    pthread_mutex_init(&h->lock, NULL);

    if (buckets && buckets->len > 0) {
        h->bucket_count = buckets->len;
        h->buckets = malloc(sizeof(HistogramBucket) * h->bucket_count);
        for (size_t i = 0; i < h->bucket_count; i++) {
            h->buckets[i].upper_bound = *(double*)buckets->items[i];
            h->buckets[i].count = 0;
        }
    } else {
        h->bucket_count = 0;
        h->buckets = NULL;
    }

    return (int64_t)h;
}

// Observe a value in histogram
void histogram_observe(int64_t histogram_ptr, double value) {
    Histogram* h = (Histogram*)histogram_ptr;
    if (!h) return;

    pthread_mutex_lock(&h->lock);
    h->sum += value;
    h->count++;

    if (h->count == 1) {
        h->min = value;
        h->max = value;
    } else {
        if (value < h->min) h->min = value;
        if (value > h->max) h->max = value;
    }

    // Update buckets
    for (size_t i = 0; i < h->bucket_count; i++) {
        if (value <= h->buckets[i].upper_bound) {
            h->buckets[i].count++;
        }
    }
    pthread_mutex_unlock(&h->lock);
}

// Get histogram sum
double histogram_sum(int64_t histogram_ptr) {
    Histogram* h = (Histogram*)histogram_ptr;
    if (!h) return 0;
    return h->sum;
}

// Get histogram count
int64_t histogram_count(int64_t histogram_ptr) {
    Histogram* h = (Histogram*)histogram_ptr;
    if (!h) return 0;
    return h->count;
}

// Get histogram mean
double histogram_mean(int64_t histogram_ptr) {
    Histogram* h = (Histogram*)histogram_ptr;
    if (!h || h->count == 0) return 0;
    return h->sum / h->count;
}

// Get histogram min
double histogram_min(int64_t histogram_ptr) {
    Histogram* h = (Histogram*)histogram_ptr;
    if (!h) return 0;
    return h->min;
}

// Get histogram max
double histogram_max(int64_t histogram_ptr) {
    Histogram* h = (Histogram*)histogram_ptr;
    if (!h) return 0;
    return h->max;
}

// Get histogram as JSON
int64_t histogram_to_json(int64_t histogram_ptr) {
    Histogram* h = (Histogram*)histogram_ptr;
    if (!h) return (int64_t)intrinsic_string_new("{}");

    char buf[4096];
    int offset = 0;

    offset += snprintf(buf + offset, sizeof(buf) - offset,
        "{\"name\":\"%s\",\"count\":%lld,\"sum\":%.2f,\"mean\":%.2f,\"min\":%.2f,\"max\":%.2f,\"buckets\":[",
        h->name, (long long)h->count, h->sum,
        h->count > 0 ? h->sum / h->count : 0, h->min, h->max);

    for (size_t i = 0; i < h->bucket_count && offset < sizeof(buf) - 100; i++) {
        if (i > 0) offset += snprintf(buf + offset, sizeof(buf) - offset, ",");
        offset += snprintf(buf + offset, sizeof(buf) - offset,
            "{\"le\":%.0f,\"count\":%lld}",
            h->buckets[i].upper_bound, (long long)h->buckets[i].count);
    }

    offset += snprintf(buf + offset, sizeof(buf) - offset, "]}");
    return (int64_t)intrinsic_string_new(buf);
}

// Get metrics count in registry
int64_t metrics_registry_count(int64_t reg_ptr) {
    MetricsRegistry* reg = (MetricsRegistry*)reg_ptr;
    if (!reg) return 0;
    return reg->count;
}

// Export all metrics as JSON
int64_t metrics_export_json(int64_t reg_ptr) {
    MetricsRegistry* reg = (MetricsRegistry*)reg_ptr;
    if (!reg) return (int64_t)intrinsic_string_new("[]");

    char* buf = malloc(65536);
    int offset = 0;

    offset += snprintf(buf + offset, 65536 - offset, "[");

    pthread_mutex_lock(&reg->lock);
    int first = 1;
    for (MetricEntry* entry = reg->metrics; entry; entry = entry->next) {
        if (!first) offset += snprintf(buf + offset, 65536 - offset, ",");
        first = 0;

        if (entry->type == METRIC_COUNTER) {
            Counter* c = (Counter*)entry->metric;
            offset += snprintf(buf + offset, 65536 - offset,
                "{\"name\":\"%s\",\"type\":\"counter\",\"value\":%.2f}",
                c->name, c->value);
        } else if (entry->type == METRIC_GAUGE) {
            Gauge* g = (Gauge*)entry->metric;
            offset += snprintf(buf + offset, 65536 - offset,
                "{\"name\":\"%s\",\"type\":\"gauge\",\"value\":%.2f}",
                g->name, g->value);
        } else if (entry->type == METRIC_HISTOGRAM) {
            Histogram* h = (Histogram*)entry->metric;
            offset += snprintf(buf + offset, 65536 - offset,
                "{\"name\":\"%s\",\"type\":\"histogram\",\"count\":%lld,\"sum\":%.2f,\"mean\":%.2f}",
                h->name, (long long)h->count, h->sum, h->count > 0 ? h->sum / h->count : 0);
        }
    }
    pthread_mutex_unlock(&reg->lock);

    offset += snprintf(buf + offset, 65536 - offset, "]");

    SxString* result = intrinsic_string_new(buf);
    free(buf);
    return (int64_t)result;
}

// Export metrics in Prometheus format
int64_t metrics_export_prometheus(int64_t reg_ptr) {
    MetricsRegistry* reg = (MetricsRegistry*)reg_ptr;
    if (!reg) return (int64_t)intrinsic_string_new("");

    char* buf = malloc(65536);
    int offset = 0;

    pthread_mutex_lock(&reg->lock);
    for (MetricEntry* entry = reg->metrics; entry; entry = entry->next) {
        if (entry->type == METRIC_COUNTER) {
            Counter* c = (Counter*)entry->metric;
            offset += snprintf(buf + offset, 65536 - offset,
                "# HELP %s %s\n# TYPE %s counter\n%s %.2f\n",
                c->name, c->description, c->name, c->name, c->value);
        } else if (entry->type == METRIC_GAUGE) {
            Gauge* g = (Gauge*)entry->metric;
            offset += snprintf(buf + offset, 65536 - offset,
                "# HELP %s %s\n# TYPE %s gauge\n%s %.2f\n",
                g->name, g->description, g->name, g->name, g->value);
        } else if (entry->type == METRIC_HISTOGRAM) {
            Histogram* h = (Histogram*)entry->metric;
            offset += snprintf(buf + offset, 65536 - offset,
                "# HELP %s %s\n# TYPE %s histogram\n",
                h->name, h->description, h->name);
            for (size_t i = 0; i < h->bucket_count; i++) {
                offset += snprintf(buf + offset, 65536 - offset,
                    "%s_bucket{le=\"%.0f\"} %lld\n",
                    h->name, h->buckets[i].upper_bound, (long long)h->buckets[i].count);
            }
            offset += snprintf(buf + offset, 65536 - offset,
                "%s_bucket{le=\"+Inf\"} %lld\n%s_sum %.2f\n%s_count %lld\n",
                h->name, (long long)h->count, h->name, h->sum, h->name, (long long)h->count);
        }
    }
    pthread_mutex_unlock(&reg->lock);

    SxString* result = intrinsic_string_new(buf);
    free(buf);
    return (int64_t)result;
}

// Close metrics registry
void metrics_registry_close(int64_t reg_ptr) {
    MetricsRegistry* reg = (MetricsRegistry*)reg_ptr;
    if (!reg) return;

    pthread_mutex_lock(&reg->lock);
    MetricEntry* entry = reg->metrics;
    while (entry) {
        MetricEntry* next = entry->next;
        free(entry->name);
        // Note: Individual metrics are not freed as they may still be in use
        free(entry);
        entry = next;
    }
    pthread_mutex_unlock(&reg->lock);
    pthread_mutex_destroy(&reg->lock);

    if (global_metrics_registry == reg) {
        global_metrics_registry = NULL;
    }
    free(reg);
}

// ==================== Tracing Functions ====================

// Generate a random trace/span ID (simple version)
static char* generate_trace_id(void) {
    char* id = malloc(33);
    for (int i = 0; i < 32; i++) {
        id[i] = "0123456789abcdef"[rand() % 16];
    }
    id[32] = '\0';
    return id;
}

static char* generate_span_id(void) {
    char* id = malloc(17);
    for (int i = 0; i < 16; i++) {
        id[i] = "0123456789abcdef"[rand() % 16];
    }
    id[16] = '\0';
    return id;
}

// Create a new tracer
int64_t tracer_new(int64_t service_name_ptr) {
    Tracer* t = malloc(sizeof(Tracer));
    SxString* name = (SxString*)service_name_ptr;

    t->service_name = strdup(name ? name->data : "unknown");
    t->span_count = 0;
    t->span_cap = 16;
    t->active_spans = malloc(sizeof(Span*) * t->span_cap);
    pthread_mutex_init(&t->lock, NULL);

    return (int64_t)t;
}

// Start a new span
int64_t span_start(int64_t tracer_ptr, int64_t name_ptr) {
    Tracer* t = (Tracer*)tracer_ptr;
    SxString* name = (SxString*)name_ptr;

    Span* span = malloc(sizeof(Span));
    span->trace_id = generate_trace_id();
    span->span_id = generate_span_id();
    span->parent_span_id = NULL;
    span->name = strdup(name ? name->data : "span");

    struct timeval tv;
    gettimeofday(&tv, NULL);
    span->start_time = tv.tv_sec * 1000000LL + tv.tv_usec;
    span->end_time = 0;
    span->status = SPAN_STATUS_UNSET;
    span->status_message = NULL;
    span->attributes = NULL;
    span->events = NULL;
    span->ended = 0;
    pthread_mutex_init(&span->lock, NULL);

    // Add to tracer's active spans
    if (t) {
        pthread_mutex_lock(&t->lock);
        if (t->span_count >= t->span_cap) {
            t->span_cap *= 2;
            t->active_spans = realloc(t->active_spans, sizeof(Span*) * t->span_cap);
        }
        t->active_spans[t->span_count++] = span;
        pthread_mutex_unlock(&t->lock);
    }

    return (int64_t)span;
}

// Start a child span
int64_t span_start_child(int64_t tracer_ptr, int64_t parent_ptr, int64_t name_ptr) {
    Span* parent = (Span*)parent_ptr;
    SxString* name = (SxString*)name_ptr;

    Span* span = malloc(sizeof(Span));
    span->trace_id = parent ? strdup(parent->trace_id) : generate_trace_id();
    span->span_id = generate_span_id();
    span->parent_span_id = parent ? strdup(parent->span_id) : NULL;
    span->name = strdup(name ? name->data : "span");

    struct timeval tv;
    gettimeofday(&tv, NULL);
    span->start_time = tv.tv_sec * 1000000LL + tv.tv_usec;
    span->end_time = 0;
    span->status = SPAN_STATUS_UNSET;
    span->status_message = NULL;
    span->attributes = NULL;
    span->events = NULL;
    span->ended = 0;
    pthread_mutex_init(&span->lock, NULL);

    // Add to tracer's active spans
    Tracer* t = (Tracer*)tracer_ptr;
    if (t) {
        pthread_mutex_lock(&t->lock);
        if (t->span_count >= t->span_cap) {
            t->span_cap *= 2;
            t->active_spans = realloc(t->active_spans, sizeof(Span*) * t->span_cap);
        }
        t->active_spans[t->span_count++] = span;
        pthread_mutex_unlock(&t->lock);
    }

    return (int64_t)span;
}

// End a span
void span_end(int64_t span_ptr) {
    Span* span = (Span*)span_ptr;
    if (!span || span->ended) return;

    pthread_mutex_lock(&span->lock);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    span->end_time = tv.tv_sec * 1000000LL + tv.tv_usec;
    span->ended = 1;

    if (span->status == SPAN_STATUS_UNSET) {
        span->status = SPAN_STATUS_OK;
    }
    pthread_mutex_unlock(&span->lock);
}

// Set span status
void span_set_status(int64_t span_ptr, int64_t status, int64_t message_ptr) {
    Span* span = (Span*)span_ptr;
    if (!span) return;

    SxString* msg = (SxString*)message_ptr;

    pthread_mutex_lock(&span->lock);
    span->status = (SpanStatus)status;
    if (span->status_message) free(span->status_message);
    span->status_message = msg ? strdup(msg->data) : NULL;
    pthread_mutex_unlock(&span->lock);
}

// Add attribute to span
void span_set_attribute(int64_t span_ptr, int64_t key_ptr, int64_t value_ptr) {
    Span* span = (Span*)span_ptr;
    if (!span) return;

    SxString* key = (SxString*)key_ptr;
    SxString* value = (SxString*)value_ptr;

    MetricLabel* attr = malloc(sizeof(MetricLabel));
    attr->key = strdup(key ? key->data : "");
    attr->value = strdup(value ? value->data : "");

    pthread_mutex_lock(&span->lock);
    attr->next = span->attributes;
    span->attributes = attr;
    pthread_mutex_unlock(&span->lock);
}

// Add event to span
void span_add_event(int64_t span_ptr, int64_t name_ptr) {
    Span* span = (Span*)span_ptr;
    if (!span) return;

    SxString* name = (SxString*)name_ptr;

    SpanEvent* event = malloc(sizeof(SpanEvent));
    event->name = strdup(name ? name->data : "event");

    struct timeval tv;
    gettimeofday(&tv, NULL);
    event->timestamp = tv.tv_sec * 1000000LL + tv.tv_usec;
    event->attributes = NULL;

    pthread_mutex_lock(&span->lock);
    event->next = span->events;
    span->events = event;
    pthread_mutex_unlock(&span->lock);
}

// Get span duration in microseconds
int64_t span_duration_us(int64_t span_ptr) {
    Span* span = (Span*)span_ptr;
    if (!span) return 0;

    if (span->ended) {
        return span->end_time - span->start_time;
    } else {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        int64_t now = tv.tv_sec * 1000000LL + tv.tv_usec;
        return now - span->start_time;
    }
}

// Get span trace ID
int64_t span_trace_id(int64_t span_ptr) {
    Span* span = (Span*)span_ptr;
    if (!span) return (int64_t)intrinsic_string_new("");
    return (int64_t)intrinsic_string_new(span->trace_id);
}

// Get span ID
int64_t span_id(int64_t span_ptr) {
    Span* span = (Span*)span_ptr;
    if (!span) return (int64_t)intrinsic_string_new("");
    return (int64_t)intrinsic_string_new(span->span_id);
}

// Export span as JSON
int64_t span_to_json(int64_t span_ptr) {
    Span* span = (Span*)span_ptr;
    if (!span) return (int64_t)intrinsic_string_new("{}");

    char buf[4096];
    int offset = 0;

    offset += snprintf(buf + offset, sizeof(buf) - offset,
        "{\"trace_id\":\"%s\",\"span_id\":\"%s\",\"name\":\"%s\",\"start_time\":%lld,\"end_time\":%lld,\"duration_us\":%lld,\"status\":%d",
        span->trace_id, span->span_id, span->name,
        (long long)span->start_time, (long long)span->end_time,
        (long long)(span->ended ? span->end_time - span->start_time : 0),
        span->status);

    if (span->parent_span_id) {
        offset += snprintf(buf + offset, sizeof(buf) - offset,
            ",\"parent_span_id\":\"%s\"", span->parent_span_id);
    }

    // Add attributes
    offset += snprintf(buf + offset, sizeof(buf) - offset, ",\"attributes\":{");
    int first = 1;
    for (MetricLabel* attr = span->attributes; attr && offset < sizeof(buf) - 100; attr = attr->next) {
        if (!first) offset += snprintf(buf + offset, sizeof(buf) - offset, ",");
        first = 0;
        offset += snprintf(buf + offset, sizeof(buf) - offset,
            "\"%s\":\"%s\"", attr->key, attr->value);
    }
    offset += snprintf(buf + offset, sizeof(buf) - offset, "}");

    // Add events
    offset += snprintf(buf + offset, sizeof(buf) - offset, ",\"events\":[");
    first = 1;
    for (SpanEvent* event = span->events; event && offset < sizeof(buf) - 100; event = event->next) {
        if (!first) offset += snprintf(buf + offset, sizeof(buf) - offset, ",");
        first = 0;
        offset += snprintf(buf + offset, sizeof(buf) - offset,
            "{\"name\":\"%s\",\"timestamp\":%lld}", event->name, (long long)event->timestamp);
    }
    offset += snprintf(buf + offset, sizeof(buf) - offset, "]}");

    return (int64_t)intrinsic_string_new(buf);
}

// Get active span count
int64_t tracer_active_spans(int64_t tracer_ptr) {
    Tracer* t = (Tracer*)tracer_ptr;
    if (!t) return 0;

    pthread_mutex_lock(&t->lock);
    int64_t count = 0;
    for (size_t i = 0; i < t->span_count; i++) {
        if (!t->active_spans[i]->ended) count++;
    }
    pthread_mutex_unlock(&t->lock);

    return count;
}

// Close span
void span_close(int64_t span_ptr) {
    Span* span = (Span*)span_ptr;
    if (!span) return;

    if (!span->ended) span_end(span_ptr);

    free(span->trace_id);
    free(span->span_id);
    if (span->parent_span_id) free(span->parent_span_id);
    free(span->name);
    if (span->status_message) free(span->status_message);

    // Free attributes
    MetricLabel* attr = span->attributes;
    while (attr) {
        MetricLabel* next = attr->next;
        free(attr->key);
        free(attr->value);
        free(attr);
        attr = next;
    }

    // Free events
    SpanEvent* event = span->events;
    while (event) {
        SpanEvent* next = event->next;
        free(event->name);
        free(event);
        event = next;
    }

    pthread_mutex_destroy(&span->lock);
    free(span);
}

// Close tracer
void tracer_close(int64_t tracer_ptr) {
    Tracer* t = (Tracer*)tracer_ptr;
    if (!t) return;

    free(t->service_name);
    free(t->active_spans);  // Individual spans should be closed separately
    pthread_mutex_destroy(&t->lock);
    free(t);
}

// ==================== Logging Functions ====================

// Get current timestamp string
static char* get_timestamp(void) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char* buf = malloc(32);
    strftime(buf, 32, "%Y-%m-%dT%H:%M:%S", tm_info);
    return buf;
}

// Get log level name
static const char* log_level_name(LogLevel level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO: return "INFO";
        case LOG_WARN: return "WARN";
        case LOG_ERROR: return "ERROR";
        case LOG_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

// Create a new logger
int64_t logger_new(int64_t name_ptr) {
    Logger* l = malloc(sizeof(Logger));
    SxString* name = (SxString*)name_ptr;

    l->name = strdup(name ? name->data : "app");
    l->min_level = LOG_INFO;
    l->console_output = 1;
    l->json_output = 0;
    l->file_output = NULL;
    l->context = NULL;
    pthread_mutex_init(&l->lock, NULL);

    return (int64_t)l;
}

// Get or create global logger
int64_t logger_global(void) {
    if (!global_logger) {
        global_logger = (Logger*)logger_new((int64_t)intrinsic_string_new("global"));
    }
    return (int64_t)global_logger;
}

// Set minimum log level
void logger_set_level(int64_t logger_ptr, int64_t level) {
    Logger* l = (Logger*)logger_ptr;
    if (!l) return;
    l->min_level = (LogLevel)level;
}

// Enable/disable console output
void logger_set_console(int64_t logger_ptr, int64_t enabled) {
    Logger* l = (Logger*)logger_ptr;
    if (!l) return;
    l->console_output = enabled ? 1 : 0;
}

// Enable/disable JSON output
void logger_set_json(int64_t logger_ptr, int64_t enabled) {
    Logger* l = (Logger*)logger_ptr;
    if (!l) return;
    l->json_output = enabled ? 1 : 0;
}

// Set file output
void logger_set_file(int64_t logger_ptr, int64_t path_ptr) {
    Logger* l = (Logger*)logger_ptr;
    if (!l) return;

    SxString* path = (SxString*)path_ptr;
    if (l->file_output) {
        fclose(l->file_output);
    }
    l->file_output = path ? fopen(path->data, "a") : NULL;
}

// Add context field to logger
void logger_add_context(int64_t logger_ptr, int64_t key_ptr, int64_t value_ptr) {
    Logger* l = (Logger*)logger_ptr;
    if (!l) return;

    SxString* key = (SxString*)key_ptr;
    SxString* value = (SxString*)value_ptr;

    LogField* field = malloc(sizeof(LogField));
    field->key = strdup(key ? key->data : "");
    field->value = strdup(value ? value->data : "");

    pthread_mutex_lock(&l->lock);
    field->next = l->context;
    l->context = field;
    pthread_mutex_unlock(&l->lock);
}

// Internal log function
static void log_message(Logger* l, LogLevel level, const char* message, LogField* extra_fields) {
    if (!l || level < l->min_level) return;

    pthread_mutex_lock(&l->lock);

    char* timestamp = get_timestamp();

    if (l->json_output) {
        // JSON format
        char buf[4096];
        int offset = 0;

        offset += snprintf(buf + offset, sizeof(buf) - offset,
            "{\"timestamp\":\"%s\",\"level\":\"%s\",\"logger\":\"%s\",\"message\":\"%s\"",
            timestamp, log_level_name(level), l->name, message);

        // Add context fields
        for (LogField* f = l->context; f && offset < sizeof(buf) - 100; f = f->next) {
            offset += snprintf(buf + offset, sizeof(buf) - offset,
                ",\"%s\":\"%s\"", f->key, f->value);
        }

        // Add extra fields
        for (LogField* f = extra_fields; f && offset < sizeof(buf) - 100; f = f->next) {
            offset += snprintf(buf + offset, sizeof(buf) - offset,
                ",\"%s\":\"%s\"", f->key, f->value);
        }

        offset += snprintf(buf + offset, sizeof(buf) - offset, "}");

        if (l->console_output) {
            printf("%s\n", buf);
        }
        if (l->file_output) {
            fprintf(l->file_output, "%s\n", buf);
            fflush(l->file_output);
        }
    } else {
        // Text format
        char buf[4096];
        int offset = 0;

        offset += snprintf(buf + offset, sizeof(buf) - offset,
            "%s [%s] %s: %s", timestamp, log_level_name(level), l->name, message);

        // Add extra fields
        for (LogField* f = extra_fields; f && offset < sizeof(buf) - 100; f = f->next) {
            offset += snprintf(buf + offset, sizeof(buf) - offset,
                " %s=%s", f->key, f->value);
        }

        if (l->console_output) {
            printf("%s\n", buf);
        }
        if (l->file_output) {
            fprintf(l->file_output, "%s\n", buf);
            fflush(l->file_output);
        }
    }

    free(timestamp);
    pthread_mutex_unlock(&l->lock);
}

// Log at debug level
void log_debug(int64_t logger_ptr, int64_t message_ptr) {
    Logger* l = (Logger*)logger_ptr;
    SxString* msg = (SxString*)message_ptr;
    log_message(l, LOG_DEBUG, msg ? msg->data : "", NULL);
}

// Log at info level
void log_info(int64_t logger_ptr, int64_t message_ptr) {
    Logger* l = (Logger*)logger_ptr;
    SxString* msg = (SxString*)message_ptr;
    log_message(l, LOG_INFO, msg ? msg->data : "", NULL);
}

// Log at warn level
void log_warn(int64_t logger_ptr, int64_t message_ptr) {
    Logger* l = (Logger*)logger_ptr;
    SxString* msg = (SxString*)message_ptr;
    log_message(l, LOG_WARN, msg ? msg->data : "", NULL);
}

// Log at error level
void log_error(int64_t logger_ptr, int64_t message_ptr) {
    Logger* l = (Logger*)logger_ptr;
    SxString* msg = (SxString*)message_ptr;
    log_message(l, LOG_ERROR, msg ? msg->data : "", NULL);
}

// Log at fatal level
void log_fatal(int64_t logger_ptr, int64_t message_ptr) {
    Logger* l = (Logger*)logger_ptr;
    SxString* msg = (SxString*)message_ptr;
    log_message(l, LOG_FATAL, msg ? msg->data : "", NULL);
}

// Log with extra field
void log_with_field(int64_t logger_ptr, int64_t level, int64_t message_ptr, int64_t key_ptr, int64_t value_ptr) {
    Logger* l = (Logger*)logger_ptr;
    SxString* msg = (SxString*)message_ptr;
    SxString* key = (SxString*)key_ptr;
    SxString* value = (SxString*)value_ptr;

    LogField field;
    field.key = key ? key->data : "";
    field.value = value ? value->data : "";
    field.next = NULL;

    log_message(l, (LogLevel)level, msg ? msg->data : "", &field);
}

// Log with span context
void log_with_span(int64_t logger_ptr, int64_t level, int64_t message_ptr, int64_t span_ptr) {
    Logger* l = (Logger*)logger_ptr;
    SxString* msg = (SxString*)message_ptr;
    Span* span = (Span*)span_ptr;

    if (!span) {
        log_message(l, (LogLevel)level, msg ? msg->data : "", NULL);
        return;
    }

    LogField trace_field;
    trace_field.key = "trace_id";
    trace_field.value = span->trace_id;

    LogField span_field;
    span_field.key = "span_id";
    span_field.value = span->span_id;
    span_field.next = NULL;

    trace_field.next = &span_field;

    log_message(l, (LogLevel)level, msg ? msg->data : "", &trace_field);
}

// Close logger
void logger_close(int64_t logger_ptr) {
    Logger* l = (Logger*)logger_ptr;
    if (!l) return;

    if (l->file_output) {
        fclose(l->file_output);
    }

    // Free context fields
    LogField* field = l->context;
    while (field) {
        LogField* next = field->next;
        free(field->key);
        free(field->value);
        free(field);
        field = next;
    }

    free(l->name);
    pthread_mutex_destroy(&l->lock);

    if (global_logger == l) {
        global_logger = NULL;
    }
    free(l);
}

// ==================== Convenience Functions ====================

// Quick timer for measuring execution time
typedef struct {
    int64_t start_time;
    char* name;
} Timer;

int64_t timer_start(int64_t name_ptr) {
    Timer* t = malloc(sizeof(Timer));
    SxString* name = (SxString*)name_ptr;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    t->start_time = tv.tv_sec * 1000000LL + tv.tv_usec;
    t->name = strdup(name ? name->data : "timer");

    return (int64_t)t;
}

int64_t timer_elapsed_us(int64_t timer_ptr) {
    Timer* t = (Timer*)timer_ptr;
    if (!t) return 0;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t now = tv.tv_sec * 1000000LL + tv.tv_usec;
    return now - t->start_time;
}

int64_t timer_elapsed_ms(int64_t timer_ptr) {
    return timer_elapsed_us(timer_ptr) / 1000;
}

double timer_elapsed_s(int64_t timer_ptr) {
    return timer_elapsed_us(timer_ptr) / 1000000.0;
}

void timer_close(int64_t timer_ptr) {
    Timer* t = (Timer*)timer_ptr;
    if (!t) return;
    free(t->name);
    free(t);
}

// Record timer value to histogram
void timer_record_to(int64_t timer_ptr, int64_t histogram_ptr) {
    Timer* t = (Timer*)timer_ptr;
    if (!t) return;

    double elapsed_ms = timer_elapsed_us(timer_ptr) / 1000.0;
    histogram_observe(histogram_ptr, elapsed_ms);
}

// ============================================================================
// TOML Parser
// ============================================================================

// Forward declarations
void toml_free(int64_t table_ptr);

// TOML Value types
typedef enum {
    TOML_STRING = 0,
    TOML_INTEGER = 1,
    TOML_FLOAT = 2,
    TOML_BOOL = 3,
    TOML_ARRAY = 4,
    TOML_TABLE = 5
} TomlType;

// Forward declarations
typedef struct TomlValue TomlValue;
typedef struct TomlEntry TomlEntry;

// TOML table entry (key-value pair in linked list)
struct TomlEntry {
    char* key;
    TomlValue* value;
    TomlEntry* next;
};

// TOML value
struct TomlValue {
    TomlType type;
    union {
        char* string_val;
        int64_t int_val;
        double float_val;
        int bool_val;
        struct {
            TomlValue** items;
            int count;
            int capacity;
        } array;
        TomlEntry* table;  // linked list of entries
    } data;
};

// Create new TOML table
int64_t toml_table_new(void) {
    TomlValue* v = malloc(sizeof(TomlValue));
    v->type = TOML_TABLE;
    v->data.table = NULL;
    return (int64_t)v;
}

// Create TOML string value
TomlValue* toml_value_string(const char* s) {
    TomlValue* v = malloc(sizeof(TomlValue));
    v->type = TOML_STRING;
    v->data.string_val = strdup(s);
    return v;
}

// Create TOML integer value
TomlValue* toml_value_int(int64_t n) {
    TomlValue* v = malloc(sizeof(TomlValue));
    v->type = TOML_INTEGER;
    v->data.int_val = n;
    return v;
}

// Create TOML float value
TomlValue* toml_value_float(double n) {
    TomlValue* v = malloc(sizeof(TomlValue));
    v->type = TOML_FLOAT;
    v->data.float_val = n;
    return v;
}

// Create TOML bool value
TomlValue* toml_value_bool(int b) {
    TomlValue* v = malloc(sizeof(TomlValue));
    v->type = TOML_BOOL;
    v->data.bool_val = b;
    return v;
}

// Create TOML array value
TomlValue* toml_value_array(void) {
    TomlValue* v = malloc(sizeof(TomlValue));
    v->type = TOML_ARRAY;
    v->data.array.items = malloc(sizeof(TomlValue*) * 8);
    v->data.array.count = 0;
    v->data.array.capacity = 8;
    return v;
}

// Add item to TOML array
void toml_array_push(TomlValue* arr, TomlValue* item) {
    if (!arr || arr->type != TOML_ARRAY) return;
    if (arr->data.array.count >= arr->data.array.capacity) {
        arr->data.array.capacity *= 2;
        arr->data.array.items = realloc(arr->data.array.items,
            sizeof(TomlValue*) * arr->data.array.capacity);
    }
    arr->data.array.items[arr->data.array.count++] = item;
}

// Set value in table
void toml_table_set_internal(TomlValue* table, const char* key, TomlValue* value) {
    if (!table || table->type != TOML_TABLE) return;

    // Check if key exists
    TomlEntry* entry = table->data.table;
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            // Replace value (should free old value)
            entry->value = value;
            return;
        }
        entry = entry->next;
    }

    // Add new entry
    TomlEntry* new_entry = malloc(sizeof(TomlEntry));
    new_entry->key = strdup(key);
    new_entry->value = value;
    new_entry->next = table->data.table;
    table->data.table = new_entry;
}

// Get value from table by key
TomlValue* toml_table_get_internal(TomlValue* table, const char* key) {
    if (!table || table->type != TOML_TABLE) return NULL;

    TomlEntry* entry = table->data.table;
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

// Get or create nested table by dotted path
TomlValue* toml_table_get_or_create(TomlValue* root, const char* path) {
    if (!root || root->type != TOML_TABLE) return NULL;

    char* path_copy = strdup(path);
    char* token = strtok(path_copy, ".");
    TomlValue* current = root;

    while (token) {
        TomlValue* next = toml_table_get_internal(current, token);
        if (!next) {
            // Create new table
            next = malloc(sizeof(TomlValue));
            next->type = TOML_TABLE;
            next->data.table = NULL;
            toml_table_set_internal(current, token, next);
        }
        current = next;
        token = strtok(NULL, ".");
    }

    free(path_copy);
    return current;
}

// Skip whitespace
static const char* skip_ws(const char* s) {
    while (*s && (*s == ' ' || *s == '\t')) s++;
    return s;
}

// Skip to end of line
static const char* skip_line(const char* s) {
    while (*s && *s != '\n') s++;
    if (*s == '\n') s++;
    return s;
}

// Parse basic string (double quoted)
static const char* parse_basic_string(const char* s, char** out) {
    if (*s != '"') return NULL;
    s++;

    char buffer[4096];
    int i = 0;

    while (*s && *s != '"' && i < 4095) {
        if (*s == '\\' && *(s+1)) {
            s++;
            switch (*s) {
                case 'n': buffer[i++] = '\n'; break;
                case 't': buffer[i++] = '\t'; break;
                case 'r': buffer[i++] = '\r'; break;
                case '\\': buffer[i++] = '\\'; break;
                case '"': buffer[i++] = '"'; break;
                default: buffer[i++] = *s;
            }
        } else {
            buffer[i++] = *s;
        }
        s++;
    }
    buffer[i] = '\0';

    if (*s == '"') s++;
    *out = strdup(buffer);
    return s;
}

// Parse literal string (single quoted)
static const char* parse_literal_string(const char* s, char** out) {
    if (*s != '\'') return NULL;
    s++;

    char buffer[4096];
    int i = 0;

    while (*s && *s != '\'' && i < 4095) {
        buffer[i++] = *s++;
    }
    buffer[i] = '\0';

    if (*s == '\'') s++;
    *out = strdup(buffer);
    return s;
}

// Parse integer
static const char* parse_integer(const char* s, int64_t* out) {
    char* end;
    *out = strtoll(s, &end, 10);
    return end;
}

// Parse float
static const char* parse_float(const char* s, double* out) {
    char* end;
    *out = strtod(s, &end);
    return end;
}

// Forward declaration
static const char* parse_value(const char* s, TomlValue** out);

// Parse array
static const char* parse_array(const char* s, TomlValue** out) {
    if (*s != '[') return NULL;
    s++;

    TomlValue* arr = toml_value_array();

    while (*s) {
        s = skip_ws(s);
        while (*s == '\n') s = skip_ws(s + 1);

        if (*s == ']') {
            s++;
            *out = arr;
            return s;
        }

        if (*s == ',') {
            s++;
            continue;
        }

        // Skip comments in arrays
        if (*s == '#') {
            s = skip_line(s);
            continue;
        }

        TomlValue* item = NULL;
        s = parse_value(s, &item);
        if (item) {
            toml_array_push(arr, item);
        }
    }

    *out = arr;
    return s;
}

// Parse inline table
static const char* parse_inline_table(const char* s, TomlValue** out) {
    if (*s != '{') return NULL;
    s++;

    TomlValue* table = malloc(sizeof(TomlValue));
    table->type = TOML_TABLE;
    table->data.table = NULL;

    while (*s) {
        s = skip_ws(s);

        if (*s == '}') {
            s++;
            *out = table;
            return s;
        }

        if (*s == ',') {
            s++;
            continue;
        }

        // Parse key
        char key[256];
        int ki = 0;
        while (*s && *s != '=' && *s != ' ' && *s != '\t' && ki < 255) {
            key[ki++] = *s++;
        }
        key[ki] = '\0';

        s = skip_ws(s);
        if (*s != '=') break;
        s++;
        s = skip_ws(s);

        // Parse value
        TomlValue* val = NULL;
        s = parse_value(s, &val);
        if (val) {
            toml_table_set_internal(table, key, val);
        }
    }

    *out = table;
    return s;
}

// Parse a value
static const char* parse_value(const char* s, TomlValue** out) {
    s = skip_ws(s);

    if (*s == '"') {
        char* str = NULL;
        s = parse_basic_string(s, &str);
        *out = toml_value_string(str);
        free(str);
        return s;
    }

    if (*s == '\'') {
        char* str = NULL;
        s = parse_literal_string(s, &str);
        *out = toml_value_string(str);
        free(str);
        return s;
    }

    if (*s == '[') {
        return parse_array(s, out);
    }

    if (*s == '{') {
        return parse_inline_table(s, out);
    }

    // true/false
    if (strncmp(s, "true", 4) == 0 && !isalnum(s[4])) {
        *out = toml_value_bool(1);
        return s + 4;
    }
    if (strncmp(s, "false", 5) == 0 && !isalnum(s[5])) {
        *out = toml_value_bool(0);
        return s + 5;
    }

    // Number (integer or float)
    if (*s == '-' || *s == '+' || isdigit(*s)) {
        const char* start = s;
        if (*s == '-' || *s == '+') s++;
        while (isdigit(*s)) s++;

        if (*s == '.' || *s == 'e' || *s == 'E') {
            // Float
            double val;
            parse_float(start, &val);
            *out = toml_value_float(val);
            // Skip rest of number
            if (*s == '.') {
                s++;
                while (isdigit(*s)) s++;
            }
            if (*s == 'e' || *s == 'E') {
                s++;
                if (*s == '-' || *s == '+') s++;
                while (isdigit(*s)) s++;
            }
        } else {
            // Integer
            int64_t val;
            parse_integer(start, &val);
            *out = toml_value_int(val);
        }
        return s;
    }

    return s;
}

// Parse TOML content
int64_t toml_parse(int64_t content_ptr) {
    SxString* content = (SxString*)content_ptr;
    if (!content || !content->data) return 0;

    TomlValue* root = malloc(sizeof(TomlValue));
    root->type = TOML_TABLE;
    root->data.table = NULL;

    TomlValue* current_table = root;
    const char* s = content->data;

    while (*s) {
        s = skip_ws(s);

        // Skip empty lines and comments
        if (*s == '\n') {
            s++;
            continue;
        }
        if (*s == '#') {
            s = skip_line(s);
            continue;
        }

        // Table header [table] or [[array of tables]]
        if (*s == '[') {
            s++;
            int is_array_table = 0;
            if (*s == '[') {
                s++;
                is_array_table = 1;
            }

            // Parse table name
            char table_name[256];
            int ti = 0;
            while (*s && *s != ']' && *s != '\n' && ti < 255) {
                table_name[ti++] = *s++;
            }
            table_name[ti] = '\0';

            // Must have closing bracket - if not, it's invalid TOML
            if (*s != ']') {
                toml_free((int64_t)root);
                return 0;
            }
            s++;
            if (is_array_table) {
                if (*s != ']') {
                    toml_free((int64_t)root);
                    return 0;
                }
                s++;
            }

            if (is_array_table) {
                // Array of tables - create or get array, add new table
                TomlValue* arr = toml_table_get_internal(root, table_name);
                if (!arr) {
                    arr = toml_value_array();
                    toml_table_set_internal(root, table_name, arr);
                }
                TomlValue* new_table = malloc(sizeof(TomlValue));
                new_table->type = TOML_TABLE;
                new_table->data.table = NULL;
                toml_array_push(arr, new_table);
                current_table = new_table;
            } else {
                // Regular table
                current_table = toml_table_get_or_create(root, table_name);
            }

            s = skip_line(s);
            continue;
        }

        // Key = value
        char key[256];
        int ki = 0;

        // Handle quoted keys
        if (*s == '"') {
            s++;
            while (*s && *s != '"' && ki < 255) {
                key[ki++] = *s++;
            }
            if (*s == '"') s++;
        } else {
            while (*s && *s != '=' && *s != ' ' && *s != '\t' && ki < 255) {
                key[ki++] = *s++;
            }
        }
        key[ki] = '\0';

        if (ki == 0) {
            s = skip_line(s);
            continue;
        }

        s = skip_ws(s);
        if (*s != '=') {
            s = skip_line(s);
            continue;
        }
        s++;
        s = skip_ws(s);

        // Parse value
        TomlValue* value = NULL;
        s = parse_value(s, &value);

        if (value) {
            // Handle dotted keys (e.g., foo.bar = value)
            char* dot = strchr(key, '.');
            if (dot) {
                *dot = '\0';
                TomlValue* nested = toml_table_get_or_create(current_table, key);
                toml_table_set_internal(nested, dot + 1, value);
            } else {
                toml_table_set_internal(current_table, key, value);
            }
        }

        s = skip_line(s);
    }

    return (int64_t)root;
}

// Get string value by path (e.g., "package.name")
int64_t toml_get_string(int64_t table_ptr, int64_t path_ptr) {
    TomlValue* table = (TomlValue*)table_ptr;
    SxString* path = (SxString*)path_ptr;
    if (!table || !path || table->type != TOML_TABLE) return 0;

    char* path_copy = strdup(path->data);
    char* token = strtok(path_copy, ".");
    TomlValue* current = table;

    while (token) {
        char* next_token = strtok(NULL, ".");

        if (current->type == TOML_TABLE) {
            TomlValue* val = toml_table_get_internal(current, token);
            if (!val) {
                free(path_copy);
                return 0;
            }
            if (!next_token) {
                // Last token - should be string
                free(path_copy);
                if (val->type == TOML_STRING) {
                    return (int64_t)intrinsic_string_new(val->data.string_val);
                }
                return 0;
            }
            current = val;
        } else {
            free(path_copy);
            return 0;
        }
        token = next_token;
    }

    free(path_copy);
    return 0;
}

// Get integer value by path
int64_t toml_get_i64(int64_t table_ptr, int64_t path_ptr) {
    TomlValue* table = (TomlValue*)table_ptr;
    SxString* path = (SxString*)path_ptr;
    if (!table || !path || table->type != TOML_TABLE) return 0;

    char* path_copy = strdup(path->data);
    char* token = strtok(path_copy, ".");
    TomlValue* current = table;

    while (token) {
        char* next_token = strtok(NULL, ".");

        if (current->type == TOML_TABLE) {
            TomlValue* val = toml_table_get_internal(current, token);
            if (!val) {
                free(path_copy);
                return 0;
            }
            if (!next_token) {
                free(path_copy);
                if (val->type == TOML_INTEGER) {
                    return val->data.int_val;
                }
                return 0;
            }
            current = val;
        } else {
            free(path_copy);
            return 0;
        }
        token = next_token;
    }

    free(path_copy);
    return 0;
}

// Get boolean value by path
int64_t toml_get_bool(int64_t table_ptr, int64_t path_ptr) {
    TomlValue* table = (TomlValue*)table_ptr;
    SxString* path = (SxString*)path_ptr;
    if (!table || !path || table->type != TOML_TABLE) return 0;

    char* path_copy = strdup(path->data);
    char* token = strtok(path_copy, ".");
    TomlValue* current = table;

    while (token) {
        char* next_token = strtok(NULL, ".");

        if (current->type == TOML_TABLE) {
            TomlValue* val = toml_table_get_internal(current, token);
            if (!val) {
                free(path_copy);
                return 0;
            }
            if (!next_token) {
                free(path_copy);
                if (val->type == TOML_BOOL) {
                    return val->data.bool_val;
                }
                return 0;
            }
            current = val;
        } else {
            free(path_copy);
            return 0;
        }
        token = next_token;
    }

    free(path_copy);
    return 0;
}

// Get float value by path
double toml_get_f64(int64_t table_ptr, int64_t path_ptr) {
    TomlValue* table = (TomlValue*)table_ptr;
    SxString* path = (SxString*)path_ptr;
    if (!table || !path || table->type != TOML_TABLE) return 0.0;

    char* path_copy = strdup(path->data);
    char* token = strtok(path_copy, ".");
    TomlValue* current = table;

    while (token) {
        char* next_token = strtok(NULL, ".");

        if (current->type == TOML_TABLE) {
            TomlValue* val = toml_table_get_internal(current, token);
            if (!val) {
                free(path_copy);
                return 0.0;
            }
            if (!next_token) {
                free(path_copy);
                if (val->type == TOML_FLOAT) {
                    return val->data.float_val;
                }
                if (val->type == TOML_INTEGER) {
                    return (double)val->data.int_val;
                }
                return 0.0;
            }
            current = val;
        } else {
            free(path_copy);
            return 0.0;
        }
        token = next_token;
    }

    free(path_copy);
    return 0.0;
}

// Check if key exists
int64_t toml_has_key(int64_t table_ptr, int64_t path_ptr) {
    TomlValue* table = (TomlValue*)table_ptr;
    SxString* path = (SxString*)path_ptr;
    if (!table || !path || table->type != TOML_TABLE) return 0;

    char* path_copy = strdup(path->data);
    char* token = strtok(path_copy, ".");
    TomlValue* current = table;

    while (token) {
        char* next_token = strtok(NULL, ".");

        if (current->type == TOML_TABLE) {
            TomlValue* val = toml_table_get_internal(current, token);
            if (!val) {
                free(path_copy);
                return 0;
            }
            if (!next_token) {
                free(path_copy);
                return 1;
            }
            current = val;
        } else {
            free(path_copy);
            return 0;
        }
        token = next_token;
    }

    free(path_copy);
    return 0;
}

// Get array by path
int64_t toml_get_array(int64_t table_ptr, int64_t path_ptr) {
    TomlValue* table = (TomlValue*)table_ptr;
    SxString* path = (SxString*)path_ptr;
    if (!table || !path || table->type != TOML_TABLE) return 0;

    char* path_copy = strdup(path->data);
    char* token = strtok(path_copy, ".");
    TomlValue* current = table;

    while (token) {
        char* next_token = strtok(NULL, ".");

        if (current->type == TOML_TABLE) {
            TomlValue* val = toml_table_get_internal(current, token);
            if (!val) {
                free(path_copy);
                return 0;
            }
            if (!next_token) {
                free(path_copy);
                if (val->type == TOML_ARRAY) {
                    // Convert to Vec of strings (simplified)
                    SxVec* vec = intrinsic_vec_new();
                    for (int i = 0; i < val->data.array.count; i++) {
                        TomlValue* item = val->data.array.items[i];
                        if (item->type == TOML_STRING) {
                            intrinsic_vec_push(vec, (void*)intrinsic_string_new(item->data.string_val));
                        } else if (item->type == TOML_INTEGER) {
                            char buf[32];
                            snprintf(buf, 32, "%lld", (long long)item->data.int_val);
                            intrinsic_vec_push(vec, (void*)intrinsic_string_new(buf));
                        }
                    }
                    return (int64_t)vec;
                }
                return 0;
            }
            current = val;
        } else {
            free(path_copy);
            return 0;
        }
        token = next_token;
    }

    free(path_copy);
    return 0;
}

// Get nested table by path
int64_t toml_get_table(int64_t table_ptr, int64_t path_ptr) {
    TomlValue* table = (TomlValue*)table_ptr;
    SxString* path = (SxString*)path_ptr;
    if (!table || !path || table->type != TOML_TABLE) return 0;

    char* path_copy = strdup(path->data);
    char* token = strtok(path_copy, ".");
    TomlValue* current = table;

    while (token) {
        if (current->type == TOML_TABLE) {
            TomlValue* val = toml_table_get_internal(current, token);
            if (!val) {
                free(path_copy);
                return 0;
            }
            current = val;
        } else {
            free(path_copy);
            return 0;
        }
        token = strtok(NULL, ".");
    }

    free(path_copy);
    if (current->type == TOML_TABLE) {
        return (int64_t)current;
    }
    return 0;
}

// Get list of keys in table
int64_t toml_keys(int64_t table_ptr) {
    TomlValue* table = (TomlValue*)table_ptr;
    if (!table || table->type != TOML_TABLE) return (int64_t)intrinsic_vec_new();

    SxVec* vec = intrinsic_vec_new();
    TomlEntry* entry = table->data.table;
    while (entry) {
        intrinsic_vec_push(vec, (void*)intrinsic_string_new(entry->key));
        entry = entry->next;
    }
    return (int64_t)vec;
}

// Set string value in table
void toml_set_string(int64_t table_ptr, int64_t key_ptr, int64_t value_ptr) {
    TomlValue* table = (TomlValue*)table_ptr;
    SxString* key = (SxString*)key_ptr;
    SxString* value = (SxString*)value_ptr;
    if (!table || !key || !value || table->type != TOML_TABLE) return;

    toml_table_set_internal(table, key->data, toml_value_string(value->data));
}

// Set integer value in table
void toml_set_i64(int64_t table_ptr, int64_t key_ptr, int64_t value) {
    TomlValue* table = (TomlValue*)table_ptr;
    SxString* key = (SxString*)key_ptr;
    if (!table || !key || table->type != TOML_TABLE) return;

    toml_table_set_internal(table, key->data, toml_value_int(value));
}

// Set boolean value in table
void toml_set_bool(int64_t table_ptr, int64_t key_ptr, int64_t value) {
    TomlValue* table = (TomlValue*)table_ptr;
    SxString* key = (SxString*)key_ptr;
    if (!table || !key || table->type != TOML_TABLE) return;

    toml_table_set_internal(table, key->data, toml_value_bool(value ? 1 : 0));
}

// Helper for stringification
static void toml_stringify_value(TomlValue* val, char* buf, int* pos, int buf_size, int indent);

static void toml_stringify_table_contents(TomlValue* table, char* buf, int* pos, int buf_size) {
    TomlEntry* entry = table->data.table;
    while (entry) {
        // Skip nested tables (handled separately)
        if (entry->value->type == TOML_TABLE) {
            entry = entry->next;
            continue;
        }

        *pos += snprintf(buf + *pos, buf_size - *pos, "%s = ", entry->key);
        toml_stringify_value(entry->value, buf, pos, buf_size, 0);
        *pos += snprintf(buf + *pos, buf_size - *pos, "\n");
        entry = entry->next;
    }
}

static void toml_stringify_value(TomlValue* val, char* buf, int* pos, int buf_size, int indent) {
    if (!val) return;

    switch (val->type) {
        case TOML_STRING:
            *pos += snprintf(buf + *pos, buf_size - *pos, "\"%s\"", val->data.string_val);
            break;
        case TOML_INTEGER:
            *pos += snprintf(buf + *pos, buf_size - *pos, "%lld", (long long)val->data.int_val);
            break;
        case TOML_FLOAT:
            *pos += snprintf(buf + *pos, buf_size - *pos, "%g", val->data.float_val);
            break;
        case TOML_BOOL:
            *pos += snprintf(buf + *pos, buf_size - *pos, "%s", val->data.bool_val ? "true" : "false");
            break;
        case TOML_ARRAY:
            *pos += snprintf(buf + *pos, buf_size - *pos, "[");
            for (int i = 0; i < val->data.array.count; i++) {
                if (i > 0) *pos += snprintf(buf + *pos, buf_size - *pos, ", ");
                toml_stringify_value(val->data.array.items[i], buf, pos, buf_size, indent);
            }
            *pos += snprintf(buf + *pos, buf_size - *pos, "]");
            break;
        case TOML_TABLE:
            *pos += snprintf(buf + *pos, buf_size - *pos, "{ ");
            TomlEntry* entry = val->data.table;
            int first = 1;
            while (entry) {
                if (!first) *pos += snprintf(buf + *pos, buf_size - *pos, ", ");
                *pos += snprintf(buf + *pos, buf_size - *pos, "%s = ", entry->key);
                toml_stringify_value(entry->value, buf, pos, buf_size, indent);
                first = 0;
                entry = entry->next;
            }
            *pos += snprintf(buf + *pos, buf_size - *pos, " }");
            break;
    }
}

// Serialize TOML table to string
int64_t toml_stringify(int64_t table_ptr) {
    TomlValue* table = (TomlValue*)table_ptr;
    if (!table || table->type != TOML_TABLE) {
        return (int64_t)intrinsic_string_new("");
    }

    char* buf = malloc(65536);
    int pos = 0;

    // First, output top-level key-values (non-tables)
    toml_stringify_table_contents(table, buf, &pos, 65536);

    // Then output nested tables
    TomlEntry* entry = table->data.table;
    while (entry) {
        if (entry->value->type == TOML_TABLE) {
            pos += snprintf(buf + pos, 65536 - pos, "\n[%s]\n", entry->key);
            toml_stringify_table_contents(entry->value, buf, &pos, 65536);
        }
        entry = entry->next;
    }

    SxString* result = intrinsic_string_new(buf);
    free(buf);
    return (int64_t)result;
}

// Free TOML value
void toml_free(int64_t table_ptr) {
    TomlValue* val = (TomlValue*)table_ptr;
    if (!val) return;

    switch (val->type) {
        case TOML_STRING:
            free(val->data.string_val);
            break;
        case TOML_ARRAY:
            for (int i = 0; i < val->data.array.count; i++) {
                toml_free((int64_t)val->data.array.items[i]);
            }
            free(val->data.array.items);
            break;
        case TOML_TABLE: {
            TomlEntry* entry = val->data.table;
            while (entry) {
                TomlEntry* next = entry->next;
                free(entry->key);
                toml_free((int64_t)entry->value);
                free(entry);
                entry = next;
            }
            break;
        }
        default:
            break;
    }
    free(val);
}
