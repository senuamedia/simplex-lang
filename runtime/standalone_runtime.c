// Simplex Runtime Library
// Standalone minimal runtime for Simplex programs
//
// Copyright (c) 2025-2026 Rod Higgins
// Licensed under MIT License - see LICENSE file
// https://github.com/senuamedia/simplex-lang

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>
#include <execinfo.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <math.h>

// ========================================
// Safe Memory Allocation Helpers
// ========================================

// Abort on OOM with diagnostic - use for allocations that cannot fail gracefully
static void* sx_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr && size > 0) {
        fprintf(stderr, "FATAL: Out of memory (malloc %zu bytes)\n", size);
        abort();
    }
    return ptr;
}

static void* sx_calloc(size_t count, size_t size) {
    void* ptr = calloc(count, size);
    if (!ptr && count > 0 && size > 0) {
        fprintf(stderr, "FATAL: Out of memory (calloc %zu x %zu bytes)\n", count, size);
        abort();
    }
    return ptr;
}

// Safe realloc - returns NULL on failure without losing original pointer
// Caller must handle NULL return
static void* sx_realloc_safe(void* ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    void* new_ptr = realloc(ptr, size);
    // Note: if new_ptr is NULL, original ptr is still valid
    return new_ptr;
}

// Realloc that aborts on OOM - use when failure cannot be handled
static void* sx_realloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        fprintf(stderr, "FATAL: Out of memory (realloc %zu bytes)\n", size);
        abort();
    }
    return new_ptr;
}

static char* sx_strdup(const char* s) {
    if (!s) return NULL;
    char* dup = strdup(s);
    if (!dup) {
        fprintf(stderr, "FATAL: Out of memory (strdup)\n");
        abort();
    }
    return dup;
}

// ========================================
// Thread-Safe Random Number Generator
// ========================================

// Thread-local xorshift32 PRNG - much faster than mutex-protected rand()
static __thread uint32_t tls_rand_state = 0;

static uint32_t sx_rand(void) {
    if (tls_rand_state == 0) {
        // Initialize with time and thread-specific value
        tls_rand_state = (uint32_t)time(NULL) ^ (uint32_t)(uintptr_t)&tls_rand_state;
        if (tls_rand_state == 0) tls_rand_state = 1;  // Avoid zero state
    }
    // xorshift32 algorithm
    tls_rand_state ^= tls_rand_state << 13;
    tls_rand_state ^= tls_rand_state >> 17;
    tls_rand_state ^= tls_rand_state << 5;
    return tls_rand_state;
}

// Thread-safe random double in [0, 1)
static double sx_rand_double(void) {
    return (double)sx_rand() / (double)UINT32_MAX;
}

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

int64_t intrinsic_vec_len(SxVec* vec) {
    return vec ? vec->len : 0;
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

// AI intrinsics (real implementation with fallback)
SxString* intrinsic_ai_infer(SxString* model, SxString* prompt, int64_t temperature) {
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

    uint8_t* bytes = (uint8_t*)data;
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

    return intrinsic_string_new(dot);
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
void http_server_route(int64_t server_ptr, int64_t method_ptr, int64_t path_ptr, int64_t handler_fn) {
    HttpServer* server = (HttpServer*)server_ptr;
    SxString* method = (SxString*)method_ptr;
    SxString* path = (SxString*)path_ptr;
    
    if (!server || !method || !path || !method->data || !path->data) return;
    
    HttpRoute* route = malloc(sizeof(HttpRoute));
    route->method = strdup(method->data);
    route->path = strdup(path->data);
    route->handler_fn = handler_fn;
    route->next = server->routes;
    server->routes = route;
}

// Create server response
int64_t http_server_response_new(void) {
    HttpServerResponse* resp = calloc(1, sizeof(HttpServerResponse));
    resp->status_code = 200;
    resp->status_text = strdup("OK");
    return (int64_t)resp;
}

// Set response status
void http_server_response_status(int64_t resp_ptr, int64_t code, int64_t text_ptr) {
    HttpServerResponse* resp = (HttpServerResponse*)resp_ptr;
    SxString* text = (SxString*)text_ptr;
    
    if (!resp) return;
    
    resp->status_code = (int)code;
    if (resp->status_text) free(resp->status_text);
    resp->status_text = text && text->data ? strdup(text->data) : strdup("OK");
}

// Add response header
void http_server_response_header(int64_t resp_ptr, int64_t name_ptr, int64_t value_ptr) {
    HttpServerResponse* resp = (HttpServerResponse*)resp_ptr;
    SxString* name = (SxString*)name_ptr;
    SxString* value = (SxString*)value_ptr;
    
    if (!resp || !name || !value || !name->data || !value->data) return;
    
    HttpHeader* h = malloc(sizeof(HttpHeader));
    h->name = strdup(name->data);
    h->value = strdup(value->data);
    h->next = resp->headers;
    resp->headers = h;
}

// Set response body
void http_server_response_body(int64_t resp_ptr, int64_t body_ptr) {
    HttpServerResponse* resp = (HttpServerResponse*)resp_ptr;
    SxString* body = (SxString*)body_ptr;
    
    if (!resp) return;
    
    if (resp->body) free(resp->body);
    if (body && body->data) {
        resp->body = strdup(body->data);
        resp->body_len = body->len;
    } else {
        resp->body = NULL;
        resp->body_len = 0;
    }
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
void http_server_stop(int64_t server_ptr) {
    HttpServer* server = (HttpServer*)server_ptr;
    if (server) server->running = 0;
}

// Close server
void http_server_close(int64_t server_ptr) {
    HttpServer* server = (HttpServer*)server_ptr;
    if (!server) return;
    
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

// --------------------------------------------------------------------------
// 26.3 Persistent Storage (SQLite)
// --------------------------------------------------------------------------

#include <sqlite3.h>

typedef struct MemoryDB {
    sqlite3* db;
    char* path;
    pthread_mutex_t lock;
} MemoryDB;

// Create memory database
int64_t memdb_new(int64_t path_ptr) {
    SxString* path = (SxString*)path_ptr;
    
    MemoryDB* mdb = (MemoryDB*)malloc(sizeof(MemoryDB));
    if (!mdb) return 0;
    
    const char* db_path = path ? path->data : ":memory:";
    
    if (sqlite3_open(db_path, &mdb->db) != SQLITE_OK) {
        free(mdb);
        return 0;
    }
    
    mdb->path = path ? strdup(path->data) : strdup(":memory:");
    pthread_mutex_init(&mdb->lock, NULL);
    
    // Create schema
    const char* schema = 
        "CREATE TABLE IF NOT EXISTS memories ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  content TEXT NOT NULL,"
        "  embedding BLOB,"
        "  importance REAL DEFAULT 0.5,"
        "  created_at INTEGER DEFAULT (strftime('%s', 'now')),"
        "  accessed_at INTEGER DEFAULT (strftime('%s', 'now')),"
        "  access_count INTEGER DEFAULT 0,"
        "  cluster_id INTEGER,"
        "  archived INTEGER DEFAULT 0"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_importance ON memories(importance);"
        "CREATE INDEX IF NOT EXISTS idx_cluster ON memories(cluster_id);";
    
    char* err = NULL;
    sqlite3_exec(mdb->db, schema, NULL, NULL, &err);
    if (err) sqlite3_free(err);
    
    return (int64_t)mdb;
}

// Store memory
int64_t memdb_store(int64_t mdb_ptr, int64_t content_ptr, int64_t emb_ptr, double importance) {
    MemoryDB* mdb = (MemoryDB*)mdb_ptr;
    SxString* content = (SxString*)content_ptr;
    Embedding* emb = (Embedding*)emb_ptr;
    if (!mdb || !content) return 0;
    
    pthread_mutex_lock(&mdb->lock);
    
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO memories (content, embedding, importance) VALUES (?, ?, ?)";
    
    if (sqlite3_prepare_v2(mdb->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&mdb->lock);
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, content->data, content->len, SQLITE_STATIC);
    
    if (emb) {
        sqlite3_bind_blob(stmt, 2, emb->values, emb->dim * sizeof(double), SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 2);
    }
    
    sqlite3_bind_double(stmt, 3, importance);
    
    int result = sqlite3_step(stmt);
    int64_t id = 0;
    if (result == SQLITE_DONE) {
        id = sqlite3_last_insert_rowid(mdb->db);
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&mdb->lock);
    
    return id;
}

// Retrieve memory by ID
int64_t memdb_get(int64_t mdb_ptr, int64_t id) {
    MemoryDB* mdb = (MemoryDB*)mdb_ptr;
    if (!mdb) return 0;
    
    pthread_mutex_lock(&mdb->lock);
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT content FROM memories WHERE id = ?";
    
    if (sqlite3_prepare_v2(mdb->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&mdb->lock);
        return 0;
    }
    
    sqlite3_bind_int64(stmt, 1, id);
    
    int64_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* text = (const char*)sqlite3_column_text(stmt, 0);
        if (text) {
            result = (int64_t)intrinsic_string_new((char*)text);
        }
        
        // Update access time and count
        sqlite3_stmt* update;
        const char* update_sql = "UPDATE memories SET accessed_at = strftime('%s', 'now'), access_count = access_count + 1 WHERE id = ?";
        if (sqlite3_prepare_v2(mdb->db, update_sql, -1, &update, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(update, 1, id);
            sqlite3_step(update);
            sqlite3_finalize(update);
        }
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&mdb->lock);
    
    return result;
}

// Get memory importance
double memdb_get_importance(int64_t mdb_ptr, int64_t id) {
    MemoryDB* mdb = (MemoryDB*)mdb_ptr;
    if (!mdb) return 0.0;
    
    pthread_mutex_lock(&mdb->lock);
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT importance FROM memories WHERE id = ?";
    
    double result = 0.0;
    if (sqlite3_prepare_v2(mdb->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = sqlite3_column_double(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    pthread_mutex_unlock(&mdb->lock);
    return result;
}

// Update memory importance
int64_t memdb_set_importance(int64_t mdb_ptr, int64_t id, double importance) {
    MemoryDB* mdb = (MemoryDB*)mdb_ptr;
    if (!mdb) return 0;
    
    pthread_mutex_lock(&mdb->lock);
    
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE memories SET importance = ? WHERE id = ?";
    
    int result = 0;
    if (sqlite3_prepare_v2(mdb->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_double(stmt, 1, importance);
        sqlite3_bind_int64(stmt, 2, id);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            result = 1;
        }
        sqlite3_finalize(stmt);
    }
    
    pthread_mutex_unlock(&mdb->lock);
    return result;
}

// Count memories
int64_t memdb_count(int64_t mdb_ptr) {
    MemoryDB* mdb = (MemoryDB*)mdb_ptr;
    if (!mdb) return 0;
    
    pthread_mutex_lock(&mdb->lock);
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT COUNT(*) FROM memories WHERE archived = 0";
    
    int64_t count = 0;
    if (sqlite3_prepare_v2(mdb->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    pthread_mutex_unlock(&mdb->lock);
    return count;
}

// Close database
void memdb_close(int64_t mdb_ptr) {
    MemoryDB* mdb = (MemoryDB*)mdb_ptr;
    if (!mdb) return;
    
    pthread_mutex_lock(&mdb->lock);
    sqlite3_close(mdb->db);
    if (mdb->path) free(mdb->path);
    pthread_mutex_unlock(&mdb->lock);
    pthread_mutex_destroy(&mdb->lock);
    free(mdb);
}

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
// 26.5 Importance-Based Pruning
// --------------------------------------------------------------------------

typedef struct PruneConfig {
    double min_importance;
    int64_t max_age_seconds;
    int64_t max_memories;
    int preserve_clusters;
} PruneConfig;

// Create prune config
int64_t prune_config_new(void) {
    PruneConfig* cfg = (PruneConfig*)malloc(sizeof(PruneConfig));
    if (!cfg) return 0;
    
    cfg->min_importance = 0.1;
    cfg->max_age_seconds = 86400 * 30;  // 30 days
    cfg->max_memories = 10000;
    cfg->preserve_clusters = 1;
    
    return (int64_t)cfg;
}

// Set minimum importance threshold
int64_t prune_set_min_importance(int64_t cfg_ptr, double min_importance) {
    PruneConfig* cfg = (PruneConfig*)cfg_ptr;
    if (!cfg) return 0;
    cfg->min_importance = min_importance;
    return 1;
}

// Set max age
int64_t prune_set_max_age(int64_t cfg_ptr, int64_t max_age_seconds) {
    PruneConfig* cfg = (PruneConfig*)cfg_ptr;
    if (!cfg) return 0;
    cfg->max_age_seconds = max_age_seconds;
    return 1;
}

// Set max memories
int64_t prune_set_max_memories(int64_t cfg_ptr, int64_t max_memories) {
    PruneConfig* cfg = (PruneConfig*)cfg_ptr;
    if (!cfg) return 0;
    cfg->max_memories = max_memories;
    return 1;
}

// Execute pruning
int64_t prune_execute(int64_t mdb_ptr, int64_t cfg_ptr) {
    MemoryDB* mdb = (MemoryDB*)mdb_ptr;
    PruneConfig* cfg = (PruneConfig*)cfg_ptr;
    if (!mdb || !cfg) return 0;
    
    pthread_mutex_lock(&mdb->lock);
    
    // Archive old low-importance memories
    char sql[512];
    snprintf(sql, sizeof(sql),
             "UPDATE memories SET archived = 1 "
             "WHERE archived = 0 AND importance < %f "
             "AND (strftime('%%s', 'now') - created_at) > %lld",
             cfg->min_importance, (long long)cfg->max_age_seconds);
    
    char* err = NULL;
    sqlite3_exec(mdb->db, sql, NULL, NULL, &err);
    if (err) sqlite3_free(err);
    
    // Count archived
    sqlite3_stmt* stmt;
    int64_t archived = 0;
    if (sqlite3_prepare_v2(mdb->db, "SELECT changes()", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            archived = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    pthread_mutex_unlock(&mdb->lock);
    return archived;
}

// Free config
void prune_config_free(int64_t cfg_ptr) {
    PruneConfig* cfg = (PruneConfig*)cfg_ptr;
    if (cfg) free(cfg);
}

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
// 29.6 Tool Registry
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
